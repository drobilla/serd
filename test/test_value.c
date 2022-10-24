// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/token_view.h>
#include <serd/value.h>
#include <zix/string_view.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

#ifdef __clang__

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
test_parse_value(void)
{
  const SerdValue no_datatype = serd_parse_value(
    serd_object_view(SERD_LITERAL, zix_string("1"), 0U, serd_no_token()));
  assert(!no_datatype.type);

  const SerdValue empty_datatype = serd_parse_value(
    serd_object_view(SERD_LITERAL,
                     zix_string("1"),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_empty_string())));
  assert(!empty_datatype.type);

  const SerdValue bad_value = serd_parse_value(
    serd_object_view(SERD_LITERAL,
                     zix_string("9223372036854775808"),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(NS_XSD "integer"))));
  assert(bad_value.type == SERD_NO_VALUE);
  assert(!bad_value.data.as_bool);
}

static void
test_parse_value_as(void)
{
  const SerdValue bad_type = serd_parse_value_as(
    serd_object_view(SERD_LITERAL,
                     zix_string("true"),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(NS_XSD "boolean"))),
    (SerdValueType)-1, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    false);

  assert(bad_type.type == SERD_BOOL);
  assert(bad_type.data.as_bool);
}

static void
check_boolean(const char* const   string,
              const char* const   datatype_uri,
              const bool          lossy,
              const SerdValueType value_type,
              const bool          expected)
{
  const SerdValue value = serd_parse_value_as(
    serd_object_view(SERD_LITERAL,
                     zix_string(string),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(datatype_uri))),
    SERD_BOOL,
    lossy);

  assert(value.type == value_type);
  assert(value.data.as_bool == expected);
}

static void
test_boolean(void)
{
  check_boolean("false", NS_XSD "boolean", false, SERD_BOOL, false);
  check_boolean("true", NS_XSD "boolean", false, SERD_BOOL, true);
  check_boolean("0", NS_XSD "boolean", false, SERD_BOOL, false);
  check_boolean("1", NS_XSD "boolean", false, SERD_BOOL, true);
  check_boolean("0", NS_XSD "integer", false, SERD_BOOL, false);
  check_boolean("1", NS_XSD "integer", false, SERD_BOOL, true);
  check_boolean("0.0", NS_XSD "double", false, SERD_BOOL, false);
  check_boolean("1.0", NS_XSD "double", false, SERD_BOOL, true);

  check_boolean("2", NS_XSD "integer", false, SERD_NO_VALUE, false);
  check_boolean("1.5", NS_XSD "double", false, SERD_NO_VALUE, false);

  check_boolean("2", NS_XSD "integer", true, SERD_BOOL, true);
  check_boolean("1.5", NS_XSD "double", true, SERD_BOOL, true);

  check_boolean("unknown", NS_XSD "string", true, SERD_NO_VALUE, false);
  check_boolean("!invalid", NS_XSD "long", true, SERD_NO_VALUE, false);
}

static void
check_double(const char*         string,
             const char*         datatype_uri,
             const bool          lossy,
             const SerdValueType value_type,
             const double        expected)
{
  const SerdValue value = serd_parse_value_as(
    serd_object_view(SERD_LITERAL,
                     zix_string(string),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(datatype_uri))),
    SERD_DOUBLE,
    lossy);

  assert(value.type == value_type);

  SERD_DISABLE_CONVERSION_WARNINGS
  // NOLINTBEGIN(bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c)

  assert(value_type == SERD_NO_VALUE ||
         ((isnan(value.data.as_double) && isnan(expected)) ||
          !memcmp(&value.data.as_double, &expected, sizeof(double))));

  // NOLINTEND(bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c)
  SERD_RESTORE_WARNINGS
}

static void
test_double(void)
{
  check_double("1.2", NS_XSD "double", false, SERD_DOUBLE, 1.2);
  check_double("-.5", NS_XSD "float", false, SERD_DOUBLE, -0.5);
  check_double("-67", NS_XSD "long", false, SERD_DOUBLE, -67.0);
  check_double("67", NS_XSD "unsignedLong", false, SERD_DOUBLE, 67.0);
  check_double("8.9", NS_XSD "decimal", false, SERD_DOUBLE, 8.9);
  check_double("false", NS_XSD "boolean", false, SERD_DOUBLE, 0.0);
  check_double("true", NS_XSD "boolean", false, SERD_DOUBLE, 1.0);

  SERD_DISABLE_CONVERSION_WARNINGS
  check_double("str", NS_XSD "string", true, SERD_NO_VALUE, NAN);
  check_double("!invalid", NS_XSD "long", true, SERD_NO_VALUE, NAN);
  check_double("D3AD", NS_XSD "hexBinary", true, SERD_NO_VALUE, NAN);
  check_double("Zm9v", NS_XSD "base64Binary", true, SERD_NO_VALUE, NAN);
  SERD_RESTORE_WARNINGS
}

