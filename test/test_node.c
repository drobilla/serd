// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/memory.h"
#include "serd/node.h"
#include "serd/string.h"
#include "serd/string_view.h"
#include "serd/uri.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

static void
check_strtod(const double dbl, const double max_delta)
{
  char buf[1024];
  snprintf(buf, sizeof(buf), "%f", dbl);

  const char*  endptr = NULL;
  const double out    = serd_strtod(buf, &endptr);
  const double diff   = fabs(out - dbl);

  assert(diff <= max_delta);
}

static void
test_strtod(void)
{
  const double expt_test_nums[] = {
    2.0E18, -5e19, +8e20, 2e+24, -5e-5, 8e0, 9e-0, 2e+0};

  const char* expt_test_strs[] = {
    "02e18", "-5e019", "+8e20", "2E+24", "-5E-5", "8E0", "9e-0", " 2e+0"};

  for (size_t i = 0; i < sizeof(expt_test_nums) / sizeof(double); ++i) {
    const double num   = serd_strtod(expt_test_strs[i], NULL);
    const double delta = fabs(num - expt_test_nums[i]);
    assert(delta <= DBL_EPSILON);

    check_strtod(expt_test_nums[i], DBL_EPSILON);
  }
}

static void
test_new_decimal(void)
{
  static const double dbl_test_nums[] = {
    0.0, 9.0, 10.0, .01, 2.05, -16.00001, 5.000000005, 0.0000000001};

  static const char* const dbl_test_strs[] = {"0.0",
                                              "9.0",
                                              "10.0",
                                              "0.01",
                                              "2.05",
                                              "-16.00001",
                                              "5.000000005",
                                              "0.0000000001"};

  for (size_t i = 0; i < sizeof(dbl_test_nums) / sizeof(double); ++i) {
    SerdNode* node = serd_new_decimal(dbl_test_nums[i], NULL);
    assert(node);

    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, dbl_test_strs[i]));

    const size_t len = node_str ? strlen(node_str) : 0;
    assert((!node && len == 0) || serd_node_length(node) == len);

    if (node) {
      const SerdNode* const datatype = serd_node_datatype(node);
      assert(datatype);
      assert(!dbl_test_strs[i] ||
             !strcmp(serd_node_string(datatype), NS_XSD "decimal"));
      serd_node_free(node);
    }
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
    SerdNode*   node     = serd_new_integer(int_test_nums[i], NULL);
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, int_test_strs[i]));
    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "integer"));
    serd_node_free(node);
  }

#undef N_TEST_NUMS
}

static void
test_boolean(void)
{
  SerdNode* const true_node = serd_new_boolean(true);
  assert(!strcmp(serd_node_string(true_node), "true"));

  const SerdNode* const true_datatype = serd_node_datatype(true_node);
  assert(true_datatype);
  assert(!strcmp(serd_node_string(true_datatype), NS_XSD "boolean"));
  serd_node_free(true_node);

  SerdNode* const false_node = serd_new_boolean(false);
  assert(!strcmp(serd_node_string(false_node), "false"));

  const SerdNode* const false_datatype = serd_node_datatype(false_node);
  assert(false_datatype);
  assert(!strcmp(serd_node_string(false_datatype), NS_XSD "boolean"));
  serd_node_free(false_node);
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
    serd_string("!nval!d$"), serd_string(NS_XSD "base64Binary"));

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
  SerdNode* rhs = serd_new_string(serd_string("123"));

  assert(serd_node_equals(lhs, lhs));
  assert(!serd_node_equals(lhs, rhs));

  SerdNode* const qnode = serd_new_curie(serd_string("foo:bar"));
  assert(!serd_node_equals(lhs, qnode));
  serd_node_free(qnode);

  assert(!serd_node_copy(NULL));

  serd_node_free(lhs);
  serd_node_free(rhs);
}

static void
test_node_from_string(void)
{
  SerdNode* const hello = serd_new_string(serd_string("hello\""));
  assert(serd_node_length(hello) == 6);
  assert(serd_node_flags(hello) == SERD_HAS_QUOTE);
  assert(!strncmp(serd_node_string(hello), "hello\"", 6));
  serd_node_free(hello);
}

static void
test_node_from_substring(void)
{
  SerdNode* const a_b = serd_new_string(serd_substring("a\"bc", 3));
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
  SerdNode* hello2 = serd_new_string(serd_string("hello\""));

  assert(serd_node_length(hello2) == 6 &&
         serd_node_flags(hello2) == SERD_HAS_QUOTE &&
         !strcmp(serd_node_string(hello2), "hello\""));

  check_copy_equals(hello2);

  SerdNode* hello3 =
    serd_new_plain_literal(serd_string("hello\""), serd_empty_string());

  assert(serd_node_equals(hello2, hello3));

  SerdNode* hello4 =
    serd_new_typed_literal(serd_string("hello\""), serd_empty_string());

  assert(!serd_new_typed_literal(serd_string("plain"),
                                 serd_string(NS_RDF "langString")));

  assert(serd_node_equals(hello4, hello2));

  serd_node_free(hello4);
  serd_node_free(hello3);
  serd_node_free(hello2);

  const char* lang_lit_str    = "\"Hello\"@en";
  SerdNode*   sliced_lang_lit = serd_new_plain_literal(
    serd_substring(lang_lit_str + 1, 5), serd_substring(lang_lit_str + 8, 2));

  assert(!strcmp(serd_node_string(sliced_lang_lit), "Hello"));

  const SerdNode* const lang = serd_node_language(sliced_lang_lit);
  assert(lang);
  assert(!strcmp(serd_node_string(lang), "en"));
  check_copy_equals(sliced_lang_lit);
  serd_node_free(sliced_lang_lit);

  const char* type_lit_str    = "\"Hallo\"^^<http://example.org/Greeting>";
  SerdNode*   sliced_type_lit = serd_new_typed_literal(
    serd_substring(type_lit_str + 1, 5), serd_substring(type_lit_str + 10, 27));

  assert(!strcmp(serd_node_string(sliced_type_lit), "Hallo"));

  const SerdNode* const datatype = serd_node_datatype(sliced_type_lit);
  assert(datatype);
  assert(!strcmp(serd_node_string(datatype), "http://example.org/Greeting"));
  serd_node_free(sliced_type_lit);

  SerdNode* const plain_lit =
    serd_new_plain_literal(serd_string("Plain"), serd_empty_string());
  assert(!strcmp(serd_node_string(plain_lit), "Plain"));
  serd_node_free(plain_lit);
}

static void
test_blank(void)
{
  SerdNode* blank = serd_new_blank(serd_string("b0"));
  assert(serd_node_length(blank) == 2);
  assert(serd_node_flags(blank) == 0);
  assert(!strcmp(serd_node_string(blank), "b0"));
  serd_node_free(blank);
}

int
main(void)
{
  test_strtod();
  test_new_decimal();
  test_integer_to_node();
  test_blob_to_node();
  test_boolean();
  test_node_equals();
  test_node_from_string();
  test_node_from_substring();
  test_literal();
  test_blank();

  printf("Success\n");
  return 0;
}
