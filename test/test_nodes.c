/*
  Copyright 2018 David Robillard <d@drobilla.net>

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

static int
test_intern(void)
{
  SerdNodes* nodes = serd_nodes_new();
  SerdNode*  node  = serd_new_string(SERD_STATIC_STRING("node"));

  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));

  const SerdNode* interned2 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned2));
  assert(interned1 == interned2);

  serd_node_free(node);
  serd_nodes_free(nodes);
  return 0;
}

static int
test_manage(void)
{
  SerdNodes* nodes = serd_nodes_new();
  SerdNode*  node  = serd_new_string(SERD_STATIC_STRING("node"));

  assert(!serd_nodes_manage(nodes, NULL));

  const SerdNode* managed1 = serd_nodes_manage(nodes, node);
  assert(managed1 == node);

  SerdNode*       equal    = serd_new_string(SERD_STATIC_STRING("node"));
  const SerdNode* managed2 = serd_nodes_manage(nodes, equal);
  assert(managed2 == node);

  serd_nodes_free(nodes);
  return 0;
}

static int
test_deref(void)
{
  SerdNodes*      nodes = serd_nodes_new();
  const SerdNode* managed =
    serd_nodes_manage(nodes, serd_new_string(SERD_STATIC_STRING("node")));

  serd_nodes_deref(nodes, managed);

  SerdNode*       node     = serd_new_string(SERD_STATIC_STRING("node"));
  const SerdNode* interned = serd_nodes_intern(nodes, node);

  assert(interned != node);

  serd_node_free(node);
  serd_nodes_free(nodes);
  return 0;
}

int
main(void)
{
  typedef int (*TestFunc)(void);

  const TestFunc tests[] = {test_intern, test_manage, test_deref, NULL};

  int ret = 0;
  for (const TestFunc* t = tests; *t; ++t) {
    ret += (*t)();
  }

  return ret;
}
