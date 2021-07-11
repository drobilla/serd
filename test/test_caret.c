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
  SerdNodes* const      nodes = serd_nodes_new();
  const SerdNode* const node  = serd_nodes_string(nodes, SERD_STRING("node"));
  SerdCaret* const      caret = serd_caret_new(node, 46, 2);

  assert(serd_caret_equals(caret, caret));
  assert(serd_caret_name(caret) == node);
  assert(serd_caret_line(caret) == 46);
  assert(serd_caret_column(caret) == 2);

  SerdCaret* const copy = serd_caret_copy(caret);

  assert(serd_caret_equals(caret, copy));
  assert(!serd_caret_copy(NULL));

  const SerdNode* const other_node =
    serd_nodes_string(nodes, SERD_STRING("other"));

  SerdCaret* const other_file = serd_caret_new(other_node, 46, 2);
  SerdCaret* const other_line = serd_caret_new(node, 47, 2);
  SerdCaret* const other_col  = serd_caret_new(node, 46, 3);

  assert(!serd_caret_equals(caret, other_file));
  assert(!serd_caret_equals(caret, other_line));
  assert(!serd_caret_equals(caret, other_col));
  assert(!serd_caret_equals(caret, NULL));
  assert(!serd_caret_equals(NULL, caret));

  serd_caret_free(other_col);
  serd_caret_free(other_line);
  serd_caret_free(other_file);
  serd_caret_free(copy);
  serd_caret_free(caret);
  serd_nodes_free(nodes);

  return 0;
}
