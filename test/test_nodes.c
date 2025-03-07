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
#include <serd/strings.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <serd/value.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#define NS_EG "http://example.org/"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

enum { N_SYMBOLS = 20U };

typedef struct {
  SerdNodes*   nodes;
  SerdStrings* strings;
} TestContext;

static const ZixStringView long_string = ZIX_STATIC_STRING(
  "data:,"
  "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWX"
  "YZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV"
  "WXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRST"
  "UVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQR"
  "STUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOP"
  "QRSTUVWXYZ");

static TestContext
test_context_make(void)
{
  SerdNodes* const   nodes   = serd_nodes_new(NULL);
  SerdStrings* const strings = serd_strings_new(NULL, nodes);
  const TestContext  ctx     = {nodes, strings};
  return ctx;
}

static void
test_context_destroy(TestContext* const ctx)
{
  serd_strings_free(ctx->strings);
  serd_nodes_free(ctx->nodes);
}

// Map a new node ID with sanity checks
static SerdNodeID
check_new_id(SerdNodes* const nodes, const SerdNodeArgs args)
{
  assert(!serd_nodes_find(nodes, args));

  const SerdNodeID id = serd_nodes_id(nodes, args);

  assert(id);
  assert(id == serd_nodes_find(nodes, args));
  assert(id == serd_nodes_id(nodes, args));
  return id;
}

static bool
token_equals(SerdStrings* const  strings,
             const SerdNodeID    id,
             const SerdTokenView token)
{
  return serd_token_view_equals(serd_strings_token(strings, id), token);
}

static bool
object_equals(SerdStrings* const   strings,
              const SerdNodeID     id,
              const SerdObjectView object)
{
  return serd_object_view_equals(serd_strings_object(strings, id), object);
}

static void
test_new_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  // Successfully allocate a node set to count the number of allocations
  SerdNodes* const nodes = serd_nodes_new(&allocator.base);
  assert(nodes);

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

  assert(!serd_nodes_type(nodes, 0U));
  assert(!serd_nodes_find(nodes, serd_a_node_id(0U)));
  assert(!serd_nodes_id(nodes, serd_a_token(SERD_NOTHING, zix_string("0"))));
  assert(!serd_nodes_find(nodes, serd_a_token(SERD_NOTHING, zix_string("0"))));

  const SerdNodeID id = check_new_id(nodes, serd_a_string(zix_string("lit")));
  assert(id == serd_nodes_id(nodes, serd_a_string(zix_string("lit"))));

  assert(serd_nodes_find(nodes, serd_a_node_id(id)) == id);
  assert(!serd_nodes_find(nodes, serd_a_node_id(0U)));
  assert(!serd_nodes_find(nodes, serd_a_node_id(N_SYMBOLS + 2U)));
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

  // Different string
  assert(id != serd_nodes_id(nodes, serd_a_token(SERD_BLANK, zix_string("c"))));

  // Different type
  assert(id !=
         serd_nodes_id(nodes, serd_a_token(SERD_LITERAL, zix_string("b"))));

  // Different type and (empty) string
  assert(id != serd_nodes_id(nodes, serd_a_token(SERD_BLANK, zix_string(""))));

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

  // Different string
  assert(
    id !=
    serd_nodes_id(
      nodes, serd_a_object(SERD_BLANK, zix_string("c"), 0U, serd_no_token())));

  // Different type
  assert(id !=
         serd_nodes_id(
           nodes,
           serd_a_object(SERD_LITERAL, zix_string("b"), 0U, serd_no_token())));

  // Different type and (empty) string
  assert(
    id !=
    serd_nodes_id(
      nodes, serd_a_object(SERD_BLANK, zix_string(""), 0U, serd_no_token())));

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
  assert(!serd_nodes_find(
    nodes,
    serd_a_object(SERD_LITERAL,
                  zix_string("s"),
                  SERD_HAS_DATATYPE,
                  serd_token_view(SERD_URI, zix_string("en-CA")))));

  serd_nodes_free(nodes);
}

static void
test_blank(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("b42");

  TestContext ctx = test_context_make();

  const SerdNodeID id = check_new_id(ctx.nodes, serd_a_blank(string));
  assert(token_equals(ctx.strings, id, serd_token_view(SERD_BLANK, string)));

  test_context_destroy(&ctx);
}

