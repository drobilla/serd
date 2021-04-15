// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/node.h"
#include "serd/status.h"
#include "serd/stream_result.h"
#include "serd/string.h"
#include "serd/uri.h"
#include "zix/string_view.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_EG "http://example.org/"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

static void
test_uri_view(void)
{
  SerdNode* const string = serd_new_string(NULL, zix_string("httpstring"));

  const SerdURIView uri = serd_node_uri_view(string);
  assert(!uri.scheme.length);

  serd_node_free(NULL, string);
}

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
    SerdNode* node = serd_new_decimal(NULL, dbl_test_nums[i]);
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
      serd_node_free(NULL, node);
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
    SerdNode*   node     = serd_new_integer(NULL, int_test_nums[i]);
    const char* node_str = serd_node_string(node);
    assert(!strcmp(node_str, int_test_strs[i]));
    const size_t len = strlen(node_str);
    assert(serd_node_length(node) == len);

    const SerdNode* const datatype = serd_node_datatype(node);
    assert(datatype);
    assert(!strcmp(serd_node_string(datatype), NS_XSD "integer"));
    serd_node_free(NULL, node);
  }

#undef N_TEST_NUMS
}

static void
test_boolean(void)
{
  SerdNode* const true_node = serd_new_boolean(NULL, true);
  assert(!strcmp(serd_node_string(true_node), "true"));

  const SerdNode* const true_datatype = serd_node_datatype(true_node);
  assert(true_datatype);
  assert(!strcmp(serd_node_string(true_datatype), NS_XSD "boolean"));
  serd_node_free(NULL, true_node);

  SerdNode* const false_node = serd_new_boolean(NULL, false);
  assert(!strcmp(serd_node_string(false_node), "false"));

  const SerdNode* const false_datatype = serd_node_datatype(false_node);
  assert(false_datatype);
  assert(!strcmp(serd_node_string(false_datatype), NS_XSD "boolean"));
  serd_node_free(NULL, false_node);
}

static void
test_base64(void)
{
  assert(!serd_new_base64(NULL, &SERD_URI_NULL, 0));

  // Test valid base64 blobs with a range of sizes
  for (size_t size = 1; size < 256; ++size) {
    uint8_t* const data = (uint8_t*)malloc(size);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (uint8_t)((size + i) % 256);
    }

    SerdNode*    blob     = serd_new_base64(NULL, data, size);
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
  SerdNode* const datatype = serd_new_uri(NULL, zix_string(datatype_uri));
  SerdNode* const node =
    serd_new_typed_literal(NULL, zix_string(string), datatype);

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
      serd_new_uri(NULL, zix_string(NS_XSD "base64Binary"));
    SerdNode* const node =
      serd_new_typed_literal(NULL, zix_string("Zm9v"), datatype);

    const SerdStreamResult r = serd_node_decode(node, sizeof(small), small);

    assert(r.status == SERD_NO_SPACE);
    serd_node_free(NULL, node);
    serd_node_free(NULL, datatype);
  }
  {
    SerdNode* const string = serd_new_string(NULL, zix_string("string"));

    assert(serd_node_decoded_size(string) == 0U);

    const SerdStreamResult r = serd_node_decode(string, sizeof(small), small);

    assert(r.status == SERD_BAD_ARG);
    assert(r.count == 0U);
    serd_node_free(NULL, string);
  }
  {
    SerdNode* const datatype = serd_new_uri(NULL, zix_string(NS_EG "Datatype"));
    SerdNode* const unknown =
      serd_new_typed_literal(NULL, zix_string("secret"), datatype);

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

  SerdNode* lhs = serd_new_string(NULL, replacement_char);
  SerdNode* rhs = serd_new_string(NULL, zix_string("123"));

  assert(serd_node_equals(lhs, lhs));
  assert(!serd_node_equals(lhs, rhs));

  SerdNode* const qnode = serd_new_curie(NULL, zix_string("foo:bar"));
  assert(!serd_node_equals(lhs, qnode));
  serd_node_free(NULL, qnode);

  assert(!serd_node_copy(NULL, NULL));

  serd_node_free(NULL, lhs);
  serd_node_free(NULL, rhs);
}

