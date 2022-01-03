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

#include "failing_allocator.h"
#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a node set to count the number of allocations
  SerdNodes* nodes = serd_nodes_new(&allocator.base);
  assert(nodes);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;
    assert(!serd_nodes_new(&allocator.base));
  }

  serd_nodes_free(nodes);
}

static void
test_intern_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdNode* const node = serd_new_string(&allocator.base, SERD_STRING("node"));

  // Successfully intern a node to count the number of allocations
  SerdNodes*      nodes     = serd_nodes_new(&allocator.base);
  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_size(nodes) == 1u);

  const size_t n_new_allocs = allocator.n_allocations;
  serd_nodes_free(nodes);

  // Test that each allocation failing is handled gracefully
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;

    if ((nodes = serd_nodes_new(&allocator.base))) {
      const SerdNode* interned2 = serd_nodes_intern(nodes, node);
      if (interned2) {
        assert(serd_node_equals(node, interned2));
        assert(serd_nodes_size(nodes) == 1u);
      }
      serd_nodes_free(nodes);
    }
  }

  serd_node_free(&allocator.base, node);
}

static void
test_intern(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* nodes = serd_nodes_new(allocator);
  SerdNode*  node  = serd_new_string(NULL, SERD_STRING("node"));

  assert(serd_nodes_size(nodes) == 0u);
  assert(!serd_nodes_intern(nodes, NULL));

  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_size(nodes) == 1u);

  const SerdNode* interned2 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned2));
  assert(interned1 == interned2);
  assert(serd_nodes_size(nodes) == 1u);

  serd_node_free(NULL, node);
  serd_nodes_free(nodes);
}