static void
check_uri(SerdNodes* const   nodes,
          SerdStrings* const strings,
          const char* const  string_data)
{
  const ZixStringView string = zix_string(string_data);
  const SerdURIView   uri    = serd_parse_uri(string.data);
  const SerdNodeID    id     = check_new_id(nodes, serd_a_uri(string));

  assert(id == serd_nodes_id(nodes, serd_a_uri_view(uri)));
  assert(token_equals(strings, id, serd_token_view(SERD_URI, string)));
}

static void
test_uri(void)
{
  SerdNodes* const   nodes   = serd_nodes_new(NULL);
  SerdStrings* const strings = serd_strings_new(NULL, nodes);

  check_uri(nodes, strings, "http:");
  check_uri(nodes, strings, "http://example.org");
  check_uri(nodes, strings, "http://example.org/");
  check_uri(nodes, strings, "http://example.org/path");
  check_uri(nodes, strings, "http://example.org/path?query");
  check_uri(nodes, strings, "http://example.org/path?query#fragment");
  check_uri(nodes, strings, "http://example.org/path#fragment");
  check_uri(nodes, strings, "http://example.org/?query");
  check_uri(nodes, strings, "http://example.org/?query#fragment");
  check_uri(nodes, strings, "http://example.org/#fragment");
  check_uri(nodes, strings, "http://example.org?query#fragment");
  check_uri(nodes, strings, "http://example.org#fragment");

  serd_strings_free(strings);
  serd_nodes_free(nodes);
}

static void
test_path(void)
{
  static const ZixStringView local_string  = ZIX_STATIC_STRING("file:///d/f");
  static const ZixStringView local_path    = ZIX_STATIC_STRING("/d/f");
  static const ZixStringView remote_host   = ZIX_STATIC_STRING("s");
  static const ZixStringView remote_string = ZIX_STATIC_STRING("file://s/d/f");

  SerdNodes* const   nodes   = serd_nodes_new(NULL);
  SerdStrings* const strings = serd_strings_new(NULL, nodes);

  const SerdNodeID local_id = check_new_id(nodes, serd_a_uri(local_string));
  assert(local_id ==
         serd_nodes_id(nodes, serd_a_path(local_path, zix_empty_string())));
  assert(
    local_id ==
    serd_nodes_id(nodes, serd_a_uri_view(serd_parse_uri(local_string.data))));
  assert(
    token_equals(strings, local_id, serd_token_view(SERD_URI, local_string)));

  const SerdNodeID remote_id = check_new_id(nodes, serd_a_uri(remote_string));
  assert(remote_id ==
         serd_nodes_id(nodes, serd_a_path(local_path, remote_host)));
  assert(
    remote_id ==
    serd_nodes_id(nodes, serd_a_uri_view(serd_parse_uri(remote_string.data))));
  assert(
    token_equals(strings, remote_id, serd_token_view(SERD_URI, remote_string)));

  serd_strings_free(strings);
  serd_nodes_free(nodes);
}

static void
test_prefixed_name(void)
{
  SerdNodes* const   nodes   = serd_nodes_new(NULL);
  SerdStrings* const strings = serd_strings_new(NULL, nodes);

  const SerdNodeArgs args =
    serd_a_prefixed_name(zix_string("eg"), zix_string("path"));

  const SerdNodeID id = check_new_id(nodes, args);
  assert(id == serd_nodes_id(nodes, args));
  assert(token_equals(
    strings, id, serd_token_view(SERD_CURIE, zix_string("eg:path"))));

  serd_strings_free(strings);
  serd_nodes_free(nodes);
}

static void
check_joined_uri(const TestContext* const ctx, const char* const string_data)
{
  const ZixStringView string = zix_string(string_data);
  const SerdNodeID    id     = check_new_id(ctx->nodes, serd_a_uri(string));

  assert(token_equals(ctx->strings, id, serd_token_view(SERD_URI, string)));

  for (size_t i = 0U; i < string.length; ++i) {
    assert(id ==
           serd_nodes_id(ctx->nodes,
                         serd_a_joined_uri(
                           zix_substring(string_data, i),
                           zix_substring(string_data + i, string.length - i))));
  }
}

static void
test_joined_uri(void)
{
  TestContext ctx = test_context_make();

  check_joined_uri(&ctx, "ftp:");
  check_joined_uri(&ctx, "urn:isbn:0140445684");
  check_joined_uri(&ctx, "http://example.org/path?query#fragment");

  test_context_destroy(&ctx);
}

