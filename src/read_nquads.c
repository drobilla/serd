/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

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

#include "read_nquads.h"

#include "byte_source.h"
#include "node.h"
#include "read_ntriples.h"
#include "reader.h"
#include "statement.h"

#include "serd/serd.h"

#include <assert.h>
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

  if ((st = read_nt_subject(reader, &ctx.subject)) ||
      (st = skip_horizontal_whitespace(reader)) ||
      (st = read_nt_predicate(reader, &ctx.predicate)) ||
      (st = skip_horizontal_whitespace(reader)) ||
      (st = read_nt_object(reader, &ctx.object, &ate_dot)) ||
      (st = skip_horizontal_whitespace(reader))) {
    if (st == SERD_FAILURE || reader->strict) {
      return st;
    }

    skip_until(reader, '\n');
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
      if (eat_byte_check(reader, '.') != '.') {
        return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected '.'");
      }
    }
  }

  assert(ctx.object);
  serd_node_zero_pad(ctx.object);
  const SerdStatement statement = {
    {ctx.subject, ctx.predicate, ctx.object, ctx.graph},
    &reader->source->cur}; // FIXME: cursor orig_cursor};

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

  // Return if the first line didn't read for any reason
  serd_stack_pop_to(&reader->stack, orig_stack_size);
  if (st) {
    return st;
  }

  // Continue reading lines for as long as possible
  while (!(st = read_line(reader))) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);
  }

  // If we made it this far, we succeeded earlier
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}
