// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "reader.h"

#include "byte_source.h"
#include "stack.h"
#include "symbols.h"
#include "system.h"
#include "world_impl.h"
#include "world_internal.h"

#include <serd/error.h>
#include <serd/event.h>
#include <serd/file_uri.h>
#include <serd/node.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/reader.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/stream.h>
#include <serd/syntax.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static SerdStatus
serd_reader_prepare(SerdReader* reader);

SerdStatus
r_err(const SerdReader* const reader,
      const SerdStatus        st,
      const char* const       fmt,
      ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);
  const SerdError e = {st, reader->source.caret, fmt, &args};
  serd_world_error(reader->world, &e);
  va_end(args);
  return st;
}

SerdStatus
r_err_char(const SerdReader* const reader, const char* const kind, const int c)
{
  const SerdStatus st = SERD_BAD_SYNTAX;

  return (c < 0x20 || c == 0x7F) ? r_err(reader, st, "bad %s character", kind)
         : (c == '\'' || c >= 0x80)
           ? r_err(reader, st, "bad %s character U+%04X", kind, (uint32_t)c)
           : r_err(reader, st, "bad %s character '%c'", kind, c);
}

void
set_blank_id(SerdReader* const reader, const Ref ref, const size_t buf_size)
{
  SerdNode*   node   = deref(reader, ref);
  const char* prefix = reader->bprefix ? reader->bprefix : "";
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
  Ref ref = push_node_space(reader, genid_size(reader), SERD_BLANK);
  set_blank_id(reader, ref, genid_size(reader));
  return ref;
}

static Ref
push_node_padded(SerdReader* const   reader,
                 const size_t        maxlen,
                 const SerdNodeType  type,
                 const ZixStringView string)
{
  void* mem = serd_stack_push_aligned(
    &reader->stack, sizeof(SerdNode) + maxlen + 1, sizeof(SerdNode));

  SerdNode* const node = (SerdNode*)mem;
  node->n_bytes        = string.length;
  node->flags          = 0;
  node->type           = type;
  node->buf            = NULL;

  char* buf = (char*)(node + 1);
  memcpy(buf, string.data, string.length + 1);

  return (Ref)((char*)node - reader->stack.buf);
}

Ref
push_node(SerdReader* const   reader,
          const SerdNodeType  type,
          const ZixStringView string)
{
  return push_node_padded(reader, string.length, type, string);
}

Ref
push_node_head(SerdReader* const reader, const SerdNodeType type)
{
  return push_node_padded(reader, 0U, type, zix_empty_string());
}

