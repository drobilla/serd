// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/node.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/uri.h"
#include "serd/value.h"

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
test_construct(void)
{
  const SerdNodeArgs args = {(SerdNodeArgsType)999,
                             {{SERD_LITERAL, serd_string("invalid")}}};

  const SerdWriteResult r = serd_node_construct(0, NULL, args);
  assert(r.status == SERD_BAD_ARG);
  assert(r.count == 0U);
}

static void
test_value(void)
{
  static const double double_one = 1.0;
  static const float  float_two  = 2.0f;

  SerdNode* const null_node =
    serd_node_new(NULL, serd_a_primitive(serd_nothing()));
  SerdNode* const bool_node =
    serd_node_new(NULL, serd_a_primitive(serd_bool(false)));
  SerdNode* const double_node =
    serd_node_new(NULL, serd_a_primitive(serd_double(1.0)));
  SerdNode* const float_node =
    serd_node_new(NULL, serd_a_primitive(serd_float(2.0f)));
  SerdNode* const long_node =
    serd_node_new(NULL, serd_a_primitive(serd_long(3)));
  SerdNode* const int_node = serd_node_new(NULL, serd_a_primitive(serd_int(4)));
  SerdNode* const short_node =
    serd_node_new(NULL, serd_a_primitive(serd_short(5)));
  SerdNode* const byte_node =
    serd_node_new(NULL, serd_a_primitive(serd_byte(6)));
  SerdNode* const ulong_node =
    serd_node_new(NULL, serd_a_primitive(serd_ulong(7U)));
  SerdNode* const uint_node =
    serd_node_new(NULL, serd_a_primitive(serd_uint(8U)));
  SerdNode* const ushort_node =
    serd_node_new(NULL, serd_a_primitive(serd_ushort(9U)));
  SerdNode* const ubyte_node =
    serd_node_new(NULL, serd_a_primitive(serd_ubyte(10U)));

  assert(!null_node);

  assert(!strcmp(serd_node_string(bool_node), "false"));
  assert(serd_node_value(bool_node).type == SERD_BOOL);
  assert(serd_node_value(bool_node).data.as_bool == false);

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

  assert(!strcmp(serd_node_string(long_node), "3"));
  assert(serd_node_value(long_node).type == SERD_LONG);
  assert(serd_node_value(long_node).data.as_long == 3);

  assert(!strcmp(serd_node_string(int_node), "4"));
  assert(serd_node_value(int_node).type == SERD_INT);
  assert(serd_node_value(int_node).data.as_int == 4);

  assert(!strcmp(serd_node_string(short_node), "5"));
  assert(serd_node_value(short_node).type == SERD_SHORT);
  assert(serd_node_value(short_node).data.as_short == 5);

  assert(!strcmp(serd_node_string(byte_node), "6"));
  assert(serd_node_value(byte_node).type == SERD_BYTE);
  assert(serd_node_value(byte_node).data.as_byte == 6);

  assert(!strcmp(serd_node_string(ulong_node), "7"));
  assert(serd_node_value(ulong_node).type == SERD_ULONG);
  assert(serd_node_value(ulong_node).data.as_ulong == 7U);

  assert(!strcmp(serd_node_string(uint_node), "8"));
  assert(serd_node_value(uint_node).type == SERD_UINT);
  assert(serd_node_value(uint_node).data.as_uint == 8U);

  assert(!strcmp(serd_node_string(ushort_node), "9"));
  assert(serd_node_value(ushort_node).type == SERD_USHORT);
  assert(serd_node_value(ushort_node).data.as_ushort == 9U);

  assert(!strcmp(serd_node_string(ubyte_node), "10"));
  assert(serd_node_value(ubyte_node).type == SERD_UBYTE);
  assert(serd_node_value(ubyte_node).data.as_ubyte == 10U);

  serd_node_free(NULL, bool_node);
  serd_node_free(NULL, double_node);
  serd_node_free(NULL, float_node);
  serd_node_free(NULL, long_node);
  serd_node_free(NULL, int_node);
  serd_node_free(NULL, short_node);
  serd_node_free(NULL, byte_node);
  serd_node_free(NULL, ulong_node);
  serd_node_free(NULL, uint_node);
  serd_node_free(NULL, ushort_node);
  serd_node_free(NULL, ubyte_node);
}

