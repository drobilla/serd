// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

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
test_new(void)
{
  const SerdNodeArgs bad_args = {(SerdNodeArgsType)-1,
                                 {{SERD_LITERAL, zix_string("invalid")}}};

  assert(!serd_node_new(NULL, bad_args));
}

static void
test_uri_view(void)
{
  SerdNode* const string = serd_node_new(NULL, serd_a_string("httpstring"));

  const SerdURIView uri = serd_node_uri_view(string);
  assert(!uri.scheme.length);

  serd_node_free(NULL, string);
}

static void
test_prefixed_name(void)
{
  SerdNode* const curie = serd_node_new(
    NULL, serd_a_prefixed_name(zix_string("prefix"), zix_string("name")));

  assert(curie);
  assert(serd_node_type(curie) == SERD_CURIE);
  assert(!serd_node_flags(curie));
  assert(serd_node_length(curie) == 11U);
  assert(!strcmp(serd_node_string(curie), "prefix:name"));

  serd_node_free(NULL, curie);
}

static void
test_joined_uri(void)
{
  SerdNode* const uri = serd_node_new(
    NULL,
    serd_a_joined_uri(zix_string("http://example.org/d/"), zix_string("name")));

  assert(uri);
  assert(serd_node_type(uri) == SERD_URI);
  assert(!serd_node_flags(uri));
  assert(serd_node_length(uri) == 25U);
  assert(!strcmp(serd_node_string(uri), "http://example.org/d/name"));

  serd_node_free(NULL, uri);
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
    assert(serd_node_type(true_node) == SERD_LITERAL);
    assert(serd_node_flags(true_node) == SERD_HAS_DATATYPE);
    assert(!strcmp(serd_node_string(true_node), "true"));
    assert(serd_node_value(true_node).data.as_bool);

    const SerdNode* const true_datatype = serd_node_datatype(true_node);
    assert(true_datatype);
    assert(serd_node_type(true_datatype) == SERD_URI);
    assert(!serd_node_flags(true_datatype));
    assert(!strcmp(serd_node_string(true_datatype), NS_XSD "boolean"));
    serd_node_free(NULL, true_node);
  }
  {
    SerdNode* const false_node =
      serd_node_new(NULL, serd_a_primitive(serd_bool(false)));

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
    serd_node_free(NULL, false_node);
  }
}

static void
check_get_bool(const char* const   string,
               const char* const   datatype_uri,
               const bool          lossy,
               const SerdValueType value_type,
               const bool          expected)
{
  SerdNode* const datatype =
    serd_node_new(NULL, serd_a_uri_string(datatype_uri));
  SerdNode* const node =
    serd_node_new(NULL, serd_a_typed_literal(zix_string(string), datatype));

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_BOOL, lossy);

  assert(value.type == value_type);
  assert(value.data.as_bool == expected);

  serd_node_free(NULL, node);
  serd_node_free(NULL, datatype);
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
  SerdNode* const datatype =
    serd_node_new(NULL, serd_a_uri_string(datatype_uri));
  SerdNode* const node =
    serd_node_new(NULL, serd_a_typed_literal(zix_string(string), datatype));

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_DOUBLE, lossy);

  assert(value.type == value_type);

  SERD_DISABLE_CONVERSION_WARNINGS

  assert(value_type == SERD_NOTHING ||
         ((isnan(value.data.as_double) && isnan(expected)) ||
          !memcmp(&value.data.as_double, &expected, sizeof(double))));

  SERD_RESTORE_WARNINGS

  serd_node_free(NULL, node);
  serd_node_free(NULL, datatype);
}

static void
test_get_double(void)
{
  SerdNode* const xsd_long =
    serd_node_new(NULL, serd_a_uri_string(NS_XSD "long"));

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

  serd_node_free(NULL, xsd_long);
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
  SerdNode* const datatype =
    serd_node_new(NULL, serd_a_uri_string(datatype_uri));
  SerdNode* const node =
    serd_node_new(NULL, serd_a_typed_literal(zix_string(string), datatype));

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_FLOAT, lossy);

  assert(value.type == value_type);

  SERD_DISABLE_CONVERSION_WARNINGS

  assert(value_type == SERD_NOTHING ||
         ((isnan(value.data.as_float) && isnan(expected)) ||
          !memcmp(&value.data.as_float, &expected, sizeof(float))));

  SERD_RESTORE_WARNINGS

  serd_node_free(NULL, node);
  serd_node_free(NULL, datatype);
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
  SerdNode* const datatype =
    serd_node_new(NULL, serd_a_uri_string(datatype_uri));
  SerdNode* const node =
    serd_node_new(NULL, serd_a_typed_literal(zix_string(string), datatype));

  assert(node);

  const SerdValue value = serd_node_value_as(node, SERD_LONG, lossy);

  assert(value.type == value_type);
  assert(value_type == SERD_NOTHING || value.data.as_long == expected);

  serd_node_free(NULL, node);
  serd_node_free(NULL, datatype);
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

    serd_node_free(NULL, blob);
    free(out);
    free(data);
  }
}

