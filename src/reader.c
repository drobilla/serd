// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_source.h"
#include "read_context.h"
#include "read_nquads.h"
#include "read_ntriples.h"
#include "read_trig.h"
#include "read_turtle.h"
#include "reader_impl.h"
#include "reader_internal.h"
#include "stack.h"
#include "string_utils.h"
#include "symbols.h"
#include "token_header.h"
#include "world_internal.h"

#include <serd/caret_view.h>
#include <serd/error.h>
#include <serd/event.h>
#include <serd/input_stream.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/reader.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/syntax.h>
#include <serd/token_view.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
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
  const SerdError e = {st, reader->source->caret, fmt, &args};
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
serd_reader_set_blank_id(SerdReader* const  reader,
                         TokenHeader* const node,
                         const size_t       buf_size)
{
  const uint32_t id  = reader->next_id++;
  char* const    buf = (char*)(node + 1U);
  size_t         i   = reader->bprefix_len;

  memcpy(buf, reader->bprefix, reader->bprefix_len);
  buf[i++] = 'b';

  const int rc = snprintf(buf + i, buf_size - i, "%u", id);
  assert(rc > 0);
  node->length = (uint32_t)i + (uint32_t)rc;
}

size_t
genid_size(const SerdReader* const reader)
{
  return reader->bprefix_len + 1 + 10 + 1; // + "b" + UINT32_MAX + \0
}

TokenHeader*
serd_reader_blank_id(SerdReader* const reader)
{
  TokenHeader* const node =
    push_node_space(reader, SERD_BLANK, genid_size(reader));
  if (node) {
    serd_reader_set_blank_id(reader, node, genid_size(reader));
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
  event.caret = reader->source->caret;

  const SerdStatus st = serd_sink_event(reader->sink, event);
  return st == SERD_NO_CHANGE ? SERD_SUCCESS : st;
}

SerdStatus
emit_statement_at(const SerdReader* const  reader,
                  const ReadContext        ctx,
                  const TokenHeader* const object,
                  const TokenHeader* const meta,
                  const SerdCaretView      caret)
{
  assert(ctx.subject);
  assert(ctx.predicate);

  const SerdObjectView object_view = {
    object->type,
    {(const char*)(object + 1U), object->length},
    object->flags,
    stack_token_view(meta)};

  const SerdStatementView statement =
    serd_quad_view(stack_token_view(ctx.subject),
                   stack_token_view(ctx.predicate),
                   object_view,
                   stack_token_view(ctx.graph));

  const SerdEvent event = {SERD_EVENT_STATEMENT,
                           (uint16_t)*ctx.flags,
                           caret,
                           {.statement = statement}};

  const SerdStatus st = serd_sink_event(reader->sink, event);

  *ctx.flags = 0U;
  return st;
}

static SerdStatus
serd_reader_prepare_if_necessary(SerdReader* const reader)
{
  return !reader->source            ? SERD_BAD_CALL
         : reader->source->prepared ? SERD_SUCCESS
                                    : serd_reader_prepare(reader);
}

static SerdStatus
read_syntax_chunk(SerdReader* const reader)
{
  typedef SerdStatus (*SyntaxReadFunc)(SerdReader*);

  assert(reader->syntax < 5);
  static const SyntaxReadFunc read_funcs[5] = {
    NULL,
    read_turtle_chunk,
    read_ntriples_line,
    read_nquads_line,
    read_trig_chunk,
  };

  return read_funcs[reader->syntax](reader);
}

SerdStatus
emit_statement(const SerdReader* const  reader,
               const ReadContext        ctx,
               const TokenHeader* const o,
               const TokenHeader* const meta)
{
  return emit_statement_at(reader, ctx, o, meta, reader->source->caret);
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

  if (!(reader->flags & SERD_READ_GLOBAL)) {
    const uint32_t id   = serd_world_next_document_id(reader->world);
    char* const    buf  = reader->bprefix;
    const size_t   size = sizeof(reader->bprefix);
    const int      rc   = snprintf(buf, size - 1U, "f%u", id);
    assert(rc > 0);
    reader->bprefix_len = (size_t)rc;
  }

  while (st <= SERD_FAILURE && !reader->source->eof) {
    st = read_syntax_chunk(reader);
    if (st > SERD_FAILURE && !reader->strict) {
      serd_reader_skip_until_byte(reader, '\n');
      st = SERD_SUCCESS;
    }
  }

  return accept_failure(st);
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
  me->flags   = flags;
  me->next_id = 1;
  me->strict  = !(flags & SERD_READ_LAX);

  if (!me->stack.buf) {
    zix_free(allocator, me);
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

  if (!(flags & SERD_READ_GLOBAL)) {
    me->bprefix[0]  = 'f';
    me->bprefix[1]  = '0';
    me->bprefix_len = 2U;
  }

  return me;
}

void
serd_reader_free(SerdReader* const reader)
{
  if (!reader) {
    return;
  }

  ZixAllocator* const allocator = serd_world_allocator(reader->world);

  if (reader->source) {
    serd_reader_finish(reader);
  }

  serd_stack_free(serd_world_allocator(reader->world), &reader->stack);
  zix_free(allocator, reader);
}

SerdStatus
serd_reader_start(SerdReader* const      reader,
                  SerdInputStream* const input,
                  const ZixStringView    input_name,
                  const size_t           block_size)
{
  assert(reader);
  assert(input);

  if (!block_size || !input->stream) {
    return SERD_BAD_ARG;
  }

  if (reader->source) {
    return SERD_BAD_CALL;
  }

  ZixAllocator* const allocator = serd_world_allocator(reader->world);

  reader->source =
    serd_byte_source_new_input(allocator, input, input_name, block_size);

  return reader->source ? SERD_SUCCESS : SERD_BAD_ALLOC;
}

static SerdStatus
serd_reader_prepare(SerdReader* const reader)
{
  SerdStatus st = serd_byte_source_prepare(reader->source);
  if (st == SERD_SUCCESS) {
    if ((st = serd_byte_source_skip_bom(reader->source))) {
      r_err(reader, st, "corrupt byte order mark");
    }
  } else if (st == SERD_FAILURE) {
    reader->source->eof = true;
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
  if (!st && reader->source->eof) {
    st = serd_byte_source_advance(reader->source);
  }

  return st ? st : read_syntax_chunk(reader);
}

SerdStatus
serd_reader_finish(SerdReader* const reader)
{
  assert(reader);

  if (reader->source) {
    serd_byte_source_free(serd_world_allocator(reader->world), reader->source);
    reader->source = NULL;
  }

  return SERD_SUCCESS;
}