static void
test_boolean(void)
{
  {
    SerdNode* const true_node =
      serd_node_new(NULL, serd_a_primitive(serd_bool(true)));

    assert(true_node);
    assert(!strcmp(serd_node_string(true_node), "true"));
    assert(serd_node_value(true_node).data.as_bool);

    const SerdNode* const true_datatype = serd_node_datatype(true_node);
    assert(true_datatype);
    assert(!strcmp(serd_node_string(true_datatype), NS_XSD "boolean"));
    serd_node_free(NULL, true_node);
  }
  {
    SerdNode* const false_node =
      serd_node_new(NULL, serd_a_primitive(serd_bool(false)));

    assert(false_node);
    assert(!strcmp(serd_node_string(false_node), "false"));
    assert(!serd_node_value(false_node).data.as_bool);

    const SerdNode* const false_datatype = serd_node_datatype(false_node);
    assert(false_datatype);
    assert(!strcmp(serd_node_string(false_datatype), NS_XSD "boolean"));
    serd_node_free(NULL, false_node);
  }
}

static void
check_get_bool(const char*         string,
               const char*         datatype_uri,
               const bool          lossy,
               const SerdValueType value_type,
               const bool          expected)
{
  SerdNode* const node = serd_node_new(
    NULL, serd_a_typed_literal(serd_string(string), serd_string(datatype_uri)));

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_BOOL, lossy);

  assert(value.type == value_type);
  assert(value.data.as_bool == expected);

  serd_node_free(NULL, node);
}

static void
test_get_bool(void)
{
  check_get_bool("false", NS_XSD "boolean", false, SERD_BOOL, false);
  check_get_bool("true", NS_XSD "boolean", false, SERD_BOOL, true);
  check_get_bool("0", NS_XSD "boolean", false, SERD_BOOL, false);
  check_get_bool("1", NS_XSD "boolean", false, SERD_BOOL, true);
  check_get_bool("0", NS_XSD "integer", false, SERD_BOOL, false);
  check_get_bool("1", NS_XSD "integer", false, SERD_BOOL, true);
  check_get_bool("0.0", NS_XSD "double", false, SERD_BOOL, false);
  check_get_bool("1.0", NS_XSD "double", false, SERD_BOOL, true);

  check_get_bool("2", NS_XSD "integer", false, SERD_NOTHING, false);
  check_get_bool("1.5", NS_XSD "double", false, SERD_NOTHING, false);

  check_get_bool("2", NS_XSD "integer", true, SERD_BOOL, true);
  check_get_bool("1.5", NS_XSD "double", true, SERD_BOOL, true);

  check_get_bool("unknown", NS_XSD "string", true, SERD_NOTHING, false);
  check_get_bool("!invalid", NS_XSD "long", true, SERD_NOTHING, false);
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
    SerdNode*   node     = serd_node_new(NULL, serd_a_decimal(test_values[i]));
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));

    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "decimal"));

    const SerdValue value = serd_node_value(node);
    assert(!memcmp(&value.data.as_double, &test_values[i], sizeof(double)));
    serd_node_free(NULL, node);
  }
}

static void
test_double(void)
{
  const double test_values[]  = {0.0, -0.0, 1.2, -2.3, 4567890};
  const char*  test_strings[] = {
     "0.0E0", "-0.0E0", "1.2E0", "-2.3E0", "4.56789E6"};

  for (size_t i = 0; i < sizeof(test_values) / sizeof(double); ++i) {
    SerdNode* const node =
      serd_node_new(NULL, serd_a_primitive(serd_double(test_values[i])));

    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));

    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "double"));

    const SerdValue value = serd_node_value(node);
    assert(!memcmp(&value.data.as_double, &test_values[i], sizeof(double)));
    serd_node_free(NULL, node);
  }
}

