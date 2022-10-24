// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>

int
main(void)
{
  SerdNode* const  node  = serd_new_string(serd_string("node"));
  SerdCaret* const caret = serd_caret_new(node, 46, 2);

  assert(serd_caret_equals(caret, caret));
  assert(serd_caret_document(caret) == node);
  assert(serd_caret_line(caret) == 46);
  assert(serd_caret_column(caret) == 2);

  SerdCaret* const copy = serd_caret_copy(caret);

  assert(serd_caret_equals(caret, copy));
  assert(!serd_caret_copy(NULL));

  SerdNode* const  other_node = serd_new_string(serd_string("other"));
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
  serd_node_free(other_node);
  serd_caret_free(copy);
  serd_caret_free(caret);
  serd_node_free(node);

  return 0;
}
