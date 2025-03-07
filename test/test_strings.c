// Copyright 2018-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/literal_view.h>
#include <serd/node_args.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/strings.h>
#include <serd/token_view.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stddef.h>

enum { INVALID_ID = 9999U };

static void
test_token(void)
{
  SerdNodes* const   nodes   = serd_nodes_new(NULL);
  SerdStrings* const strings = serd_strings_new(NULL, nodes);

  {
    const SerdTokenView token = serd_strings_token(strings, INVALID_ID);
    assert(!token.type);
    assert(zix_string_view_equals(token.string, zix_empty_string()));
  }
  {
    const ZixStringView label = zix_string("b");
    const SerdNodeID    id    = serd_nodes_id(nodes, serd_a_blank(label));
    const SerdTokenView token = serd_strings_token(strings, id);
    assert(token.type == SERD_BLANK);
    assert(zix_string_view_equals(token.string, label));
  }

  serd_nodes_free(nodes);
  serd_strings_free(strings);
}

static void
test_object(void)
{
  SerdNodes* const   nodes   = serd_nodes_new(NULL);
  SerdStrings* const strings = serd_strings_new(NULL, nodes);

  {
    const SerdObjectView object = serd_strings_object(strings, INVALID_ID);
    assert(!object.type);
    assert(!object.flags);
    assert(zix_string_view_equals(object.string, zix_empty_string()));
    assert(!object.meta.type);
    assert(zix_string_view_equals(object.meta.string, zix_empty_string()));
  }
  {
    const ZixStringView  label  = zix_string("b");
    const SerdNodeID     id     = serd_nodes_id(nodes, serd_a_blank(label));
    const SerdObjectView object = serd_strings_object(strings, id);
    assert(object.type == SERD_BLANK);
    assert(zix_string_view_equals(object.string, label));
    assert(!object.flags);
    assert(!object.meta.type);
    assert(!object.meta.string.length);
  }

  serd_strings_free(strings);
  serd_nodes_free(nodes);
}

static void
test_literal(void)
{
  SerdNodes* const   nodes   = serd_nodes_new(NULL);
  SerdStrings* const strings = serd_strings_new(NULL, nodes);

  {
    const SerdLiteralView literal = serd_strings_literal(strings, INVALID_ID);
    assert(!literal.flags);
    assert(zix_string_view_equals(literal.string, zix_empty_string()));
    assert(!literal.meta);
  }
  {
    const ZixStringView   string  = zix_string("s");
    const SerdNodeID      id      = serd_nodes_id(nodes, serd_a_string(string));
    const SerdLiteralView literal = serd_strings_literal(strings, id);
    assert(zix_string_view_equals(literal.string, string));
    assert(!literal.flags);
    assert(!literal.meta);
  }

  serd_strings_free(strings);
  serd_nodes_free(nodes);
}

int
main(void)
{
  test_token();
  test_object();
  test_literal();
  return 0;
}