static void
check_get_double(const char*         string,
                 const char*         datatype_uri,
                 const bool          lossy,
                 const SerdValueType value_type,
                 const double        expected)
{
  SerdNode* const node = serd_node_new(
    NULL, serd_a_typed_literal(serd_string(string), serd_string(datatype_uri)));

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_DOUBLE, lossy);

  assert(value.type == value_type);

  SERD_DISABLE_CONVERSION_WARNINGS

  assert(value_type == SERD_NOTHING ||
         ((isnan(value.data.as_double) && isnan(expected)) ||
          !memcmp(&value.data.as_double, &expected, sizeof(double))));

  SERD_RESTORE_WARNINGS

  serd_node_free(NULL, node);
}

static void
test_get_double(void)
{
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
}

static void
test_float(void)
{
  const float test_values[]  = {0.0f, -0.0f, 1.5f, -2.5f, 4567890.0f};
  const char* test_strings[] = {
    "0.0E0", "-0.0E0", "1.5E0", "-2.5E0", "4.56789E6"};

  for (size_t i = 0; i < sizeof(test_values) / sizeof(float); ++i) {
    SerdNode* const node =
      serd_node_new(NULL, serd_a_primitive(serd_float(test_values[i])));

    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));

    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "float"));

    const SerdValue value = serd_node_value(node);
    assert(!memcmp(&value.data.as_float, &test_values[i], sizeof(float)));
    serd_node_free(NULL, node);
  }
}

static void
check_get_float(const char*         string,
                const char*         datatype_uri,
                const bool          lossy,
                const SerdValueType value_type,
                const float         expected)
{
  SerdNode* const node = serd_node_new(
    NULL, serd_a_typed_literal(serd_string(string), serd_string(datatype_uri)));

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_FLOAT, lossy);

  assert(value.type == value_type);

  SERD_DISABLE_CONVERSION_WARNINGS

  assert(value_type == SERD_NOTHING ||
         ((isnan(value.data.as_float) && isnan(expected)) ||
          !memcmp(&value.data.as_float, &expected, sizeof(float))));

  SERD_RESTORE_WARNINGS

  serd_node_free(NULL, node);
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
    SerdNode*   node     = serd_node_new(NULL, serd_a_integer(test_values[i]));
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, test_strings[i]));
    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "integer"));

    assert(serd_node_value(node).data.as_long == test_values[i]);
    serd_node_free(NULL, node);
  }
}

static void
check_get_integer(const char*         string,
                  const char*         datatype_uri,
                  const bool          lossy,
                  const SerdValueType value_type,
                  const int64_t       expected)
{
  SerdNode* const node = serd_node_new(
    NULL, serd_a_typed_literal(serd_string(string), serd_string(datatype_uri)));

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_LONG, lossy);

  assert(value.type == value_type);
  assert(value_type == SERD_NOTHING || value.data.as_long == expected);

  serd_node_free(NULL, node);
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
test_hex(void)
{
  assert(!serd_node_new(NULL, serd_a_hex(0, &SERD_URI_NULL)));

  // Test valid hex blobs with a range of sizes
  for (size_t size = 1; size < 256; ++size) {
    uint8_t* const data = (uint8_t*)malloc(size);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (uint8_t)((size + i) % 256);
    }

    SerdNode*    blob     = serd_node_new(NULL, serd_a_hex(size, data));
    const char*  blob_str = serd_node_string(blob);
    const size_t max_size = serd_node_decoded_size(blob);
    uint8_t*     out      = (uint8_t*)calloc(1, max_size);

    const SerdWriteResult r = serd_node_decode(blob, max_size, out);
    assert(r.status == SERD_SUCCESS);
    assert(r.count == size);
    assert(r.count <= max_size);
    assert(serd_node_length(blob) == strlen(blob_str));

    for (size_t i = 0; i < size; ++i) {
      assert(out[i] == data[i]);
    }

    const SerdNode* const datatype = serd_node_datatype(blob);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "hexBinary"));

    serd_node_free(NULL, blob);
    free(out);
    free(data);
  }
}

