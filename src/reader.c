/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "reader.h"
#include "byte_source.h"
#include "node.h"
#include "stack.h"
#include "system.h"

#include "serd_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SerdStatus
serd_reader_prepare(SerdReader* reader);

SerdStatus
r_err(SerdReader* const reader, const SerdStatus st, const char* const fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  const Cursor* const cur = &reader->source.cur;
  const SerdError     e = {st, cur->filename, cur->line, cur->col, fmt, &args};
  serd_error(reader->error_func, reader->error_handle, &e);
  va_end(args);
  return st;
}

void
set_blank_id(SerdReader* const reader, const Ref ref, const size_t buf_size)
{
  SerdNode*   node   = deref(reader, ref);
  char*       buf    = (char*)(node + 1);
  const char* prefix = reader->bprefix ? (const char*)reader->bprefix : "";

  node->length =
    (size_t)snprintf(buf, buf_size, "%sb%u", prefix, reader->next_id++);
}

size_t
genid_size(const SerdReader* const reader)
{
  return reader->bprefix_len + 1 + 10 + 1; // + "b" + UINT32_MAX + \0
}

Ref
blank_id(SerdReader* const reader)
{
  Ref ref = push_node_padded(reader, genid_size(reader), SERD_BLANK, "", 0);
  set_blank_id(reader, ref, genid_size(reader));
  return ref;
}

Ref
push_node_padded(SerdReader* const  reader,
                 const size_t       maxlen,
                 const SerdNodeType type,
                 const char* const  str,
                 const size_t       length)
{
  void* mem = serd_stack_push_aligned(
    &reader->stack, sizeof(SerdNode) + maxlen + 1, sizeof(SerdNode));

  SerdNode* const node = (SerdNode*)mem;

  node->length = length;
  node->flags  = 0;
  node->type   = type;

  char* buf = (char*)(node + 1);
  memcpy(buf, str, length + 1);

#ifdef SERD_STACK_CHECK
  reader->allocs                       = (Ref*)realloc(reader->allocs,
                                 sizeof(reader->allocs) * (++reader->n_allocs));
  reader->allocs[reader->n_allocs - 1] = ((char*)mem - reader->stack.buf);
#endif
  return (Ref)((char*)node - reader->stack.buf);
}

Ref
push_node(SerdReader* const  reader,
          const SerdNodeType type,
          const char* const  str,
          const size_t       length)
{
  return push_node_padded(reader, length, type, str, length);
}

SERD_PURE_FUNC
SerdNode*
deref(SerdReader* const reader, const Ref ref)
{
  return ref ? (SerdNode*)(reader->stack.buf + ref) : NULL;
}

Ref
pop_node(SerdReader* const reader, const Ref ref)
{
  if (ref && ref != reader->rdf_first && ref != reader->rdf_rest &&
      ref != reader->rdf_nil) {
#ifdef SERD_STACK_CHECK
    SERD_STACK_ASSERT_TOP(reader, ref);
    --reader->n_allocs;
#endif
    SerdNode* const node = deref(reader, ref);
    char* const     top  = reader->stack.buf + reader->stack.size;
    serd_stack_pop_aligned(&reader->stack, (size_t)(top - (char*)node));
  }
  return 0;
}

SerdStatus
emit_statement(SerdReader* const reader, const ReadContext ctx, const Ref o)
{
  SerdNode* graph = deref(reader, ctx.graph);
  if (!graph && reader->default_graph) {
    graph = reader->default_graph;
  }

  const SerdStatus st = !reader->statement_func
                          ? SERD_SUCCESS
                          : reader->statement_func(reader->handle,
                                                   *ctx.flags,
                                                   graph,
                                                   deref(reader, ctx.subject),
                                                   deref(reader, ctx.predicate),
                                                   deref(reader, o));

  *ctx.flags &= SERD_ANON_CONT | SERD_LIST_CONT; // Preserve only cont flags
  return st;
}

static SerdStatus
read_statement(SerdReader* const reader)
{
  return read_n3_statement(reader);
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  if (!reader->source.prepared) {
    SerdStatus st = serd_reader_prepare(reader);
    if (st) {
      return st;
    }
  }

  return ((reader->syntax == SERD_NQUADS) ? read_nquadsDoc(reader)
                                          : read_turtleTrigDoc(reader));
}

SerdReader*
serd_reader_new(const SerdSyntax syntax,
                void* const      handle,
                void (*const free_handle)(void*),
                const SerdBaseFunc      base_func,
                const SerdPrefixFunc    prefix_func,
                const SerdStatementFunc statement_func,
                const SerdEndFunc       end_func)
{
  SerdReader* me     = (SerdReader*)calloc(1, sizeof(SerdReader));
  me->handle         = handle;
  me->free_handle    = free_handle;
  me->base_func      = base_func;
  me->prefix_func    = prefix_func;
  me->statement_func = statement_func;
  me->end_func       = end_func;
  me->default_graph  = NULL;
  me->stack          = serd_stack_new(SERD_PAGE_SIZE);
  me->syntax         = syntax;
  me->next_id        = 1;
  me->strict         = true;

  me->rdf_first = push_node(me, SERD_URI, NS_RDF "first", 48);
  me->rdf_rest  = push_node(me, SERD_URI, NS_RDF "rest", 47);
  me->rdf_nil   = push_node(me, SERD_URI, NS_RDF "nil", 46);

  return me;
}

