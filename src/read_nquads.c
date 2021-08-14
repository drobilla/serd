// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_nquads.h"

#include "byte_source.h"
#include "caret.h"
#include "node.h"
#include "read_ntriples.h"
#include "reader.h"
#include "stack.h"
#include "statement.h"

#include "serd/caret.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement.h"

#include <stdbool.h>
#include <stdio.h>

/// [6] graphLabel
static SerdStatus
read_graphLabel(SerdReader* const reader, SerdNode** const dest)
{
  return read_nt_subject(reader, dest); // Equivalent rule
}

/// [2] statement
static SerdStatus
read_statement(SerdReader* const reader)
{
  SerdStatementFlags flags   = 0;
  ReadContext        ctx     = {0, 0, 0, 0, &flags};
  SerdStatus         st      = SERD_SUCCESS;
  bool               ate_dot = false;

  // Read subject and predicate
  if ((st = read_nt_subject(reader, &ctx.subject)) ||
      (st = skip_horizontal_whitespace(reader)) ||
      (st = read_nt_predicate(reader, &ctx.predicate)) ||
      (st = skip_horizontal_whitespace(reader))) {
    return st;
  }

  // Preserve the caret for error reporting and read object
  SerdCaret orig_caret = reader->source->caret;
  if ((st = read_nt_object(reader, &ctx.object, &ate_dot)) ||
      (st = skip_horizontal_whitespace(reader))) {
    return st;
  }

  if (!ate_dot) {
    if (peek_byte(reader) == '.') {
      eat_byte(reader);
    } else {
      if ((st = read_graphLabel(reader, &ctx.graph))) {
        return st;
      }

      skip_horizontal_whitespace(reader);
      if ((st = eat_byte_check(reader, '.'))) {
        return st;
      }
    }
  }

  serd_node_zero_pad(ctx.object);
  const SerdStatement statement = {
    {ctx.subject, ctx.predicate, ctx.object, ctx.graph}, &orig_caret};

  return serd_sink_write_statement(reader->sink, *ctx.flags, &statement);
}

static SerdStatus
read_line(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;

  skip_horizontal_whitespace(reader);

  switch (peek_byte(reader)) {
  case EOF:
    return SERD_FAILURE;

  case '\n':
  case '\r':
    return read_EOL(reader);

  case '#':
    st = read_comment(reader);
    break;

  default:
    if (!(st = read_statement(reader))) {
      skip_horizontal_whitespace(reader);
      if (peek_byte(reader) == '#') {
        st = read_comment(reader);
      }
    }
    break;
  }

  return (st || peek_byte(reader) == EOF) ? st : read_EOL(reader);
}

/// [1] nquadsDoc
SerdStatus
read_nquadsDoc(SerdReader* const reader)
{
  // Record the initial stack size and read the first line
  const size_t orig_stack_size = reader->stack.size;
  SerdStatus   st              = read_line(reader);

  // Return early if we failed to read anything at all
  serd_stack_pop_to(&reader->stack, orig_stack_size);
  if (st == SERD_FAILURE || !tolerate_status(reader, st)) {
    return st;
  }

  // Continue reading lines for as long as possible
  for (st = SERD_SUCCESS; !st;) {
    st = read_line(reader);
    serd_stack_pop_to(&reader->stack, orig_stack_size);

    if (st > SERD_FAILURE && !reader->strict && tolerate_status(reader, st)) {
      serd_reader_skip_until_byte(reader, '\n');
      st = SERD_SUCCESS;
    }
  }

  // If we made it this far, we succeeded at reading at least one line
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}