static void
test_base64(void)
{
  assert(!serd_node_new(NULL, serd_a_base64(0, &SERD_URI_NULL)));

  // Test valid base64 blobs with a range of sizes
  for (size_t size = 1; size < 256; ++size) {
    uint8_t* const data = (uint8_t*)malloc(size);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (uint8_t)((size + i) % 256);
    }

    SerdNode*    blob     = serd_node_new(NULL, serd_a_base64(size, data));
    const char*  blob_str = serd_node_string(blob);
    const size_t max_size = serd_node_decoded_size(blob);
    uint8_t*     out      = (uint8_t*)calloc(1, max_size);

    const SerdWriteResult r = serd_node_decode(blob, max_size, out);
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

    serd_node_free(NULL, blob);
    free(out);
    free(data);
  }
}

static void
check_decode(const char* string, const char* datatype_uri, const char* expected)
{
  SerdNode* const node = serd_node_new(
    NULL, serd_a_typed_literal(serd_string(string), serd_string(datatype_uri)));

  assert(node);

  const size_t max_size = serd_node_decoded_size(node);
  char* const  decoded  = (char*)calloc(1, max_size + 1);

  const SerdWriteResult r = serd_node_decode(node, max_size, decoded);
  assert(!r.status);
  assert(r.count <= max_size);

  assert(!strcmp(decoded, expected));
  assert(strlen(decoded) <= max_size);

  free(decoded);
  serd_node_free(NULL, node);
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
    SerdNode* const node =
      serd_node_new(NULL,
                    serd_a_typed_literal(serd_string("Zm9v"),
                                         serd_string(NS_XSD "base64Binary")));

    const SerdWriteResult r = serd_node_decode(node, sizeof(small), small);

    assert(r.status == SERD_OVERFLOW);
    serd_node_free(NULL, node);
  }
  {
    SerdNode* const string = serd_node_new(NULL, serd_a_string("string"));

    assert(serd_node_decoded_size(string) == 0U);

    const SerdWriteResult r = serd_node_decode(string, sizeof(small), small);

    assert(r.status == SERD_BAD_ARG);
    assert(r.count == 0U);
    serd_node_free(NULL, string);
  }
  {
    SerdNode* const unknown = serd_node_new(
      NULL,
      serd_a_typed_literal(serd_string("secret"),
                           serd_string("http://example.org/Datatype")));

    assert(serd_node_decoded_size(unknown) == 0U);

    const SerdWriteResult r = serd_node_decode(unknown, sizeof(small), small);

    assert(r.status == SERD_BAD_ARG);
    assert(r.count == 0U);
    serd_node_free(NULL, unknown);
  }
}

static void
test_node_equals(void)
{
  static const uint8_t replacement_char_str[] = {0xEF, 0xBF, 0xBD, 0};

  static const SerdStringView replacement_char = {
    (const char*)replacement_char_str, 3};

  SerdNode* lhs = serd_node_new(NULL, serd_a_string_view(replacement_char));
  SerdNode* rhs = serd_node_new(NULL, serd_a_string("123"));

  assert(serd_node_equals(lhs, lhs));
  assert(!serd_node_equals(lhs, rhs));

  assert(!serd_node_copy(NULL, NULL));

  serd_node_free(NULL, lhs);
  serd_node_free(NULL, rhs);
}

static void
test_node_from_syntax(void)
{
  SerdNode* const hello = serd_node_new(NULL, serd_a_string("hello\""));

  assert(serd_node_length(hello) == 6);
  assert(!serd_node_flags(hello));
  assert(!strncmp(serd_node_string(hello), "hello\"", 6));
  serd_node_free(NULL, hello);
}