static void
check_decode(const char* string, const char* datatype_uri, const char* expected)
{
  SerdNode* const datatype =
    serd_node_new(NULL, serd_a_uri_string(datatype_uri));

  SerdNode* const node =
    serd_node_new(NULL, serd_a_typed_literal(zix_string(string), datatype));

  assert(node);

  const size_t max_size = serd_node_decoded_size(node);
  char* const  decoded  = (char*)calloc(1, max_size + 1);

  const SerdStreamResult r = serd_node_decode(node, max_size, decoded);
  assert(!r.status);
  assert(r.count <= max_size);

  assert(!strcmp(decoded, expected));
  assert(strlen(decoded) <= max_size);

  free(decoded);
  serd_node_free(NULL, node);
  serd_node_free(NULL, datatype);
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
    SerdNode* const datatype =
      serd_node_new(NULL, serd_a_uri_string(NS_XSD "base64Binary"));
    SerdNode* const node =
      serd_node_new(NULL, serd_a_typed_literal(zix_string("Zm9v"), datatype));

    const SerdStreamResult r = serd_node_decode(node, sizeof(small), small);

    assert(r.status == SERD_NO_SPACE);
    serd_node_free(NULL, node);
    serd_node_free(NULL, datatype);
  }
  {
    SerdNode* const string = serd_node_new(NULL, serd_a_string("string"));

    assert(serd_node_decoded_size(string) == 0U);

    const SerdStreamResult r = serd_node_decode(string, sizeof(small), small);

    assert(r.status == SERD_BAD_ARG);
    assert(r.count == 0U);
    serd_node_free(NULL, string);
  }
  {
    SerdNode* const datatype =
      serd_node_new(NULL, serd_a_uri_string(NS_EG "Datatype"));
    SerdNode* const unknown =
      serd_node_new(NULL, serd_a_typed_literal(zix_string("secret"), datatype));

    assert(serd_node_decoded_size(unknown) == 0U);

    const SerdStreamResult r = serd_node_decode(unknown, sizeof(small), small);

    assert(r.status == SERD_BAD_ARG);
    assert(r.count == 0U);
    serd_node_free(NULL, unknown);
    serd_node_free(NULL, datatype);
  }
}

static void
test_node_equals(void)
{
  static const uint8_t replacement_char_str[] = {0xEF, 0xBF, 0xBD, 0};

  static const ZixStringView replacement_char = {
    (const char*)replacement_char_str, 3};

  SerdNode* lhs = serd_node_new(NULL, serd_a_string_view(replacement_char));
  SerdNode* rhs = serd_node_new(NULL, serd_a_string("123"));

  assert(serd_node_equals(lhs, lhs));
  assert(!serd_node_equals(lhs, rhs));

  SerdNode* const qnode = serd_node_new(NULL, serd_a_curie_string("foo:bar"));
  assert(!serd_node_equals(lhs, qnode));
  serd_node_free(NULL, qnode);

  assert(!serd_node_copy(NULL, NULL));

  serd_node_free(NULL, lhs);
  serd_node_free(NULL, rhs);
}

static void
test_node_from_syntax(void)
{
  SerdNode* const     hello = serd_node_new(NULL, serd_a_string("hello\""));
  const ZixStringView hello_string = serd_node_string_view(hello);

  assert(serd_node_type(hello) == SERD_LITERAL);
  assert(!serd_node_flags(hello));
  assert(serd_node_length(hello) == 6U);
  assert(hello_string.length == 6U);
  assert(!strcmp(hello_string.data, "hello\""));
  serd_node_free(NULL, hello);

  SerdNode* const uri = serd_node_new(NULL, serd_a_uri_string(NS_EG));
  assert(serd_node_length(uri) == 19);
  assert(!strcmp(serd_node_string(uri), NS_EG));
  assert(serd_node_uri_view(uri).authority.length == 11);
  assert(!strncmp(serd_node_uri_view(uri).authority.data, "example.org", 11));
  serd_node_free(NULL, uri);
}

