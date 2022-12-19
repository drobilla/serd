// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/memory.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/stream_result.h"
#include "serd/uri.h"
#include "serd/value.h"
#include "zix/string_view.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_EG "http://example.org/"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

#if defined(__clang__)

#  define SERD_DISABLE_CONVERSION_WARNINGS                     \
    _Pragma("clang diagnostic push")                           \
    _Pragma("clang diagnostic ignored \"-Wconversion\"")       \
    _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"") \
    _Pragma("clang diagnostic ignored \"-Wc11-extensions\"")

#  define SERD_RESTORE_WARNINGS _Pragma("clang diagnostic pop")

#elif defined(__GNUC__) && __GNUC__ >= 8

#  define SERD_DISABLE_CONVERSION_WARNINGS                   \
    _Pragma("GCC diagnostic push")                           \
    _Pragma("GCC diagnostic ignored \"-Wconversion\"")       \
    _Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"") \
    _Pragma("GCC diagnostic ignored \"-Wdouble-promotion\"")

#  define SERD_RESTORE_WARNINGS _Pragma("GCC diagnostic pop")

#else

#  define SERD_DISABLE_CONVERSION_WARNINGS
#  define SERD_RESTORE_WARNINGS

#endif

static void
test_uri_view(void)
{
  SerdNode* const string = serd_new_string(zix_string("httpstring"));

  const SerdURIView uri = serd_node_uri_view(string);
  assert(!uri.scheme.length);

  serd_node_free(string);
}

static void
test_value(void)
{
  static const double double_one = 1.0;
  static const float  float_two  = 2.0f;

  SerdNode* const bool_node    = serd_new_boolean(false);
  SerdNode* const decimal_node = serd_new_decimal(1.0);
  SerdNode* const double_node  = serd_new_double(1.0);
  SerdNode* const float_node   = serd_new_float(2.0f);
  SerdNode* const integer_node = serd_new_integer(2);

  assert(!strcmp(serd_node_string(bool_node), "false"));
  assert(serd_node_value(bool_node).type == SERD_BOOL);
  assert(serd_node_value(bool_node).data.as_bool == false);

  assert(!strcmp(serd_node_string(decimal_node), "1.0"));
  assert(serd_node_value(decimal_node).type == SERD_DOUBLE);
  {
    const double double_value = serd_node_value(decimal_node).data.as_double;
    assert(!memcmp(&double_value, &double_one, sizeof(double)));
  }

  assert(!strcmp(serd_node_string(double_node), "1.0E0"));
  assert(serd_node_value(double_node).type == SERD_DOUBLE);
  {
    const double double_value = serd_node_value(double_node).data.as_double;
    assert(!memcmp(&double_value, &double_one, sizeof(double)));
  }

  assert(!strcmp(serd_node_string(float_node), "2.0E0"));
  assert(serd_node_value(float_node).type == SERD_FLOAT);
  {
    const float float_value = serd_node_value(float_node).data.as_float;
    assert(!memcmp(&float_value, &float_two, sizeof(float)));
  }

  assert(!strcmp(serd_node_string(integer_node), "2"));
  assert(serd_node_value(integer_node).type == SERD_LONG);
  assert(serd_node_value(integer_node).data.as_long == 2);

  serd_node_free(integer_node);
  serd_node_free(float_node);
  serd_node_free(double_node);
  serd_node_free(decimal_node);
  serd_node_free(bool_node);
}

static void
test_boolean(void)
{
  {
    SerdNode* const true_node = serd_new_boolean(true);
    assert(true_node);
    assert(serd_node_type(true_node) == SERD_LITERAL);
    assert(serd_node_flags(true_node) == SERD_HAS_DATATYPE);
    assert(!strcmp(serd_node_string(true_node), "true"));
    assert(serd_node_value(true_node).data.as_bool);

    const SerdNode* const true_datatype = serd_node_datatype(true_node);
    assert(true_datatype);
    assert(serd_node_type(true_datatype) == SERD_URI);
    assert(!serd_node_flags(true_datatype));
    assert(!strcmp(serd_node_string(true_datatype), NS_XSD "boolean"));
    serd_node_free(true_node);
  }
  {
    SerdNode* const false_node = serd_new_boolean(false);
    assert(false_node);
    assert(serd_node_type(false_node) == SERD_LITERAL);
    assert(serd_node_flags(false_node) == SERD_HAS_DATATYPE);
    assert(!strcmp(serd_node_string(false_node), "false"));
    assert(!serd_node_value(false_node).data.as_bool);

    const SerdNode* const false_datatype = serd_node_datatype(false_node);
    assert(false_datatype);
    assert(serd_node_type(false_datatype) == SERD_URI);
    assert(!serd_node_flags(false_datatype));
    assert(!strcmp(serd_node_string(false_datatype), NS_XSD "boolean"));
    serd_node_free(false_node);
  }
}

