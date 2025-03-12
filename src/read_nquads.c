// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_nquads.h"

#include "read_context.h"
#include "read_ntriples.h"
#include "reader_impl.h"
#include "reader_internal.h"
#include "stack.h"
#include "token_header.h"
#include "try.h"

#include <serd/event.h>
#include <serd/reader.h>
#include <serd/status.h>

#include <stdbool.h>
#include <stddef.h>

/// [6] graphLabel
static SerdStatus
read_graphLabel(SerdReader* const   reader,
                TokenHeader** const dest,
                bool* const         ate_dot)
{
  return read_nt_subject(reader, dest, ate_dot); // Equivalent rule
}

/// [2] statement
static SerdStatus
read_nquads_statement(SerdReader* const reader)
{
  SerdEventFlags flags   = 0U;
  ReadContext    ctx     = {NULL, NULL, NULL, &flags};
  SerdStatus     st      = SERD_SUCCESS;
  bool           ate_dot = false;

  // Read subject and predicate
  if ((st = read_nt_subject(reader, &ctx.subject, &ate_dot)) ||
      (st = read_horizontal_whitespace(reader)) ||
      (st = read_nt_predicate(reader, &ctx.predicate)) ||
      (st = read_horizontal_whitespace(reader))) {
    return st;
  }

  TokenHeader* object = NULL;
  TokenHeader* meta   = NULL;
  if ((st = read_nt_object(reader, &object, &meta, &ate_dot)) ||
      (st = read_horizontal_whitespace(reader))) {
    return st;
  }

  if (!ate_dot) {
    if (peek_byte(reader) != '.') {
      TRY(st, read_graphLabel(reader, &ctx.graph, &ate_dot));
      TRY(st, read_horizontal_whitespace(reader));
    }

    if (!ate_dot) {
      TRY(st, eat_byte_check(reader, '.'));
    }
  }

  return emit_statement(reader, ctx, object, meta);
}

SerdStatus
read_nquads_line(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, read_horizontal_whitespace(reader));

  const int c = peek_byte(reader);

  if (c == '\n' || c == '\r') {
    return read_EOL(reader);
  }

  if (c == '#') {
    return read_comment(reader);
  }

  const size_t orig_stack_size = reader->stack.size;

  if (!(st = read_nquads_statement(reader)) &&
      !(st = read_horizontal_whitespace(reader))) {
    if (peek_byte(reader) == '#') {
      st = read_comment(reader);
    }
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);

  return (st || peek_byte(reader) < 0) ? st : read_EOL(reader);
}
