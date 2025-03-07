// Copyright 2018-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/node_args.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/nodes.h>
#include <serd/object_view.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <serd/value.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define NS_EG "http://example.org/"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

#define N_SYMBOLS 20U

// Check that the expected number of nodes is mapped, ignoring builtin symbols
#define ASSERT_SIZE(nodes, count) \
  assert(serd_nodes_size(nodes) == N_SYMBOLS + (count))

static const ZixStringView long_string = ZIX_STATIC_STRING(
  "data:,"
  "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWX"
  "YZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV"
  "WXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRST"
  "UVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQR"
  "STUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOP"
  "QRSTUVWXYZ");

// Map a new node ID with sanity checks
static SerdNodeID
check_new_id(SerdNodes* const nodes, const SerdNodeArgs args)
{
  assert(!serd_nodes_existing_id(nodes, args));

  const SerdNodeID id = serd_nodes_id(nodes, args);

  assert(id);
  assert(id == serd_nodes_existing_id(nodes, args));
  assert(id == serd_nodes_id(nodes, args));
  return id;
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a node set to count the number of allocations
  SerdNodes* const nodes = serd_nodes_new(&allocator.base);
  assert(nodes);
  ASSERT_SIZE(nodes, 0U);

  // Test that each allocation failing is handled gracefully
  const size_t n_new_allocs = allocator.n_allocations;
  for (size_t i = 0; i < n_new_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(!serd_nodes_new(&allocator.base));
  }

  serd_nodes_free(nodes);
}

static void
check_bad_alloc(const SerdNodeArgs args)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully intern node to count the number of allocations
  SerdNodes*       nodes = serd_nodes_new(&allocator.base);
  const SerdNodeID id1   = serd_nodes_id(nodes, args);
  assert(id1);

  const size_t n_allocs = allocator.n_allocations;
  serd_nodes_free(nodes);

  // Test that each allocation failing is handled gracefully
  for (size_t i = 0; i < n_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);

    if ((nodes = serd_nodes_new(&allocator.base))) {
      assert(!serd_nodes_id(nodes, args));
    }

    serd_nodes_free(nodes);
  }
}

static void
test_intern_failed_alloc(void)
{
  check_bad_alloc(serd_a_token(SERD_URI, zix_string("a")));
  check_bad_alloc(
    serd_a_object(SERD_LITERAL,
                  zix_string("b"),
                  SERD_HAS_DATATYPE,
                  serd_token_view(SERD_URI, zix_string(NS_EG "Datatype"))));
  check_bad_alloc(
    serd_a_object(SERD_LITERAL,
                  zix_string("c"),
                  SERD_HAS_LANGUAGE,
                  serd_token_view(SERD_LITERAL, zix_string("en"))));
  check_bad_alloc(serd_a_uri_view(serd_parse_uri(NS_EG "d")));
  check_bad_alloc(serd_a_path(zix_string("e"), zix_string("f")));
  check_bad_alloc(serd_a_prefixed_name(zix_string("g"), zix_string("h")));
  check_bad_alloc(serd_a_joined_uri(zix_string(NS_EG), zix_string("i")));
  check_bad_alloc(serd_a_value(serd_byte(106U)));
  check_bad_alloc(serd_a_decimal(-78.9));
  check_bad_alloc(serd_a_integer(-12));
  check_bad_alloc(serd_a_hex(1U, "m"));
  check_bad_alloc(serd_a_base64(1U, "n"));
  check_bad_alloc(serd_a_hex(long_string.length, long_string.data));
}