static void
check_get_boolean(const char* const   string,
                  const char* const   datatype_uri,
                  const bool          lossy,
                  const SerdValueType value_type,
                  const bool          expected)
{
  SerdNode* const datatype = serd_new_uri(zix_string(datatype_uri));
  SerdNode* const node = serd_new_typed_literal(zix_string(string), datatype);

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_BOOL, lossy);

  assert(value.type == value_type);
  assert(value.data.as_bool == expected);

  serd_node_free(node);
  serd_node_free(datatype);
}

static void
test_get_boolean(void)
{
  check_get_boolean("false", NS_XSD "boolean", false, SERD_BOOL, false);
  check_get_boolean("true", NS_XSD "boolean", false, SERD_BOOL, true);
  check_get_boolean("0", NS_XSD "boolean", false, SERD_BOOL, false);
  check_get_boolean("1", NS_XSD "boolean", false, SERD_BOOL, true);
  check_get_boolean("0", NS_XSD "integer", false, SERD_BOOL, false);
  check_get_boolean("1", NS_XSD "integer", false, SERD_BOOL, true);
  check_get_boolean("0.0", NS_XSD "double", false, SERD_BOOL, false);
  check_get_boolean("1.0", NS_XSD "double", false, SERD_BOOL, true);

  check_get_boolean("2", NS_XSD "integer", false, SERD_NOTHING, false);
  check_get_boolean("1.5", NS_XSD "double", false, SERD_NOTHING, false);

  check_get_boolean("2", NS_XSD "integer", true, SERD_BOOL, true);
  check_get_boolean("1.5", NS_XSD "double", true, SERD_BOOL, true);

  check_get_boolean("unknown", NS_XSD "string", true, SERD_NOTHING, false);
  check_get_boolean("!invalid", NS_XSD "long", true, SERD_NOTHING, false);
}

static void
test_decimal(void)
{
  const double test_values[] = {
    0.0, 9.0, 10.0, .01, 2.05, -16.00001, 5.000000005, 0.0000000001};

  static const char* const test_strings[] = {"0.0",
                                             "9.0",
                                             "10.0",
                                             "0.01",
                                             "2.05",
                                             "-16.00001",
                                             "5.000000005",
                                             "0.0000000001"};

  for (size_t i = 0; i < sizeof(test_values) / sizeof(double); ++i) {
    SerdNode*   node     = serd_new_decimal(test_values[i]);
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));

    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "decimal"));

    const SerdValue value = serd_node_value(node);
    assert(!memcmp(&value.data.as_double, &test_values[i], sizeof(double)));
    serd_node_free(node);
  }
}

static void
test_double(void)
{
  const double test_values[]  = {0.0, -0.0, 1.2, -2.3, 4567890};
  const char*  test_strings[] = {
    "0.0E0", "-0.0E0", "1.2E0", "-2.3E0", "4.56789E6"};

  for (size_t i = 0; i < sizeof(test_values) / sizeof(double); ++i) {
    SerdNode*   node     = serd_new_double(test_values[i]);
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));

    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "double"));

    const SerdValue value = serd_node_value(node);
    assert(!memcmp(&value.data.as_double, &test_values[i], sizeof(double)));
    serd_node_free(node);
  }
}

static void
check_get_double(const char*         string,
                 const char*         datatype_uri,
                 const bool          lossy,
                 const SerdValueType value_type,
                 const double        expected)
{
  SerdNode* const datatype = serd_new_uri(zix_string(datatype_uri));
  SerdNode* const node = serd_new_typed_literal(zix_string(string), datatype);

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_DOUBLE, lossy);

  assert(value.type == value_type);

  SERD_DISABLE_CONVERSION_WARNINGS

  assert(value_type == SERD_NOTHING ||
         ((isnan(value.data.as_double) && isnan(expected)) ||
          !memcmp(&value.data.as_double, &expected, sizeof(double))));

  SERD_RESTORE_WARNINGS

  serd_node_free(node);
  serd_node_free(datatype);
}

