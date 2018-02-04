// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "base64.h"
#include "node_impl.h"
#include "string_utils.h"
#include "system.h"

#include "serd/buffer.h"
#include "serd/node.h"
#include "serd/string.h"
#include "serd/uri.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  ifndef isnan
#    define isnan(x) _isnan(x)
#  endif
#  ifndef isinf
#    define isinf(x) (!_finite(x))
#  endif
#endif

static size_t
string_sink(const void* const buf, const size_t len, void* const stream)
{
  char** ptr = (char**)stream;
  memcpy(*ptr, buf, len);
  *ptr += len;
  return len;
}

char*
serd_node_buffer(SerdNode* const node)
{
  return (char*)(node + 1);
}

ZIX_CONST_FUNC static size_t
serd_node_pad_length(const size_t length)
{
  const size_t terminated = length + 1U;
  const size_t padded     = (terminated + 3U) & ~0x03U;
  assert(padded % 4U == 0U);
  return padded;
}

static ZIX_PURE_FUNC size_t
serd_node_total_size(const SerdNode* const node)
{
  return node ? (sizeof(SerdNode) + serd_node_pad_length(node->length)) : 0;
}

static SerdNode*
serd_node_malloc(const size_t        max_length,
                 const SerdNodeFlags flags,
                 const SerdNodeType  type)
{
  const size_t    size = sizeof(SerdNode) + serd_node_pad_length(max_length);
  SerdNode* const node = (SerdNode*)calloc(1U, size);

  node->flags = flags;
  node->type  = type;

  return node;
}

void
serd_node_set_header(SerdNode* const     node,
                     const size_t        length,
                     const SerdNodeFlags flags,
                     const SerdNodeType  type)
{
  node->length = length;
  node->flags  = flags;
  node->type   = type;
}

void
serd_node_set(SerdNode** const dst, const SerdNode* const src)
{
  assert(dst);
  assert(src);

  const size_t size = serd_node_total_size(src);
  if (!*dst || serd_node_total_size(*dst) < size) {
    serd_free_aligned(*dst);
    *dst = (SerdNode*)calloc(1U, size);
  }

  assert(*dst);
  memcpy(*dst, src, sizeof(SerdNode) + src->length + 1);
}

SerdNode*
serd_new_string(SerdNodeType type, const char* str)
{
  SerdNodeFlags flags  = 0;
  const size_t  length = serd_strlen(str, &flags);
  SerdNode*     node   = serd_node_malloc(length, flags, type);
  memcpy(serd_node_buffer(node), str, length);
  node->length = length;
  return node;
}

SerdNode*
serd_new_substring(const SerdNodeType type,
                   const char* const  str,
                   const size_t       len)
{
  SerdNodeFlags flags  = 0;
  const size_t  length = serd_substrlen(str, len, &flags);
  SerdNode*     node   = serd_node_malloc(length, flags, type);
  memcpy(serd_node_buffer(node), str, length);
  node->length = length;
  return node;
}

SerdNode*
serd_new_expanded_uri(const ZixStringView prefix, const ZixStringView suffix)
{
  const size_t    length = prefix.length + suffix.length;
  SerdNode* const node   = serd_node_malloc(length, 0, SERD_URI);
  char* const     buffer = serd_node_buffer(node);

  memcpy(buffer, prefix.data, prefix.length);
  memcpy(buffer + prefix.length, suffix.data, suffix.length);
  node->length = length;
  return node;
}

SerdNode*
serd_node_copy(const SerdNode* node)
{
  if (!node) {
    return NULL;
  }

  const size_t    size = serd_node_total_size(node);
  SerdNode* const copy = (SerdNode*)calloc(1U, size);
  memcpy(copy, node, size);
  return copy;
}

bool
serd_node_equals(const SerdNode* const a, const SerdNode* const b)
{
  return (a == b) ||
         (a && b && a->type == b->type && a->length == b->length &&
          !memcmp(serd_node_string(a), serd_node_string(b), a->length));
}

SerdNode*
serd_new_uri(const char* const str)
{
  assert(str);

  const size_t length = strlen(str);
  SerdNode*    node   = serd_node_malloc(length, 0, SERD_URI);
  memcpy(serd_node_buffer(node), str, length);
  node->length = length;
  return node;
}

SerdNode*
serd_new_parsed_uri(const SerdURIView uri)
{
  const size_t    len        = serd_uri_string_length(uri);
  SerdNode* const node       = serd_node_malloc(len, 0, SERD_URI);
  char*           ptr        = serd_node_buffer(node);
  const size_t    actual_len = serd_write_uri(uri, string_sink, &ptr);

  assert(actual_len == len);

  serd_node_buffer(node)[actual_len] = '\0';
  node->length                       = actual_len;

  return node;
}

