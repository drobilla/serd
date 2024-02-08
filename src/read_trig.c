// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_trig.h"
#include "read_ntriples.h"
#include "read_turtle.h"
#include "reader.h"
#include "stack.h"
#include "token_header.h"
#include "try.h"

#include <serd/event.h>
#include <serd/reader.h>
#include <serd/status.h>
#include <zix/string_view.h>

#include <stdbool.h>
#include <stddef.h>

static SerdStatus
read_wrappedGraph(SerdReader* const reader, ReadContext* const ctx)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, eat_byte_check(reader, '{'));
  TRY(st, read_turtle_ws_star(reader));

  while (peek_byte(reader) != '}') {
    const size_t orig_stack_size = reader->stack.size;
    bool         ate_dot         = false;
    int          s_type          = 0;

    ctx->subject = 0;
    if ((st = read_turtle_subject(reader, *ctx, &ctx->subject, &s_type))) {
      return r_err(reader, st, "expected subject");
    }

    st = read_turtle_triples(reader, *ctx, &ate_dot);
    if (st > SERD_FAILURE || (st && s_type != '[')) {
      return r_err(reader, st, "expected predicate object list");
    }

    serd_stack_pop_to(&reader->stack, orig_stack_size);
    TRY(st, read_turtle_ws_star(reader));
    if (peek_byte(reader) == '.') {
      TRY(st, skip_byte(reader, '.'));
    }
    TRY(st, read_turtle_ws_star(reader));
  }

  TRY(st, skip_byte(reader, '}'));
  TRY(st, read_turtle_ws_star(reader));
  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_BAD_SYNTAX, "graph followed by '.'");
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_labelOrSubject(SerdReader* const reader, TokenHeader** const dest)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;

  switch (peek_byte(reader)) {
  case '[':
    if (!(st = skip_byte(reader, '[')) && !(st = read_turtle_ws_star(reader)) &&
        !(st = eat_byte_check(reader, ']'))) {
      *dest = blank_id(reader);
      st    = *dest ? SERD_SUCCESS : SERD_BAD_STACK;
    }
    break;
  case '_':
    st = read_BLANK_NODE_LABEL(reader, dest, &ate_dot);
    break;
  default:
    if ((st = read_turtle_iri(reader, dest, &ate_dot)) == SERD_FAILURE) {
      st = r_err(reader, st, "expected label or subject");
    }
  }

  return st;
}

static SerdStatus
read_sparql_directive(SerdReader* const        reader,
                      ReadContext* const       ctx,
                      const TokenHeader* const token)
{
  static const ZixStringView base_token   = ZIX_STATIC_STRING("base");
  static const ZixStringView graph_token  = ZIX_STATIC_STRING("graph");
  static const ZixStringView prefix_token = ZIX_STATIC_STRING("prefix");

  if (token_equals(token, base_token)) {
    return read_turtle_base(reader, true, false);
  }

  if (token_equals(token, prefix_token)) {
    return read_turtle_prefixID(reader, true, false);
  }

  if (token_equals(token, graph_token)) {
    SerdStatus st = SERD_SUCCESS;
    TRY(st, read_turtle_ws_star(reader));
    TRY(st, read_labelOrSubject(reader, &ctx->graph));
    TRY(st, read_turtle_ws_star(reader));
    return read_wrappedGraph(reader, ctx);
  }

  return SERD_FAILURE;
}

static SerdStatus
read_block(SerdReader* const reader, ReadContext* const ctx)
{
  // Try to read a subject, though it may actually be a directive or graph name
  TokenHeader* token  = NULL;
  int          s_type = 0;
  SerdStatus   st     = read_turtle_subject(reader, *ctx, &token, &s_type);
  if (st > SERD_FAILURE) {
    return st;
  }

  // Try to interpret as a SPARQL "PREFIX" or "BASE" directive
  if (st && (st = read_sparql_directive(reader, ctx, token)) != SERD_FAILURE) {
    return st;
  }

  // Try to interpret as a named TriG graph like "graphname { ..."
  TRY(st, read_turtle_ws_star(reader));
  if (peek_byte(reader) == '{') {
    if (s_type == '(' || (s_type == '[' && !*ctx->flags)) {
      return r_err(reader, SERD_BAD_SYNTAX, "invalid graph name");
    }

    ctx->graph = token;
    return read_wrappedGraph(reader, ctx);
  }

  // Our token is really a subject, read some triples
  bool ate_dot = false;
  ctx->subject = token;
  if ((st = read_turtle_triples(reader, *ctx, &ate_dot)) > SERD_FAILURE) {
    return st;
  }

  // "Failure" is only allowed for anonymous subjects like "[ ... ] ."
  if (st == SERD_FAILURE && s_type != '[') {
    return r_err(reader, SERD_BAD_SYNTAX, "expected triples");
  }

  // Ensure that triples are properly terminated
  return ate_dot ? st : eat_byte_check(reader, '.');
}

SerdStatus
read_trig_statement(SerdReader* const reader)
{
  SerdEventFlags flags = 0U;
  ReadContext    ctx   = {NULL, NULL, NULL, &flags};
  SerdStatus     st    = SERD_SUCCESS;

  TRY(st, read_turtle_ws_star(reader));

  const int c = peek_byte(reader);
  if (c <= 0) {
    TRY(st, skip_byte(reader, c));
    return SERD_FAILURE;
  }

  return (c == '@')   ? read_turtle_directive(reader)
         : (c == '{') ? read_wrappedGraph(reader, &ctx)
                      : read_block(reader, &ctx);
}

SerdStatus
read_trigDoc(SerdReader* const reader)
{
  while (!reader->source.eof) {
    const size_t     orig_stack_size = reader->stack.size;
    const SerdStatus st              = read_trig_statement(reader);

    if (st > SERD_FAILURE) {
      if (!tolerate_status(reader, st)) {
        serd_stack_pop_to(&reader->stack, orig_stack_size);
        return st;
      }
      serd_reader_skip_until_byte(reader, '\n');
    }

    serd_stack_pop_to(&reader->stack, orig_stack_size);
  }

  return SERD_SUCCESS;
}