static void
test_get_double(void)
{
  SerdNode* const xsd_long = serd_new_uri(zix_string(NS_XSD "long"));

  check_get_double("1.2", NS_XSD "double", false, SERD_DOUBLE, 1.2);
  check_get_double("-.5", NS_XSD "float", false, SERD_DOUBLE, -0.5);
  check_get_double("-67", NS_XSD "long", false, SERD_DOUBLE, -67.0);
  check_get_double("67", NS_XSD "unsignedLong", false, SERD_DOUBLE, 67.0);
  check_get_double("8.9", NS_XSD "decimal", false, SERD_DOUBLE, 8.9);
  check_get_double("false", NS_XSD "boolean", false, SERD_DOUBLE, 0.0);
  check_get_double("true", NS_XSD "boolean", false, SERD_DOUBLE, 1.0);

  SERD_DISABLE_CONVERSION_WARNINGS
  check_get_double("str", NS_XSD "string", true, SERD_NOTHING, NAN);
  check_get_double("!invalid", NS_XSD "long", true, SERD_NOTHING, NAN);
  check_get_double("D3AD", NS_XSD "hexBinary", true, SERD_NOTHING, NAN);
  check_get_double("Zm9v", NS_XSD "base64Binary", true, SERD_NOTHING, NAN);
  SERD_RESTORE_WARNINGS

  serd_node_free(xsd_long);
}

static void
test_float(void)
{
  const float test_values[]  = {0.0f, -0.0f, 1.5f, -2.5f, 4567890.0f};
  const char* test_strings[] = {
    "0.0E0", "-0.0E0", "1.5E0", "-2.5E0", "4.56789E6"};

  for (size_t i = 0; i < sizeof(test_values) / sizeof(float); ++i) {
    SerdNode*   node     = serd_new_float(test_values[i]);
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));

    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "float"));

    const SerdValue value = serd_node_value(node);
    assert(!memcmp(&value.data.as_float, &test_values[i], sizeof(float)));
    serd_node_free(node);
  }
}

static void
check_get_float(const char*         string,
                const char*         datatype_uri,
                const bool          lossy,
                const SerdValueType value_type,
                const float         expected)
{
  SerdNode* const datatype = serd_new_uri(zix_string(datatype_uri));
  SerdNode* const node = serd_new_typed_literal(zix_string(string), datatype);

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_FLOAT, lossy);

  assert(value.type == value_type);

  SERD_DISABLE_CONVERSION_WARNINGS

  assert(value_type == SERD_NOTHING ||
         ((isnan(value.data.as_float) && isnan(expected)) ||
          !memcmp(&value.data.as_float, &expected, sizeof(float))));

  SERD_RESTORE_WARNINGS

  serd_node_free(node);
  serd_node_free(datatype);
}

static void
test_get_float(void)
{
  check_get_float("1.2", NS_XSD "float", false, SERD_FLOAT, 1.2f);
  check_get_float("-.5", NS_XSD "float", false, SERD_FLOAT, -0.5f);
  check_get_float("-67", NS_XSD "long", false, SERD_FLOAT, -67.0f);
  check_get_float("false", NS_XSD "boolean", false, SERD_FLOAT, 0.0f);
  check_get_float("true", NS_XSD "boolean", false, SERD_FLOAT, 1.0f);

  check_get_float("1.5", NS_XSD "decimal", true, SERD_FLOAT, 1.5f);

  SERD_DISABLE_CONVERSION_WARNINGS
  check_get_float("str", NS_XSD "string", true, SERD_NOTHING, NAN);
  check_get_float("!invalid", NS_XSD "long", true, SERD_NOTHING, NAN);
  check_get_float("D3AD", NS_XSD "hexBinary", true, SERD_NOTHING, NAN);
  check_get_float("Zm9v", NS_XSD "base64Binary", true, SERD_NOTHING, NAN);
  SERD_RESTORE_WARNINGS
}

static void
test_integer(void)
{
  const int64_t test_values[]  = {0, -0, -23, 23, -12340, 1000, -1000};
  const char*   test_strings[] = {
    "0", "0", "-23", "23", "-12340", "1000", "-1000"};

  for (size_t i = 0; i < sizeof(test_values) / sizeof(double); ++i) {
    SerdNode*   node     = serd_new_integer(test_values[i]);
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));
    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "integer"));

    assert(serd_node_value(node).data.as_long == test_values[i]);
    serd_node_free(node);
  }
}