static void
test_string(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("string");

  TestContext ctx = test_context_make();

  const SerdNodeID short_id = check_new_id(ctx.nodes, serd_a_string(string));
  const SerdObjectView short_view =
    serd_object_view(SERD_LITERAL, string, 0U, serd_no_token());

  assert(short_id == serd_nodes_id(ctx.nodes, serd_a_object_view(short_view)));
  assert(object_equals(ctx.strings, short_id, short_view));

  const SerdNodeID long_id = check_new_id(
    ctx.nodes,
    serd_a_object(SERD_LITERAL, string, SERD_IS_LONG, serd_no_token()));
  const SerdObjectView long_view =
    serd_object_view(SERD_LITERAL, string, SERD_IS_LONG, serd_no_token());

  assert(long_id != short_id);
  assert(object_equals(ctx.strings, long_id, long_view));

  test_context_destroy(&ctx);
}

static void
test_literal(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("42");

  SerdNodes* const nodes = serd_nodes_new(NULL);

  const SerdNodeID rel_id = serd_nodes_id(nodes, serd_a_uri(zix_string("rel")));
  assert(rel_id);

  const SerdNodeID xsd_short_id =
    serd_nodes_id(nodes, serd_a_uri(zix_string(NS_XSD "short")));
  assert(xsd_short_id);

  const SerdNodeID short_id = serd_nodes_id(
    nodes, serd_a_literal(string, SERD_HAS_DATATYPE, xsd_short_id));
  assert(short_id);
  assert(short_id ==
         serd_nodes_id(nodes,
                       serd_a_object(SERD_LITERAL,
                                     string,
                                     SERD_HAS_DATATYPE,
                                     serd_token_view(
                                       SERD_URI, zix_string(NS_XSD "short")))));
  assert(short_id ==
         serd_nodes_find(
           nodes, serd_a_literal(string, SERD_HAS_DATATYPE, xsd_short_id)));

  // Invalid datatype ID
  assert(!serd_nodes_id(nodes, serd_a_node_id(N_SYMBOLS + 3U)));
  assert(!serd_nodes_id(
    nodes, serd_a_literal(string, SERD_HAS_DATATYPE, N_SYMBOLS + 3U)));
  assert(!serd_nodes_find(
    nodes, serd_a_literal(string, SERD_HAS_DATATYPE, N_SYMBOLS + 3U)));

  assert(!serd_nodes_id(nodes,
                        serd_a_literal(zix_string("double meta"),
                                       SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE,
                                       xsd_short_id)));
  assert(!serd_nodes_id(nodes,
                        serd_a_literal(zix_string("relative URI datatype"),
                                       SERD_HAS_DATATYPE,
                                       rel_id)));
  assert(!serd_nodes_id(nodes,
                        serd_a_literal(zix_string("URI language"),
                                       SERD_HAS_LANGUAGE,
                                       xsd_short_id)));

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

  TestContext ctx = test_context_make();

  const SerdNodeID node_id =
    check_new_id(ctx.nodes, serd_a_plain_literal(string, en));
  assert(node_id);
  assert(object_equals(ctx.strings,
                       node_id,
                       serd_object_view(SERD_LITERAL,
                                        string,
                                        SERD_HAS_LANGUAGE,
                                        serd_token_view(SERD_LITERAL, en))));

  const SerdNodeID long_node_id =
    check_new_id(ctx.nodes,
                 serd_a_object(SERD_LITERAL,
                               string,
                               SERD_HAS_LANGUAGE | SERD_IS_LONG,
                               serd_token_view(SERD_LITERAL, en)));
  assert(long_node_id != node_id);
  assert(object_equals(ctx.strings,
                       long_node_id,
                       serd_object_view(SERD_LITERAL,
                                        string,
                                        (SERD_HAS_LANGUAGE | SERD_IS_LONG),
                                        serd_token_view(SERD_LITERAL, en))));

  const SerdNodeID other_id =
    check_new_id(ctx.nodes, serd_a_plain_literal(string, de));
  assert(other_id != node_id);
  assert(object_equals(ctx.strings,
                       other_id,
                       serd_object_view(SERD_LITERAL,
                                        string,
                                        SERD_HAS_LANGUAGE,
                                        serd_token_view(SERD_LITERAL, de))));

  // More complex language tags
  assert(serd_nodes_id(ctx.nodes,
                       serd_a_plain_literal(string, zix_string("es-419"))));
  assert(serd_nodes_id(
    ctx.nodes, serd_a_plain_literal(string, zix_string("gsw-u-sd-chzh"))));

  // Invalid language tags
  assert(
    !serd_nodes_id(ctx.nodes, serd_a_plain_literal(string, zix_string("3n"))));
  assert(
    !serd_nodes_id(ctx.nodes, serd_a_plain_literal(string, zix_string("d3"))));
  assert(!serd_nodes_id(ctx.nodes,
                        serd_a_plain_literal(string, zix_string("en-!"))));
  assert(
    !serd_nodes_id(ctx.nodes, serd_a_plain_literal(string, zix_string("-en"))));
  assert(!serd_nodes_id(
    ctx.nodes,
    serd_a_object(SERD_LITERAL,
                  string,
                  SERD_HAS_LANGUAGE,
                  serd_token_view(SERD_LITERAL, zix_string("")))));

  test_context_destroy(&ctx);
}

