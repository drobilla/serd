// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"

#include <serd/node.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/string.h>
#include <serd/token_view.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef INFINITY
#  define INFINITY (DBL_MAX + DBL_MAX)
#endif
#ifndef NAN
#  define NAN (INFINITY - INFINITY)
#endif

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

static void
test_free(void)
{
  serd_node_free(NULL, NULL);
}

static void
check_strtod(const double dbl, const double max_delta)
{
  char buf[1024];
  snprintf(buf, sizeof(buf), "%f", dbl);

  char*        endptr = NULL;
  const double out    = serd_strtod(buf, &endptr);
  const double diff   = fabs(out - dbl);

  assert(diff <= max_delta);
}

static void
test_string_to_double(void)
{
  const double expt_test_nums[] = {
    2.0E18, -5e19, +8e20, 2e+22, -5e-5, 8e0, 9e-0, 2e+0};

  const char* expt_test_strs[] = {"02e18",
                                  "-5e019",
                                  " +8e20",
                                  "\f2E+22",
                                  "\n-5E-5",
                                  "\r8E0",
                                  "\t9e-0",
                                  "\v2e+0"};

  for (size_t i = 0; i < sizeof(expt_test_nums) / sizeof(double); ++i) {
    const double num   = serd_strtod(expt_test_strs[i], NULL);
    const double delta = fabs(num - expt_test_nums[i]);
    assert(delta <= DBL_EPSILON);

    check_strtod(expt_test_nums[i], DBL_EPSILON);
  }
}

static void
test_double_to_node(void)
{
  const double dbl_test_nums[] = {0.0,
                                  9.0,
                                  10.0,
                                  .01,
                                  2.05,
                                  -16.00001,
                                  5.000000005,
                                  0.0000000001,
                                  (double)NAN,
                                  (double)INFINITY};

  const char* dbl_test_strs[] = {"0.0",
                                 "9.0",
                                 "10.0",
                                 "0.01",
                                 "2.05",
                                 "-16.00001",
                                 "5.00000001",
                                 "0.0",
                                 NULL,
                                 NULL};

  for (size_t i = 0; i < sizeof(dbl_test_nums) / sizeof(double); ++i) {
    SerdNode   node = serd_node_new_decimal(NULL, dbl_test_nums[i], 8);
    const bool pass = (node.buf && dbl_test_strs[i])
                        ? expect_string(node.buf, dbl_test_strs[i])
                        : (node.buf == dbl_test_strs[i]);
    assert(pass);
    const size_t len = node.buf ? strlen(node.buf) : 0;
    assert(node.n_bytes == len);
    serd_node_free(NULL, &node);
  }
}

static void
test_integer_to_node(void)
{
#define N_TEST_NUMS 7U

  const long int_test_nums[N_TEST_NUMS] = {0, -0, -23, 23, -12340, 1000, -1000};

  const char* int_test_strs[N_TEST_NUMS] = {
    "0", "0", "-23", "23", "-12340", "1000", "-1000"};

  for (size_t i = 0; i < N_TEST_NUMS; ++i) {
    SerdNode node = serd_node_new_integer(NULL, int_test_nums[i]);
    assert(expect_string_view(serd_node_string_view(&node), int_test_strs[i]));
    serd_node_free(NULL, &node);
  }

#undef N_TEST_NUMS
}

static void
test_blob_to_node(void)
{
  for (size_t size = 1; size < 256; ++size) {
    uint8_t* const data = (uint8_t*)zix_malloc(NULL, size);
    assert(data);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (uint8_t)((size + i) % 256);
    }

    SerdNode            blob   = serd_node_new_blob(NULL, data, size, size % 5);
    const ZixStringView string = serd_node_string_view(&blob);

    assert(string.length > size);

    size_t   out_size = 0;
    uint8_t* out =
      (uint8_t*)serd_base64_decode(NULL, string.data, blob.n_bytes, &out_size);
    assert(out_size == size);

    for (size_t i = 0; i < size; ++i) {
      assert(out[i] == data[i]);
    }

    serd_node_free(NULL, &blob);
    zix_free(NULL, out);
    zix_free(NULL, data);
  }
}

static void
check_decode(const char* const encoded,
             const char* const datatype_uri,
             const char* const expected)
{
  assert(!strcmp(datatype_uri, NS_XSD "base64Binary"));

  const size_t expected_len = strlen(expected);
  size_t       size         = 0U;
  void* const  data = serd_base64_decode(NULL, encoded, strlen(encoded), &size);

  assert(data);
  assert(size == expected_len);
  assert(!strncmp((const char*)data, expected, expected_len));
  zix_free(NULL, data);
}