static void
check_get_integer(const char*         string,
                  const char*         datatype_uri,
                  const bool          lossy,
                  const SerdValueType value_type,
                  const int64_t       expected)
{
  SerdNode* const datatype = serd_new_uri(zix_string(datatype_uri));
  SerdNode* const node = serd_new_typed_literal(zix_string(string), datatype);

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_LONG, lossy);

  assert(value.type == value_type);
  assert(value_type == SERD_NOTHING || value.data.as_long == expected);

  serd_node_free(node);
  serd_node_free(datatype);
}

static void
test_get_integer(void)
{
  check_get_integer("12", NS_XSD "long", false, SERD_LONG, 12);
  check_get_integer("-34", NS_XSD "long", false, SERD_LONG, -34);
  check_get_integer("56", NS_XSD "integer", false, SERD_LONG, 56);
  check_get_integer("false", NS_XSD "boolean", false, SERD_LONG, 0);
  check_get_integer("true", NS_XSD "boolean", false, SERD_LONG, 1);
  check_get_integer("78.0", NS_XSD "decimal", false, SERD_LONG, 78);

  check_get_integer("0", NS_XSD "nonPositiveInteger", false, SERD_LONG, 0);
  check_get_integer("-1", NS_XSD "negativeInteger", false, SERD_LONG, -1);
  check_get_integer("2", NS_XSD "nonNegativeInteger", false, SERD_LONG, 2);
  check_get_integer("3", NS_XSD "positiveInteger", false, SERD_LONG, 3);

  check_get_integer("78.5", NS_XSD "decimal", false, SERD_NOTHING, 0);
  check_get_integer("78.5", NS_XSD "decimal", true, SERD_LONG, 78);

  check_get_integer("unknown", NS_XSD "string", true, SERD_NOTHING, 0);
  check_get_integer("!invalid", NS_XSD "long", true, SERD_NOTHING, 0);
}

static void
test_base64(void)
{
  assert(!serd_new_base64(&SERD_URI_NULL, 0));

  // Test valid base64 blobs with a range of sizes
  for (size_t size = 1; size < 256; ++size) {
    uint8_t* const data = (uint8_t*)malloc(size);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (uint8_t)((size + i) % 256);
    }

    SerdNode*    blob     = serd_new_base64(data, size);
    const char*  blob_str = serd_node_string(blob);
    const size_t max_size = serd_node_decoded_size(blob);
    uint8_t*     out      = (uint8_t*)calloc(1, max_size);

    const SerdStreamResult r = serd_node_decode(blob, max_size, out);
    assert(r.status == SERD_SUCCESS);
    assert(r.count == size);
    assert(r.count <= max_size);
    assert(serd_node_length(blob) == strlen(blob_str));

    for (size_t i = 0; i < size; ++i) {
      assert(out[i] == data[i]);
    }

    const SerdNode* const datatype = serd_node_datatype(blob);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "base64Binary"));

    serd_node_free(blob);
    serd_free(out);
    free(data);
  }
}

static void
check_decode(const char* string, const char* datatype_uri, const char* expected)
{
  SerdNode* const datatype = serd_new_uri(zix_string(datatype_uri));
  SerdNode* const node = serd_new_typed_literal(zix_string(string), datatype);

  assert(node);

  const size_t max_size = serd_node_decoded_size(node);
  char* const  decoded  = (char*)calloc(1, max_size + 1);

  const SerdStreamResult r = serd_node_decode(node, max_size, decoded);
  assert(!r.status);
  assert(r.count <= max_size);

  assert(!strcmp(decoded, expected));
  assert(strlen(decoded) <= max_size);

  free(decoded);
  serd_node_free(node);
  serd_node_free(datatype);
}

static void
test_decode(void)
{
  check_decode("666F6F626172", NS_XSD "hexBinary", "foobar");
  check_decode("666F6F62", NS_XSD "hexBinary", "foob");

  check_decode("Zm9vYmFy", NS_XSD "base64Binary", "foobar");
  check_decode("Zm9vYg==", NS_XSD "base64Binary", "foob");
  check_decode(" \f\n\r\t\vZm9v \f\n\r\t\v", NS_XSD "base64Binary", "foo");

  char small[2] = {0};

  {
    SerdNode* const datatype = serd_new_uri(zix_string(NS_XSD "base64Binary"));
    SerdNode* const node = serd_new_typed_literal(zix_string("Zm9v"), datatype);

    const SerdStreamResult r = serd_node_decode(node, sizeof(small), small);

    assert(r.status == SERD_NO_SPACE);
    serd_node_free(node);
    serd_node_free(datatype);
  }
  {
    SerdNode* const string = serd_new_string(zix_string("string"));

    assert(serd_node_decoded_size(string) == 0U);

    const SerdStreamResult r = serd_node_decode(string, sizeof(small), small);

    assert(r.status == SERD_BAD_ARG);
    assert(r.count == 0U);
    serd_node_free(string);
  }
  {
    SerdNode* const datatype = serd_new_uri(zix_string(NS_EG "Datatype"));
    SerdNode* const unknown =
      serd_new_typed_literal(zix_string("secret"), datatype);

    assert(serd_node_decoded_size(unknown) == 0U);

    const SerdStreamResult r = serd_node_decode(unknown, sizeof(small), small);

    assert(r.status == SERD_BAD_ARG);
    assert(r.count == 0U);
    serd_node_free(unknown);
    serd_node_free(datatype);
  }
}

