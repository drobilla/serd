// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_nquads.h"

#include "read_ntriples.h"
#include "reader.h"
#include "stack.h"
#include "try.h"

#include "serd/event.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement_view.h"

#include <stdbool.h>
#include <stdio.h>

/// [6] graphLabel
static SerdStatus
read_graphLabel(SerdReader* const reader,
                SerdNode** const  dest,
                bool* const       ate_dot)
{
  return read_nt_subject(reader, dest, ate_dot); // Equivalent rule
}

/// [2] statement
static SerdStatus
read_nquads_statement(SerdReader* const reader)
{
  SerdStatementEventFlags flags   = 0U;
  ReadContext             ctx     = {0, 0, 0, 0, &flags};
  SerdStatus              st      = SERD_SUCCESS;
  bool                    ate_dot = false;

  // Read subject and predicate
  if ((st = read_nt_subject(reader, &ctx.subject, &ate_dot)) ||
      (st = skip_horizontal_whitespace(reader)) ||
      (st = read_nt_predicate(reader, &ctx.predicate)) ||
      (st = skip_horizontal_whitespace(reader))) {
    return st;
  }

  if ((st = read_nt_object(reader, &ctx.object, &ate_dot)) ||
      (st = skip_horizontal_whitespace(reader))) {
    return st;
  }

  if (!ate_dot) {
    if (peek_byte(reader) == '.') {
      eat_byte(reader);
    } else {
      TRY(st, read_graphLabel(reader, &ctx.graph, &ate_dot));
      skip_horizontal_whitespace(reader);
      if (!ate_dot) {
        TRY(st, eat_byte_check(reader, '.'));
      }
    }
  }

  TRY(st, push_node_termination(reader));

  const SerdStatementView statement = {
    ctx.subject, ctx.predicate, ctx.object, ctx.graph};

  return serd_sink_write_statement(reader->sink, *ctx.flags, statement);
}

SerdStatus
read_nquads_line(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;

  skip_horizontal_whitespace(reader);

  const int c = peek_byte(reader);
  if (c < 0) {
    return SERD_FAILURE;
  }

  if (c == 0) {
    skip_byte(reader, '\0');
    return SERD_FAILURE;
  }

  if (c == '\n' || c == '\r') {
    return read_EOL(reader);
  }

  if (c == '#') {
    return read_comment(reader);
  }

  const size_t orig_stack_size = reader->stack.size;

  if (!(st = read_nquads_statement(reader))) {
    skip_horizontal_whitespace(reader);
    if (peek_byte(reader) == '#') {
      st = read_comment(reader);
    }
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);

  return (st || peek_byte(reader) == EOF) ? st : read_EOL(reader);
}

SerdStatus
read_nquadsDoc(SerdReader* const reader)
{
  // Read the first line
  SerdStatus st = read_nquads_line(reader);
  if (st == SERD_FAILURE || !tolerate_status(reader, st)) {
    return st;
  }

  // Continue reading lines for as long as possible
  for (st = SERD_SUCCESS; !st;) {
    st = read_nquads_line(reader);
    if (st > SERD_FAILURE && !reader->strict && tolerate_status(reader, st)) {
      serd_reader_skip_until_byte(reader, '\n');
      st = SERD_SUCCESS;
    }
  }

  // If we made it this far, we succeeded at reading at least one line
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}