static void
test_string(void)
{
  static const SerdStringView string = SERD_STRING("string");

  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
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
test_invalid_literal(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  assert(!serd_nodes_literal(nodes,
                             SERD_STRING("double meta"),
                             SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE,
                             SERD_STRING("What am I?")));

  assert(!serd_nodes_literal(nodes,
                             SERD_STRING("empty language"),
                             SERD_HAS_LANGUAGE,
                             SERD_EMPTY_STRING()));

  assert(!serd_nodes_literal(nodes,
                             SERD_STRING("empty datatype"),
                             SERD_HAS_DATATYPE,
                             SERD_EMPTY_STRING()));

  serd_nodes_free(nodes);
}

static void
test_plain_literal(void)
{
  static const SerdStringView string   = SERD_STRING("string");
  static const SerdStringView language = SERD_STRING("en");

  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdNode* const node =
    serd_nodes_literal(nodes, string, SERD_HAS_LANGUAGE, language);

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

  const SerdNode* const alias =
    serd_nodes_literal(nodes, string, SERD_HAS_LANGUAGE, language);

  assert(alias == node);

  const SerdNode* const other =
    serd_nodes_literal(nodes, string, SERD_HAS_LANGUAGE, SERD_STRING("de"));

  assert(other != node);

  const SerdNode* const other_language_node = serd_node_language(other);
  assert(other_language_node);
  assert(serd_node_type(other_language_node) == SERD_LITERAL);
  assert(serd_node_length(other_language_node) == 2);
  assert(!strcmp(serd_node_string(other_language_node), "de"));
  assert(serd_node_length(other) == string.len);
  assert(!strcmp(serd_node_string(other), string.buf));

  serd_nodes_free(nodes);
}

static void
test_typed_literal(void)
{
  static const SerdStringView string   = SERD_STRING("string");
  static const SerdStringView datatype = SERD_STRING("http://example.org/Type");

  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdNode* const node =
    serd_nodes_literal(nodes, string, SERD_HAS_DATATYPE, datatype);

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

  const SerdNode* const alias =
    serd_nodes_literal(nodes, string, SERD_HAS_DATATYPE, datatype);

  assert(alias == node);

  serd_nodes_free(nodes);
}

static void
test_boolean(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const false1 = serd_nodes_boolean(nodes, false);
  const SerdNode* const false2 = serd_nodes_boolean(nodes, false);
  const SerdNode* const true1  = serd_nodes_boolean(nodes, true);
  const SerdNode* const true2  = serd_nodes_boolean(nodes, true);

  assert(false1 == false2);
  assert(true1 == true2);
  assert(false1 != true1);
  assert(!strcmp(serd_node_string(false1), "false"));
  assert(!strcmp(serd_node_string(true1), "true"));
  assert(serd_node_length(false1) == strlen(serd_node_string(false1)));
  assert(serd_node_length(true1) == strlen(serd_node_string(true1)));
  assert(serd_nodes_size(nodes) == 2);

  const SerdNode* const true_datatype = serd_node_datatype(true1);
  assert(true_datatype);
  assert(!strcmp(serd_node_string(true_datatype), NS_XSD "boolean"));

  const SerdNode* const false_datatype = serd_node_datatype(false1);
  assert(false_datatype);
  assert(!strcmp(serd_node_string(false_datatype), NS_XSD "boolean"));

  serd_nodes_free(nodes);
}

static void
test_decimal(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdNode* const a     = serd_nodes_decimal(nodes, -12.3456789);
  const SerdNode* const b     = serd_nodes_decimal(nodes, -12.3456789);

  assert(a == b);
  assert(!strcmp(serd_node_string(a), "-12.3456789"));
  assert(serd_node_length(a) == strlen(serd_node_string(a)));
  assert(serd_nodes_size(nodes) == 1);

  const SerdNode* const default_datatype = serd_node_datatype(a);
  assert(default_datatype);
  assert(!strcmp(serd_node_string(default_datatype), NS_XSD "decimal"));

  serd_nodes_free(nodes);
}

static void
test_double(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const a = serd_nodes_double(nodes, -1.2E3);
  const SerdNode* const b = serd_nodes_double(nodes, -1.2E3);

  assert(a == b);
  assert(!strcmp(serd_node_string(a), "-1.2E3"));
  assert(serd_node_length(a) == strlen(serd_node_string(a)));
  assert(serd_nodes_size(nodes) == 1);

  serd_nodes_free(nodes);
}

static void
test_float(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const a = serd_nodes_float(nodes, -1.2E3f);
  const SerdNode* const b = serd_nodes_float(nodes, -1.2E3f);

  assert(a == b);
  assert(!strcmp(serd_node_string(a), "-1.2E3"));
  assert(serd_node_length(a) == strlen(serd_node_string(a)));
  assert(serd_nodes_size(nodes) == 1);

  serd_nodes_free(nodes);
}

static void
test_integer(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const a = serd_nodes_integer(nodes, -1234567890);
  const SerdNode* const b = serd_nodes_integer(nodes, -1234567890);

  assert(a == b);
  assert(!strcmp(serd_node_string(a), "-1234567890"));
  assert(serd_node_length(a) == strlen(serd_node_string(a)));
  assert(serd_nodes_size(nodes) == 1);

  const SerdNode* const default_datatype = serd_node_datatype(a);
  assert(default_datatype);
  assert(!strcmp(serd_node_string(default_datatype), NS_XSD "integer"));

  serd_nodes_free(nodes);
}

static void
test_base64(void)
{
  static const char data[] = {'f', 'o', 'o', 'b', 'a', 'r'};

  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const a = serd_nodes_base64(nodes, &data, sizeof(data));
  const SerdNode* const b = serd_nodes_base64(nodes, &data, sizeof(data));

  assert(a == b);
  assert(!strcmp(serd_node_string(a), "Zm9vYmFy"));
  assert(serd_node_length(a) == strlen(serd_node_string(a)));
  assert(serd_nodes_size(nodes) == 1);

  const SerdNode* const default_datatype = serd_node_datatype(a);
  assert(default_datatype);
  assert(!strcmp(serd_node_string(default_datatype), NS_XSD "base64Binary"));

  serd_nodes_free(nodes);
}

static void
test_uri(void)
{
  static const SerdStringView string = SERD_STRING("http://example.org/");

  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
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
test_parsed_uri(void)
{
  static const SerdStringView string = SERD_STRING("http://example.org/");

  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdURIView     uri   = serd_parse_uri(string.buf);
  const SerdNode* const node  = serd_nodes_parsed_uri(nodes, uri);

  assert(node);
  assert(serd_node_type(node) == SERD_URI);
  assert(!serd_node_flags(node));
  assert(!serd_node_datatype(node));
  assert(!serd_node_language(node));
  assert(serd_node_length(node) == string.len);
  assert(!strcmp(serd_node_string(node), string.buf));

  const SerdNode* const alias = serd_nodes_parsed_uri(nodes, uri);
  assert(alias == node);

  const SerdNode* const other =
    serd_nodes_parsed_uri(nodes, serd_parse_uri("http://example.org/x"));

  assert(other != node);

  serd_nodes_free(nodes);
}

static void
test_blank(void)
{
  static const SerdStringView string = SERD_STRING("b42");

  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
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
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes*      nodes    = serd_nodes_new(allocator);
  const SerdNode* original = serd_nodes_string(nodes, SERD_STRING("node"));
  const SerdNode* another  = serd_nodes_string(nodes, SERD_STRING("node"));

  assert(original == another);

  // Dereference the original and ensure the other reference kept it alive
  serd_nodes_deref(nodes, original);
  assert(serd_node_length(another) == 4);
  assert(!strcmp(serd_node_string(another), "node"));

  // Drop the other/final reference (freeing the node)
  serd_nodes_deref(nodes, another);

  /* Intern some other irrelevant node first to (hopefully) avoid the allocator
     just giving us back the same piece of memory. */

  const SerdNode* other = serd_nodes_string(nodes, SERD_STRING("XXXX"));
  assert(!strcmp(serd_node_string(other), "XXXX"));

  // Intern a new equivalent node to the original and check that it's really new
  const SerdNode* imposter = serd_nodes_string(nodes, SERD_STRING("node"));
  assert(imposter != original);
  assert(serd_node_length(imposter) == 4);
  assert(!strcmp(serd_node_string(imposter), "node"));

  // Check that dereferencing some random unknown node doesn't crash
  SerdNode* unmanaged = serd_new_string(NULL, SERD_STRING("unmanaged"));
  serd_nodes_deref(nodes, unmanaged);
  serd_node_free(NULL, unmanaged);

  serd_nodes_deref(nodes, NULL);
  serd_nodes_deref(nodes, imposter);
  serd_nodes_deref(nodes, other);
  serd_nodes_free(nodes);
}

static void
test_get(void)
{
  SerdAllocator* const allocator = serd_default_allocator();

  SerdNodes* nodes = serd_nodes_new(allocator);
  SerdNode*  node  = serd_new_string(NULL, SERD_STRING("node"));

  assert(!serd_nodes_get(nodes, NULL));
  assert(!serd_nodes_get(nodes, node));

  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_get(nodes, node) == interned1);

  serd_node_free(NULL, node);
  serd_nodes_free(nodes);
}

int
main(void)
{
  test_new_failed_alloc();
  test_intern_failed_alloc();
  test_intern();
  test_string();
  test_invalid_literal();
  test_plain_literal();
  test_typed_literal();
  test_boolean();
  test_decimal();
  test_double();
  test_float();
  test_integer();
  test_base64();
  test_uri();
  test_parsed_uri();
  test_blank();
  test_deref();
  test_get();
  return 0;
}