static void
test_node_from_substring(void)
{
  SerdNode* const a_b =
    serd_node_new(NULL, serd_a_string_view(zix_substring("a\"bc", 3)));

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
  static const ZixStringView base = ZIX_STATIC_STRING(NS_EG "base/");
  static const ZixStringView rel  = ZIX_STATIC_STRING("a/b");
  static const ZixStringView abs  = ZIX_STATIC_STRING(NS_EG "base/a/b");

  const SerdURIView base_uri = serd_parse_uri(base.data);
  const SerdURIView rel_uri  = serd_parse_uri(rel.data);
  const SerdURIView abs_uri  = serd_resolve_uri(rel_uri, base_uri);

  SerdNode* const from_string = serd_node_new(NULL, serd_a_uri(abs));
  SerdNode* const from_uri    = serd_node_new(NULL, serd_a_parsed_uri(abs_uri));

  assert(from_string);
  assert(from_uri);
  assert(!strcmp(serd_node_string(from_string), serd_node_string(from_uri)));

  serd_node_free(NULL, from_uri);
  serd_node_free(NULL, from_string);
}

static void
test_lang_tagged_literal(void)
{
  static const ZixStringView hello_str = ZIX_STATIC_STRING("hello");

  SerdNode* const empty_node = serd_node_new(NULL, serd_a_string(""));
  SerdNode* const rel        = serd_node_new(NULL, serd_a_uri_string("rel"));
  SerdNode* const de         = serd_node_new(NULL, serd_a_string("de"));
  SerdNode* const long_tag   = serd_node_new(NULL, serd_a_string("en-l0-ng"));
  SerdNode* const bad_start  = serd_node_new(NULL, serd_a_string("3n"));
  SerdNode* const bad_char   = serd_node_new(NULL, serd_a_string("d3"));
  SerdNode* const bad_suffix = serd_node_new(NULL, serd_a_string("en-!"));

  assert(!serd_node_new(NULL,
                        serd_a_literal(hello_str,
                                       SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE,
                                       empty_node)));

  assert(!serd_node_new(
    NULL, serd_a_literal(hello_str, SERD_HAS_DATATYPE, empty_node)));
  assert(!serd_node_new(
    NULL, serd_a_literal(hello_str, SERD_HAS_LANGUAGE, empty_node)));

  assert(
    !serd_node_new(NULL, serd_a_literal(hello_str, SERD_HAS_DATATYPE, rel)));
  assert(
    !serd_node_new(NULL, serd_a_literal(hello_str, SERD_HAS_DATATYPE, de)));

  assert(
    !serd_node_new(NULL, serd_a_literal(hello_str, SERD_HAS_LANGUAGE, rel)));
  assert(!serd_node_new(
    NULL, serd_a_literal(hello_str, SERD_HAS_LANGUAGE, bad_start)));
  assert(!serd_node_new(
    NULL, serd_a_literal(hello_str, SERD_HAS_LANGUAGE, bad_char)));
  assert(!serd_node_new(
    NULL, serd_a_literal(hello_str, SERD_HAS_LANGUAGE, bad_suffix)));

  SerdNode* const tagged =
    serd_node_new(NULL, serd_a_literal(hello_str, SERD_HAS_LANGUAGE, long_tag));
  assert(tagged);
  serd_node_free(NULL, tagged);

  serd_node_free(NULL, bad_suffix);
  serd_node_free(NULL, bad_char);
  serd_node_free(NULL, bad_start);
  serd_node_free(NULL, long_tag);
  serd_node_free(NULL, de);
  serd_node_free(NULL, rel);
  serd_node_free(NULL, empty_node);
}

static void
test_literal(void)
{
  SerdNode* hello2 = serd_node_new(NULL, serd_a_string("hello\""));

  assert(serd_node_length(hello2) == 6 &&
         !strcmp(serd_node_string(hello2), "hello\""));

  check_copy_equals(hello2);
  serd_node_free(NULL, hello2);

  SerdNode* const rdf_langString =
    serd_node_new(NULL, serd_a_uri_string(NS_RDF "langString"));

  assert(!serd_node_new(
    NULL, serd_a_typed_literal(zix_string("plain"), rdf_langString)));
  serd_node_free(NULL, rdf_langString);

  SerdNode* const en_ca           = serd_node_new(NULL, serd_a_string("en-ca"));
  const char*     lang_lit_str    = "\"Hello\"@en-ca";
  SerdNode*       sliced_lang_lit = serd_node_new(
    NULL, serd_a_plain_literal(zix_substring(lang_lit_str + 1, 5), en_ca));

  assert(!strcmp(serd_node_string(sliced_lang_lit), "Hello"));

  const SerdNode* const lang = serd_node_language(sliced_lang_lit);
  assert(lang);
  assert(!strcmp(serd_node_string(lang), "en-ca"));
  check_copy_equals(sliced_lang_lit);
  serd_node_free(NULL, sliced_lang_lit);
  serd_node_free(NULL, en_ca);

  SerdNode* const eg_Greeting =
    serd_node_new(NULL, serd_a_uri_string(NS_EG "Greeting"));

  const char* const type_lit_str = "\"Hallo\"^^<http://example.org/Greeting>";
  SerdNode* const   sliced_type_lit = serd_node_new(
    NULL,
    serd_a_typed_literal(zix_substring(type_lit_str + 1, 5), eg_Greeting));

  assert(!strcmp(serd_node_string(sliced_type_lit), "Hallo"));

  const SerdNode* const datatype = serd_node_datatype(sliced_type_lit);
  assert(datatype);
  assert(!strcmp(serd_node_string(datatype), NS_EG "Greeting"));
  serd_node_free(NULL, sliced_type_lit);
  serd_node_free(NULL, eg_Greeting);
}

