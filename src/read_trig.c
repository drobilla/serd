// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_trig.h"
#include "read_ntriples.h"
#include "read_turtle.h"
#include "reader.h"
#include "stack.h"
#include "try.h"

#include "serd/event.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/status.h"

#include <stdbool.h>
#include <stdio.h>

static SerdStatus
read_wrappedGraph(SerdReader* const reader, ReadContext* const ctx)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, eat_byte_check(reader, '{'));
  read_turtle_ws_star(reader);

  while (peek_byte(reader) != '}') {
    const size_t orig_stack_size = reader->stack.size;
    bool         ate_dot         = false;
    int          s_type          = 0;

    ctx->subject = 0;
    if ((st = read_turtle_subject(reader, *ctx, &ctx->subject, &s_type))) {
      return r_err(reader, st, "expected subject");
    }

    if ((st = read_turtle_triples(reader, *ctx, &ate_dot)) && s_type != '[') {
      return r_err(reader, st, "bad predicate object list");
    }

    serd_stack_pop_to(&reader->stack, orig_stack_size);
    read_turtle_ws_star(reader);
    if (peek_byte(reader) == '.') {
      skip_byte(reader, '.');
    }
    read_turtle_ws_star(reader);
  }

  skip_byte(reader, '}');
  read_turtle_ws_star(reader);
  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_BAD_SYNTAX, "graph followed by '.'");
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_labelOrSubject(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;

  switch (peek_byte(reader)) {
  case '[':
    skip_byte(reader, '[');
    read_turtle_ws_star(reader);
    TRY(st, eat_byte_check(reader, ']'));
    *dest = blank_id(reader);
    return *dest ? SERD_SUCCESS : SERD_BAD_STACK;
  case '_':
    return read_BLANK_NODE_LABEL(reader, dest, &ate_dot);
  default:
    if (!read_turtle_iri(reader, dest, &ate_dot)) {
      return SERD_SUCCESS;
    } else {
      return r_err(reader, SERD_BAD_SYNTAX, "expected label or subject");
    }
  }
}

static SerdStatus
read_sparql_directive(SerdReader* const     reader,
                      ReadContext* const    ctx,
                      const SerdNode* const token)
{
  if (token_equals(token, "base", 4)) {
    return read_turtle_base(reader, true, false);
  }

  if (token_equals(token, "prefix", 6)) {
    return read_turtle_prefixID(reader, true, false);
  }

  if (token_equals(token, "graph", 5)) {
    SerdStatus st = SERD_SUCCESS;
    read_turtle_ws_star(reader);
    TRY(st, read_labelOrSubject(reader, &ctx->graph));
    read_turtle_ws_star(reader);
    return read_wrappedGraph(reader, ctx);
  }

  return SERD_FAILURE;
}

static SerdStatus
read_block(SerdReader* const reader, ReadContext* const ctx)
{
  SerdStatus st = SERD_SUCCESS;

  // Try to read a subject, though it may actually be a directive or graph name
  SerdNode* token  = NULL;
  int       s_type = 0;
  if ((st = read_turtle_subject(reader, *ctx, &token, &s_type)) >
      SERD_FAILURE) {
    return st;
  }

  // Try to interpret as a SPARQL "PREFIX" or "BASE" directive
  if (st && (st = read_sparql_directive(reader, ctx, token)) != SERD_FAILURE) {
    return st;
  }

  // Try to interpret as a named TriG graph like "graphname { ..."
  read_turtle_ws_star(reader);
  if (peek_byte(reader) == '{') {
    if (s_type == '(' || (s_type == '[' && !*ctx->flags)) {
      return r_err(reader, SERD_BAD_SYNTAX, "invalid graph name");
    }

    ctx->graph = token;
    (*ctx->flags) |= (s_type == '[' ? SERD_EMPTY_G : 0U);
    return read_wrappedGraph(reader, ctx);
  }

  if (st) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected directive or subject");
  }

  // Our token is really a subject, read some triples
  bool ate_dot = false;
  ctx->subject = token;
  if ((st = read_turtle_triples(reader, *ctx, &ate_dot)) > SERD_FAILURE) {
    return st;
  }

  // "Failure" is only allowed for anonymous subjects like "[ ... ] ."
  if (st && s_type != '[') {
    return r_err(reader, SERD_BAD_SYNTAX, "expected triples");
  }

  // Ensure that triples are properly terminated
  return ate_dot ? st : eat_byte_check(reader, '.');
}

SerdStatus
read_trig_statement(SerdReader* const reader)
{
  SerdStatementEventFlags flags = 0U;
  ReadContext             ctx   = {0, 0, 0, 0, &flags};

  // Handle nice cases we can distinguish from the next byte
  read_turtle_ws_star(reader);
  switch (peek_byte(reader)) {
  case EOF:
    return SERD_FAILURE;

  case '\0':
    eat_byte(reader);
    return SERD_FAILURE;

  case '@':
    return read_turtle_directive(reader);

  case '{':
    return read_wrappedGraph(reader, &ctx);

  default:
    break;
  }

  // No such luck, figure out what to read from the first token
  return read_block(reader, &ctx);
}

SerdStatus
read_trigDoc(SerdReader* const reader)
{
  while (!reader->source->eof) {
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