static void
test_id(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  assert(!serd_nodes_get_token(nodes, 0U).type);
  assert(!serd_nodes_get_object(nodes, 0U).type);
  assert(!serd_nodes_existing_id(nodes, serd_a_node_id(0U)));
  assert(!serd_nodes_id(nodes, serd_a_token(SERD_NOTHING, zix_string("0"))));
  assert(!serd_nodes_existing_id(nodes,
                                 serd_a_token(SERD_NOTHING, zix_string("0"))));

  const SerdNodeID id = check_new_id(nodes, serd_a_string(zix_string("lit")));
  ASSERT_SIZE(nodes, 1U);
  assert(id == serd_nodes_id(nodes, serd_a_string(zix_string("lit"))));
  ASSERT_SIZE(nodes, 1U);

  assert(serd_nodes_existing_id(nodes, serd_a_node_id(id)) == id);
  assert(!serd_nodes_existing_id(nodes, serd_a_node_id(0U)));
  assert(!serd_nodes_existing_id(nodes, serd_a_node_id(N_SYMBOLS + 2U)));
  assert(serd_nodes_id(nodes, serd_a_node_id(id)) == id);
  assert(!serd_nodes_id(nodes, serd_a_node_id(0U)));
  assert(!serd_nodes_id(nodes, serd_a_node_id(N_SYMBOLS + 2U)));

  serd_nodes_free(nodes);
}

static void
test_token(void)
{
  SerdNodes* const    nodes = serd_nodes_new(NULL);
  const SerdTokenView token = serd_token_view(SERD_BLANK, zix_string("b"));

  const SerdNodeID id = check_new_id(nodes, serd_a_token_view(token));

  // Identical representations
  assert(id == serd_nodes_id(nodes, serd_a_token_view(token)));
  assert(id == serd_nodes_id(nodes, serd_a_token(SERD_BLANK, zix_string("b"))));
  ASSERT_SIZE(nodes, 1U);

  // Different string
  assert(id != serd_nodes_id(nodes, serd_a_token(SERD_BLANK, zix_string("c"))));
  ASSERT_SIZE(nodes, 2U);

  // Different type
  assert(id !=
         serd_nodes_id(nodes, serd_a_token(SERD_LITERAL, zix_string("b"))));
  ASSERT_SIZE(nodes, 3U);

  // Different type and (empty) string
  assert(id != serd_nodes_id(nodes, serd_a_token(SERD_BLANK, zix_string(""))));
  ASSERT_SIZE(nodes, 4U);

  serd_nodes_free(nodes);
}