static void
test_base64_decode(void)
{
  // Test decoding clean base64
  check_decode("dGVzdA==", NS_XSD "base64Binary", "test");

  // Test decoding equivalent dirty base64 with ignored junk characters
  check_decode("d-G#V!z*d(A$%==", NS_XSD "base64Binary", "test");

  // Test decoding effectively nothing
  check_decode("@#$%", NS_XSD "base64Binary", "");

  // Test decoding various lengths
  check_decode("Y2h1bms=", NS_XSD "base64Binary", "chunk");
  check_decode("bGV0dGVy", NS_XSD "base64Binary", "letter");
  check_decode("bm90ZQ==", NS_XSD "base64Binary", "note");

  // Test ignoring junk characters
  check_decode(" \f\n\r\t\vZm9v \f\n\r\t\v", NS_XSD "base64Binary", "foo");
}

static void
test_node_equals(void)
{
  static const uint8_t replacement_char_str[] = {0xEF, 0xBF, 0xBD, 0};

  SerdNode lhs =
    serd_node_from_string(SERD_LITERAL, (const char*)replacement_char_str);
  SerdNode rhs = serd_node_from_string(SERD_LITERAL, "123");
  assert(!serd_node_equals(&lhs, &rhs));

  SerdNode qnode = serd_node_from_string(SERD_CURIE, "foo:bar");
  assert(!serd_node_equals(&lhs, &qnode));
  assert(serd_node_equals(&lhs, &lhs));

  SerdNode qnode_copy = serd_node_copy(NULL, &qnode);
  assert(serd_node_equals(&qnode, &qnode_copy));
  serd_node_free(NULL, &qnode_copy);

  SerdNode null_copy = serd_node_copy(NULL, &SERD_NODE_NULL);
  assert(serd_node_equals(&SERD_NODE_NULL, &null_copy));
}

static void
test_node_from_string(void)
{
  SerdNode node = serd_node_from_string(SERD_LITERAL, "hello\"");
  assert(node.flags == SERD_HAS_QUOTE &&
         expect_string_view(serd_node_string_view(&node), "hello\""));

  assert(node.flags == SERD_HAS_QUOTE &&
         expect_string_view(serd_node_string_view(&node), "hello\""));

  node = serd_node_from_string(SERD_URI, NULL);
  assert(serd_node_equals(&node, &SERD_NODE_NULL));
}

static void
test_node_from_substring(void)
{
  static const uint8_t utf8_str[] = {'l', 0xC3, 0xB6, 'n', 'g', 0};

  SerdNode empty = serd_node_from_substring(SERD_LITERAL, NULL, 32);
  assert(!empty.buf && !empty.n_bytes && !empty.flags && !empty.type);

  SerdNode a_b = serd_node_from_substring(SERD_LITERAL, "a\"bc", 3);
  assert(a_b.n_bytes == 3 && a_b.flags == SERD_HAS_QUOTE &&
         !strncmp(a_b.buf, "a\"b", 3));

  a_b = serd_node_from_substring(SERD_LITERAL, "a\"bc", 10);
  assert(a_b.n_bytes == 4 && a_b.flags == SERD_HAS_QUOTE &&
         !strncmp(a_b.buf, "a\"bc", 4));

  SerdNode utf8 =
    serd_node_from_substring(SERD_LITERAL, (const char*)utf8_str, 5);
  assert(utf8.n_bytes == 5 && !utf8.flags &&
         !strncmp(utf8.buf, (const char*)utf8_str, 6));
}

static void
test_uri_node_from_string(void)
{
  assert(!serd_node_new_uri_from_string(NULL, NULL).buf);
  assert(!serd_node_new_uri_from_string(NULL, "").buf);

  SerdNode uri_node =
    serd_node_new_uri_from_string(NULL, "http://example.org/p");
  assert(uri_node.n_bytes == 20U);
  serd_node_free(NULL, &uri_node);
}

static void
test_uri_node_from_node(void)
{
  const SerdNode string      = serd_node_from_string(SERD_LITERAL, "s");
  SerdNode       string_node = serd_node_new_uri_from_node(NULL, &string);
  assert(!string_node.n_bytes);
  serd_node_free(NULL, &string_node);

  const SerdNode nouri      = {NULL, 0U, 0U, SERD_URI};
  SerdNode       nouri_node = serd_node_new_uri_from_node(NULL, &nouri);
  assert(!nouri_node.n_bytes);
  serd_node_free(NULL, &nouri_node);

  const SerdNode uri = serd_node_from_string(SERD_URI, "http://example.org/p");
  SerdNode       uri_node = serd_node_new_uri_from_node(NULL, &uri);
  assert(uri_node.n_bytes == 20U);
  serd_node_free(NULL, &uri_node);
}

static void
test_views(void)
{
  const SerdNode uri = serd_node_from_string(SERD_URI, "http://example.org/");

  const SerdTokenView tok = serd_node_token_view(&uri);
  assert(tok.type == SERD_URI);
  assert(zix_string_view_equals(tok.string, zix_string("http://example.org/")));
}

int
main(void)
{
  test_free();
  test_string_to_double();
  test_double_to_node();
  test_integer_to_node();
  test_blob_to_node();
  test_base64_decode();
  test_node_equals();
  test_node_from_string();
  test_node_from_substring();
  test_uri_node_from_string();
  test_uri_node_from_node();
  test_views();
  return 0;
}
