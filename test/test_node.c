/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

#if defined(__clang__)

#  define SERD_DISABLE_CONVERSION_WARNINGS               \
    _Pragma("clang diagnostic push")                     \
    _Pragma("clang diagnostic ignored \"-Wconversion\"") \
    _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")

#  define SERD_RESTORE_WARNINGS _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)

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
test_boolean(void)
{
  SerdNode* const true_node = serd_new_boolean(true);
  assert(!strcmp(serd_node_string(true_node), "true"));
  assert(serd_get_boolean(true_node));

  const SerdNode* const true_datatype = serd_node_datatype(true_node);
  assert(true_datatype);
  assert(!strcmp(serd_node_string(true_datatype), NS_XSD "boolean"));
  serd_node_free(true_node);

  SerdNode* const false_node = serd_new_boolean(false);
  assert(!strcmp(serd_node_string(false_node), "false"));
  assert(!serd_get_boolean(false_node));

  const SerdNode* const false_datatype = serd_node_datatype(false_node);
  assert(false_datatype);
  assert(!strcmp(serd_node_string(false_datatype), NS_XSD "boolean"));
  serd_node_free(false_node);
}

static void
check_get_boolean(const char* string,
                  const char* datatype_uri,
                  const bool  expected)
{
  SerdNode* const node = serd_new_typed_literal(
    SERD_MEASURE_STRING(string), SERD_MEASURE_STRING(datatype_uri));

  assert(node);
  assert(serd_get_boolean(node) == expected);

  serd_node_free(node);
}

static void
test_get_boolean(void)
{
  check_get_boolean("false", NS_XSD "boolean", false);
  check_get_boolean("true", NS_XSD "boolean", true);
  check_get_boolean("0", NS_XSD "boolean", false);
  check_get_boolean("1", NS_XSD "boolean", true);
  check_get_boolean("0", NS_XSD "integer", false);
  check_get_boolean("1", NS_XSD "integer", true);
  check_get_boolean("0.0", NS_XSD "double", false);
  check_get_boolean("1.0", NS_XSD "double", true);
  check_get_boolean("unknown", NS_XSD "string", false);
  check_get_boolean("!invalid", NS_XSD "long", false);
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

    const double value = serd_get_double(node);
    assert(!memcmp(&value, &test_values[i], sizeof(value)));
    serd_node_free(node);
  }
}

static void
check_get_double(const char*  string,
                 const char*  datatype_uri,
                 const double expected)
{
  SerdNode* const node = serd_new_typed_literal(
    SERD_MEASURE_STRING(string), SERD_MEASURE_STRING(datatype_uri));

  assert(node);

  const double value = serd_get_double(node);
  assert(!memcmp(&value, &expected, sizeof(value)));

  serd_node_free(node);
}

static void
test_get_double(void)
{
  check_get_double("1.2", NS_XSD "double", 1.2);
  check_get_double("-.5", NS_XSD "float", -0.5);
  check_get_double("-67", NS_XSD "long", -67.0);
  check_get_double("8.9", NS_XSD "decimal", 8.9);
  check_get_double("false", NS_XSD "boolean", 0.0);
  check_get_double("true", NS_XSD "boolean", 1.0);

  SERD_DISABLE_CONVERSION_WARNINGS

  SerdNode* const nan = serd_new_string(SERD_MEASURE_STRING("unknown"));
  assert(isnan(serd_get_double(nan)));
  serd_node_free(nan);

  SerdNode* const invalid = serd_new_typed_literal(
    SERD_STATIC_STRING("!invalid"), SERD_STATIC_STRING(NS_XSD "long"));

  assert(isnan(serd_get_double(invalid)));

  SERD_RESTORE_WARNINGS

  serd_node_free(invalid);
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

    const float value = serd_get_float(node);
    assert(!memcmp(&value, &test_values[i], sizeof(value)));
    serd_node_free(node);
  }
}

static void
check_get_float(const char* string,
                const char* datatype_uri,
                const float expected)
{
  SerdNode* const node = serd_new_typed_literal(
    SERD_MEASURE_STRING(string), SERD_MEASURE_STRING(datatype_uri));

  assert(node);

  const float value = serd_get_float(node);
  assert(!memcmp(&value, &expected, sizeof(value)));

  serd_node_free(node);
}