void
serd_reader_set_strict(SerdReader* const reader, const bool strict)
{
  reader->strict = strict;
}

void
serd_reader_set_error_sink(SerdReader* const   reader,
                           const SerdErrorFunc error_func,
                           void* const         error_handle)
{
  reader->error_func   = error_func;
  reader->error_handle = error_handle;
}

void
serd_reader_free(SerdReader* const reader)
{
  if (!reader) {
    return;
  }

  pop_node(reader, reader->rdf_nil);
  pop_node(reader, reader->rdf_rest);
  pop_node(reader, reader->rdf_first);
  serd_node_free(reader->default_graph);

#ifdef SERD_STACK_CHECK
  free(reader->allocs);
#endif
  free(reader->stack.buf);
  free(reader->bprefix);
  if (reader->free_handle) {
    reader->free_handle(reader->handle);
  }
  free(reader);
}

void*
serd_reader_handle(const SerdReader* const reader)
{
  return reader->handle;
}

void
serd_reader_add_blank_prefix(SerdReader* const reader, const char* const prefix)
{
  free(reader->bprefix);
  reader->bprefix_len = 0;
  reader->bprefix     = NULL;

  const size_t prefix_len = prefix ? strlen(prefix) : 0;
  if (prefix_len) {
    reader->bprefix_len = prefix_len;
    reader->bprefix     = (char*)malloc(reader->bprefix_len + 1);
    memcpy(reader->bprefix, prefix, reader->bprefix_len + 1);
  }
}

void
serd_reader_set_default_graph(SerdReader* const     reader,
                              const SerdNode* const graph)
{
  serd_node_free(reader->default_graph);
  reader->default_graph = serd_node_copy(graph);
}

SerdStatus
serd_reader_read_file(SerdReader* const reader, const char* const uri)
{
  char* const path = serd_parse_file_uri(uri, NULL);
  if (!path) {
    return SERD_ERR_BAD_ARG;
  }

  FILE* fd = serd_fopen(path, "rb");
  if (!fd) {
    serd_free(path);
    return SERD_ERR_UNKNOWN;
  }

  SerdStatus st = serd_reader_start_stream(reader,
                                           (SerdReadFunc)fread,
                                           (SerdStreamErrorFunc)ferror,
                                           fd,
                                           path,
                                           SERD_PAGE_SIZE);

  if (!st) {
    st = serd_reader_read_document(reader);
  }

  const SerdStatus est = serd_reader_end_stream(reader);

  fclose(fd);
  free(path);

  return st ? st : est;
}

static SerdStatus
skip_bom(SerdReader* const me)
{
  if (serd_byte_source_peek(&me->source) == 0xEF) {
    serd_byte_source_advance(&me->source);
    if (serd_byte_source_peek(&me->source) != 0xBB ||
        serd_byte_source_advance(&me->source) ||
        serd_byte_source_peek(&me->source) != 0xBF ||
        serd_byte_source_advance(&me->source)) {
      r_err(me, SERD_ERR_BAD_SYNTAX, "corrupt byte order mark\n");
      return SERD_ERR_BAD_SYNTAX;
    }
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_reader_start_stream(SerdReader* const         reader,
                         const SerdReadFunc        read_func,
                         const SerdStreamErrorFunc error_func,
                         void* const               stream,
                         const char* const         name,
                         const size_t              page_size)
{
  return serd_byte_source_open_source(
    &reader->source, read_func, error_func, stream, name, page_size);
}

SerdStatus
serd_reader_start_string(SerdReader* const reader, const char* const utf8)
{
  return serd_byte_source_open_string(&reader->source, utf8);
}

static SerdStatus
serd_reader_prepare(SerdReader* const reader)
{
  SerdStatus st = serd_byte_source_prepare(&reader->source);
  if (st == SERD_SUCCESS) {
    st = skip_bom(reader);
  } else if (st == SERD_FAILURE) {
    reader->source.eof = true;
  } else {
    r_err(reader, st, "read error: %s\n", strerror(errno));
  }
  return st;
}

SerdStatus
serd_reader_read_chunk(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;
  if (!reader->source.prepared) {
    st = serd_reader_prepare(reader);
  } else if (reader->source.eof) {
    st = serd_byte_source_advance(&reader->source);
  }

  if (peek_byte(reader) == 0) {
    // Skip leading null byte, for reading from a null-delimited socket
    eat_byte_safe(reader, 0);
  }

  return st ? st : read_statement(reader);
}

SerdStatus
serd_reader_end_stream(SerdReader* const reader)
{
  return serd_byte_source_close(&reader->source);
}

SerdStatus
serd_reader_read_string(SerdReader* const reader, const char* const utf8)
{
  serd_reader_start_string(reader, utf8);

  const SerdStatus st  = serd_reader_read_document(reader);
  const SerdStatus est = serd_byte_source_close(&reader->source);

  return st ? st : est;
}