static void
test_node_from_substring(void)
{
  SerdNode* const a_b =
    serd_node_new(NULL, serd_a_string_view(serd_substring("a\"bc", 3)));

  assert(serd_node_length(a_b) == 3);
  assert(!serd_node_flags(a_b));
  assert(strlen(serd_node_string(a_b)) == 3);
  assert(!strncmp(serd_node_string(a_b), "a\"b", 3));
  serd_node_free(NULL, a_b);
}

static void
check_copy_equals(const SerdNode* const node)
{
  SerdNode* const copy = serd_node_copy(NULL, node);

  assert(serd_node_equals(node, copy));

  serd_node_free(NULL, copy);
}

static void
test_uri(void)
{
  const SerdStringView base = serd_string("http://example.org/base/");
  const SerdStringView rel  = serd_string("a/b");
  const SerdStringView abs  = serd_string("http://example.org/base/a/b");

  const SerdURIView base_uri = serd_parse_uri(base.buf);
  const SerdURIView rel_uri  = serd_parse_uri(rel.buf);
  const SerdURIView abs_uri  = serd_resolve_uri(rel_uri, base_uri);

  SerdNode* const from_string = serd_node_new(NULL, serd_a_uri(abs));

  SerdNode* const from_uri = serd_node_new(NULL, serd_a_parsed_uri(abs_uri));

  assert(from_string);
  assert(from_uri);
  assert(!strcmp(serd_node_string(from_string), serd_node_string(from_uri)));

  serd_node_free(NULL, from_uri);
  serd_node_free(NULL, from_string);
}

static void
test_literal(void)
{
  const SerdStringView hello_str = serd_string("hello");
  const SerdStringView empty_str = serd_empty_string();

  assert(!serd_node_new(NULL,
                        serd_a_literal(hello_str,
                                       SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE,
                                       serd_string("whatever"))));

  assert(!serd_node_new(NULL, serd_a_typed_literal(hello_str, empty_str)));
  assert(!serd_node_new(NULL, serd_a_plain_literal(hello_str, empty_str)));

  assert(
    !serd_node_new(NULL, serd_a_typed_literal(hello_str, serd_string("Type"))));

  assert(
    !serd_node_new(NULL, serd_a_typed_literal(hello_str, serd_string("de"))));

  assert(
    !serd_node_new(NULL, serd_a_plain_literal(hello_str, serd_string("3n"))));
  assert(
    !serd_node_new(NULL, serd_a_plain_literal(hello_str, serd_string("d3"))));
  assert(
    !serd_node_new(NULL, serd_a_plain_literal(hello_str, serd_string("d3"))));
  assert(
    !serd_node_new(NULL, serd_a_plain_literal(hello_str, serd_string("en-!"))));

  SerdNode* hello2 = serd_node_new(NULL, serd_a_string("hello\""));

  assert(serd_node_length(hello2) == 6 &&
         !strcmp(serd_node_string(hello2), "hello\""));

  check_copy_equals(hello2);

  assert(
    !serd_node_new(NULL,
                   serd_a_typed_literal(serd_string("plain"),
                                        serd_string(NS_RDF "langString"))));

  serd_node_free(NULL, hello2);

  const char* lang_lit_str = "\"Hello\"@en-ca";
  SerdNode*   sliced_lang_lit =
    serd_node_new(NULL,
                  serd_a_plain_literal(serd_substring(lang_lit_str + 1, 5),
                                       serd_substring(lang_lit_str + 8, 5)));

  assert(!strcmp(serd_node_string(sliced_lang_lit), "Hello"));

  const SerdNode* const lang = serd_node_language(sliced_lang_lit);
  assert(lang);
  assert(!strcmp(serd_node_string(lang), "en-ca"));
  check_copy_equals(sliced_lang_lit);
  serd_node_free(NULL, sliced_lang_lit);

  const char* type_lit_str = "\"Hallo\"^^<http://example.org/Greeting>";
  SerdNode*   sliced_type_lit =
    serd_node_new(NULL,
                  serd_a_typed_literal(serd_substring(type_lit_str + 1, 5),
                                       serd_substring(type_lit_str + 10, 27)));

  assert(!strcmp(serd_node_string(sliced_type_lit), "Hallo"));

  const SerdNode* const datatype = serd_node_datatype(sliced_type_lit);
  assert(datatype);
  assert(!strcmp(serd_node_string(datatype), "http://example.org/Greeting"));
  serd_node_free(NULL, sliced_type_lit);
}

