// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "reader.h"

#include "byte_source.h"
#include "serd_internal.h"
#include "stack.h"
#include "system.h"

#include "serd/memory.h"
#include "serd/stream.h"
#include "serd/uri.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SerdStatus
r_err(SerdReader* const reader, const SerdStatus st, const char* const fmt, ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)
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
  const char* prefix = reader->bprefix ? (const char*)reader->bprefix : "";
  node->n_bytes      = (size_t)snprintf(
    (char*)node->buf, buf_size, "%sb%u", prefix, reader->next_id++);
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

/** fread-like wrapper for getc (which is faster). */
static size_t
serd_file_read_byte(void* const  buf,
                    const size_t size,
                    const size_t nmemb,
                    void* const  stream)
{
  (void)size;
  (void)nmemb;

  const int c = getc((FILE*)stream);
  if (c == EOF) {
    *((uint8_t*)buf) = 0;
    return 0;
  }
  *((uint8_t*)buf) = (uint8_t)c;
  return 1;
}

Ref
push_node_padded(SerdReader* const  reader,
                 const size_t       maxlen,
                 const SerdNodeType type,
                 const char* const  str,
                 const size_t       n_bytes)
{
  void* mem = serd_stack_push_aligned(
    &reader->stack, sizeof(SerdNode) + maxlen + 1, sizeof(SerdNode));

  SerdNode* const node = (SerdNode*)mem;
  node->n_bytes        = n_bytes;
  node->flags          = 0;
  node->type           = type;
  node->buf            = NULL;

  char* buf = (char*)(node + 1);
  memcpy(buf, str, n_bytes + 1);

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
          const size_t       n_bytes)
{
  return push_node_padded(reader, n_bytes, type, str, n_bytes);
}

