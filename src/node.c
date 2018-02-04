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

#include "node.h"

#include "base64.h"
#include "string_utils.h"

#include "serd/serd.h"

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

SerdNode*
serd_node_malloc(const size_t        length,
                 const SerdNodeFlags flags,
                 const SerdNodeType  type)
{
  SerdNode* node = (SerdNode*)calloc(1, sizeof(SerdNode) + length + 1);

  node->length = 0;
  node->flags  = flags;
  node->type   = type;

  return node;
}

void
serd_node_set(SerdNode** const dst, const SerdNode* const src)
{
  if (src) {
    if (!(*dst) || (*dst)->length < src->length) {
      (*dst) = (SerdNode*)realloc(*dst, sizeof(SerdNode) + src->length + 1);
    }

    memcpy(*dst, src, sizeof(SerdNode) + src->length + 1);
  } else if (*dst) {
    memset(*dst, 0, sizeof(SerdNode));
  }
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
serd_node_copy(const SerdNode* node)
{
  if (!node) {
    return NULL;
  }

  const size_t size = sizeof(SerdNode) + node->length + 1;
  SerdNode*    copy = (SerdNode*)malloc(size);
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

static size_t
serd_uri_string_length(const SerdURIView* const uri)
{
  size_t len = uri->path_base.len;

#define ADD_LEN(field, n_delims)     \
  if ((field).len) {                 \
    len += (field).len + (n_delims); \
  }

  ADD_LEN(uri->path, 1)      // + possible leading `/'
  ADD_LEN(uri->scheme, 1)    // + trailing `:'
  ADD_LEN(uri->authority, 2) // + leading `//'
  ADD_LEN(uri->query, 1)     // + leading `?'
  ADD_LEN(uri->fragment, 1)  // + leading `#'

  return len + 2; // + 2 for authority `//'
}

static size_t
string_sink(const void* const buf, const size_t len, void* const stream)
{
  char** ptr = (char**)stream;
  memcpy(*ptr, buf, len);
  *ptr += len;
  return len;
}

SerdNode*
serd_new_uri_from_node(const SerdNode* const    uri_node,
                       const SerdURIView* const base,
                       SerdURIView* const       out)
{
  const char* uri_str = serd_node_string(uri_node);
  return (uri_node && uri_node->type == SERD_URI && uri_str)
           ? serd_new_uri_from_string(uri_str, base, out)
           : NULL;
}

SerdNode*
serd_new_uri_from_string(const char* const        str,
                         const SerdURIView* const base,
                         SerdURIView* const       out)
{
  if (!str || str[0] == '\0') {
    // Empty URI => Base URI, or nothing if no base is given
    return base ? serd_new_uri(base, NULL, out) : NULL;
  }

  SerdURIView uri;
  serd_uri_parse(str, &uri);
  return serd_new_uri(&uri, base, out); // Resolve/Serialise
}

static bool
is_uri_path_char(const char c)
{
  if (is_alpha(c) || is_digit(c)) {
    return true;
  }

  switch (c) {
  // unreserved:
  case '-':
  case '.':
  case '_':
  case '~':
  case ':':

  case '@': // pchar
  case '/': // separator

  // sub-delimiters:
  case '!':
  case '$':
  case '&':
  case '\'':
  case '(':
  case ')':
  case '*':
  case '+':
  case ',':
  case ';':
  case '=':
    return true;
  default:
    return false;
  }
}

SerdNode*
serd_new_file_uri(const char* const  path,
                  const char* const  hostname,
                  SerdURIView* const out)
{
  const size_t path_len     = strlen(path);
  const size_t hostname_len = hostname ? strlen(hostname) : 0;
  const bool   is_windows   = is_windows_path(path);
  size_t       uri_len      = 0;
  char*        uri          = NULL;

  if (path[0] == '/' || is_windows) {
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
    if (is_windows && path[i] == '\\') {
      serd_buffer_sink("/", 1, &buffer);
    } else if (path[i] == '%') {
      serd_buffer_sink("%%", 2, &buffer);
    } else if (is_uri_path_char(path[i])) {
      serd_buffer_sink(path + i, 1, &buffer);
    } else {
      char escape_str[10] = {'%', 0, 0, 0, 0, 0, 0, 0, 0, 0};
      snprintf(escape_str + 1, sizeof(escape_str) - 1, "%X", (unsigned)path[i]);
      serd_buffer_sink(escape_str, 3, &buffer);
    }
  }
  serd_buffer_sink_finish(&buffer);

  SerdNode* node =
    serd_new_substring(SERD_URI, (const char*)buffer.buf, buffer.len);
  if (out) {
    serd_uri_parse(serd_node_buffer(node), out);
  }

  free(buffer.buf);
  return node;
}

SerdNode*
serd_new_uri(const SerdURIView* const uri,
             const SerdURIView* const base,
             SerdURIView* const       out)
{
  SerdURIView abs_uri = *uri;
  if (base) {
    serd_uri_resolve(uri, base, &abs_uri);
  }

  const size_t len        = serd_uri_string_length(&abs_uri);
  SerdNode*    node       = serd_node_malloc(len, 0, SERD_URI);
  char*        ptr        = serd_node_buffer(node);
  const size_t actual_len = serd_uri_serialise(&abs_uri, string_sink, &ptr);

  serd_node_buffer(node)[actual_len] = '\0';
  node->length                       = actual_len;

  if (out) {
    serd_uri_parse(serd_node_buffer(node), out); // TODO: avoid double parse
  }

  return node;
}

SerdNode*
serd_new_relative_uri(const SerdURIView* const uri,
                      const SerdURIView* const base,
                      const SerdURIView* const root,
                      SerdURIView* const       out)
{
  const size_t uri_len  = serd_uri_string_length(uri);
  const size_t base_len = serd_uri_string_length(base);
  SerdNode*    node     = serd_node_malloc(uri_len + base_len, 0, SERD_URI);
  char*        ptr      = serd_node_buffer(node);
  const size_t actual_len =
    serd_uri_serialise_relative(uri, base, root, string_sink, &ptr);

  serd_node_buffer(node)[actual_len] = '\0';
  node->length                       = actual_len;

  if (out) {
    serd_uri_parse(serd_node_buffer(node), out); // TODO: avoid double parse
  }

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

    node->length = (size_t)(s - buf) + 1u;

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
  uint64_t       abs_i  = (i < 0) ? -i : i;
  const unsigned digits = serd_digits((double)abs_i);
  SerdNode*      node   = serd_node_malloc(digits + 2, 0, SERD_LITERAL);
  char*          buf    = serd_node_buffer(node);

  // Point s to the end
  char* s = buf + digits - 1;
  if (i < 0) {
    *buf = '-';
    ++s;
  }

  node->length = (size_t)(s - buf) + 1u;

  // Write integer part (right to left)
  do {
    *s-- = (char)('0' + (abs_i % 10));
  } while ((abs_i /= 10) > 0);

  return node;
}

SerdNode*
serd_new_blob(const void* const buf, const size_t size, const bool wrap_lines)
{
  if (!buf || !size) {
    return NULL;
  }

  const size_t len  = serd_base64_get_length(size, wrap_lines);
  SerdNode*    node = serd_node_malloc(len + 1, 0, SERD_LITERAL);
  uint8_t*     str  = (uint8_t*)serd_node_buffer(node);

  if (serd_base64_encode(str, buf, size, wrap_lines)) {
    node->flags |= SERD_HAS_NEWLINE;
  }

  node->length = len;
  return node;
}

SerdNodeType
serd_node_type(const SerdNode* const node)
{
  return node->type;
}

const char*
serd_node_string(const SerdNode* const node)
{
  return (const char*)(node + 1);
}

size_t
serd_node_length(const SerdNode* const node)
{
  return node->length;
}

SerdNodeFlags
serd_node_flags(const SerdNode* const node)
{
  return node->flags;
}

void
serd_node_free(SerdNode* const node)
{
  free(node);
}
