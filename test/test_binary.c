// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/binary.h>

#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <serd/token_view.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <string.h>

#define NS_EG "http://example.org/"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

static void
check_decode(const char* const encoded,
             const char* const datatype_uri,
             const char* const expected)
{
  const SerdObjectView node =
    serd_object_view(SERD_LITERAL,
                     zix_string(encoded),
                     SERD_HAS_DATATYPE,
                     serd_token_view(SERD_URI, zix_string(datatype_uri)));

  const size_t max_size = serd_binary_decoded_size(node);
  char* const  decoded  = (char*)zix_calloc(NULL, 1, max_size + 1);

  const SerdStreamResult r = serd_binary_decode(node, max_size, decoded);
  assert(!r.status);
  assert(r.count <= max_size);

  assert(!strcmp(decoded, expected));
  assert(strlen(decoded) <= max_size);

  zix_free(NULL, decoded);
}

static void
test_decode_hex(void)
{
  // Test decoding clean hex
  check_decode("74657374", NS_XSD "hexBinary", "test");

  // Test decoding effectively nothing
  check_decode("\t\n \n", NS_XSD "hexBinary", "");

  // Test decoding various lengths
  check_decode("6368756E6B", NS_XSD "hexBinary", "chunk");
  check_decode("6C6574746572", NS_XSD "hexBinary", "letter");
  check_decode("6E6F7465", NS_XSD "hexBinary", "note");

  // Test ignoring whitespace characters
  check_decode(" \n\r\t666F6F \n\r\t", NS_XSD "hexBinary", "foo");
}

static void
test_decode_base64(void)
{
  // Test decoding clean base64
  check_decode("dGVzdA==", NS_XSD "base64Binary", "test");

  // Test decoding equivalent dirty base64 with ignored whitespace characters
  check_decode("d G\nV\rz\td A\n= =", NS_XSD "base64Binary", "test");

  // Test decoding effectively nothing
  check_decode("\t\n \n", NS_XSD "base64Binary", "");

  // Test decoding various lengths
  check_decode("Y2h1bms=", NS_XSD "base64Binary", "chunk");
  check_decode("bGV0dGVy", NS_XSD "base64Binary", "letter");
  check_decode("bm90ZQ==", NS_XSD "base64Binary", "note");

  // Test ignoring whitespace characters
  check_decode(" \n\r\tZm9v \n\r\t", NS_XSD "base64Binary", "foo");
}

static void
test_decode_no_space(void)
{
  const SerdObjectView blob = serd_object_view(
    SERD_LITERAL,
    zix_string("Zm9v"),
    SERD_HAS_DATATYPE,
    serd_token_view(SERD_LITERAL, zix_string(NS_XSD "base64Binary")));

  char                   small[2] = {0};
  const SerdStreamResult r = serd_binary_decode(blob, sizeof(small), small);

  assert(r.status == SERD_NO_SPACE);
}

static void
test_decode_no_datatype(void)
{
  const SerdObjectView string =
    serd_object_view(SERD_LITERAL, zix_string("Zm9v"), 0U, serd_no_token());

  assert(serd_binary_decoded_size(string) == 0U);

  char                   small[2] = {0};
  const SerdStreamResult r = serd_binary_decode(string, sizeof(small), small);
  assert(r.status == SERD_BAD_ARG);
  assert(r.count == 0U);
}

static void
test_decode_unknown_datatype(void)
{
  const SerdObjectView unknown = serd_object_view(
    SERD_LITERAL,
    zix_string("secret"),
    SERD_HAS_DATATYPE,
    serd_token_view(SERD_LITERAL, zix_string(NS_EG "Datatype")));

  assert(serd_binary_decoded_size(unknown) == 0U);

  char                   small[2] = {0};
  const SerdStreamResult r = serd_binary_decode(unknown, sizeof(small), small);
  assert(r.status == SERD_BAD_ARG);
  assert(r.count == 0U);
}

int
main(void)
{
  test_decode_hex();
  test_decode_base64();
  test_decode_no_space();
  test_decode_no_datatype();
  test_decode_unknown_datatype();
  return 0;
}

#undef NS_XSD
#undef NS_EG