static void
test_get_float(void)
{
  check_get_float("1.2", NS_XSD "float", 1.2f);
  check_get_float("-.5", NS_XSD "float", -0.5f);
  check_get_float("-67", NS_XSD "long", -67.0f);
  check_get_float("1.5", NS_XSD "decimal", 1.5f);
  check_get_float("false", NS_XSD "boolean", 0.0f);
  check_get_float("true", NS_XSD "boolean", 1.0f);

  SERD_DISABLE_CONVERSION_WARNINGS

  SerdNode* const nan = serd_new_string(SERD_MEASURE_STRING("unknown"));
  assert(isnan(serd_get_float(nan)));
  serd_node_free(nan);

  SerdNode* const invalid = serd_new_typed_literal(
    SERD_STATIC_STRING("!invalid"), SERD_STATIC_STRING(NS_XSD "long"));

  assert(isnan(serd_get_double(invalid)));

  SERD_RESTORE_WARNINGS

  serd_node_free(invalid);
}

static void
test_integer(void)
{
  const int64_t test_values[]  = {0, -0, -23, 23, -12340, 1000, -1000};
  const char*   test_strings[] = {
    "0", "0", "-23", "23", "-12340", "1000", "-1000"};

  for (size_t i = 0; i < sizeof(test_values) / sizeof(double); ++i) {
    SerdNode*   node     = serd_new_integer(test_values[i], NULL);
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));
    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "integer"));

    assert(serd_get_integer(node) == test_values[i]);
    serd_node_free(node);
  }
}

static void
check_get_integer(const char*   string,
                  const char*   datatype_uri,
                  const int64_t expected)
{
  SerdNode* const node = serd_new_typed_literal(
    SERD_MEASURE_STRING(string), SERD_MEASURE_STRING(datatype_uri));

  assert(node);
  assert(serd_get_integer(node) == expected);

  serd_node_free(node);
}

static void
test_get_integer(void)
{
  check_get_integer("12", NS_XSD "long", 12);
  check_get_integer("-34", NS_XSD "long", -34);
  check_get_integer("56", NS_XSD "integer", 56);
  check_get_integer("false", NS_XSD "boolean", 0);
  check_get_integer("true", NS_XSD "boolean", 1);
  check_get_integer("78.0", NS_XSD "decimal", 78);
  check_get_integer("unknown", NS_XSD "string", 0);
  check_get_integer("!invalid", NS_XSD "long", 0);
}

static void
test_blob_to_node(void)
{
  assert(!serd_new_base64(&SERD_URI_NULL, 0, NULL));

  // Test valid base64 blobs with a range of sizes
  for (size_t size = 1; size < 256; ++size) {
    uint8_t* const data = (uint8_t*)malloc(size);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (uint8_t)((size + i) % 256);
    }

    size_t      out_size = 0;
    SerdNode*   blob     = serd_new_base64(data, size, NULL);
    const char* blob_str = serd_node_string(blob);
    uint8_t*    out =
      (uint8_t*)serd_base64_decode(blob_str, serd_node_length(blob), &out_size);

    assert(serd_node_length(blob) == strlen(blob_str));
    assert(out_size == size);

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

  // Test invalid base64 blob

  SerdNode* const blob = serd_new_typed_literal(
    SERD_STATIC_STRING("!nval!d$"), SERD_STATIC_STRING(NS_XSD "base64Binary"));

  const char* const blob_str = serd_node_string(blob);
  size_t            out_size = 42;
  uint8_t*          out =
    (uint8_t*)serd_base64_decode(blob_str, serd_node_length(blob), &out_size);

  assert(!out);
  assert(out_size == 0);

  serd_node_free(blob);
}

static void
test_node_equals(void)
{
  static const uint8_t replacement_char_str[] = {0xEF, 0xBF, 0xBD, 0};

  static const SerdStringView replacement_char = {
    (const char*)replacement_char_str, 3};

  SerdNode* lhs = serd_new_string(replacement_char);
  SerdNode* rhs = serd_new_string(SERD_STATIC_STRING("123"));

  assert(serd_node_equals(lhs, lhs));
  assert(!serd_node_equals(lhs, rhs));

  SerdNode* const qnode = serd_new_curie(SERD_STATIC_STRING("foo:bar"));
  assert(!serd_node_equals(lhs, qnode));
  serd_node_free(qnode);

  assert(!serd_node_copy(NULL));

  serd_node_free(lhs);
  serd_node_free(rhs);
}

