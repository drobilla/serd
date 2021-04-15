// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/memory.h"
#include "serd/node.h"
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
  SerdNode* const string = serd_new_string(zix_string("httpstring"));

  const SerdURIView uri = serd_node_uri_view(string);
  assert(!uri.scheme.length);

  serd_node_free(string);
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
    SerdNode* node = serd_new_decimal(dbl_test_nums[i]);
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
    SerdNode*   node     = serd_new_integer(int_test_nums[i]);
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
  assert(!serd_new_base64(&SERD_URI_NULL, 0));

  // Test valid base64 blobs with a range of sizes
  for (size_t size = 1; size < 256; ++size) {
    uint8_t* const data = (uint8_t*)malloc(size);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (uint8_t)((size + i) % 256);
    }

    size_t      out_size = 0;
    SerdNode*   blob     = serd_new_base64(data, size);
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

  SerdNode* const xsd_base64Binary =
    serd_new_uri(zix_string(NS_XSD "base64Binary"));
  SerdNode* const blob =
    serd_new_typed_literal(zix_string("!nval!d$"), xsd_base64Binary);

  const char* const blob_str = serd_node_string(blob);
  size_t            out_size = 42;
  uint8_t*          out =
    (uint8_t*)serd_base64_decode(blob_str, serd_node_length(blob), &out_size);

  assert(!out);
  assert(out_size == 0);

  serd_node_free(blob);
  serd_node_free(xsd_base64Binary);
}

static void
test_base64_decode(void)
{
  static const char* const decoded     = "test";
  static const size_t      decoded_len = 4U;

  // Test decoding clean base64
  {
    static const char* const encoded     = "dGVzdA==";
    static const size_t      encoded_len = 8U;

    size_t      size = 0U;
    void* const data = serd_base64_decode(encoded, encoded_len, &size);

    assert(data);
    assert(size == decoded_len);
    assert(!strncmp((const char*)data, decoded, decoded_len));
    serd_free(data);
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
  test_strtod();
  test_new_decimal();
  test_integer_to_node();
  test_boolean();
  test_blob_to_node();
  test_base64_decode();
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