static void
test_node_equals(void)
{
  static const uint8_t replacement_char_str[] = {0xEF, 0xBF, 0xBD, 0};

  static const ZixStringView replacement_char = {
    (const char*)replacement_char_str, 3};

  SerdNode* lhs = serd_new_string(replacement_char);
  SerdNode* rhs = serd_new_string(zix_string("123"));

  assert(serd_node_equals(lhs, lhs));
  assert(!serd_node_equals(lhs, rhs));

  SerdNode* const qnode = serd_new_curie(zix_string("foo:bar"));
  assert(!serd_node_equals(lhs, qnode));
  serd_node_free(qnode);

  assert(!serd_node_copy(NULL));

  serd_node_free(lhs);
  serd_node_free(rhs);
}

static void
test_node_from_string(void)
{
  SerdNode* const     hello        = serd_new_string(zix_string("hello\""));
  const ZixStringView hello_string = serd_node_string_view(hello);

  assert(serd_node_type(hello) == SERD_LITERAL);
  assert(serd_node_flags(hello) == SERD_HAS_QUOTE);
  assert(serd_node_length(hello) == 6U);
  assert(hello_string.length == 6U);
  assert(!strcmp(hello_string.data, "hello\""));
  serd_node_free(hello);

  SerdNode* const uri = serd_new_uri(zix_string(NS_EG));
  assert(serd_node_length(uri) == 19);
  assert(!strcmp(serd_node_string(uri), NS_EG));
  assert(serd_node_uri_view(uri).authority.length == 11);
  assert(!strncmp(serd_node_uri_view(uri).authority.data, "example.org", 11));
  serd_node_free(uri);
}

static void
test_node_from_substring(void)
{
  SerdNode* const a_b = serd_new_string(zix_substring("a\"bc", 3));
  assert(serd_node_length(a_b) == 3);
  assert(serd_node_flags(a_b) == SERD_HAS_QUOTE);
  assert(strlen(serd_node_string(a_b)) == 3);
  assert(!strncmp(serd_node_string(a_b), "a\"b", 3));
  serd_node_free(a_b);
}

static void
check_copy_equals(const SerdNode* const node)
{
  SerdNode* const copy = serd_node_copy(node);

  assert(serd_node_equals(node, copy));

  serd_node_free(copy);
}

static void
test_literal(void)
{
  SerdNode* hello2 = serd_new_string(zix_string("hello\""));

  assert(serd_node_length(hello2) == 6 &&
         serd_node_flags(hello2) == SERD_HAS_QUOTE &&
         !strcmp(serd_node_string(hello2), "hello\""));

  check_copy_equals(hello2);

  SerdNode* hello3 = serd_new_plain_literal(zix_string("hello\""), NULL);

  assert(serd_node_equals(hello2, hello3));

  SerdNode* hello4 = serd_new_typed_literal(zix_string("hello\""), NULL);
  assert(serd_node_equals(hello4, hello2));

  serd_node_free(hello4);
  serd_node_free(hello3);
  serd_node_free(hello2);

  // Test literals with language tag

  SerdNode* rdf_langString = serd_new_uri(zix_string(NS_RDF "langString"));

  assert(!serd_new_typed_literal(zix_string("plain"), rdf_langString));
  assert(!serd_new_plain_literal(zix_string("badlang"), rdf_langString));

  SerdNode* const   en           = serd_new_string(zix_string("en"));
  const char* const lang_lit_str = "\"Hello\"@en";
  SerdNode* const   sliced_lang_lit =
    serd_new_plain_literal(zix_substring(lang_lit_str + 1, 5), en);

  assert(!strcmp(serd_node_string(sliced_lang_lit), "Hello"));

  const SerdNode* const lang = serd_node_language(sliced_lang_lit);
  assert(lang);
  assert(!strcmp(serd_node_string(lang), "en"));
  check_copy_equals(sliced_lang_lit);
  serd_node_free(sliced_lang_lit);
  serd_node_free(en);
  serd_node_free(rdf_langString);

  // Test literals with datatype URI

  SerdNode* const   eg_Greeting  = serd_new_uri(zix_string(NS_EG "Greeting"));
  const char* const type_lit_str = "\"Hallo\"^^<http://example.org/Greeting>";
  SerdNode* const   sliced_type_lit =
    serd_new_typed_literal(zix_substring(type_lit_str + 1, 5), eg_Greeting);

  assert(!strcmp(serd_node_string(sliced_type_lit), "Hallo"));

  const SerdNode* const datatype = serd_node_datatype(sliced_type_lit);
  assert(datatype);
  assert(!strcmp(serd_node_string(datatype), NS_EG "Greeting"));
  serd_node_free(sliced_type_lit);
  serd_node_free(eg_Greeting);

  // Test plain string literals

  SerdNode* const plain_lit = serd_new_plain_literal(zix_string("Plain"), NULL);
  assert(!strcmp(serd_node_string(plain_lit), "Plain"));
  serd_node_free(plain_lit);
}