Ref
push_node_space(SerdReader* const  reader,
                const size_t       size,
                const SerdNodeType type)
{
  return push_node_padded(reader, size, type, zix_empty_string());
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

bool
pop_last_node_char(SerdReader* const reader, SerdNode* const node)
{
  --node->n_bytes;
  serd_stack_pop(&reader->stack, 1);
  return true;
}

Ref
pop_node(SerdReader* const reader, const Ref ref)
{
  if (ref && ref != reader->rdf_first && ref != reader->rdf_rest &&
      ref != reader->rdf_nil) {
    const SerdNode* const node = deref(reader, ref);
    const char* const     top  = reader->stack.buf + reader->stack.size;
    assert(top > (const char*)node);
    serd_stack_pop_aligned(&reader->stack, (size_t)(top - (const char*)node));
  }
  return 0;
}

static SerdTokenView
stack_token_view(SerdReader* const reader, const Ref ref)
{
  static const SerdTokenView no_tok = {SERD_LITERAL, {"", 0U}};

  const SerdNode* const object = deref(reader, ref);
  if (!object) {
    return no_tok;
  }

  const SerdTokenView view = {object->type,
                              {(const char*)(object + 1U), object->n_bytes}};
  return view;
}

SerdStatus
emit_event(const SerdReader* const reader, SerdEvent event)
{
  event.caret = reader->source.caret;

  return serd_sink_event(reader->sink, event);
}

SerdStatus
emit_statement(SerdReader* const reader,
               const ReadContext ctx,
               const Ref         o,
               const Ref         m)
{
  SerdStatus st = SERD_UNKNOWN_ERROR;

  const SerdNode* const object = deref(reader, o);
  if (object) {
    const SerdObjectView object_view = {object->type,
                                        serd_node_string_view(object),
                                        object->flags,
                                        stack_token_view(reader, m)};

    st = emit_event(reader,
                    serd_statement_event(
                      *ctx.flags,
                      serd_quad_view(stack_token_view(reader, ctx.subject),
                                     stack_token_view(reader, ctx.predicate),
                                     object_view,
                                     stack_token_view(reader, ctx.graph))));
  }

  *ctx.flags = 0U;
  return st;
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  assert(reader);

#ifndef NDEBUG
  const size_t orig_stack_size = reader->stack.size;
#endif

  if (reader->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_SUCCESS;
  }

  SerdStatus st = SERD_SUCCESS;
  if (!reader->source.prepared) {
    if ((st = serd_reader_prepare(reader))) {
      return st;
    }
  }

  st = (reader->syntax == SERD_SYNTAX_EMPTY) ? SERD_SUCCESS
       : (reader->syntax == SERD_NQUADS)     ? read_nquadsDoc(reader)
                                             : read_turtleTrigDoc(reader);

  assert(reader->stack.size == orig_stack_size);
  return st;
}

SerdReader*
serd_reader_new(SerdWorld* const      world,
                const SerdSyntax      syntax,
                const SerdReaderFlags flags,
                const SerdSink* const sink)
{
  assert(world);
  assert(sink);

  ZixAllocator* const allocator = serd_world_allocator(world);

  SerdReader* const me =
    (SerdReader*)zix_calloc(allocator, 1U, sizeof(SerdReader));
  if (!me) {
    return NULL;
  }

  me->world   = world;
  me->sink    = sink;
  me->stack   = serd_stack_new(world->allocator, SERD_PAGE_SIZE);
  me->syntax  = syntax;
  me->next_id = 1;
  me->strict  = !(flags & SERD_READ_LAX);

  if (!me->stack.buf) {
    zix_free(world->allocator, me);
    return NULL;
  }

  me->rdf_first = push_node(me, SERD_URI, serd_symbols[RDF_FIRST]);
  me->rdf_rest  = push_node(me, SERD_URI, serd_symbols[RDF_REST]);
  me->rdf_nil   = push_node(me, SERD_URI, serd_symbols[RDF_NIL]);

  return me;
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
  serd_reader_finish(reader);

  serd_stack_free(&reader->stack);
  zix_free(reader->world->allocator, reader->bprefix);
  zix_free(reader->world->allocator, reader);
}

void
serd_reader_add_blank_prefix(SerdReader* const reader, const char* const prefix)
{
  assert(reader);

  zix_free(reader->world->allocator, reader->bprefix);
  reader->bprefix_len = 0;
  reader->bprefix     = NULL;

  const size_t prefix_len = prefix ? strlen(prefix) : 0;
  if (prefix_len) {
    reader->bprefix_len = prefix_len;
    reader->bprefix =
      (char*)zix_malloc(reader->world->allocator, reader->bprefix_len + 1);
    memcpy(reader->bprefix, prefix, reader->bprefix_len + 1);
  }
}

SerdStatus
serd_reader_start_stream(SerdReader* const   reader,
                         const SerdReadFunc  read_func,
                         const SerdErrorFunc error_func,
                         void* const         stream,
                         const ZixStringView name,
                         const size_t        page_size)
{
  assert(reader);
  assert(read_func);
  assert(error_func);

  return serd_byte_source_open_source(reader->world->allocator,
                                      &reader->source,
                                      read_func,
                                      error_func,
                                      NULL,
                                      stream,
                                      name,
                                      page_size);
}

SerdStatus
serd_reader_start_file(SerdReader* reader, const char* uri, bool bulk)
{
  assert(reader);
  assert(uri);

  char* const path = serd_parse_file_uri(NULL, uri, NULL);
  if (!path) {
    return SERD_BAD_ARG;
  }

  FILE* fd = serd_world_fopen(reader->world, path, "rb");
  zix_free(NULL, path);
  if (!fd) {
    return SERD_BAD_STREAM;
  }

  const SerdStatus st = serd_byte_source_open_source(
    reader->world->allocator,
    &reader->source,
    bulk ? (SerdReadFunc)fread : serd_file_read_byte,
    (SerdErrorFunc)ferror,
    (SerdCloseFunc)fclose,
    fd,
    zix_string(uri),
    bulk ? SERD_PAGE_SIZE : 1U);

  return st;
}

SerdStatus
serd_reader_start_string(SerdReader* const   reader,
                         const char* const   utf8,
                         const ZixStringView name)
{
  assert(reader);
  assert(utf8);
  return serd_byte_source_open_string(
    reader->world->allocator, &reader->source, utf8, name);
}

static SerdStatus
serd_reader_prepare(SerdReader* const reader)
{
  SerdStatus st = serd_byte_source_prepare(&reader->source);
  if (st == SERD_SUCCESS) {
    if ((st = serd_byte_source_skip_bom(&reader->source))) {
      r_err(reader, st, "corrupt byte order mark");
    }
  } else if (st == SERD_FAILURE) {
    reader->source.eof = true;
  } else {
    r_err(reader, st, "read error: %s", strerror(errno));
  }
  return st;
}

SerdStatus
serd_reader_read_chunk(SerdReader* const reader)
{
  assert(reader);

  if (reader->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_FAILURE;
  }

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

#ifndef NDEBUG
  const size_t orig_stack_size = reader->stack.size;
#endif

  if (!st) {
    st = (reader->syntax == SERD_NQUADS) ? read_nquads_statement(reader)
                                         : read_n3_statement(reader);
  }

  assert(reader->stack.size == orig_stack_size);
  return st;
}

SerdStatus
serd_reader_finish(SerdReader* const reader)
{
  assert(reader);

  return serd_byte_source_close(reader->world->allocator, &reader->source);
}