static void
test_object(void)
{
  SerdNodes* const     nodes = serd_nodes_new(NULL);
  const SerdObjectView object =
    serd_object_view(SERD_BLANK, zix_string("b"), 0U, serd_no_token());

  const SerdNodeID id = check_new_id(nodes, serd_a_object_view(object));

  // Identical representations
  assert(id == serd_nodes_id(nodes, serd_a_object_view(object)));
  assert(
    id ==
    serd_nodes_id(
      nodes, serd_a_object(SERD_BLANK, zix_string("b"), 0U, serd_no_token())));
  ASSERT_SIZE(nodes, 1U);

  // Different string
  assert(
    id !=
    serd_nodes_id(
      nodes, serd_a_object(SERD_BLANK, zix_string("c"), 0U, serd_no_token())));
  ASSERT_SIZE(nodes, 2U);

  // Different type
  assert(id !=
         serd_nodes_id(
           nodes,
           serd_a_object(SERD_LITERAL, zix_string("b"), 0U, serd_no_token())));
  ASSERT_SIZE(nodes, 3U);

  // Different type and (empty) string
  assert(
    id !=
    serd_nodes_id(
      nodes, serd_a_object(SERD_BLANK, zix_string(""), 0U, serd_no_token())));
  ASSERT_SIZE(nodes, 4U);

  // Different datatypes
  assert(serd_nodes_id(
           nodes,
           serd_a_object(SERD_LITERAL,
                         zix_string("4"),
                         SERD_HAS_DATATYPE,
                         serd_token_view(SERD_URI, zix_string(NS_EG "int")))) !=
         serd_nodes_id(
           nodes,
           serd_a_object(SERD_LITERAL,
                         zix_string("4"),
                         SERD_HAS_DATATYPE,
                         serd_token_view(SERD_URI, zix_string(NS_EG "long")))));
  ASSERT_SIZE(nodes, 8U);

  // Different languages
  assert(serd_nodes_id(
           nodes,
           serd_a_object(SERD_LITERAL,
                         zix_string("hello"),
                         SERD_HAS_LANGUAGE,
                         serd_token_view(SERD_LITERAL, zix_string("en")))) !=
         serd_nodes_id(
           nodes,
           serd_a_object(SERD_LITERAL,
                         zix_string("hallo"),
                         SERD_HAS_LANGUAGE,
                         serd_token_view(SERD_LITERAL, zix_string("de")))));
  ASSERT_SIZE(nodes, 12U);

  // Bad meta node type
  assert(
    !serd_nodes_id(nodes,
                   serd_a_object(SERD_LITERAL,
                                 zix_string("hello"),
                                 SERD_HAS_LANGUAGE,
                                 serd_token_view(SERD_URI, zix_string("en")))));
  assert(!serd_nodes_id(
    nodes,
    serd_a_object(SERD_LITERAL,
                  zix_string("hallo"),
                  SERD_HAS_DATATYPE,
                  serd_token_view(SERD_LITERAL, zix_string(NS_EG "uri")))));

  // Invalid flags with ambiguous meta node
  assert(!serd_nodes_id(
    nodes,
    serd_a_object(SERD_LITERAL,
                  zix_string("s"),
                  SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE,
                  serd_token_view(SERD_LITERAL, zix_string("ambiguous")))));

  // Mismatched flags and meta node string
  assert(!serd_nodes_id(
    nodes,
    serd_a_object(SERD_LITERAL,
                  zix_string("s"),
                  SERD_HAS_DATATYPE,
                  serd_token_view(SERD_LITERAL, zix_string("tag")))));
  assert(!serd_nodes_id(
    nodes,
    serd_a_object(SERD_LITERAL,
                  zix_string("s"),
                  SERD_HAS_LANGUAGE,
                  serd_token_view(SERD_URI, zix_string(NS_EG "uri")))));

  // Invalid datatype URI
  assert(!serd_nodes_id(
    nodes,
    serd_a_object(SERD_LITERAL,
                  zix_string("s"),
                  SERD_HAS_DATATYPE,
                  serd_token_view(SERD_URI, zix_string("rel")))));
  assert(!serd_nodes_existing_id(
    nodes,
    serd_a_object(SERD_LITERAL,
                  zix_string("s"),
                  SERD_HAS_DATATYPE,
                  serd_token_view(SERD_URI, zix_string("en-CA")))));

  ASSERT_SIZE(nodes, 12U);
  serd_nodes_free(nodes);
}

static void
test_blank(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("b42");

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID id = check_new_id(nodes, serd_a_blank(string));
  ASSERT_SIZE(nodes, 1U);

  const SerdTokenView node = serd_nodes_get_token(nodes, id);
  assert(serd_token_view_equals(node, serd_token_view(SERD_BLANK, string)));

  serd_nodes_free(nodes);
}

static void
check_uri(SerdNodes* const nodes, const char* const string_data)
{
  const ZixStringView string = zix_string(string_data);
  const SerdURIView   uri    = serd_parse_uri(string.data);
  const SerdNodeID    id     = check_new_id(nodes, serd_a_uri(string));

  assert(id == serd_nodes_id(nodes, serd_a_uri_view(uri)));
  assert(serd_token_view_equals(serd_nodes_get_token(nodes, id),
                                serd_token_view(SERD_URI, string)));
}

static void
test_uri(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  check_uri(nodes, "http:");
  check_uri(nodes, "http://example.org");
  check_uri(nodes, "http://example.org/");
  check_uri(nodes, "http://example.org/path");
  check_uri(nodes, "http://example.org/path?query");
  check_uri(nodes, "http://example.org/path?query#fragment");
  check_uri(nodes, "http://example.org/path#fragment");
  check_uri(nodes, "http://example.org/?query");
  check_uri(nodes, "http://example.org/?query#fragment");
  check_uri(nodes, "http://example.org/#fragment");
  check_uri(nodes, "http://example.org?query#fragment");
  check_uri(nodes, "http://example.org#fragment");

  serd_nodes_free(nodes);
}