static void
test_blank(void)
{
  SerdNode* blank = serd_new_blank(zix_string("b0"));
  assert(serd_node_length(blank) == 2);
  assert(serd_node_flags(blank) == 0);
  assert(!strcmp(serd_node_string(blank), "b0"));
  serd_node_free(blank);
}

static void
test_compare(void)
{
  SerdNode* const de = serd_new_string(zix_string("de"));
  SerdNode* const en = serd_new_string(zix_string("en"));

  SerdNode* const eg_Aardvark =
    serd_new_uri(zix_string("http://example.org/Aardvark"));

  SerdNode* const eg_Badger =
    serd_new_uri(zix_string("http://example.org/Badger"));

  SerdNode* angst    = serd_new_plain_literal(zix_string("angst"), NULL);
  SerdNode* angst_de = serd_new_plain_literal(zix_string("angst"), de);
  SerdNode* angst_en = serd_new_plain_literal(zix_string("angst"), en);
  SerdNode* hallo    = serd_new_plain_literal(zix_string("Hallo"), de);

  SerdNode* hello    = serd_new_string(zix_string("Hello"));
  SerdNode* universe = serd_new_string(zix_string("Universe"));
  SerdNode* integer  = serd_new_integer(4);
  SerdNode* blank    = serd_new_blank(zix_string("b1"));
  SerdNode* uri      = serd_new_uri(zix_string("http://example.org/"));

  SerdNode* aardvark = serd_new_typed_literal(zix_string("alex"), eg_Aardvark);
  SerdNode* badger   = serd_new_typed_literal(zix_string("bobby"), eg_Badger);

  // Types are ordered according to their SerdNodeType (more or less arbitrary)
  assert(serd_node_compare(hello, uri) < 0);
  assert(serd_node_compare(uri, blank) < 0);

  // If the types are the same, then strings are compared
  assert(serd_node_compare(hello, universe) < 0);

  // If literal strings are the same, languages or datatypes are compared
  assert(serd_node_compare(angst, angst_de) < 0);
  assert(serd_node_compare(angst_de, angst_en) < 0);
  assert(serd_node_compare(aardvark, badger) < 0);

  serd_node_free(uri);
  serd_node_free(blank);
  serd_node_free(integer);
  serd_node_free(badger);
  serd_node_free(aardvark);
  serd_node_free(universe);
  serd_node_free(hello);
  serd_node_free(hallo);
  serd_node_free(angst_en);
  serd_node_free(angst_de);
  serd_node_free(angst);
  serd_node_free(eg_Badger);
  serd_node_free(eg_Aardvark);
  serd_node_free(en);
  serd_node_free(de);
}

int
main(void)
{
  test_uri_view();
  test_value();
  test_boolean();
  test_get_boolean();
  test_decimal();
  test_double();
  test_get_double();
  test_float();
  test_get_float();
  test_integer();
  test_get_integer();
  test_base64();
  test_decode();
  test_node_equals();
  test_node_from_string();
  test_node_from_substring();
  test_literal();
  test_blank();
  test_compare();

  printf("Success\n");
  return 0;
}

#undef NS_XSD
#undef NS_RDF
#undef NS_EG