static void
test_blank(void)
{
  SerdNode* blank = serd_node_new(NULL, serd_a_blank_string("b0"));
  assert(serd_node_length(blank) == 2);
  assert(serd_node_flags(blank) == 0);
  assert(!strcmp(serd_node_string(blank), "b0"));
  serd_node_free(NULL, blank);
}

static void
test_compare(void)
{
  SerdNode* const de = serd_node_new(NULL, serd_a_string("de"));
  SerdNode* const en = serd_node_new(NULL, serd_a_string("en"));

  SerdNode* const eg_Aardvark =
    serd_node_new(NULL, serd_a_uri_string(NS_EG "Aardvark"));

  SerdNode* const eg_Badger =
    serd_node_new(NULL, serd_a_uri_string(NS_EG "Badger"));

  SerdNode* angst = serd_node_new(NULL, serd_a_string("angst"));

  SerdNode* angst_de =
    serd_node_new(NULL, serd_a_plain_literal(zix_string("angst"), de));

  SerdNode* angst_en =
    serd_node_new(NULL, serd_a_plain_literal(zix_string("angst"), en));

  SerdNode* hallo =
    serd_node_new(NULL, serd_a_plain_literal(zix_string("Hallo"), de));

  assert(!serd_node_language(angst));
  assert(serd_node_language(angst_de) == de);
  assert(serd_node_language(angst_en) == en);
  assert(serd_node_language(hallo) == de);

  SerdNode* hello     = serd_node_new(NULL, serd_a_string("Hello"));
  SerdNode* universe  = serd_node_new(NULL, serd_a_string("Universe"));
  SerdNode* integer   = serd_node_new(NULL, serd_a_integer(4));
  SerdNode* short_int = serd_node_new(NULL, serd_a_primitive(serd_short(4)));
  SerdNode* blank     = serd_node_new(NULL, serd_a_blank_string("b1"));
  SerdNode* uri       = serd_node_new(NULL, serd_a_uri_string(NS_EG));

  SerdNode* aardvark =
    serd_node_new(NULL, serd_a_typed_literal(zix_string("alex"), eg_Aardvark));

  SerdNode* badger =
    serd_node_new(NULL, serd_a_typed_literal(zix_string("bobby"), eg_Badger));

  // Types are ordered according to their SerdNodeType (more or less arbitrary)
  assert(serd_node_compare(integer, hello) < 0);
  assert(serd_node_compare(hello, uri) < 0);
  assert(serd_node_compare(uri, blank) < 0);

  // If the types are the same, then strings are compared
  assert(serd_node_compare(hello, universe) < 0);

  // If literal strings are the same, languages or datatypes are compared
  assert(serd_node_compare(angst, angst_de) < 0);
  assert(serd_node_compare(angst_de, angst_en) < 0);
  assert(serd_node_compare(aardvark, badger) < 0);
  assert(serd_node_compare(integer, short_int) < 0);

  serd_node_free(NULL, badger);
  serd_node_free(NULL, aardvark);
  serd_node_free(NULL, uri);
  serd_node_free(NULL, blank);
  serd_node_free(NULL, short_int);
  serd_node_free(NULL, integer);
  serd_node_free(NULL, universe);
  serd_node_free(NULL, hello);
  serd_node_free(NULL, hallo);
  serd_node_free(NULL, angst_en);
  serd_node_free(NULL, angst_de);
  serd_node_free(NULL, angst);
  serd_node_free(NULL, eg_Badger);
  serd_node_free(NULL, eg_Aardvark);
  serd_node_free(NULL, en);
  serd_node_free(NULL, de);
}

int
main(void)
{
  test_new();
  test_uri_view();
  test_prefixed_name();
  test_joined_uri();
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
  test_lang_tagged_literal();
  test_literal();
  test_blank();
  test_compare();

  printf("Success\n");
  return 0;
}

#undef NS_XSD
#undef NS_RDF
#undef NS_EG
