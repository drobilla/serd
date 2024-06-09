// Copyright 2018 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include "serd/node.h"
#include "serd/nodes.h"
#include "serd/uri.h"
#include "serd/value.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define NS_EG "http://example.org/"
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

  SerdNode* const node = serd_node_new(&allocator.base, serd_a_string("node"));

  // Successfully intern a node to count the number of allocations
  SerdNodes*      nodes     = serd_nodes_new(&allocator.base);
  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_size(nodes) == 1U);

  const size_t n_new_allocs = allocator.n_allocations;
  serd_nodes_free(nodes);

  // Test that each allocation failing is handled gracefully
  for (size_t i = 0; i < n_new_allocs; ++i) {
    allocator.n_remaining = i;

    if ((nodes = serd_nodes_new(&allocator.base))) {
      const SerdNode* interned2 = serd_nodes_intern(nodes, node);
      if (interned2) {
        assert(serd_node_equals(node, interned2));
        assert(serd_nodes_size(nodes) == 1U);
      }
      serd_nodes_free(nodes);
    }
  }

  serd_node_free(&allocator.base, node);
}

static void
test_intern(void)
{
  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* nodes = serd_nodes_new(allocator);
  SerdNode*  node  = serd_node_new(NULL, serd_a_string("node"));

  assert(serd_nodes_size(nodes) == 0U);
  assert(!serd_nodes_intern(nodes, NULL));

  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_size(nodes) == 1U);

  const SerdNode* interned2 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned2));
  assert(interned1 == interned2);
  assert(serd_nodes_size(nodes) == 1U);

  serd_node_free(NULL, node);
  serd_nodes_free(nodes);
}

static void
test_string(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("string");

  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdNode* const node =
    serd_nodes_get(nodes, serd_a_string_view(string));

  assert(node);
  assert(serd_node_type(node) == SERD_LITERAL);
  assert(!serd_node_flags(node));
  assert(!serd_node_datatype(node));
  assert(!serd_node_language(node));
  assert(serd_node_length(node) == string.length);
  assert(!strcmp(serd_node_string(node), string.data));

  const SerdNode* const long_node =
    serd_nodes_get(nodes, serd_a_literal(string, SERD_IS_LONG, NULL));

  assert(long_node);
  assert(long_node != node);
  assert(serd_node_type(long_node) == SERD_LITERAL);
  assert(serd_node_flags(long_node) == SERD_IS_LONG);
  assert(!serd_node_datatype(long_node));
  assert(!serd_node_language(long_node));
  assert(serd_node_length(long_node) == string.length);
  assert(!strcmp(serd_node_string(long_node), string.data));

  serd_nodes_free(nodes);
}

static void
test_invalid_literal(void)
{
  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const      nodes  = serd_nodes_new(allocator);
  const SerdNode* const string = serd_nodes_get(nodes, serd_a_string("s"));

  assert(!serd_nodes_get(nodes,
                         serd_a_literal(zix_string("double meta"),
                                        SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE,
                                        string)));

  assert(!serd_nodes_get(
    nodes,
    serd_a_literal(zix_string("empty language"), SERD_HAS_LANGUAGE, NULL)));

  assert(!serd_nodes_get(
    nodes,
    serd_a_literal(zix_string("empty datatype"), SERD_HAS_DATATYPE, NULL)));

  serd_nodes_free(nodes);
}

static void
test_plain_literal(void)
{
  static const ZixStringView string   = ZIX_STATIC_STRING("string");
  static const ZixStringView language = ZIX_STATIC_STRING("en");

  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const en = serd_nodes_get(nodes, serd_a_string("en"));
  const SerdNode* const de = serd_nodes_get(nodes, serd_a_string("de"));

  const SerdNode* const node =
    serd_nodes_get(nodes, serd_a_plain_literal(string, en));

  assert(node);
  assert(serd_node_type(node) == SERD_LITERAL);
  assert(serd_node_flags(node) == SERD_HAS_LANGUAGE);
  assert(!serd_node_datatype(node));

  const SerdNode* const language_node = serd_node_language(node);
  assert(language_node);
  assert(serd_node_type(language_node) == SERD_LITERAL);
  assert(!serd_node_flags(language_node));
  assert(serd_node_length(language_node) == language.length);
  assert(!strcmp(serd_node_string(language_node), language.data));
  assert(serd_node_length(node) == string.length);
  assert(!strcmp(serd_node_string(node), string.data));

  const SerdNode* const alias =
    serd_nodes_get(nodes, serd_a_plain_literal(string, en));

  assert(alias == node);

  const SerdNode* const long_version = serd_nodes_get(
    nodes, serd_a_literal(string, SERD_HAS_LANGUAGE | SERD_IS_LONG, en));

  assert(long_version != node);

  const SerdNode* const other =
    serd_nodes_get(nodes, serd_a_plain_literal(string, de));

  assert(other != node);

  const SerdNode* const other_language_node = serd_node_language(other);
  assert(other_language_node);
  assert(serd_node_type(other_language_node) == SERD_LITERAL);
  assert(!serd_node_flags(other_language_node));
  assert(serd_node_length(other_language_node) == 2);
  assert(!strcmp(serd_node_string(other_language_node), "de"));
  assert(serd_node_length(other) == string.length);
  assert(!strcmp(serd_node_string(other), string.data));

  serd_nodes_free(nodes);
}

