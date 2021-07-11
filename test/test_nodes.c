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
#include <string.h>

static void
test_intern(void)
{
  SerdNodes* nodes = serd_nodes_new();
  SerdNode*  node  = serd_new_string(SERD_STRING("node"));

  assert(serd_nodes_size(nodes) == 0u);
  assert(!serd_nodes_intern(nodes, NULL));

  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_size(nodes) == 1u);

  const SerdNode* interned2 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned2));
  assert(interned1 == interned2);
  assert(serd_nodes_size(nodes) == 1u);

  serd_node_free(node);
  serd_nodes_free(nodes);
}

static void
test_manage(void)
{
  SerdNodes* nodes = serd_nodes_new();
  SerdNode*  node  = serd_new_string(SERD_STRING("node"));

  assert(!serd_nodes_manage(nodes, NULL));

  const SerdNode* managed1 = serd_nodes_manage(nodes, node);
  assert(managed1 == node);

  SerdNode*       equal    = serd_new_string(SERD_STRING("node"));
  const SerdNode* managed2 = serd_nodes_manage(nodes, equal);
  assert(managed2 == node);

  serd_nodes_free(nodes);
}

static void
test_string(void)
{
  static const SerdStringView string = SERD_STRING("string");

  SerdNodes* const      nodes = serd_nodes_new();
  const SerdNode* const node  = serd_nodes_string(nodes, string);

  assert(node);
  assert(serd_node_type(node) == SERD_LITERAL);
  assert(!serd_node_datatype(node));
  assert(!serd_node_language(node));
  assert(serd_node_length(node) == string.len);
  assert(!strcmp(serd_node_string(node), string.buf));

  serd_nodes_free(nodes);
}

static void
test_plain_literal(void)
{
  static const SerdStringView string   = SERD_STRING("string");
  static const SerdStringView language = SERD_STRING("en");

  SerdNodes* const      nodes = serd_nodes_new();
  const SerdNode* const node =
    serd_nodes_plain_literal(nodes, string, language);

  assert(node);
  assert(serd_node_type(node) == SERD_LITERAL);
  assert(!serd_node_datatype(node));

  const SerdNode* const language_node = serd_node_language(node);
  assert(language_node);
  assert(serd_node_type(language_node) == SERD_LITERAL);
  assert(serd_node_length(language_node) == language.len);
  assert(!strcmp(serd_node_string(language_node), language.buf));
  assert(serd_node_length(node) == string.len);
  assert(!strcmp(serd_node_string(node), string.buf));

  serd_nodes_free(nodes);
}

static void
test_typed_literal(void)
{
  static const SerdStringView string   = SERD_STRING("string");
  static const SerdStringView datatype = SERD_STRING("http://example.org/Type");

  SerdNodes* const      nodes = serd_nodes_new();
  const SerdNode* const node =
    serd_nodes_typed_literal(nodes, string, datatype);

  assert(node);
  assert(serd_node_type(node) == SERD_LITERAL);
  assert(!serd_node_language(node));

  const SerdNode* const datatype_node = serd_node_datatype(node);
  assert(datatype_node);

  assert(serd_node_type(datatype_node) == SERD_URI);
  assert(serd_node_length(datatype_node) == datatype.len);
  assert(!strcmp(serd_node_string(datatype_node), datatype.buf));
  assert(serd_node_length(node) == string.len);
  assert(!strcmp(serd_node_string(node), string.buf));

  serd_nodes_free(nodes);
}

static void
test_uri(void)
{
  static const SerdStringView string = SERD_STRING("http://example.org/");

  SerdNodes* const      nodes = serd_nodes_new();
  const SerdNode* const node  = serd_nodes_uri(nodes, string);

  assert(node);
  assert(serd_node_type(node) == SERD_URI);
  assert(!serd_node_datatype(node));
  assert(!serd_node_language(node));
  assert(serd_node_length(node) == string.len);
  assert(!strcmp(serd_node_string(node), string.buf));

  serd_nodes_free(nodes);
}

static void
test_blank(void)
{
  static const SerdStringView string = SERD_STRING("b42");

  SerdNodes* const      nodes = serd_nodes_new();
  const SerdNode* const node  = serd_nodes_blank(nodes, string);

  assert(node);
  assert(serd_node_type(node) == SERD_BLANK);
  assert(!serd_node_datatype(node));
  assert(!serd_node_language(node));
  assert(serd_node_length(node) == string.len);
  assert(!strcmp(serd_node_string(node), string.buf));

  serd_nodes_free(nodes);
}

static void
test_deref(void)
{
  SerdNodes*      nodes   = serd_nodes_new();
  const SerdNode* managed = serd_nodes_string(nodes, SERD_STRING("node"));

  serd_nodes_deref(nodes, managed);

  SerdNode*       node     = serd_new_string(SERD_STRING("node"));
  const SerdNode* interned = serd_nodes_intern(nodes, node);

  assert(interned != node);

  serd_node_free(node);
  serd_nodes_free(nodes);
}

static void
test_get(void)
{
  SerdNodes* nodes = serd_nodes_new();
  SerdNode*  node  = serd_new_string(SERD_STRING("node"));

  assert(!serd_nodes_get(nodes, NULL));
  assert(!serd_nodes_get(nodes, node));

  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_get(nodes, node) == interned1);

  serd_node_free(node);
  serd_nodes_free(nodes);
}

int
main(void)
{
  test_intern();
  test_manage();
  test_string();
  test_plain_literal();
  test_typed_literal();
  test_uri();
  test_blank();
  test_deref();
  test_get();
  return 0;
}