static void
test_node_from_syntax(void)
{
  SerdNode* const hello = serd_new_string(SERD_STATIC_STRING("hello\""));
  assert(serd_node_length(hello) == 6);
  assert(!strncmp(serd_node_string(hello), "hello\"", 6));
  serd_node_free(hello);
}

static void
test_node_from_substring(void)
{
  SerdNode* const a_b = serd_new_string(SERD_STRING_VIEW("a\"bc", 3));
  assert(serd_node_length(a_b) == 3);
  assert(strlen(serd_node_string(a_b)) == 3);
  assert(!strncmp(serd_node_string(a_b), "a\"b", 3));
  serd_node_free(a_b);
}

static void
test_literal(void)
{
  SerdNode* hello2 = serd_new_string(SERD_STATIC_STRING("hello\""));

  assert(serd_node_length(hello2) == 6 &&
         !strcmp(serd_node_string(hello2), "hello\""));

  SerdNode* hello3 =
    serd_new_plain_literal(SERD_STATIC_STRING("hello\""), SERD_EMPTY_STRING());

  assert(serd_node_equals(hello2, hello3));

  SerdNode* hello4 =
    serd_new_typed_literal(SERD_STATIC_STRING("hello\""), SERD_EMPTY_STRING());

  assert(!serd_new_typed_literal(SERD_STATIC_STRING("plain"),
                                 SERD_STATIC_STRING(NS_RDF "langString")));

  assert(serd_node_equals(hello4, hello2));

  serd_node_free(hello4);
  serd_node_free(hello3);
  serd_node_free(hello2);

  const char* lang_lit_str = "\"Hello\"@en";
  SerdNode*   sliced_lang_lit =
    serd_new_plain_literal(SERD_STRING_VIEW(lang_lit_str + 1, 5),
                           SERD_STRING_VIEW(lang_lit_str + 8, 2));

  assert(!strcmp(serd_node_string(sliced_lang_lit), "Hello"));

  const SerdNode* const lang = serd_node_language(sliced_lang_lit);
  assert(lang);
  assert(!strcmp(serd_node_string(lang), "en"));
  serd_node_free(sliced_lang_lit);

  const char* type_lit_str = "\"Hallo\"^^<http://example.org/Greeting>";
  SerdNode*   sliced_type_lit =
    serd_new_typed_literal(SERD_STRING_VIEW(type_lit_str + 1, 5),
                           SERD_STRING_VIEW(type_lit_str + 10, 27));

  assert(!strcmp(serd_node_string(sliced_type_lit), "Hallo"));

  const SerdNode* const datatype = serd_node_datatype(sliced_type_lit);
  assert(datatype);
  assert(!strcmp(serd_node_string(datatype), "http://example.org/Greeting"));
  serd_node_free(sliced_type_lit);

  SerdNode* const plain_lit =
    serd_new_plain_literal(SERD_STATIC_STRING("Plain"), SERD_EMPTY_STRING());
  assert(!strcmp(serd_node_string(plain_lit), "Plain"));
  serd_node_free(plain_lit);
}

static void
test_blank(void)
{
  SerdNode* blank = serd_new_blank(SERD_STATIC_STRING("b0"));
  assert(serd_node_type(blank) == SERD_BLANK);
  assert(serd_node_length(blank) == 2);
  assert(!strcmp(serd_node_string(blank), "b0"));
  assert(!serd_node_datatype(blank));
  assert(!serd_node_language(blank));
  serd_node_free(blank);
}

int
main(void)
{
  test_boolean();
  test_get_boolean();
  test_double();
  test_get_double();
  test_float();
  test_get_float();
  test_integer();
  test_get_integer();
  test_blob_to_node();
  test_node_equals();
  test_node_from_syntax();
  test_node_from_substring();
  test_literal();
  test_blank();

  printf("Success\n");
  return 0;
}