static void
test_blank(void)
{
  SerdNode* blank = serd_node_new(NULL, serd_a_blank(serd_string("b0")));

  assert(serd_node_length(blank) == 2);
  assert(serd_node_flags(blank) == 0);
  assert(!strcmp(serd_node_string(blank), "b0"));
  serd_node_free(NULL, blank);
}

static void
test_compare(void)
{
  SerdNode* xsd_short = serd_node_new(
    NULL, serd_a_uri_string("http://www.w3.org/2001/XMLSchema#short"));

  SerdNode* angst = serd_node_new(NULL, serd_a_string("angst"));

  SerdNode* angst_de = serd_node_new(
    NULL, serd_a_plain_literal(serd_string("angst"), serd_string("de")));

  SerdNode* angst_en = serd_node_new(
    NULL, serd_a_plain_literal(serd_string("angst"), serd_string("en")));

  SerdNode* hallo = serd_node_new(
    NULL, serd_a_plain_literal(serd_string("Hallo"), serd_string("de")));

  SerdNode* hello    = serd_node_new(NULL, serd_a_string("Hello"));
  SerdNode* universe = serd_node_new(NULL, serd_a_string("Universe"));
  SerdNode* integer  = serd_node_new(NULL, serd_a_integer(4));

  SerdNode* short_integer =
    serd_node_new(NULL, serd_a_primitive(serd_short(4)));

  SerdNode* blank = serd_node_new(NULL, serd_a_blank(serd_string("b1")));

  SerdNode* uri = serd_node_new(NULL, serd_a_uri_string("http://example.org/"));

  SerdNode* aardvark = serd_node_new(
    NULL,
    serd_a_typed_literal(serd_string("alex"),
                         serd_string("http://example.org/Aardvark")));

  SerdNode* badger = serd_node_new(
    NULL,
    serd_a_typed_literal(serd_string("bobby"),
                         serd_string("http://example.org/Badger")));

  // Types are ordered according to their SerdNodeType (more or less arbitrary)
  assert(serd_node_compare(hello, uri) < 0);
  assert(serd_node_compare(uri, blank) < 0);

  // If the types are the same, then strings are compared
  assert(serd_node_compare(hello, universe) < 0);

  // If literal strings are the same, languages or datatypes are compared
  assert(serd_node_compare(angst, angst_de) < 0);
  assert(serd_node_compare(angst_de, angst_en) < 0);
  assert(serd_node_compare(aardvark, badger) < 0);
  assert(serd_node_compare(integer, short_integer) < 0);

  serd_node_free(NULL, badger);
  serd_node_free(NULL, aardvark);
  serd_node_free(NULL, uri);
  serd_node_free(NULL, blank);
  serd_node_free(NULL, short_integer);
  serd_node_free(NULL, integer);
  serd_node_free(NULL, universe);
  serd_node_free(NULL, hello);
  serd_node_free(NULL, hallo);
  serd_node_free(NULL, angst_en);
  serd_node_free(NULL, angst_de);
  serd_node_free(NULL, angst);
  serd_node_free(NULL, xsd_short);
}

int
main(void)
{
  test_construct();
  test_value();
  test_boolean();
  test_get_bool();
  test_decimal();
  test_double();
  test_get_double();
  test_float();
  test_get_float();
  test_integer();
  test_get_integer();
  test_hex();
  test_base64();
  test_decode();
  test_node_equals();
  test_node_from_syntax();
  test_node_from_substring();
  test_uri();
  test_literal();
  test_blank();
  test_compare();

  printf("Success\n");
  return 0;
}
