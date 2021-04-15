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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

static void
test_integer_to_node(void)
{
  const long int_test_nums[] = {0, -0, -23, 23, -12340, 1000, -1000};

  const char* int_test_strs[] = {
    "0", "0", "-23", "23", "-12340", "1000", "-1000"};

  for (size_t i = 0; i < sizeof(int_test_nums) / sizeof(double); ++i) {
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

  assert(!serd_node_equals(lhs, rhs));

  SerdNode* const qnode = serd_new_curie(SERD_STATIC_STRING("foo:bar"));
  assert(!serd_node_equals(lhs, qnode));
  serd_node_free(qnode);

  assert(!serd_node_copy(NULL));

  serd_node_free(lhs);
  serd_node_free(rhs);
}

static void
test_node_from_string(void)
{
  SerdNode* const hello = serd_new_string(SERD_STATIC_STRING("hello\""));
  assert(serd_node_length(hello) == 6);
  assert(serd_node_flags(hello) == SERD_HAS_QUOTE);
  assert(!strncmp(serd_node_string(hello), "hello\"", 6));
  serd_node_free(hello);
}

static void
test_node_from_substring(void)
{
  SerdNode* const a_b = serd_new_string(SERD_STRING_VIEW("a\"bc", 3));
  assert(serd_node_length(a_b) == 3);
  assert(serd_node_flags(a_b) == SERD_HAS_QUOTE);
  assert(strlen(serd_node_string(a_b)) == 3);
  assert(!strncmp(serd_node_string(a_b), "a\"b", 3));
  serd_node_free(a_b);
}

static void
test_simple_node(void)
{
  assert(!serd_new_simple_node(SERD_LITERAL, SERD_STATIC_STRING("Literal")));
}

static void
test_literal(void)
{
  SerdNode* hello2 = serd_new_string(SERD_STATIC_STRING("hello\""));

  assert(serd_node_length(hello2) == 6 &&
         serd_node_flags(hello2) == SERD_HAS_QUOTE &&
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
  assert(serd_node_length(blank) == 2);
  assert(serd_node_flags(blank) == 0);
  assert(!strcmp(serd_node_string(blank), "b0"));
  serd_node_free(blank);
}

int
main(void)
{
  test_integer_to_node();
  test_blob_to_node();
  test_node_equals();
  test_node_from_string();
  test_node_from_substring();
  test_simple_node();
  test_literal();
  test_blank();

  printf("Success\n");
  return 0;
}