static void
test_typed_literal(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("string");

  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdNode* const datatype =
    serd_nodes_get(nodes, serd_a_uri_string(NS_EG "Type"));

  const SerdNode* const node =
    serd_nodes_get(nodes, serd_a_typed_literal(string, datatype));

  assert(node);
  assert(serd_node_type(node) == SERD_LITERAL);
  assert(serd_node_flags(node) == SERD_HAS_DATATYPE);
  assert(!serd_node_language(node));

  const SerdNode* const datatype_node = serd_node_datatype(node);
  assert(datatype_node);

  assert(serd_node_type(datatype_node) == SERD_URI);
  assert(!serd_node_flags(datatype_node));
  assert(serd_node_length(datatype_node) == serd_node_length(datatype));
  assert(!strcmp(serd_node_string(datatype_node), serd_node_string(datatype)));
  assert(serd_node_length(node) == string.length);
  assert(!strcmp(serd_node_string(node), string.data));

  const SerdNode* const alias =
    serd_nodes_get(nodes, serd_a_typed_literal(string, datatype_node));

  assert(alias == node);

  serd_nodes_free(nodes);
}

static void
test_boolean(void)
{
  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const false1 =
    serd_nodes_get(nodes, serd_a_primitive(serd_bool(false)));

  const SerdNode* const false2 =
    serd_nodes_get(nodes, serd_a_primitive(serd_bool(false)));

  const SerdNode* const true1 =
    serd_nodes_get(nodes, serd_a_primitive(serd_bool(true)));

  const SerdNode* const true2 =
    serd_nodes_get(nodes, serd_a_primitive(serd_bool(true)));

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
  ZixAllocator* const allocator = zix_default_allocator();
  SerdNodes* const    nodes     = serd_nodes_new(allocator);

  const SerdNode* const a = serd_nodes_get(nodes, serd_a_decimal(-12.3456789));
  const SerdNode* const b = serd_nodes_get(nodes, serd_a_decimal(-12.3456789));

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
  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const a =
    serd_nodes_get(nodes, serd_a_primitive(serd_double(-1.2E3)));

  const SerdNode* const b =
    serd_nodes_get(nodes, serd_a_primitive(serd_double(-1.2E3)));

  assert(a == b);
  assert(!strcmp(serd_node_string(a), "-1.2E3"));
  assert(serd_node_length(a) == strlen(serd_node_string(a)));
  assert(serd_nodes_size(nodes) == 1);

  serd_nodes_free(nodes);
}

static void
test_float(void)
{
  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const a =
    serd_nodes_get(nodes, serd_a_primitive(serd_float(-1.2E3f)));

  const SerdNode* const b =
    serd_nodes_get(nodes, serd_a_primitive(serd_float(-1.2E3f)));

  assert(a == b);
  assert(!strcmp(serd_node_string(a), "-1.2E3"));
  assert(serd_node_length(a) == strlen(serd_node_string(a)));
  assert(serd_nodes_size(nodes) == 1);

  serd_nodes_free(nodes);
}

static void
test_integer(void)
{
  ZixAllocator* const allocator = zix_default_allocator();
  SerdNodes* const    nodes     = serd_nodes_new(allocator);

  const SerdNode* const a = serd_nodes_get(nodes, serd_a_integer(-1234567890));
  const SerdNode* const b = serd_nodes_get(nodes, serd_a_integer(-1234567890));

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

  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const a =
    serd_nodes_get(nodes, serd_a_base64(sizeof(data), &data));

  const SerdNode* const b =
    serd_nodes_get(nodes, serd_a_base64(sizeof(data), &data));

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
  static const ZixStringView string = ZIX_STATIC_STRING("http://example.org/");

  ZixAllocator* const allocator = zix_default_allocator();
  SerdNodes* const    nodes     = serd_nodes_new(allocator);

  const SerdNode* const node = serd_nodes_get(nodes, serd_a_uri(string));

  assert(node);
  assert(serd_node_type(node) == SERD_URI);
  assert(!serd_node_flags(node));
  assert(!serd_node_datatype(node));
  assert(!serd_node_language(node));
  assert(serd_node_length(node) == string.length);
  assert(!strcmp(serd_node_string(node), string.data));

  serd_nodes_free(nodes);
}