static void
check_float(const char*         string,
            const char*         datatype_uri,
            const bool          lossy,
            const SerdValueType value_type,
            const float         expected)
{
  const SerdValue value = serd_parse_value_as(
    serd_object_view(SERD_LITERAL,
                     zix_string(string),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(datatype_uri))),
    SERD_FLOAT,
    lossy);

  assert(value.type == value_type);

  SERD_DISABLE_CONVERSION_WARNINGS
  // NOLINTBEGIN(bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c)

  assert(value_type == SERD_NO_VALUE ||
         ((isnan(value.data.as_float) && isnan(expected)) ||
          !memcmp(&value.data.as_float, &expected, sizeof(float))));

  // NOLINTEND(bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c)
  SERD_RESTORE_WARNINGS
}

static void
test_float(void)
{
  check_float("1.2", NS_XSD "float", false, SERD_FLOAT, 1.2f);
  check_float("-.5", NS_XSD "float", false, SERD_FLOAT, -0.5f);
  check_float("-67", NS_XSD "long", false, SERD_FLOAT, -67.0f);
  check_float("false", NS_XSD "boolean", false, SERD_FLOAT, 0.0f);
  check_float("true", NS_XSD "boolean", false, SERD_FLOAT, 1.0f);

  check_float("1.5", NS_XSD "decimal", true, SERD_FLOAT, 1.5f);

  SERD_DISABLE_CONVERSION_WARNINGS
  check_float("str", NS_XSD "string", true, SERD_NO_VALUE, NAN);
  check_float("!invalid", NS_XSD "long", true, SERD_NO_VALUE, NAN);
  check_float("D3AD", NS_XSD "hexBinary", true, SERD_NO_VALUE, NAN);
  check_float("Zm9v", NS_XSD "base64Binary", true, SERD_NO_VALUE, NAN);
  SERD_RESTORE_WARNINGS
}

static void
check_integer(const char*         string,
              const char*         datatype_uri,
              const bool          lossy,
              const SerdValueType value_type,
              const int64_t       expected)
{
  const SerdValue value = serd_parse_value_as(
    serd_object_view(SERD_LITERAL,
                     zix_string(string),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(datatype_uri))),
    SERD_LONG,
    lossy);

  assert(value.type == value_type);
  assert(value_type == SERD_NO_VALUE || value.data.as_long == expected);
}

static void
test_integer(void)
{
  check_integer("12", NS_XSD "long", false, SERD_LONG, 12);
  check_integer("-34", NS_XSD "long", false, SERD_LONG, -34);
  check_integer("56", NS_XSD "integer", false, SERD_LONG, 56);
  check_integer("false", NS_XSD "boolean", false, SERD_LONG, 0);
  check_integer("true", NS_XSD "boolean", false, SERD_LONG, 1);
  check_integer("78.0", NS_XSD "decimal", false, SERD_LONG, 78);

  check_integer("0", NS_XSD "nonPositiveInteger", false, SERD_LONG, 0);
  check_integer("-1", NS_XSD "negativeInteger", false, SERD_LONG, -1);
  check_integer("2", NS_XSD "nonNegativeInteger", false, SERD_LONG, 2);
  check_integer("3", NS_XSD "positiveInteger", false, SERD_LONG, 3);

  check_integer("78.5", NS_XSD "decimal", false, SERD_NO_VALUE, 0);
  check_integer("78.5", NS_XSD "decimal", true, SERD_LONG, 78);

  check_integer("unknown", NS_XSD "string", true, SERD_NO_VALUE, 0);
  check_integer("!invalid", NS_XSD "long", true, SERD_NO_VALUE, 0);
}

int
main(void)
{
  test_parse_value();
  test_parse_value_as();
  test_boolean();
  test_double();
  test_float();
  test_integer();
  return 0;
}

#undef NS_XSD
#undef NS_RDF
#undef NS_EG
