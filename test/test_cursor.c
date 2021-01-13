/*
  Copyright 2019-2020 David Robillard <d@drobilla.net>

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

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>

int
main(void)
{
  SerdNode* const   node   = serd_new_string(SERD_STATIC_STRING("node"));
  SerdCursor* const cursor = serd_cursor_new(node, 46, 2);

  assert(serd_cursor_name(cursor) == node);
  assert(serd_cursor_line(cursor) == 46);
  assert(serd_cursor_column(cursor) == 2);

  SerdCursor* const copy = serd_cursor_copy(cursor);

  assert(serd_cursor_equals(cursor, copy));
  assert(!serd_cursor_copy(NULL));

  SerdNode* const   other_node = serd_new_string(SERD_STATIC_STRING("other"));
  SerdCursor* const other_file = serd_cursor_new(other_node, 46, 2);
  SerdCursor* const other_line = serd_cursor_new(node, 47, 2);
  SerdCursor* const other_col  = serd_cursor_new(node, 46, 3);

  assert(!serd_cursor_equals(cursor, other_file));
  assert(!serd_cursor_equals(cursor, other_line));
  assert(!serd_cursor_equals(cursor, other_col));
  assert(!serd_cursor_equals(cursor, NULL));
  assert(!serd_cursor_equals(NULL, cursor));

  serd_cursor_free(other_col);
  serd_cursor_free(other_line);
  serd_cursor_free(other_file);
  serd_node_free(other_node);
  serd_cursor_free(copy);
  serd_cursor_free(cursor);
  serd_node_free(node);

  return 0;
}