static void
test_path(void)
{
  static const ZixStringView local_string  = ZIX_STATIC_STRING("file:///d/f");
  static const ZixStringView local_path    = ZIX_STATIC_STRING("/d/f");
  static const ZixStringView remote_host   = ZIX_STATIC_STRING("s");
  static const ZixStringView remote_string = ZIX_STATIC_STRING("file://s/d/f");

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID local_id = check_new_id(nodes, serd_a_uri(local_string));
  assert(local_id ==
         serd_nodes_id(nodes, serd_a_path(local_path, zix_empty_string())));
  assert(
    local_id ==
    serd_nodes_id(nodes, serd_a_uri_view(serd_parse_uri(local_string.data))));
  ASSERT_SIZE(nodes, 1U);
  assert(serd_token_view_equals(serd_nodes_get_token(nodes, local_id),
                                serd_token_view(SERD_URI, local_string)));

  const SerdNodeID remote_id = check_new_id(nodes, serd_a_uri(remote_string));
  assert(remote_id ==
         serd_nodes_id(nodes, serd_a_path(local_path, remote_host)));
  assert(
    remote_id ==
    serd_nodes_id(nodes, serd_a_uri_view(serd_parse_uri(remote_string.data))));
  ASSERT_SIZE(nodes, 2U);
  assert(serd_token_view_equals(serd_nodes_get_token(nodes, remote_id),
                                serd_token_view(SERD_URI, remote_string)));

  serd_nodes_free(nodes);
}

static void
test_prefixed_name(void)
{
  SerdNodes* const   nodes = serd_nodes_new(NULL);
  const SerdNodeArgs args =
    serd_a_prefixed_name(zix_string("eg"), zix_string("path"));

  const SerdNodeID id = check_new_id(nodes, args);
  assert(id == serd_nodes_id(nodes, args));
  ASSERT_SIZE(nodes, 1U);
  assert(
    serd_token_view_equals(serd_nodes_get_token(nodes, id),
                           serd_token_view(SERD_CURIE, zix_string("eg:path"))));

  serd_nodes_free(nodes);
}

static void
check_joined_uri(SerdNodes* const nodes, const char* const string_data)
{
  const ZixStringView string = zix_string(string_data);
  const SerdNodeID    id     = check_new_id(nodes, serd_a_uri(string));

  assert(serd_token_view_equals(serd_nodes_get_token(nodes, id),
                                serd_token_view(SERD_URI, string)));

  for (size_t i = 0U; i < string.length; ++i) {
    assert(id ==
           serd_nodes_id(nodes,
                         serd_a_joined_uri(
                           zix_substring(string_data, i),
                           zix_substring(string_data + i, string.length - i))));
  }
}

static void
test_joined_uri(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  check_joined_uri(nodes, "ftp:");
  check_joined_uri(nodes, "urn:isbn:0140445684");
  check_joined_uri(nodes, "http://example.org/path?query#fragment");

  serd_nodes_free(nodes);
}

static void
test_string(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("string");

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID short_id = check_new_id(nodes, serd_a_string(string));
  ASSERT_SIZE(nodes, 1U);
  assert(short_id ==
         serd_nodes_id(
           nodes, serd_a_object(SERD_LITERAL, string, 0U, serd_no_token())));

  const SerdObjectView short_view = serd_nodes_get_object(nodes, short_id);
  assert(short_view.type == SERD_LITERAL);
  assert(!short_view.flags);
  assert(!short_view.meta.string.length);
  assert(short_view.string.length == string.length);
  assert(!strcmp(short_view.string.data, string.data));

  const SerdNodeID long_id = check_new_id(
    nodes, serd_a_object(SERD_LITERAL, string, SERD_IS_LONG, serd_no_token()));
  assert(long_id != short_id);
  ASSERT_SIZE(nodes, 2U);

  const SerdObjectView long_view = serd_nodes_get_object(nodes, long_id);
  assert(long_view.type == SERD_LITERAL);
  assert(long_view.flags == SERD_IS_LONG);
  assert(!long_view.meta.string.length);
  assert(long_view.string.length == string.length);
  assert(!strcmp(long_view.string.data, string.data));

  serd_nodes_free(nodes);
}

