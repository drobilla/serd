// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "expect_string.h"

#include <serd/node.h>
#include <serd/node_type.h>
#include <serd/token_view.h>
#include <zix/string_view.h>

#include <assert.h>
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

static void
test_free(void)
{
  serd_node_free(NULL, NULL);
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
  assert(expect_string_view(serd_node_string_view(&node), "hello\""));
  assert(expect_string_view(serd_node_string_view(&node), "hello\""));

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
  assert(a_b.n_bytes == 3 && !a_b.flags && !strncmp(a_b.buf, "a\"b", 3));

  a_b = serd_node_from_substring(SERD_LITERAL, "a\"bc", 10);
  assert(a_b.n_bytes == 4 && !a_b.flags && !strncmp(a_b.buf, "a\"bc", 4));

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
  test_double_to_node();
  test_integer_to_node();
  test_node_equals();
  test_node_from_string();
  test_node_from_substring();
  test_uri_node_from_string();
  test_uri_node_from_node();
  test_views();
  return 0;
}