SerdNode*
serd_new_resolved_uri(const ZixStringView string, const SerdURIView base)
{
  const SerdURIView uri     = serd_parse_uri(string.data);
  const SerdURIView abs_uri = serd_resolve_uri(uri, base);
  SerdNode* const   result  = serd_new_parsed_uri(abs_uri);

  if (!serd_uri_string_has_scheme(serd_node_string(result))) {
    serd_node_free(result);
    return NULL;
  }

  return result;
}

static bool
is_uri_path_char(const char c)
{
  return is_alpha(c) || is_digit(c) || strchr("!$&\'()*+,-./:;=@_~", c);
}

static bool
is_dir_sep(const char c)
{
#ifdef _WIN32
  return c == '\\' || c == '/';
#else
  return c == '/';
#endif
}

SerdNode*
serd_new_file_uri(const char* const  path,
                  const char* const  hostname,
                  SerdURIView* const out)
{
  assert(path);

  const size_t path_len     = strlen(path);
  const size_t hostname_len = hostname ? strlen(hostname) : 0;
  const bool   is_windows   = is_windows_path(path);
  size_t       uri_len      = 0;
  char*        uri          = NULL;

  if (is_dir_sep(path[0]) || is_windows) {
    uri_len = strlen("file://") + hostname_len + is_windows;
    uri     = (char*)calloc(uri_len + 1, 1);

    memcpy(uri, "file://", 7);

    if (hostname) {
      memcpy(uri + 7, hostname, hostname_len + 1);
    }

    if (is_windows) {
      uri[7 + hostname_len] = '/';
    }
  }

  SerdBuffer buffer = {uri, uri_len};
  for (size_t i = 0; i < path_len; ++i) {
    if (is_uri_path_char(path[i])) {
      serd_buffer_sink(path + i, 1, &buffer);
#ifdef _WIN32
    } else if (path[i] == '\\') {
      serd_buffer_sink("/", 1, &buffer);
#endif
    } else {
      char escape_str[10] = {'%', 0, 0, 0, 0, 0, 0, 0, 0, 0};
      snprintf(escape_str + 1, sizeof(escape_str) - 1, "%X", (unsigned)path[i]);
      serd_buffer_sink(escape_str, 3, &buffer);
    }
  }

  const size_t      length = buffer.len;
  const char* const string = serd_buffer_sink_finish(&buffer);
  SerdNode* const   node   = serd_new_substring(SERD_URI, string, length);
  if (out) {
    *out = serd_parse_uri(serd_node_buffer(node));
  }

  free(buffer.buf);
  return node;
}

static unsigned
serd_digits(const double abs)
{
  const double lg = ceil(log10(floor(abs) + 1.0));
  return lg < 1.0 ? 1U : (unsigned)lg;
}

SerdNode*
serd_new_decimal(const double d, const unsigned frac_digits)
{
  if (isnan(d) || isinf(d)) {
    return NULL;
  }

  const double    abs_d      = fabs(d);
  const unsigned  int_digits = serd_digits(abs_d);
  const size_t    len        = int_digits + frac_digits + 3;
  SerdNode* const node       = serd_node_malloc(len, 0, SERD_LITERAL);
  char* const     buf        = serd_node_buffer(node);
  const double    int_part   = floor(abs_d);

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

  return node;
}

SerdNode*
serd_new_integer(const int64_t i)
{
  uint64_t       abs_i  = (uint64_t)((i < 0) ? -i : i);
  const unsigned digits = serd_digits((double)abs_i);
  SerdNode*      node   = serd_node_malloc(digits + 2, 0, SERD_LITERAL);
  char*          buf    = serd_node_buffer(node);

  // Point s to the end
  char* s = buf + digits - 1;
  if (i < 0) {
    *buf = '-';
    ++s;
  }

  node->length = (size_t)(s - buf) + 1U;

  // Write integer part (right to left)
  do {
    *s-- = (char)('0' + (abs_i % 10));
  } while ((abs_i /= 10) > 0);

  return node;
}

SerdNode*
serd_new_blob(const void* const buf, const size_t size, const bool wrap_lines)
{
  assert(buf);

  if (!size) {
    return NULL;
  }

  const size_t    len  = serd_base64_get_length(size, wrap_lines);
  SerdNode* const node = serd_node_malloc(len + 1, 0, SERD_LITERAL);
  uint8_t* const  str  = (uint8_t*)serd_node_buffer(node);

  if (serd_base64_encode(str, buf, size, wrap_lines)) {
    node->flags |= SERD_HAS_NEWLINE;
  }

  node->length = len;
  return node;
}

void
serd_node_free(SerdNode* const node)
{
  serd_free_aligned(node);
}

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
  return (const char*)(node + 1);
}

ZixStringView
serd_node_string_view(const SerdNode* const node)
{
  assert(node);
  const ZixStringView r = {(const char*)(node + 1), node->length};
  return r;
}

ZIX_PURE_FUNC SerdURIView
serd_node_uri_view(const SerdNode* const node)
{
  assert(node);
  return (node->type == SERD_URI) ? serd_parse_uri(serd_node_string(node))
                                  : SERD_URI_NULL;
}