static void
test_literal(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("42");

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID xsd_short_id =
    serd_nodes_id(nodes, serd_a_uri(zix_string(NS_XSD "short")));

  const SerdNodeID short_id = serd_nodes_id(
    nodes, serd_a_literal(string, SERD_HAS_DATATYPE, xsd_short_id));
  ASSERT_SIZE(nodes, 1U);
  assert(short_id);
  assert(short_id ==
         serd_nodes_id(nodes,
                       serd_a_object(SERD_LITERAL,
                                     string,
                                     SERD_HAS_DATATYPE,
                                     serd_token_view(
                                       SERD_URI, zix_string(NS_XSD "short")))));
  assert(short_id ==
         serd_nodes_existing_id(
           nodes, serd_a_literal(string, SERD_HAS_DATATYPE, xsd_short_id)));

  // Invalid datatype ID
  assert(!serd_nodes_id(nodes, serd_a_node_id(N_SYMBOLS + 2U)));
  assert(!serd_nodes_id(
    nodes, serd_a_literal(string, SERD_HAS_DATATYPE, N_SYMBOLS + 2U)));
  assert(!serd_nodes_existing_id(
    nodes, serd_a_literal(string, SERD_HAS_DATATYPE, N_SYMBOLS + 2U)));

  serd_nodes_free(nodes);
}

static void
test_invalid_literal(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  assert(!serd_nodes_id(
    nodes,
    serd_a_object(SERD_LITERAL,
                  zix_string("double meta"),
                  SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE,
                  serd_token_view(SERD_LITERAL, zix_string("s")))));

  assert(!serd_nodes_id(nodes,
                        serd_a_object(SERD_LITERAL,
                                      zix_string("empty language"),
                                      SERD_HAS_LANGUAGE,
                                      serd_no_token())));

  assert(!serd_nodes_id(nodes,
                        serd_a_object(SERD_LITERAL,
                                      zix_string("empty datatype"),
                                      SERD_HAS_DATATYPE,
                                      serd_no_token())));

  serd_nodes_free(nodes);
}

static void
test_plain_literal(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("string");
  static const ZixStringView en     = ZIX_STATIC_STRING("en");
  static const ZixStringView de     = ZIX_STATIC_STRING("de");

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID node_id =
    check_new_id(nodes, serd_a_plain_literal(string, en));
  ASSERT_SIZE(nodes, 2U); // node and en

  const SerdObjectView node = serd_nodes_get_object(nodes, node_id);
  assert(node.type == SERD_LITERAL);
  assert(node.flags == SERD_HAS_LANGUAGE);
  assert(zix_string_view_equals(node.meta.string, en));

  const SerdNodeID long_node_id =
    check_new_id(nodes,
                 serd_a_object(SERD_LITERAL,
                               string,
                               SERD_HAS_LANGUAGE | SERD_IS_LONG,
                               serd_token_view(SERD_LITERAL, en)));
  assert(long_node_id != node_id);
  ASSERT_SIZE(nodes, 3U); // node, long_node, and en

  const SerdObjectView long_node = serd_nodes_get_object(nodes, long_node_id);
  assert(long_node.type == SERD_LITERAL);
  assert(long_node.flags == (SERD_HAS_LANGUAGE | SERD_IS_LONG));
  assert(zix_string_view_equals(long_node.meta.string, en));

  const SerdNodeID other_id =
    check_new_id(nodes, serd_a_plain_literal(string, de));
  assert(other_id != node_id);
  ASSERT_SIZE(nodes, 5U); // node, long_node, other, en, and de

  const SerdObjectView other = serd_nodes_get_object(nodes, other_id);
  assert(other.flags == SERD_HAS_LANGUAGE);
  assert(zix_string_view_equals(other.string, string));
  assert(zix_string_view_equals(other.meta.string, de));

  // More complex language tags
  assert(
    serd_nodes_id(nodes, serd_a_plain_literal(string, zix_string("es-419"))));
  assert(serd_nodes_id(
    nodes, serd_a_plain_literal(string, zix_string("gsw-u-sd-chzh"))));

  // Invalid language tags
  assert(!serd_nodes_id(nodes, serd_a_plain_literal(string, zix_string("3n"))));
  assert(!serd_nodes_id(nodes, serd_a_plain_literal(string, zix_string("d3"))));
  assert(
    !serd_nodes_id(nodes, serd_a_plain_literal(string, zix_string("en-!"))));
  assert(
    !serd_nodes_id(nodes, serd_a_plain_literal(string, zix_string("-en"))));
  assert(!serd_nodes_id(
    nodes,
    serd_a_object(SERD_LITERAL,
                  string,
                  SERD_HAS_LANGUAGE,
                  serd_token_view(SERD_LITERAL, zix_string("")))));

  serd_nodes_free(nodes);
}

