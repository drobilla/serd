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
set_blank_id(SerdReader* const  reader,
             TokenHeader* const node,
             const size_t       buf_size)
{
  char* const       buf    = (char*)(node + 1);
  const char* const prefix = reader->bprefix ? reader->bprefix : "";

  node->length =
    (uint32_t)snprintf(buf, buf_size, "%sb%u", prefix, reader->next_id++);
}

bool
tolerate_status(const SerdReader* const reader, const SerdStatus status)
{
  if (status == SERD_SUCCESS || status == SERD_FAILURE) {
    return true;
  }

  if (status == SERD_BAD_STREAM || status == SERD_BAD_STACK ||
      status == SERD_BAD_WRITE || status == SERD_NO_DATA) {
    return false;
  }

  return !reader->strict;
}

size_t
genid_size(const SerdReader* const reader)
{
  return reader->bprefix_len + 1 + 10 + 1; // + "b" + UINT32_MAX + \0
}

TokenHeader*
blank_id(SerdReader* const reader)
{
  TokenHeader* const node =
    push_node_space(reader, SERD_BLANK, genid_size(reader));
  if (node) {
    set_blank_id(reader, node, genid_size(reader));
  }
  return node;
}

static TokenHeader*
push_node_padded(SerdReader* const   reader,
                 const size_t        maxlen,
                 const SerdNodeType  type,
                 const ZixStringView string)
{
  // Push a null byte to ensure the previous node was null terminated
  char* terminator = (char*)serd_stack_push(&reader->stack, 1);
  if (!terminator) {
    return NULL;
  }
  *terminator = 0;

  void* mem = serd_stack_push_aligned(
    &reader->stack, sizeof(TokenHeader) + maxlen + 1U, sizeof(TokenHeader));
  if (!mem) {
    return NULL;
  }

  TokenHeader* const node = (TokenHeader*)mem;

  node->type   = type;
  node->flags  = 0U;
  node->length = (uint32_t)string.length;

  char* const buf = (char*)(node + 1U);
  memcpy(buf, string.data, string.length + 1U);

  return node;
}

TokenHeader*
push_node(SerdReader* const   reader,
          const SerdNodeType  type,
          const ZixStringView string)
{
  return push_node_padded(reader, string.length, type, string);
}

TokenHeader*
push_node_head(SerdReader* const reader, const SerdNodeType type)
{
  return push_node_padded(reader, 0U, type, zix_empty_string());
}

TokenHeader*
push_node_space(SerdReader* const  reader,
                const SerdNodeType type,
                const size_t       size)
{
  return push_node_padded(reader, size, type, zix_empty_string());
}

bool
pop_last_node_char(SerdReader* const reader, TokenHeader* const header)
{
  assert(header);
  --header->length;
  serd_stack_pop(&reader->stack, 1);
  return !!header;
}

SerdTokenView
stack_token_view(const TokenHeader* const header)
{
  if (!header) {
    return serd_no_token();
  }

  const SerdTokenView view = {header->type,
                              {(const char*)(header + 1U), header->length}};
  return view;
}

SerdStatus
emit_event(const SerdReader* const reader, SerdEvent event)
{
  event.caret = reader->source.caret;

  return serd_sink_event(reader->sink, event);
}

SerdStatus
emit_statement(const SerdReader* const  reader,
               const ReadContext        ctx,
               const TokenHeader* const object,
               const TokenHeader* const meta)
{
  const SerdObjectView object_view = {
    object->type,
    {(const char*)(object + 1U), object->length},
    object->flags,
    stack_token_view(meta)};

  const SerdStatus st = emit_event(
    reader,
    serd_statement_event(*ctx.flags,
                         serd_quad_view(stack_token_view(ctx.subject),
                                        stack_token_view(ctx.predicate),
                                        object_view,
                                        stack_token_view(ctx.graph))));

  *ctx.flags = 0U;
  return st;
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  assert(reader);

  if (reader->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_SUCCESS;
  }

  if (!reader->source.prepared) {
    const SerdStatus st = serd_reader_prepare(reader);
    if (st) {
      return st;
    }
  }

  return (reader->syntax == SERD_SYNTAX_EMPTY) ? SERD_SUCCESS
         : (reader->syntax == SERD_NQUADS)     ? read_nquadsDoc(reader)
                                               : read_turtleTrigDoc(reader);
}

SerdReader*
serd_reader_new(SerdWorld* const      world,
                const SerdSyntax      syntax,
                const SerdReaderFlags flags,
                const SerdSink* const sink)
{
  assert(world);
  assert(sink);

  ZixAllocator* const allocator  = serd_world_allocator(world);
  const SerdLimits    limits     = serd_world_limits(world);
  const size_t        stack_size = limits.reader_stack_size;
  if (stack_size < 198U) {
    return NULL;
  }

  SerdReader* const me =
    (SerdReader*)zix_calloc(allocator, 1U, sizeof(SerdReader));
  if (!me) {
    return NULL;
  }

  me->world   = world;
  me->sink    = sink;
  me->stack   = serd_stack_new(allocator, stack_size);
  me->syntax  = syntax;
  me->next_id = 1;
  me->strict  = !(flags & SERD_READ_LAX);

  if (!me->stack.buf) {
    zix_free(world->allocator, me);
    return NULL;
  }

  // Reserve a bit of space at the end of the stack to zero pad nodes
  me->stack.buf_size -= sizeof(void*);

  me->rdf_first = push_node(me, SERD_URI, serd_symbols[RDF_FIRST]);
  me->rdf_rest  = push_node(me, SERD_URI, serd_symbols[RDF_REST]);
  me->rdf_nil   = push_node(me, SERD_URI, serd_symbols[RDF_NIL]);

  // The initial stack size check should cover this
  assert(me->rdf_first);
  assert(me->rdf_rest);
  assert(me->rdf_nil);

  return me;
}

void
serd_reader_free(SerdReader* const reader)
{
  if (!reader) {
    return;
  }

  serd_reader_finish(reader);

  serd_stack_free(serd_world_allocator(reader->world), &reader->stack);
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
    r_err(reader, st, "error preparing read: %s", strerror(errno));
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

  return st                                ? st
         : (reader->syntax == SERD_NQUADS) ? read_nquads_statement(reader)
                                           : read_n3_statement(reader);
}

SerdStatus
serd_reader_finish(SerdReader* const reader)
{
  assert(reader);

  return serd_byte_source_close(reader->world->allocator, &reader->source);
}
