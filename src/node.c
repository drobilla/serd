// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "base64.h"
#include "string_utils.h"

#include "serd/buffer.h"
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

SerdNode
serd_node_from_string(const SerdNodeType type, const char* const str)
{
  if (!str) {
    return SERD_NODE_NULL;
  }

  SerdNodeFlags  flags   = 0;
  const size_t   n_bytes = serd_strlen(str, &flags);
  const SerdNode ret     = {str, n_bytes, flags, type};
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

  SerdNodeFlags  flags   = 0;
  const size_t   n_bytes = serd_substrlen(str, len, &flags);
  const SerdNode ret     = {str, n_bytes, flags, type};
  return ret;
}

SerdNode
serd_node_copy(const SerdNode* const node)
{
  if (!node || !node->buf) {
    return SERD_NODE_NULL;
  }

  SerdNode copy = *node;
  char*    buf  = (char*)malloc(copy.n_bytes + 1);
  memcpy(buf, node->buf, copy.n_bytes + 1);
  copy.buf = buf;
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
serd_new_uri_from_node(const SerdNode* const    uri_node,
                       const SerdURIView* const base,
                       SerdURIView* const       out)
{
  assert(uri_node);

  return (uri_node->type == SERD_URI && uri_node->buf)
           ? serd_new_uri_from_string(uri_node->buf, base, out)
           : SERD_NODE_NULL;
}

SerdNode
serd_new_uri_from_string(const char* const        str,
                         const SerdURIView* const base,
                         SerdURIView* const       out)
{
  if (!str || str[0] == '\0') {
    // Empty URI => Base URI, or nothing if no base is given
    return base ? serd_new_uri(base, NULL, out) : SERD_NODE_NULL;
  }

  SerdURIView uri = serd_parse_uri(str);
  return serd_new_uri(&uri, base, out); // Resolve/Serialise
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

SerdNode
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

  const char* const string = serd_buffer_sink_finish(&buffer);
  if (string && out) {
    *out = serd_parse_uri(string);
  }

  return serd_node_from_substring(SERD_URI, string, buffer.len);
}

SerdNode
serd_new_uri(const SerdURIView* const uri,
             const SerdURIView* const base,
             SerdURIView* const       out)
{
  assert(uri);

  SerdURIView abs_uri = *uri;
  if (base) {
    abs_uri = serd_resolve_uri(*uri, *base);
  }

  const size_t len        = serd_uri_string_length(abs_uri);
  char*        buf        = (char*)malloc(len + 1);
  SerdNode     node       = {buf, len, 0, SERD_URI};
  char*        ptr        = buf;
  const size_t actual_len = serd_write_uri(abs_uri, string_sink, &ptr);

  buf[actual_len] = '\0';
  node.n_bytes    = actual_len;

  if (out) {
    *out = serd_parse_uri(buf); // TODO: cleverly avoid double parse
  }

  return node;
}

static unsigned
serd_digits(const double abs)
{
  const double lg = ceil(log10(floor(abs) + 1.0));
  return lg < 1.0 ? 1U : (unsigned)lg;
}

SerdNode
serd_new_decimal(const double d, const unsigned frac_digits)
{
  if (isnan(d) || isinf(d)) {
    return SERD_NODE_NULL;
  }

  const double   abs_d      = fabs(d);
  const unsigned int_digits = serd_digits(abs_d);
  char*          buf        = (char*)calloc(int_digits + frac_digits + 3, 1);
  SerdNode       node       = {buf, 0, 0, SERD_LITERAL};
  const double   int_part   = floor(abs_d);

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
serd_new_integer(const int64_t i)
{
  uint64_t       abs_i  = (uint64_t)((i < 0) ? -i : i);
  const unsigned digits = serd_digits((double)abs_i);
  char*          buf    = (char*)calloc(digits + 2, 1);
  SerdNode       node   = {(const char*)buf, 0, 0, SERD_LITERAL};

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
serd_new_blob(const void* const buf, const size_t size, const bool wrap_lines)
{
  assert(buf);

  const size_t len  = serd_base64_get_length(size, wrap_lines);
  char* const  str  = (char*)calloc(len + 2, 1);
  SerdNode     node = {str, len, 0, SERD_LITERAL};

  if (serd_base64_encode((uint8_t*)str, buf, size, wrap_lines)) {
    node.flags |= SERD_HAS_NEWLINE;
  }

  return node;
}

void
serd_node_free(SerdNode* const node)
{
  if (node && node->buf) {
    free((char*)node->buf);
    node->buf = NULL;
  }
}