static void
test_typed_literal(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("string");

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID node_id =
    check_new_id(nodes, serd_a_typed_literal(string, zix_string(NS_EG "Type")));
  ASSERT_SIZE(nodes, 2U); // node and datatype

  const SerdObjectView node = serd_nodes_get_object(nodes, node_id);
  assert(node.type == SERD_LITERAL);
  assert(node.flags == SERD_HAS_DATATYPE);
  assert(zix_string_view_equals(node.string, string));
  assert(zix_string_view_equals(node.meta.string, zix_string(NS_EG "Type")));

  serd_nodes_free(nodes);
}

static void
check_primitive(const SerdValue   value,
                const char* const node_string_data,
                const char* const datatype_string_data)
{
  static const ZixStringView other_node_string = ZIX_STATIC_STRING("0");
  static const ZixStringView other_datatype_string =
    ZIX_STATIC_STRING(NS_EG "Datatype");

  const ZixStringView node_string     = zix_string(node_string_data);
  const ZixStringView datatype_string = zix_string(datatype_string_data);

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID id = check_new_id(nodes, serd_a_value(value));
  assert(id == serd_nodes_id(
                 nodes,
                 serd_a_object(SERD_LITERAL,
                               node_string,
                               SERD_HAS_DATATYPE,
                               serd_token_view(SERD_URI, datatype_string))));

  ASSERT_SIZE(nodes, 1U); // just the node (datatype is built in)

  assert(serd_object_view_equals(
    serd_nodes_get_object(nodes, id),
    serd_object_view(SERD_LITERAL,
                     node_string,
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, datatype_string))));

  assert(id != serd_nodes_id(
                 nodes,
                 serd_a_object(SERD_LITERAL,
                               other_node_string,
                               SERD_HAS_DATATYPE,
                               serd_token_view(SERD_URI, datatype_string))));
  assert(id !=
         serd_nodes_id(
           nodes,
           serd_a_object(SERD_LITERAL,
                         node_string,
                         SERD_HAS_DATATYPE,
                         serd_token_view(SERD_URI, other_datatype_string))));

  serd_nodes_free(nodes);
}

static void
test_primitive(void)
{
  {
    SerdNodes* const nodes = serd_nodes_new(NULL);
    assert(!serd_nodes_id(nodes, serd_a_value(serd_no_value())));
    serd_nodes_free(nodes);
  }

  check_primitive(serd_bool(false), "false", NS_XSD "boolean");

  check_primitive(serd_double(-1.2E3), "-1.2E3", NS_XSD "double");
  check_primitive(serd_float(-1.2E3f), "-1.2E3", NS_XSD "float");

  check_primitive(serd_long(-12345678901L), "-12345678901", NS_XSD "long");
  check_primitive(serd_int(-1234567890), "-1234567890", NS_XSD "int");
  check_primitive(serd_short(-12345), "-12345", NS_XSD "short");
  check_primitive(serd_byte(-123), "-123", NS_XSD "byte");

  check_primitive(
    serd_ulong(12345678901UL), "12345678901", NS_XSD "unsignedLong");
  check_primitive(serd_uint(1234567890U), "1234567890", NS_XSD "unsignedInt");
  check_primitive(serd_ushort(12345U), "12345", NS_XSD "unsignedShort");
  check_primitive(serd_ubyte(123U), "123", NS_XSD "unsignedByte");
}