static void
test_typed_literal(void)
{
  static const ZixStringView string = ZIX_STATIC_STRING("string");

  TestContext ctx = test_context_make();

  const SerdNodeID node_id = check_new_id(
    ctx.nodes, serd_a_typed_literal(string, zix_string(NS_EG "Type")));

  assert(object_equals(
    ctx.strings,
    node_id,
    serd_object_view(SERD_LITERAL,
                     string,
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(NS_EG "Type")))));

  test_context_destroy(&ctx);
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

  TestContext ctx = test_context_make();

  const SerdNodeID id = check_new_id(ctx.nodes, serd_a_value(value));
  assert(id == serd_nodes_id(
                 ctx.nodes,
                 serd_a_object(SERD_LITERAL,
                               node_string,
                               SERD_HAS_DATATYPE,
                               serd_token_view(SERD_URI, datatype_string))));

  assert(object_equals(
    ctx.strings,
    id,
    serd_object_view(SERD_LITERAL,
                     node_string,
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, datatype_string))));

  assert(id != serd_nodes_id(
                 ctx.nodes,
                 serd_a_object(SERD_LITERAL,
                               other_node_string,
                               SERD_HAS_DATATYPE,
                               serd_token_view(SERD_URI, datatype_string))));
  assert(id !=
         serd_nodes_id(
           ctx.nodes,
           serd_a_object(SERD_LITERAL,
                         node_string,
                         SERD_HAS_DATATYPE,
                         serd_token_view(SERD_URI, other_datatype_string))));

  test_context_destroy(&ctx);
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
  TestContext ctx = test_context_make();

  const SerdNodeID false_id =
    check_new_id(ctx.nodes, serd_a_value(serd_bool(false)));

  assert(
    false_id ==
    serd_nodes_id(
      ctx.nodes,
      serd_a_object(SERD_LITERAL,
                    zix_string("false"),
                    SERD_HAS_DATATYPE,
                    serd_token_view(SERD_URI, zix_string(NS_XSD "boolean")))));

  assert(object_equals(
    ctx.strings,
    false_id,
    serd_object_view(SERD_LITERAL,
                     zix_string("false"),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(NS_XSD "boolean")))));

  const SerdNodeID true_id =
    check_new_id(ctx.nodes, serd_a_value(serd_bool(true)));
  assert(true_id != false_id);

  assert(
    true_id ==
    serd_nodes_id(
      ctx.nodes,
      serd_a_object(SERD_LITERAL,
                    zix_string("true"),
                    SERD_HAS_DATATYPE,
                    serd_token_view(SERD_URI, zix_string(NS_XSD "boolean")))));

  assert(object_equals(
    ctx.strings,
    true_id,
    serd_object_view(SERD_LITERAL,
                     zix_string("true"),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(NS_XSD "boolean")))));

  test_context_destroy(&ctx);
}

static void
test_decimal(void)
{
  TestContext ctx = test_context_make();

  const SerdNodeID id = check_new_id(ctx.nodes, serd_a_decimal(-12.3456789));
  assert(object_equals(
    ctx.strings,
    id,
    serd_object_view(SERD_LITERAL,
                     zix_string("-12.3456789"),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(NS_XSD "decimal")))));

  test_context_destroy(&ctx);
}

