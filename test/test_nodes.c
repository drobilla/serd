// Copyright 2018 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

static void
test_intern(void)
{
  SerdNodes* nodes = serd_nodes_new();
  SerdNode*  node  = serd_new_string(serd_string("node"));

  assert(serd_nodes_size(nodes) == 0U);
  assert(!serd_nodes_intern(nodes, NULL));

  const SerdNode* interned1 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned1));
  assert(serd_nodes_size(nodes) == 1U);

  const SerdNode* interned2 = serd_nodes_intern(nodes, node);
  assert(serd_node_equals(node, interned2));
  assert(interned1 == interned2);
  assert(serd_nodes_size(nodes) == 1U);

  serd_node_free(node);
  serd_nodes_free(nodes);
}

static void
test_string(void)
{
  const SerdStringView string = serd_string("string");

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
test_invalid_literal(void)
{
  SerdNodes* const nodes = serd_nodes_new();

  assert(!serd_nodes_literal(nodes,
                             serd_string("double meta"),
                             SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE,
                             serd_string("What am I?")));

  assert(!serd_nodes_literal(nodes,
                             serd_string("empty language"),
                             SERD_HAS_LANGUAGE,
                             serd_empty_string()));

  assert(!serd_nodes_literal(nodes,
                             serd_string("empty datatype"),
                             SERD_HAS_DATATYPE,
                             serd_empty_string()));

  serd_nodes_free(nodes);
}

static void
test_plain_literal(void)
{
  const SerdStringView string   = serd_string("string");
  const SerdStringView language = serd_string("en");

  SerdNodes* const      nodes = serd_nodes_new();
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
    serd_nodes_literal(nodes, string, SERD_HAS_LANGUAGE, serd_string("de"));

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
  const SerdStringView string   = serd_string("string");
  const SerdStringView datatype = serd_string("http://example.org/Type");

  SerdNodes* const      nodes = serd_nodes_new();
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
  SerdNodes* const nodes = serd_nodes_new();

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
  SerdNodes* const      nodes = serd_nodes_new();
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
  SerdNodes* const nodes = serd_nodes_new();

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
  SerdNodes* const nodes = serd_nodes_new();

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
  SerdNodes* const nodes = serd_nodes_new();

  const SerdNode* const a =
    serd_nodes_integer(nodes, -1234567890, serd_empty_string());

  const SerdNode* const b =
    serd_nodes_integer(nodes, -1234567890, serd_empty_string());

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

  SerdNodes* const nodes = serd_nodes_new();

  const SerdNode* const a =
    serd_nodes_base64(nodes, &data, sizeof(data), serd_empty_string());

  const SerdNode* const b =
    serd_nodes_base64(nodes, &data, sizeof(data), serd_empty_string());

  assert(a == b);
  assert(!strcmp(serd_node_string(a), "Zm9vYmFy"));
  assert(serd_node_length(a) == strlen(serd_node_string(a)));
  assert(serd_nodes_size(nodes) == 1);

  const SerdNode* const default_datatype = serd_node_datatype(a);
  assert(default_datatype);
  assert(!strcmp(serd_node_string(default_datatype), NS_XSD "base64Binary"));

  const SerdStringView user_datatype =
    serd_string("http://example.org/UserDatatype");

  const SerdNode* const custom =
    serd_nodes_base64(nodes, &data, sizeof(data), user_datatype);
  assert(custom);
  assert(!strcmp(serd_node_string(custom), "Zm9vYmFy"));

  const SerdNode* const custom_datatype = serd_node_datatype(custom);
  assert(custom_datatype);
  assert(!strcmp(serd_node_string(custom_datatype), user_datatype.buf));

  serd_nodes_free(nodes);
}

static void
test_uri(void)
{
  const SerdStringView string = serd_string("http://example.org/");

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
test_parsed_uri(void)
{
  const SerdStringView string = serd_string("http://example.org/");

  SerdNodes* const      nodes = serd_nodes_new();
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
  const SerdStringView string = serd_string("b42");

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
  SerdNodes*      nodes    = serd_nodes_new();
  const SerdNode* original = serd_nodes_string(nodes, serd_string("node"));
  const SerdNode* another  = serd_nodes_string(nodes, serd_string("node"));

  assert(original == another);

  // Dereference the original and ensure the other reference kept it alive
  serd_nodes_deref(nodes, original);
  assert(serd_node_length(another) == 4);
  assert(!strcmp(serd_node_string(another), "node"));

  // Drop the other/final reference (freeing the node)
  serd_nodes_deref(nodes, another);

  /* Intern some other irrelevant node first to (hopefully) avoid the allocator
     just giving us back the same piece of memory. */

  const SerdNode* other = serd_nodes_string(nodes, serd_string("XXXX"));
  assert(!strcmp(serd_node_string(other), "XXXX"));

  // Intern a new equivalent node to the original and check that it's really new
  const SerdNode* imposter = serd_nodes_string(nodes, serd_string("node"));
  assert(imposter != original);
  assert(serd_node_length(imposter) == 4);
  assert(!strcmp(serd_node_string(imposter), "node"));

  // Check that dereferencing some random unknown node doesn't crash
  SerdNode* unmanaged = serd_new_string(serd_string("unmanaged"));
  serd_nodes_deref(nodes, unmanaged);
  serd_node_free(unmanaged);

  serd_nodes_deref(nodes, NULL);
  serd_nodes_deref(nodes, imposter);
  serd_nodes_deref(nodes, other);
  serd_nodes_free(nodes);
}

static void
test_get(void)
{
  SerdNodes* nodes = serd_nodes_new();
  SerdNode*  node  = serd_new_string(serd_string("node"));

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