static void
test_boolean(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID false_id =
    check_new_id(nodes, serd_a_value(serd_bool(false)));
  assert(
    false_id ==
    serd_nodes_id(
      nodes,
      serd_a_object(SERD_LITERAL,
                    zix_string("false"),
                    SERD_HAS_DATATYPE,
                    serd_token_view(SERD_URI, zix_string(NS_XSD "boolean")))));
  ASSERT_SIZE(nodes, 1U); // just the node (datatype is built in)

  const SerdObjectView false_node = serd_nodes_get_object(nodes, false_id);
  assert(false_node.type == SERD_LITERAL);
  assert(false_node.flags == SERD_HAS_DATATYPE);
  assert(zix_string_view_equals(false_node.string, zix_string("false")));
  assert(zix_string_view_equals(false_node.meta.string,
                                zix_string(NS_XSD "boolean")));

  const SerdNodeID true_id = check_new_id(nodes, serd_a_value(serd_bool(true)));
  assert(true_id != false_id);
  assert(
    true_id ==
    serd_nodes_id(
      nodes,
      serd_a_object(SERD_LITERAL,
                    zix_string("true"),
                    SERD_HAS_DATATYPE,
                    serd_token_view(SERD_URI, zix_string(NS_XSD "boolean")))));
  ASSERT_SIZE(nodes, 2U); // just the node (datatype is built in)

  const SerdObjectView true_node = serd_nodes_get_object(nodes, true_id);
  assert(true_node.type == SERD_LITERAL);
  assert(true_node.flags == SERD_HAS_DATATYPE);
  assert(zix_string_view_equals(true_node.string, zix_string("true")));
  assert(zix_string_view_equals(true_node.meta.string,
                                zix_string(NS_XSD "boolean")));

  serd_nodes_free(nodes);
}

static void
test_decimal(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID id = check_new_id(nodes, serd_a_decimal(-12.3456789));
  ASSERT_SIZE(nodes, 1U); // just the node (datatype is built in)

  const SerdObjectView node = serd_nodes_get_object(nodes, id);
  assert(node.type == SERD_LITERAL);
  assert(node.flags == SERD_HAS_DATATYPE);
  assert(zix_string_view_equals(node.string, zix_string("-12.3456789")));
  assert(
    zix_string_view_equals(node.meta.string, zix_string(NS_XSD "decimal")));

  serd_nodes_free(nodes);
}

static void
test_integer(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID id = check_new_id(nodes, serd_a_integer(-1234567890));
  ASSERT_SIZE(nodes, 1U); // just the node (datatype is built in)

  const SerdObjectView node = serd_nodes_get_object(nodes, id);
  assert(node.type == SERD_LITERAL);
  assert(node.flags == SERD_HAS_DATATYPE);
  assert(zix_string_view_equals(node.string, zix_string("-1234567890")));
  assert(
    zix_string_view_equals(node.meta.string, zix_string(NS_XSD "integer")));

  serd_nodes_free(nodes);
}

static void
test_hex(void)
{
  static const char data[] = {'f', 'o', 'o', 'b', 'a', 'r'};

  SerdNodes* const nodes = serd_nodes_new(NULL);

  assert(!serd_nodes_id(nodes, serd_a_hex(0U, &data)));

  const SerdNodeID id = check_new_id(nodes, serd_a_hex(sizeof(data), &data));
  ASSERT_SIZE(nodes, 1U); // just the node (datatype is built in)

  assert(serd_object_view_equals(
    serd_nodes_get_object(nodes, id),
    serd_object_view(
      SERD_LITERAL,
      zix_string("666F6F626172"),
      SERD_HAS_DATATYPE,
      serd_token_view(SERD_URI, zix_string(NS_XSD "hexBinary")))));

  assert(id == serd_nodes_existing_id(
                 nodes,
                 serd_a_object(
                   SERD_LITERAL,
                   zix_string("666F6F626172"),
                   SERD_HAS_DATATYPE,
                   serd_token_view(SERD_URI, zix_string(NS_XSD "hexBinary")))));

  serd_nodes_free(nodes);
}