static void
test_integer(void)
{
  TestContext ctx = test_context_make();

  const SerdNodeID id = check_new_id(ctx.nodes, serd_a_integer(-1234567890));
  assert(object_equals(
    ctx.strings,
    id,
    serd_object_view(SERD_LITERAL,
                     zix_string("-1234567890"),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(NS_XSD "integer")))));

  test_context_destroy(&ctx);
}

static void
test_hex(void)
{
  static const char data[] = {'f', 'o', 'o', 'b', 'a', 'r'};

  TestContext ctx = test_context_make();

  assert(!serd_nodes_id(ctx.nodes, serd_a_hex(0U, &data)));

  const SerdNodeID id =
    check_new_id(ctx.nodes, serd_a_hex(sizeof(data), &data));

  assert(object_equals(
    ctx.strings,
    id,
    serd_object_view(
      SERD_LITERAL,
      zix_string("666F6F626172"),
      SERD_HAS_DATATYPE,
      serd_token_view(SERD_URI, zix_string(NS_XSD "hexBinary")))));

  assert(id == serd_nodes_find(
                 ctx.nodes,
                 serd_a_object(
                   SERD_LITERAL,
                   zix_string("666F6F626172"),
                   SERD_HAS_DATATYPE,
                   serd_token_view(SERD_URI, zix_string(NS_XSD "hexBinary")))));

  test_context_destroy(&ctx);
}

static void
test_base64(void)
{
  static const char data[] = {'f', 'o', 'o', 'b', 'a', 'r'};

  TestContext ctx = test_context_make();

  assert(!serd_nodes_id(ctx.nodes, serd_a_base64(0U, &data)));

  const SerdNodeID id =
    check_new_id(ctx.nodes, serd_a_base64(sizeof(data), &data));

  assert(object_equals(
    ctx.strings,
    id,
    serd_object_view(
      SERD_LITERAL,
      zix_string("Zm9vYmFy"),
      SERD_HAS_DATATYPE,
      serd_token_view(SERD_URI, zix_string(NS_XSD "base64Binary")))));

  assert(id ==
         serd_nodes_find(
           ctx.nodes,
           serd_a_object(
             SERD_LITERAL,
             zix_string("Zm9vYmFy"),
             SERD_HAS_DATATYPE,
             serd_token_view(SERD_URI, zix_string(NS_XSD "base64Binary")))));

  test_context_destroy(&ctx);
}

static void
check_large(const SerdNodeArgs args)
{
  SerdNodes* const nodes = serd_nodes_new(NULL);

  // Initially not present (possibly because the initial buffer is exceeded)
  assert(!serd_nodes_find(nodes, args));

  // Present after mapping (and the initially buffer may have been expanded)
  const SerdNodeID id = serd_nodes_id(nodes, args);
  assert(id);
  assert(id == serd_nodes_find(nodes, args));

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
test_crib(void)
{
  SerdNodes* const   from       = serd_nodes_new(NULL);
  SerdNodes* const   to         = serd_nodes_new(NULL);
  SerdStrings* const to_strings = serd_strings_new(NULL, to);

  assert(!serd_nodes_crib(to, from, 0U));

  // Add an otherwise unused node so that IDs differ
  assert(check_new_id(from, serd_a_value(serd_int(42))));

  // Invalid ID
  assert(!serd_nodes_crib(to, from, 0xDEADBEEFU));

  // Token
  const SerdTokenView tok   = serd_token_view(SERD_BLANK, zix_string("b"));
  const SerdNodeID    f_tok = check_new_id(from, serd_a_token_view(tok));
  const SerdNodeID    t_tok = serd_nodes_crib(to, from, f_tok);
  assert(t_tok);
  assert(token_equals(to_strings, t_tok, tok));
  assert(serd_nodes_crib(to, to, t_tok) == t_tok);

  // Object
  const SerdObjectView obj =
    serd_object_view(SERD_LITERAL,
                     zix_string("1"),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(NS_XSD "int")));

  const SerdNodeID f_obj = check_new_id(from, serd_a_object_view(obj));
  const SerdNodeID t_obj = serd_nodes_crib(to, from, f_obj);
  assert(t_obj);
  assert(object_equals(to_strings, t_obj, obj));
  assert(serd_nodes_crib(to, from, f_obj) == t_obj);
  assert(serd_nodes_crib(to, to, t_obj) == t_obj);

  serd_strings_free(to_strings);
  serd_nodes_free(to);
  serd_nodes_free(from);
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
  test_crib();
  return 0;
}