static void
test_node_from_string(void)
{
  SerdNode* const     hello = serd_new_string(NULL, zix_string("hello\""));
  const ZixStringView hello_string = serd_node_string_view(hello);

  assert(serd_node_type(hello) == SERD_LITERAL);
  assert(serd_node_flags(hello) == SERD_HAS_QUOTE);
  assert(serd_node_length(hello) == 6U);
  assert(hello_string.length == 6U);
  assert(!strcmp(hello_string.data, "hello\""));
  serd_node_free(NULL, hello);

  SerdNode* const uri = serd_new_uri(NULL, zix_string(NS_EG));
  assert(serd_node_length(uri) == 19);
  assert(!strcmp(serd_node_string(uri), NS_EG));
  assert(serd_node_uri_view(uri).authority.length == 11);
  assert(!strncmp(serd_node_uri_view(uri).authority.data, "example.org", 11));
  serd_node_free(NULL, uri);
}

static void
test_node_from_substring(void)
{
  SerdNode* const a_b = serd_new_string(NULL, zix_substring("a\"bc", 3));
  assert(serd_node_length(a_b) == 3);
  assert(serd_node_flags(a_b) == SERD_HAS_QUOTE);
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
test_literal(void)
{
  SerdNode* hello2 = serd_new_string(NULL, zix_string("hello\""));

  assert(serd_node_length(hello2) == 6 &&
         serd_node_flags(hello2) == SERD_HAS_QUOTE &&
         !strcmp(serd_node_string(hello2), "hello\""));

  check_copy_equals(hello2);

  SerdNode* hello3 = serd_new_plain_literal(NULL, zix_string("hello\""), NULL);

  assert(serd_node_equals(hello2, hello3));

  SerdNode* hello4 = serd_new_typed_literal(NULL, zix_string("hello\""), NULL);
  assert(serd_node_equals(hello4, hello2));

  serd_node_free(NULL, hello4);
  serd_node_free(NULL, hello3);
  serd_node_free(NULL, hello2);

  // Test literals with language tag

  SerdNode* rdf_langString =
    serd_new_uri(NULL, zix_string(NS_RDF "langString"));

  assert(!serd_new_typed_literal(NULL, zix_string("plain"), rdf_langString));
  assert(!serd_new_plain_literal(NULL, zix_string("badlang"), rdf_langString));

  SerdNode* const   en           = serd_new_string(NULL, zix_string("en"));
  const char* const lang_lit_str = "\"Hello\"@en";
  SerdNode* const   sliced_lang_lit =
    serd_new_plain_literal(NULL, zix_substring(lang_lit_str + 1, 5), en);

  assert(!strcmp(serd_node_string(sliced_lang_lit), "Hello"));

  const SerdNode* const lang = serd_node_language(sliced_lang_lit);
  assert(lang);
  assert(!strcmp(serd_node_string(lang), "en"));
  check_copy_equals(sliced_lang_lit);
  serd_node_free(NULL, sliced_lang_lit);
  serd_node_free(NULL, en);
  serd_node_free(NULL, rdf_langString);

  // Test literals with datatype URI

  SerdNode* const eg_Greeting =
    serd_new_uri(NULL, zix_string(NS_EG "Greeting"));
  const char* const type_lit_str = "\"Hallo\"^^<http://example.org/Greeting>";
  SerdNode* const   sliced_type_lit = serd_new_typed_literal(
    NULL, zix_substring(type_lit_str + 1, 5), eg_Greeting);

  assert(!strcmp(serd_node_string(sliced_type_lit), "Hallo"));

  const SerdNode* const datatype = serd_node_datatype(sliced_type_lit);
  assert(datatype);
  assert(!strcmp(serd_node_string(datatype), NS_EG "Greeting"));
  serd_node_free(NULL, sliced_type_lit);
  serd_node_free(NULL, eg_Greeting);

  // Test plain string literals

  SerdNode* const plain_lit =
    serd_new_plain_literal(NULL, zix_string("Plain"), NULL);
  assert(!strcmp(serd_node_string(plain_lit), "Plain"));
  serd_node_free(NULL, plain_lit);
}

static void
test_blank(void)
{
  SerdNode* blank = serd_new_blank(NULL, zix_string("b0"));
  assert(serd_node_length(blank) == 2);
  assert(serd_node_flags(blank) == 0);
  assert(!strcmp(serd_node_string(blank), "b0"));
  serd_node_free(NULL, blank);
}

int
main(void)
{
  test_uri_view();
  test_strtod();
  test_new_decimal();
  test_integer_to_node();
  test_boolean();
  test_base64();
  test_decode();
  test_node_equals();
  test_node_from_string();
  test_node_from_substring();
  test_literal();
  test_blank();

  printf("Success\n");
  return 0;
}

#undef NS_XSD
#undef NS_RDF
#undef NS_EG