static void
test_base64(void)
{
  static const char data[] = {'f', 'o', 'o', 'b', 'a', 'r'};

  SerdNodes* const nodes = serd_nodes_new(NULL);

  assert(!serd_nodes_id(nodes, serd_a_base64(0U, &data)));

  const SerdNodeID id = check_new_id(nodes, serd_a_base64(sizeof(data), &data));
  ASSERT_SIZE(nodes, 1U); // just the node (datatype is built in)

  assert(serd_object_view_equals(
    serd_nodes_get_object(nodes, id),
    serd_object_view(
      SERD_LITERAL,
      zix_string("Zm9vYmFy"),
      SERD_HAS_DATATYPE,
      serd_token_view(SERD_URI, zix_string(NS_XSD "base64Binary")))));

  assert(id ==
         serd_nodes_existing_id(
           nodes,
           serd_a_object(
             SERD_LITERAL,
             zix_string("Zm9vYmFy"),
             SERD_HAS_DATATYPE,
             serd_token_view(SERD_URI, zix_string(NS_XSD "base64Binary")))));

  serd_nodes_free(nodes);
}

static void
check_large(const SerdNodeArgs args)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  // Initially not present (possibly because the initial buffer is exceeded)
  assert(!serd_nodes_existing_id(nodes, args));

  // Present after mapping (and the initially buffer may have been expanded)
  const SerdNodeID id = serd_nodes_id(nodes, args);
  assert(id);
  assert(id == serd_nodes_existing_id(nodes, args));

  serd_nodes_free(nodes);
}

static void
test_large(void)
{
  const SerdURIView long_uri = serd_parse_uri(long_string.data);

  check_large(serd_a_token(SERD_LITERAL, long_string));
  check_large(serd_a_object(SERD_LITERAL, long_string, 0U, serd_no_token()));
  check_large(serd_a_uri_view(long_uri));
  check_large(serd_a_path(long_string, zix_string("host")));
  check_large(serd_a_prefixed_name(zix_string("ns"), long_string));
  check_large(serd_a_joined_uri(zix_string("data:,"), long_string));
  check_large(serd_a_hex(long_string.length, long_string.data));
  check_large(serd_a_base64(long_string.length, long_string.data));
}

static void
test_get_token(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdTokenView token = serd_nodes_get_token(nodes, N_SYMBOLS + 1U);
  assert(!token.type);
  assert(zix_string_view_equals(token.string, zix_empty_string()));

  const SerdNodeID id = check_new_id(nodes, serd_a_blank(zix_string("b")));
  assert(serd_nodes_get_token(nodes, id).type == SERD_BLANK);

  serd_nodes_free(nodes);
}

static void
test_get_object(void)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdObjectView object = serd_nodes_get_object(nodes, N_SYMBOLS + 1U);
  assert(!object.type);
  assert(!object.flags);
  assert(zix_string_view_equals(object.string, zix_empty_string()));
  assert(!object.meta.type);
  assert(zix_string_view_equals(object.meta.string, zix_empty_string()));

  const SerdNodeID id = check_new_id(nodes, serd_a_blank(zix_string("b")));
  assert(serd_nodes_get_object(nodes, id).type == SERD_BLANK);

  serd_nodes_free(nodes);
}

int
main(void)
{
  test_new_failed_alloc();
  test_intern_failed_alloc();
  test_id();
  test_token();
  test_object();
  test_blank();
  test_uri();
  test_path();
  test_prefixed_name();
  test_joined_uri();
  test_string();
  test_literal();
  test_invalid_literal();
  test_plain_literal();
  test_typed_literal();
  test_primitive();
  test_boolean();
  test_decimal();
  test_integer();
  test_hex();
  test_base64();
  test_large();
  test_get_token();
  test_get_object();
  return 0;
}