static void
test_parsed_uri(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("http://example.org/");

  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdURIView     uri   = serd_parse_uri(string.data);
  const SerdNode* const node  = serd_nodes_get(nodes, serd_a_parsed_uri(uri));

  assert(node);
  assert(serd_node_type(node) == SERD_URI);
  assert(!serd_node_flags(node));
  assert(!serd_node_datatype(node));
  assert(!serd_node_language(node));
  assert(serd_node_length(node) == string.length);
  assert(!strcmp(serd_node_string(node), string.data));

  const SerdNode* const alias = serd_nodes_get(nodes, serd_a_parsed_uri(uri));

  assert(alias == node);

  const SerdNode* const other =
    serd_nodes_get(nodes, serd_a_parsed_uri(serd_parse_uri(NS_EG "x")));

  assert(other != node);

  serd_nodes_free(nodes);
}

static void
test_file_uri(void)
{
  ZixAllocator* const allocator = zix_default_allocator();

  static const ZixStringView local_string =
    ZIX_STATIC_STRING("file:///d/f.txt");
  static const ZixStringView local_path  = ZIX_STATIC_STRING("/d/f.txt");
  static const ZixStringView remote_host = ZIX_STATIC_STRING("s");
  static const ZixStringView remote_string =
    ZIX_STATIC_STRING("file://s/d/f.txt");

  SerdNodes* const nodes = serd_nodes_new(allocator);

  const SerdNode* const local_uri =
    serd_nodes_get(nodes, serd_a_uri(local_string));
  const SerdNode* const local_file_uri =
    serd_nodes_get(nodes, serd_a_file_uri(local_path, zix_empty_string()));

  assert(local_uri);
  assert(local_file_uri);
  assert(local_uri == local_file_uri);
  assert(serd_node_type(local_uri) == SERD_URI);
  assert(!serd_node_flags(local_uri));
  assert(!serd_node_datatype(local_uri));
  assert(!serd_node_language(local_uri));
  assert(serd_node_length(local_uri) == local_string.length);
  assert(!strcmp(serd_node_string(local_uri), local_string.data));

  const SerdNode* const remote_uri =
    serd_nodes_get(nodes, serd_a_uri(remote_string));
  const SerdNode* const remote_file_uri =
    serd_nodes_get(nodes, serd_a_file_uri(local_path, remote_host));

  assert(remote_uri);
  assert(remote_file_uri);
  assert(remote_uri == remote_file_uri);
  assert(serd_node_type(remote_uri) == SERD_URI);
  assert(!serd_node_flags(remote_uri));
  assert(!serd_node_datatype(remote_uri));
  assert(!serd_node_language(remote_uri));
  assert(serd_node_length(remote_uri) == remote_string.length);
  assert(!strcmp(serd_node_string(remote_uri), remote_string.data));

  serd_nodes_free(nodes);
}

static void
test_blank(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("b42");

  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* const      nodes = serd_nodes_new(allocator);
  const SerdNode* const node  = serd_nodes_get(nodes, serd_a_blank(string));

  assert(node);
  assert(serd_node_type(node) == SERD_BLANK);
  assert(!serd_node_flags(node));
  assert(!serd_node_datatype(node));
  assert(!serd_node_language(node));
  assert(serd_node_length(node) == string.length);
  assert(!strcmp(serd_node_string(node), string.data));

  serd_nodes_free(nodes);
}

static void
test_deref(void)
{
  ZixAllocator* const allocator = zix_default_allocator();
  SerdNodes*          nodes     = serd_nodes_new(allocator);

  const SerdNode* original = serd_nodes_get(nodes, serd_a_string("node"));
  const SerdNode* another  = serd_nodes_get(nodes, serd_a_string("node"));

  assert(original == another);

  // Dereference the original and ensure the other reference kept it alive
  serd_nodes_deref(nodes, original);
  assert(serd_node_length(another) == 4);
  assert(!strcmp(serd_node_string(another), "node"));

  // Drop the other/final reference (freeing the node)
  serd_nodes_deref(nodes, another);

  /* Intern some other irrelevant node first to (hopefully) avoid the allocator
     just giving us back the same piece of memory. */

  const SerdNode* other = serd_nodes_get(nodes, serd_a_string("XXXX"));
  assert(!strcmp(serd_node_string(other), "XXXX"));

  // Intern a new equivalent node to the original and check that it's really new
  const SerdNode* imposter = serd_nodes_get(nodes, serd_a_string("node"));
  assert(imposter != original);
  assert(serd_node_length(imposter) == 4);
  assert(!strcmp(serd_node_string(imposter), "node"));

  // Check that dereferencing some random unknown node doesn't crash
  SerdNode* unmanaged = serd_node_new(NULL, serd_a_string("unmanaged"));
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
  ZixAllocator* const allocator = zix_default_allocator();

  SerdNodes* nodes = serd_nodes_new(allocator);
  SerdNode*  node  = serd_node_new(NULL, serd_a_string("node"));

  assert(!serd_nodes_existing(nodes, NULL));
  assert(!serd_nodes_existing(nodes, node));

  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_existing(nodes, node) == interned1);

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
  test_file_uri();
  test_blank();
  test_deref();
  test_get();
  return 0;
}
