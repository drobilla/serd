// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "base64.h"
#include "macros.h"

#include <serd/node.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct SerdNodeImpl {
  size_t        n_bytes; ///< Size in bytes (not including null)
  SerdNodeFlags flags;   ///< Node flags (e.g. string properties)
  SerdNodeType  type;    ///< Node type
};

SerdTokenView
serd_node_token_view(const SerdNode* const node)
{
  const SerdTokenView view = {node->type, serd_node_string_view(node)};
  return view;
}

static SerdStreamResult
string_sink(void* const stream, const size_t len, const void* const buf)
{
  char** const ptr = (char**)stream;
  memcpy(*ptr, buf, len);
  *ptr += len;
  const SerdStreamResult r = {SERD_SUCCESS, len};
  return r;
}

SerdNode
serd_node_from_string(const SerdNodeType type, const char* const str)
{
  if (!str) {
    return SERD_NODE_NULL;
  }

  const size_t   n_bytes = strlen(str);
  const SerdNode ret     = {str, n_bytes, 0U, type};
  return ret;
}

SerdNode
serd_node_from_substring(const SerdNodeType type,
                         const char* const  str,
                         const size_t       len)
{
  if (!str) {
    return SERD_NODE_NULL;
  }

  const size_t   n_bytes = MIN(len, strlen(str));
  const SerdNode ret     = {str, n_bytes, 0U, type};
  return ret;
}

SerdNode
serd_node_copy(ZixAllocator* const allocator, const SerdNode* const node)
{
  if (!node || !node->buf) {
    return SERD_NODE_NULL;
  }

  SerdNode    copy = SERD_NODE_NULL;
  char* const buf  = (char*)zix_malloc(allocator, node->n_bytes + 1U);
  if (buf) {
    copy     = *node;
    copy.buf = buf;
    memcpy(buf, node->buf, node->n_bytes + 1);
  }
  return copy;
}

bool
serd_node_equals(const SerdNode* const a, const SerdNode* const b)
{
  assert(a);
  assert(b);

  return (a == b) ||
         (a->type == b->type && a->n_bytes == b->n_bytes &&
          ((a->buf == b->buf) || !memcmp(a->buf, b->buf, a->n_bytes + 1)));
}

SerdNode
serd_node_new_uri_from_node(ZixAllocator* const   allocator,
                            const SerdNode* const uri_node)
{
  assert(uri_node);

  return (uri_node->type == SERD_URI && uri_node->buf)
           ? serd_node_new_uri_from_string(allocator, uri_node->buf)
           : SERD_NODE_NULL;
}

SerdNode
serd_node_new_uri_from_string(ZixAllocator* const allocator,
                              const char* const   str)
{
  if (!str || str[0] == '\0') {
    // Empty URI => Base URI, or nothing if no base is given
    return SERD_NODE_NULL;
  }

  const SerdURIView uri = serd_parse_uri(str);
  return serd_node_new_uri(allocator, &uri);
}

SerdNode
serd_node_new_uri(ZixAllocator* const allocator, const SerdURIView* const uri)
{
  assert(uri);

  const size_t len  = serd_uri_string_length(*uri);
  char*        buf  = (char*)zix_malloc(allocator, len + 1);
  SerdNode     node = {buf, len, 0, SERD_URI};
  char*        ptr  = buf;

  const SerdStreamResult r = serd_write_uri(*uri, string_sink, &ptr);

  buf[r.count] = '\0';
  node.n_bytes = r.count;
  return node;
}

static unsigned
serd_digits(const double abs)
{
  const double lg = ceil(log10(floor(abs) + 1.0));
  return lg < 1.0 ? 1U : (unsigned)lg;
}

SerdNode
serd_node_new_decimal(ZixAllocator* const allocator,
                      const double        d,
                      const unsigned      frac_digits)
{
  if (isnan(d) || isinf(d)) {
    return SERD_NODE_NULL;
  }

  const double   abs_d      = fabs(d);
  const unsigned int_digits = serd_digits(abs_d);
  const unsigned max_digits = int_digits + frac_digits + 2U;

  char* const  buf      = (char*)zix_calloc(allocator, max_digits + 1U, 1);
  SerdNode     node     = {buf, 0, 0, SERD_LITERAL};
  const double int_part = floor(abs_d);

  // Point s to decimal point location
  char* s = buf + int_digits;
  if (d < 0.0) {
    *buf = '-';
    ++s;
  }

  // Write integer part (right to left)
  char*    t   = s - 1;
  uint64_t dec = (uint64_t)int_part;
  do {
    *t-- = (char)('0' + (dec % 10));
  } while ((dec /= 10) > 0);

  *s++ = '.';

  // Write fractional part (right to left)
  double frac_part = fabs(d - int_part);
  if (frac_part < DBL_EPSILON) {
    *s++         = '0';
    node.n_bytes = (size_t)(s - buf);
  } else {
    uint64_t frac = (uint64_t)llround(frac_part * pow(10.0, (int)frac_digits));
    s += frac_digits - 1;
    unsigned i = 0;

    // Skip trailing zeros
    for (; i < frac_digits - 1 && !(frac % 10); ++i, --s, frac /= 10) {
    }

    node.n_bytes = (size_t)(s - buf) + 1U;

    // Write digits from last trailing zero to decimal point
    for (; i < frac_digits; ++i) {
      *s-- = (char)('0' + (frac % 10));
      frac /= 10;
    }
  }

  return node;
}

SerdNode
serd_node_new_integer(ZixAllocator* const allocator, const int64_t i)
{
  uint64_t       abs_i  = (uint64_t)((i < 0) ? -i : i);
  const unsigned digits = serd_digits((double)abs_i);
  char* const    buf    = (char*)zix_calloc(allocator, digits + 2, 1);
  SerdNode       node   = {buf, 0, 0, SERD_LITERAL};

  // Point s to the end
  char* s = buf + digits - 1;
  if (i < 0) {
    *buf = '-';
    ++s;
  }

  node.n_bytes = (size_t)(s - buf) + 1U;

  // Write integer part (right to left)
  do {
    *s-- = (char)('0' + (abs_i % 10));
  } while ((abs_i /= 10) > 0);

  return node;
}

SerdNode
serd_node_new_blob(ZixAllocator* const allocator,
                   const void* const   buf,
                   const size_t        size,
                   const bool          wrap_lines)
{
  assert(buf);

  const size_t len  = serd_base64_get_length(size, wrap_lines);
  char* const  str  = (char*)zix_calloc(allocator, len + 2, 1);
  SerdNode     node = {str, len, 0, SERD_LITERAL};

  serd_base64_encode((uint8_t*)str, buf, size, wrap_lines);

  return node;
}

void
serd_node_free(ZixAllocator* const allocator, SerdNode* const node)
{
  if (node && node->buf) {
    zix_free(allocator, (char*)node->buf);
    node->buf = NULL;
  }
}

ZixStringView
serd_node_string_view(const SerdNode* const node)
{
  assert(node);
  assert(node->buf);
  const ZixStringView r = {(const char*)node->buf, node->n_bytes};
  return r;
}