SerdNode*
deref(SerdReader* const reader, const Ref ref)
{
  if (ref) {
    SerdNode* node = (SerdNode*)(reader->stack.buf + ref);
    node->buf      = (char*)node + sizeof(SerdNode);
    return node;
  }
  return NULL;
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
emit_statement(SerdReader* const reader,
               const ReadContext ctx,
               const Ref         o,
               const Ref         d,
               const Ref         l)
{
  const SerdStatus st = !reader->statement_func
                          ? SERD_SUCCESS
                          : reader->statement_func(reader->handle,
                                                   *ctx.flags,
                                                   deref(reader, ctx.graph),
                                                   deref(reader, ctx.subject),
                                                   deref(reader, ctx.predicate),
                                                   deref(reader, o),
                                                   deref(reader, d),
                                                   deref(reader, l));

  *ctx.flags = 0;
  return st;
}

static SerdStatus
read_doc(SerdReader* const reader)
{
  return ((reader->syntax == SERD_NQUADS) ? read_nquadsDoc(reader)
                                          : read_turtleTrigDoc(reader));
}

SerdReader*
serd_reader_new(const SerdSyntax      syntax,
                const SerdReaderFlags flags,
                void* const           handle,
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
  me->stack          = serd_stack_new(SERD_PAGE_SIZE);
  me->syntax         = syntax;
  me->next_id        = 1;
  me->strict         = !(flags & SERD_READ_LAX);

  me->rdf_first = push_node(me, SERD_URI, NS_RDF "first", 48);
  me->rdf_rest  = push_node(me, SERD_URI, NS_RDF "rest", 47);
  me->rdf_nil   = push_node(me, SERD_URI, NS_RDF "nil", 46);

  return me;
}

void
serd_reader_set_error_sink(SerdReader* const reader,
                           const SerdLogFunc error_func,
                           void* const       error_handle)
{
  assert(reader);
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
  assert(reader);
  return reader->handle;
}

void
serd_reader_add_blank_prefix(SerdReader* const reader, const char* const prefix)
{
  assert(reader);

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

SerdStatus
serd_reader_read_file(SerdReader* const reader, const char* const uri)
{
  assert(reader);
  assert(uri);

  char* const path = serd_parse_file_uri(uri, NULL);
  if (!path) {
    return SERD_BAD_ARG;
  }

  FILE* fd = serd_fopen(path, "rb");
  if (!fd) {
    serd_free(path);
    return SERD_BAD_STREAM;
  }

  SerdStatus ret = serd_reader_read_file_handle(reader, fd, path);
  fclose(fd);
  free(path);
  return ret;
}

static SerdStatus
skip_bom(SerdReader* const me)
{
  if (serd_byte_source_peek(&me->source) == 0xEF) {
    if (serd_byte_source_advance(&me->source) ||
        serd_byte_source_peek(&me->source) != 0xBB ||
        serd_byte_source_advance(&me->source) ||
        serd_byte_source_peek(&me->source) != 0xBF ||
        serd_byte_source_advance(&me->source)) {
      r_err(me, SERD_BAD_SYNTAX, "corrupt byte order mark\n");
      return SERD_BAD_SYNTAX;
    }
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_reader_start_stream(SerdReader* const reader,
                         FILE* const       file,
                         const char* const name,
                         const bool        bulk)
{
  assert(reader);
  assert(file);

  return serd_reader_start_source_stream(reader,
                                         bulk ? (SerdReadFunc)fread
                                              : serd_file_read_byte,
                                         (SerdErrorFunc)ferror,
                                         file,
                                         name,
                                         bulk ? SERD_PAGE_SIZE : 1);
}

SerdStatus
serd_reader_start_source_stream(SerdReader* const   reader,
                                const SerdReadFunc  read_func,
                                const SerdErrorFunc error_func,
                                void* const         stream,
                                const char* const   name,
                                const size_t        page_size)
{
  assert(reader);
  assert(read_func);
  assert(error_func);

  return serd_byte_source_open_source(
    &reader->source, read_func, error_func, stream, name, page_size);
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
  assert(reader);

  SerdStatus st = SERD_SUCCESS;
  if (!reader->source.prepared) {
    st = serd_reader_prepare(reader);
  } else if (reader->source.eof) {
    st = serd_byte_source_advance(&reader->source);
  }

  if (peek_byte(reader) == 0) {
    // Skip leading null byte, for reading from a null-delimited socket
    st = skip_byte(reader, 0);
  }

  return st                                ? st
         : (reader->syntax == SERD_NQUADS) ? read_nquads_statement(reader)
                                           : read_n3_statement(reader);
}

SerdStatus
serd_reader_end_stream(SerdReader* const reader)
{
  assert(reader);

  return serd_byte_source_close(&reader->source);
}

SerdStatus
serd_reader_read_file_handle(SerdReader* const reader,
                             FILE* const       file,
                             const char* const name)
{
  assert(reader);
  assert(file);

  return serd_reader_read_source(reader,
                                 (SerdReadFunc)fread,
                                 (SerdErrorFunc)ferror,
                                 file,
                                 name,
                                 SERD_PAGE_SIZE);
}

SerdStatus
serd_reader_read_source(SerdReader* const   reader,
                        const SerdReadFunc  source,
                        const SerdErrorFunc error,
                        void* const         stream,
                        const char* const   name,
                        const size_t        page_size)
{
  assert(reader);
  assert(source);
  assert(error);

  SerdStatus st = serd_reader_start_source_stream(
    reader, source, error, stream, name, page_size);

  if (st || (st = serd_reader_prepare(reader))) {
    serd_reader_end_stream(reader);
    return st;
  }

  if ((st = read_doc(reader))) {
    serd_reader_end_stream(reader);
    return st;
  }

  return serd_reader_end_stream(reader);
}

SerdStatus
serd_reader_read_string(SerdReader* const reader, const char* const utf8)
{
  assert(reader);
  assert(utf8);

  serd_byte_source_open_string(&reader->source, utf8);

  SerdStatus st = serd_reader_prepare(reader);
  if (!st) {
    st = read_doc(reader);
  }

  serd_byte_source_close(&reader->source);

  return st;
}
