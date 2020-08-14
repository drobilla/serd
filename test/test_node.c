// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/memory.h"
#include "serd/node.h"
#include "serd/string.h"
#include "serd/uri.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INFINITY
#  define INFINITY (DBL_MAX + DBL_MAX)
#endif
#ifndef NAN
#  define NAN (INFINITY - INFINITY)
#endif

static void
test_strtod(double dbl, double max_delta)
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
    2.0E18, -5e19, +8e20, 2e+24, -5e-5, 8e0, 9e-0, 2e+0};

  const char* expt_test_strs[] = {
    "02e18", "-5e019", "+8e20", "2E+24", "-5E-5", "8E0", "9e-0", " 2e+0"};

  for (size_t i = 0; i < sizeof(expt_test_nums) / sizeof(double); ++i) {
    const double num   = serd_strtod(expt_test_strs[i], NULL);
    const double delta = fabs(num - expt_test_nums[i]);
    assert(delta <= DBL_EPSILON);

    test_strtod(expt_test_nums[i], DBL_EPSILON);
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
                                  NAN,
                                  INFINITY};

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
    SerdNode*   node     = serd_new_decimal(dbl_test_nums[i], 8);
    const char* node_str = node ? serd_node_string(node) : NULL;
    const bool  pass     = (node_str && dbl_test_strs[i])
                             ? !strcmp(node_str, dbl_test_strs[i])
                             : (node_str == dbl_test_strs[i]);
    assert(pass);
    assert(!node || serd_node_length(node) == strlen(node_str));
    serd_node_free(node);
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
    SerdNode*   node     = serd_new_integer(int_test_nums[i]);
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, int_test_strs[i]));
    assert(serd_node_length(node) == strlen(node_str));
    serd_node_free(node);
  }

#undef N_TEST_NUMS
}

static void
test_blob_to_node(void)
{
  assert(!serd_new_blob(&SERD_URI_NULL, 0, false));

  for (size_t size = 1; size < 256; ++size) {
    uint8_t* const data = (uint8_t*)malloc(size);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (uint8_t)((size + i) % 256);
    }

    size_t      out_size = 0;
    SerdNode*   blob     = serd_new_blob(data, size, size % 5);
    const char* blob_str = serd_node_string(blob);
    uint8_t*    out =
      (uint8_t*)serd_base64_decode(blob_str, serd_node_length(blob), &out_size);

    assert(serd_node_length(blob) == strlen(blob_str));
    assert(out_size == size);

    for (size_t i = 0; i < size; ++i) {
      assert(out[i] == data[i]);
    }

    serd_node_free(blob);
    serd_free(out);
    free(data);
  }
}

static void
test_node_equals(void)
{
  const uint8_t replacement_char_str[] = {0xEF, 0xBF, 0xBD, 0};
  SerdNode*     lhs =
    serd_new_string(SERD_LITERAL, (const char*)replacement_char_str);
  SerdNode* rhs = serd_new_string(SERD_LITERAL, "123");
  assert(!serd_node_equals(lhs, rhs));

  SerdNode* qnode = serd_new_string(SERD_CURIE, "foo:bar");
  assert(!serd_node_equals(lhs, qnode));
  assert(serd_node_equals(lhs, lhs));

  assert(!serd_node_copy(NULL));

  serd_node_free(qnode);
  serd_node_free(lhs);
  serd_node_free(rhs);
}

static void
test_node_from_string(void)
{
  SerdNode* hello = serd_new_string(SERD_LITERAL, "hello\"");
  assert(serd_node_length(hello) == 6 &&
         serd_node_flags(hello) == SERD_HAS_QUOTE &&
         !strcmp(serd_node_string(hello), "hello\""));

  serd_node_free(hello);
}

static void
test_node_from_substring(void)
{
  SerdNode* a_b = serd_new_substring(SERD_LITERAL, "a\"bc", 3);
  assert(serd_node_length(a_b) == 3 && serd_node_flags(a_b) == SERD_HAS_QUOTE &&
         !strncmp(serd_node_string(a_b), "a\"b", 3));

  serd_node_free(a_b);
  a_b = serd_new_substring(SERD_LITERAL, "a\"bc", 10);
  assert(serd_node_length(a_b) == 4 && serd_node_flags(a_b) == SERD_HAS_QUOTE &&
         !strncmp(serd_node_string(a_b), "a\"bc", 4));
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
  SerdNode* hello2 = serd_new_literal("hello\"", NULL, NULL);
  assert(serd_node_length(hello2) == 6 &&
         serd_node_flags(hello2) == SERD_HAS_QUOTE &&
         !strcmp(serd_node_string(hello2), "hello\""));
  check_copy_equals(hello2);
  serd_node_free(hello2);

  SerdNode* hello_l = serd_new_literal("hello_l\"", NULL, "en");
  assert(serd_node_length(hello_l) == 8);
  assert(!strcmp(serd_node_string(hello_l), "hello_l\""));
  assert(serd_node_flags(hello_l) == (SERD_HAS_QUOTE | SERD_HAS_LANGUAGE));

  const SerdNode* const lang = serd_node_language(hello_l);
  assert(lang);
  assert(!strcmp(serd_node_string(lang), "en"));
  check_copy_equals(hello_l);
  serd_node_free(hello_l);

  SerdNode* hello_dt =
    serd_new_literal("hello_dt\"", "http://example.org/Thing", NULL);
  assert(serd_node_length(hello_dt) == 9);
  assert(!strcmp(serd_node_string(hello_dt), "hello_dt\""));
  assert(serd_node_flags(hello_dt) == (SERD_HAS_QUOTE | SERD_HAS_DATATYPE));

  const SerdNode* const datatype = serd_node_datatype(hello_dt);
  assert(datatype);
  assert(!strcmp(serd_node_string(datatype), "http://example.org/Thing"));

  check_copy_equals(hello_dt);
  serd_node_free(hello_dt);
}

int
main(void)
{
  test_string_to_double();
  test_double_to_node();
  test_integer_to_node();
  test_blob_to_node();
  test_node_equals();
  test_node_from_string();
  test_node_from_substring();
  test_literal();

  printf("Success\n");
  return 0;
}
