/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#include "statement.h"

const SerdNode*
serd_statement_node(const SerdStatement* const statement, const SerdField field)
{
  return statement->nodes[field];
}

const SerdNode*
serd_statement_subject(const SerdStatement* const statement)
{
  return statement->nodes[SERD_SUBJECT];
}

const SerdNode*
serd_statement_predicate(const SerdStatement* const statement)
{
  return statement->nodes[SERD_PREDICATE];
}

const SerdNode*
serd_statement_object(const SerdStatement* const statement)
{
  return statement->nodes[SERD_OBJECT];
}

const SerdNode*
serd_statement_graph(const SerdStatement* const statement)
{
  return statement->nodes[SERD_GRAPH];
}

const SerdCursor*
serd_statement_cursor(const SerdStatement* const statement)
{
  return statement->cursor;
}
