// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "base64.h"
#include "namespaces.h"
#include "node_impl.h"
#include "string_utils.h"

#include "serd/buffer.h"
#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/string.h"
#include "serd/uri.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#  ifndef isnan
#    define isnan(x) _isnan(x)
#  endif
#  ifndef isinf
#    define isinf(x) (!_finite(x))
#  endif
#endif

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

typedef struct StaticNode {
  SerdNode node;
  char     buf[sizeof(NS_XSD "base64Binary")];
} StaticNode;

#define DEFINE_XSD_NODE(name)                 \
  static const StaticNode serd_xsd_##name = { \
    {NULL, sizeof(NS_XSD #name) - 1U, 0U, SERD_URI}, NS_XSD #name};

DEFINE_XSD_NODE(base64Binary)
DEFINE_XSD_NODE(boolean)
DEFINE_XSD_NODE(decimal)
DEFINE_XSD_NODE(integer)

static const SerdNodeFlags meta_mask = (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE);

static size_t
string_sink(const void* const buf,
            const size_t      size,
            const size_t      nmemb,
            void* const       stream)
{
  char** ptr = (char**)stream;
  memcpy(*ptr, buf, size * nmemb);
  *ptr += size * nmemb;
  return nmemb;
}

char*
serd_node_buffer(SerdNode* const node)
{
  return (char*)(node + 1U);
}

size_t
serd_node_pad_length(const size_t length)
{
  const size_t terminated = length + 1U;
  const size_t padded     = (terminated + 3U) & ~0x03U;
  assert(padded % 4U == 0U);
  return padded;
}

const SerdNode*
serd_node_meta(const SerdNode* const node)
{
  return node->meta;
}

static ZIX_PURE_FUNC size_t
serd_node_total_size(const SerdNode* const node)
{
  return node ? (sizeof(SerdNode) + serd_node_pad_length(node->length)) : 0U;
}

static SerdNode*
serd_node_malloc(ZixAllocator* const allocator,
                 const size_t        max_length,
                 const SerdNodeFlags flags,
                 const SerdNodeType  type)
{
  const size_t    size = sizeof(SerdNode) + serd_node_pad_length(max_length);
  SerdNode* const node = (SerdNode*)zix_calloc(allocator, 1U, size);

  if (node) {
    node->flags = flags;
    node->type  = type;
  }

  return node;
}

void
serd_node_set_header(SerdNode* const     node,
                     const size_t        length,
                     const SerdNodeFlags flags,
                     const SerdNodeType  type)
{
  node->meta   = NULL;
  node->length = length;
  node->flags  = flags;
  node->type   = type;
}

SerdStatus
serd_node_set(ZixAllocator* const   allocator,
              SerdNode** const      dst,
              const SerdNode* const src)
{
  assert(dst);
  assert(src);

  const size_t size = serd_node_total_size(src);
  if (!*dst || serd_node_total_size(*dst) < size) {
    zix_aligned_free(allocator, *dst);
    if (!(*dst = (SerdNode*)zix_calloc(allocator, 1U, size))) {
      return SERD_BAD_ALLOC;
    }
  }

  assert(*dst);
  memcpy(*dst, src, size);
  return SERD_SUCCESS;
}

// Dynamic allocation

SerdNode*
serd_new_token(ZixAllocator* const allocator,
               const SerdNodeType  type,
               const ZixStringView str)
{
  SerdNodeFlags flags  = 0U;
  const size_t  length = str.data ? str.length : 0U;
  SerdNode*     node   = serd_node_malloc(allocator, length, flags, type);

  if (node) {
    if (str.data) {
      memcpy(serd_node_buffer(node), str.data, length);
    }

    node->length = length;
  }

  return node;
}

SerdNode*
serd_new_string(ZixAllocator* const allocator, const ZixStringView str)
{
  SerdNodeFlags flags  = 0;
  const size_t  length = serd_substrlen(str.data, str.length, &flags);
  SerdNode*     node = serd_node_malloc(allocator, length, flags, SERD_LITERAL);

  if (node) {
    node->length = length;
    memcpy(serd_node_buffer(node), str.data, str.length);
    serd_node_buffer(node)[str.length] = '\0';
  }

  return node;
}

/// Internal pre-measured implementation of serd_new_plain_literal
static SerdNode*
serd_new_plain_literal_i(ZixAllocator* const   allocator,
                         const ZixStringView   str,
                         SerdNodeFlags         flags,
                         const SerdNode* const lang)
{
  assert(str.length);
  assert(serd_node_length(lang));

  flags |= SERD_HAS_LANGUAGE;

  SerdNode* const node =
    serd_node_malloc(allocator, str.length, flags, SERD_LITERAL);

  memcpy(serd_node_buffer(node), str.data, str.length);
  node->meta   = lang;
  node->length = str.length;

  return node;
}

SerdNode*
serd_new_plain_literal(ZixAllocator* const   allocator,
                       const ZixStringView   str,
                       const SerdNode* const lang)
{
  if (!lang) {
    return serd_new_string(allocator, str);
  }

  if (serd_node_type(lang) != SERD_LITERAL) {
    return NULL;
  }

  SerdNodeFlags flags = 0;
  serd_strlen(str.data, &flags);

  return serd_new_plain_literal_i(allocator, str, flags, lang);
}

SerdNode*
serd_new_typed_literal(ZixAllocator* const   allocator,
                       const ZixStringView   str,
                       const SerdNode* const datatype_uri)
{
  if (!datatype_uri) {
    return serd_new_string(allocator, str);
  }

  if (serd_node_type(datatype_uri) != SERD_URI ||
      !strcmp(serd_node_string(datatype_uri), NS_RDF "langString")) {
    return NULL;
  }

  SerdNodeFlags flags = 0U;
  serd_strlen(str.data, &flags);

  flags |= SERD_HAS_DATATYPE;

  SerdNode* const node =
    serd_node_malloc(allocator, str.length, flags, SERD_LITERAL);

  memcpy(serd_node_buffer(node), str.data, str.length);
  node->meta   = datatype_uri;
  node->length = str.length;

  return node;
}

SerdNode*
serd_new_blank(ZixAllocator* const allocator, const ZixStringView str)
{
  return serd_new_token(allocator, SERD_BLANK, str);
}

SerdNode*
serd_new_curie(ZixAllocator* const allocator, const ZixStringView str)
{
  return serd_new_token(allocator, SERD_CURIE, str);
}

SerdNode*
serd_new_uri(ZixAllocator* const allocator, const ZixStringView string)
{
  return serd_new_token(allocator, SERD_URI, string);
}

SerdNode*
serd_new_parsed_uri(ZixAllocator* const allocator, const SerdURIView uri)
{
  const size_t    len  = serd_uri_string_length(uri);
  SerdNode* const node = serd_node_malloc(allocator, len, 0, SERD_URI);

  if (node) {
    char*        ptr        = serd_node_buffer(node);
    const size_t actual_len = serd_write_uri(uri, string_sink, &ptr);

    assert(actual_len == len);

    serd_node_buffer(node)[actual_len] = '\0';
    node->length                       = actual_len;
  }

  return node;
}

SerdNode*
serd_new_file_uri(ZixAllocator* const allocator,
                  const ZixStringView path,
                  const ZixStringView hostname)
{
  SerdBuffer       buffer = {NULL, NULL, 0U};
  SerdOutputStream out    = serd_open_output_buffer(&buffer);

  serd_write_file_uri(path, hostname, out.write, out.stream);
  serd_close_output(&out);

  const size_t      length = buffer.len;
  const char* const string = (char*)buffer.buf;
  if (!string) {
    return NULL;
  }

  SerdNode* const node =
    serd_new_string(allocator, zix_substring(string, length));

  zix_free(buffer.allocator, buffer.buf);
  return node;
}

static unsigned
serd_digits(const double abs)
{
  const double lg = ceil(log10(floor(abs) + 1.0));
  return lg < 1.0 ? 1U : (unsigned)lg;
}

SerdNode*
serd_new_boolean(ZixAllocator* const allocator, bool b)
{
  static const ZixStringView true_string  = ZIX_STATIC_STRING("true");
  static const ZixStringView false_string = ZIX_STATIC_STRING("false");

  return serd_new_typed_literal(
    allocator, b ? true_string : false_string, &serd_xsd_boolean.node);
}

SerdNode*
serd_new_decimal(ZixAllocator* const allocator,
                 const double        d,
                 const unsigned      frac_digits)
{
  static const SerdNode* const datatype = &serd_xsd_decimal.node;

  if (isnan(d) || isinf(d)) {
    return NULL;
  }

  const double   abs_d      = fabs(d);
  const unsigned int_digits = serd_digits(abs_d);
  const size_t   max_len    = int_digits + frac_digits + 3;

  SerdNode* const node =
    serd_node_malloc(allocator, max_len, SERD_HAS_DATATYPE, SERD_LITERAL);

  // Point s to decimal point location
  char* const  buf      = serd_node_buffer(node);
  const double int_part = floor(abs_d);
  char*        s        = buf + int_digits;
  if (d < 0.0) {
    *buf = '-';
    ++s;
  }

  // Write integer part (right to left)
  char*    t   = s - 1;
  uint64_t dec = (uint64_t)int_part;
  do {
    *t-- = (char)('0' + dec % 10);
  } while ((dec /= 10) > 0);

  *s++ = '.';

  // Write fractional part (right to left)
  double frac_part = fabs(d - int_part);
  if (frac_part < DBL_EPSILON) {
    *s++         = '0';
    node->length = (size_t)(s - buf);
  } else {
    uint64_t frac = (uint64_t)llround(frac_part * pow(10.0, (int)frac_digits));
    s += frac_digits - 1;
    unsigned i = 0;

    // Skip trailing zeros
    for (; i < frac_digits - 1 && !(frac % 10); ++i, --s, frac /= 10) {
    }

    node->length = (size_t)(s - buf) + 1U;

    // Write digits from last trailing zero to decimal point
    for (; i < frac_digits; ++i) {
      *s-- = (char)('0' + (frac % 10));
      frac /= 10;
    }
  }

  node->meta = datatype;
  return node;
}

SerdNode*
serd_new_integer(ZixAllocator* const allocator, const int64_t i)
{
  uint64_t       abs_i   = (uint64_t)((i < 0) ? -i : i);
  const unsigned digits  = serd_digits((double)abs_i);
  const size_t   max_len = digits + 1U;

  SerdNode* const node =
    serd_node_malloc(allocator, max_len, SERD_HAS_DATATYPE, SERD_LITERAL);

  // Point s to the end
  char* buf = serd_node_buffer(node);
  char* s   = buf + digits - 1;
  if (i < 0) {
    *buf = '-';
    ++s;
  }

  node->meta   = &serd_xsd_integer.node;
  node->length = (size_t)(s - buf) + 1U;

  // Write integer part (right to left)
  do {
    *s-- = (char)('0' + (abs_i % 10));
  } while ((abs_i /= 10) > 0);

  return node;
}

SerdNode*
serd_new_blob(ZixAllocator* const allocator,
              const void* const   buf,
              const size_t        size,
              const bool          wrap_lines)
{
  assert(buf);

  if (!size) {
    return NULL;
  }

  const size_t    len = serd_base64_get_length(size, wrap_lines);
  SerdNode* const node =
    serd_node_malloc(allocator, len, SERD_HAS_DATATYPE, SERD_LITERAL);

  uint8_t* const str = (uint8_t*)serd_node_buffer(node);
  if (serd_base64_encode(str, buf, size, wrap_lines)) {
    node->flags |= SERD_HAS_NEWLINE;
  }

  node->meta   = &serd_xsd_base64Binary.node;
  node->length = len;
  return node;
}

SerdNode*
serd_node_copy(ZixAllocator* const allocator, const SerdNode* node)
{
  if (!node) {
    return NULL;
  }

  const size_t size = serd_node_total_size(node);
  SerdNode*    copy = (SerdNode*)zix_calloc(allocator, 1U, size);

  if (copy) {
    memcpy(copy, node, size);
  }

  return copy;
}

void
serd_node_free(ZixAllocator* const allocator, SerdNode* const node)
{
  zix_aligned_free(allocator, node);
}

// Operators

bool
serd_node_equals(const SerdNode* const a, const SerdNode* const b)
{
  if (a == b) {
    return true;
  }

  if (!a || !b || a->length != b->length || a->flags != b->flags ||
      a->type != b->type) {
    return false;
  }

  const size_t length = a->length;
  if (!!memcmp(serd_node_string(a), serd_node_string(b), length)) {
    return false;
  }

  const SerdNodeFlags flags = a->flags;
  if (flags & meta_mask) {
    const SerdNode* const am = serd_node_meta(a);
    const SerdNode* const bm = serd_node_meta(b);

    return am->length == bm->length && am->type == bm->type &&
           !memcmp(serd_node_string(am), serd_node_string(bm), am->length);
  }

  return true;
}

// Accessors

SerdNodeType
serd_node_type(const SerdNode* const node)
{
  assert(node);
  return node->type;
}

SerdNodeFlags
serd_node_flags(const SerdNode* const node)
{
  assert(node);
  return node->flags;
}

size_t
serd_node_length(const SerdNode* const node)
{
  assert(node);
  return node->length;
}

const char*
serd_node_string(const SerdNode* const node)
{
  assert(node);
  return (const char*)(node + 1U);
}

ZixStringView
serd_node_string_view(const SerdNode* const node)
{
  assert(node);
  const ZixStringView r = {(const char*)(node + 1U), node->length};
  return r;
}

ZIX_PURE_FUNC SerdURIView
serd_node_uri_view(const SerdNode* const node)
{
  assert(node);
  return (node->type == SERD_URI) ? serd_parse_uri(serd_node_string(node))
                                  : SERD_URI_NULL;
}

const SerdNode*
serd_node_datatype(const SerdNode* const node)
{
  assert(node);
  return (node->flags & SERD_HAS_DATATYPE) ? node->meta : NULL;
}

const SerdNode*
serd_node_language(const SerdNode* const node)
{
  assert(node);
  return (node->flags & SERD_HAS_LANGUAGE) ? node->meta : NULL;
}
