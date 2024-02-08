// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "reader.h"

#include "byte_source.h"
#include "read_nquads.h"
#include "read_ntriples.h"
#include "read_trig.h"
#include "read_turtle.h"
#include "stack.h"
#include "string_utils.h"
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

SerdStatus
r_err_eof(const SerdReader* const reader, const SerdStatus status)
{
  return r_err(reader, status, "unexpected end of input");
}

SerdStatus
r_err_expected(const SerdReader* const reader,
               const char* const       expected,
               const int               c)
{
  const SerdStatus st   = SERD_BAD_SYNTAX;
  const uint32_t   code = (uint32_t)c;

  return (c < 0x20 || c == 0x7F) ? r_err(reader, st, "expected %s", expected)
         : (c == '\'' || c >= 0x80)
           ? r_err(reader, st, "expected %s, not U+%04X", expected, code)
           : r_err(reader, st, "expected %s, not '%c'", expected, c);
}

SerdStatus
skip_horizontal_whitespace(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;

  for (int c = 0; !st && (c = peek_byte(reader)) && (c == '\t' || c == ' ');) {
    st = skip_byte(reader, c);
  }

  return st;
}

SerdStatus
serd_reader_skip_until_byte(SerdReader* const reader, const uint8_t byte)
{
  SerdStatus st = SERD_SUCCESS;
  int        c  = peek_byte(reader);

  while (!st && c > 0 && c != byte) {
    st = skip_byte(reader, c);
    c  = peek_byte(reader);
  }

  return st ? st : c > 0 ? SERD_FAILURE : SERD_SUCCESS;
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
push_node_start(SerdReader* const  reader,
                const SerdNodeType type,
                const size_t       body_size)
{
  const SerdStatus st = push_node_termination(reader);
  if (st) {
    return 0;
  }

  const size_t total_size = sizeof(TokenHeader) + body_size;
  void* const  mem        = serd_stack_push(&reader->stack, total_size);
  if (!mem) {
    return NULL;
  }

  TokenHeader* const node = (TokenHeader*)mem;
  node->length            = 0;
  node->flags             = 0U;
  node->type              = type;
  return node;
}

TokenHeader*
push_node_head(SerdReader* const reader, const SerdNodeType type)
{
  return push_node_start(reader, type, 0);
}

TokenHeader*
push_node_space(SerdReader* const  reader,
                const SerdNodeType type,
                const size_t       size)
{
  TokenHeader* const node = push_node_start(reader, type, size);
  if (node) {
    void* const body = serd_stack_push(&reader->stack, size);
    if (body) {
      ((char*)body)[0] = '\0';
    }
  }
  return node;
}

TokenHeader*
push_node(SerdReader* const   reader,
          const SerdNodeType  type,
          const ZixStringView string)
{
  assert(string.length);

  TokenHeader* const node = push_node_start(reader, type, string.length + 1);
  if (node) {
    char* const buf = (char*)(node + 1);
    memcpy(buf, string.data, string.length);
    buf[string.length] = '\0';
    node->length       = (uint32_t)string.length;
  }
  return node;
}

bool
pop_last_node_char(SerdReader* const reader, TokenHeader* const header)
{
  assert(header);
  --header->length;
  serd_stack_pop(&reader->stack, 1);
  return !!header;
}

SerdStatus
push_node_termination(SerdReader* const reader)
{
  // Push first mandatory null termination byte
  reader->stack.buf[reader->stack.size++] = '\0';

  // Push extra null bytes for 32-bit alignment
  const size_t top         = reader->stack.size;
  const size_t n_end_bytes = ((top + 3U) & ~0x03U) - top;
  void* const  end         = serd_stack_push(&reader->stack, n_end_bytes);
  if (!end) {
    return SERD_BAD_STACK;
  }

  memset(end, '\0', n_end_bytes);
  return SERD_SUCCESS;
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

bool
token_equals(const TokenHeader* const node, const ZixStringView tok)
{
  assert(node);
  if (node->length != tok.length) {
    return false;
  }

  const char* const node_string = (const char*)(node + 1U);
  for (size_t i = 0U; i < tok.length; ++i) {
    if (serd_to_upper(node_string[i]) != serd_to_upper(tok.data[i])) {
      return false;
    }
  }

  return true;
}

SerdStatus
emit_event(const SerdReader* const reader, SerdEvent event)
{
  event.caret = reader->source.caret;

  const SerdStatus st = serd_sink_event(reader->sink, event);
  return st == SERD_NO_CHANGE ? SERD_SUCCESS : st;
}

SerdStatus
emit_statement(const SerdReader* const  reader,
               const ReadContext        ctx,
               const TokenHeader* const object,
               const TokenHeader* const meta)
{
  assert(ctx.subject);
  assert(ctx.predicate);

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

static SerdStatus
serd_reader_prepare_if_necessary(SerdReader* const reader)
{
  return reader->source.prepared ? SERD_SUCCESS : serd_reader_prepare(reader);
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  assert(reader);

  if (reader->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_SUCCESS;
  }

  SerdStatus st = serd_reader_prepare_if_necessary(reader);
  if (st) {
    return st;
  }

  return (reader->syntax == SERD_TURTLE)     ? read_turtleDoc(reader)
         : (reader->syntax == SERD_NTRIPLES) ? read_ntriplesDoc(reader)
         : (reader->syntax == SERD_NQUADS)   ? read_nquadsDoc(reader)
         : (reader->syntax == SERD_TRIG)     ? read_trigDoc(reader)
                                             : SERD_SUCCESS;
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
  if (stack_size < 244U) {
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
  me->rdf_type  = push_node(me, SERD_URI, serd_symbols[RDF_TYPE]);

  // The initial stack size check should cover this
  assert(me->rdf_first);
  assert(me->rdf_rest);
  assert(me->rdf_nil);
  assert(me->rdf_type);

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
      r_err(reader, SERD_BAD_SYNTAX, "corrupt byte order mark");
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

  SerdStatus st = serd_reader_prepare_if_necessary(reader);
  if (!st && reader->source.eof) {
    st = serd_byte_source_advance(&reader->source);
  }

  return st                                  ? st
         : (reader->syntax == SERD_TURTLE)   ? read_turtle_statement(reader)
         : (reader->syntax == SERD_NTRIPLES) ? read_ntriples_line(reader)
         : (reader->syntax == SERD_NQUADS)   ? read_nquads_line(reader)
         : (reader->syntax == SERD_TRIG)     ? read_trig_statement(reader)
                                             : SERD_FAILURE;
}

SerdStatus
serd_reader_finish(SerdReader* const reader)
{
  assert(reader);

  return serd_byte_source_close(reader->world->allocator, &reader->source);
}
