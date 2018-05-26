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
#include "serd_internal.h"
#include "static_nodes.h"
#include "string_utils.h"
#include "system.h"

#include "serd/serd.h"

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

static const size_t serd_node_align = 2 * sizeof(size_t);

static SerdNode*
serd_new_from_uri(SerdURIView uri, SerdURIView base);

static size_t
serd_node_pad_size(const size_t n_bytes)
{
  const size_t pad  = sizeof(SerdNode) - (n_bytes + 2) % sizeof(SerdNode);
  const size_t size = n_bytes + 2 + pad;
  assert(size % sizeof(SerdNode) == 0);
  return size;
}

static const SerdNode*
serd_node_meta_c(const SerdNode* const node)
{
  return node + 1 + (serd_node_pad_size(node->length) / sizeof(SerdNode));
}

static SerdNode*
serd_node_meta(SerdNode* const node)
{
  return node + 1 + (serd_node_pad_size(node->length) / sizeof(SerdNode));
}

static const SerdNode*
serd_node_maybe_get_meta_c(const SerdNode* const node)
{
  return (node->flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE))
           ? (node + 1 + (serd_node_pad_size(node->length) / sizeof(SerdNode)))
           : NULL;
}

static void
serd_node_check_padding(const SerdNode* node)
{
  (void)node;
#ifndef NDEBUG
  if (node) {
    const size_t unpadded_size = node->length;
    const size_t padded_size   = serd_node_pad_size(unpadded_size);
    for (size_t i = 0; i < padded_size - unpadded_size; ++i) {
      assert(serd_node_buffer_c(node)[unpadded_size + i] == '\0');
    }

    serd_node_check_padding(serd_node_maybe_get_meta_c(node));
  }
#endif
}

static SERD_PURE_FUNC
size_t
serd_node_total_size(const SerdNode* const node)
{
  return node ? (sizeof(SerdNode) + serd_node_pad_size(node->length) +
                 serd_node_total_size(serd_node_maybe_get_meta_c(node)))
              : 0;
}

SerdNode*
serd_node_malloc(const size_t        length,
                 const SerdNodeFlags flags,
                 const SerdNodeType  type)
{
  const size_t size = sizeof(SerdNode) + serd_node_pad_size(length);
  SerdNode*    node = (SerdNode*)serd_calloc_aligned(serd_node_align, size);

  node->length = 0;
  node->flags  = flags;
  node->type   = type;

  assert((intptr_t)node % serd_node_align == 0);
  return node;
}

void
serd_node_set(SerdNode** const dst, const SerdNode* const src)
{
  if (!src) {
    free(*dst);
    *dst = NULL;
    return;
  }

  const size_t size = serd_node_total_size(src);
  if (serd_node_total_size(*dst) < size) {
    serd_free_aligned(*dst);
    *dst = (SerdNode*)serd_calloc_aligned(serd_node_align, size);
  }

  if (*dst) {
    memcpy(*dst, src, size);
  }
}

/**
   Zero node padding.

   This is used for nodes which live in re-used stack memory during reading,
   which must be normalized before being passed to a sink so comparison will
   work correctly.
*/
void
serd_node_zero_pad(SerdNode* node)
{
  char*        buf         = serd_node_buffer(node);
  const size_t size        = node->length;
  const size_t padded_size = serd_node_pad_size(size);

  memset(buf + size, 0, padded_size - size);

  if (node->flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE)) {
    serd_node_zero_pad(serd_node_meta(node));
  }

  if (node->flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE)) {
    serd_node_zero_pad(serd_node_meta(node));
  }
}

SerdNode*
serd_new_simple_node(const SerdNodeType type, const SerdStringView str)
{
  if (type != SERD_BLANK && type != SERD_CURIE && type != SERD_URI) {
    return NULL;
  }

  SerdNodeFlags flags  = 0;
  const size_t  length = str.buf ? serd_strlen(str.buf, &flags) : 0;
  SerdNode*     node   = serd_node_malloc(length, flags, type);

  if (node) {
    if (str.buf) {
      memcpy(serd_node_buffer(node), str.buf, length);
    }

    node->length = length;
  }

  serd_node_check_padding(node);

  return node;
}

SerdNode*
serd_new_string(const SerdStringView str)
{
  SerdNodeFlags flags  = 0;
  const size_t  length = serd_substrlen(str.buf, str.len, &flags);
  SerdNode*     node   = serd_node_malloc(length, flags, SERD_LITERAL);

  memcpy(serd_node_buffer(node), str.buf, str.len);
  node->length = length;

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_literal(const SerdStringView str,
                 const SerdStringView datatype_uri,
                 const SerdStringView lang)
{
  if (!str.len || (lang.len && datatype_uri.len &&
                   strcmp(datatype_uri.buf, NS_RDF "langString"))) {
    return NULL;
  }

  SerdNodeFlags flags  = 0;
  const size_t  length = serd_substrlen(str.buf, str.len, &flags);
  const size_t  len    = serd_node_pad_size(length);

  SerdNode* node = NULL;
  if (lang.len) {
    const size_t total_len = len + sizeof(SerdNode) + lang.len;

    node = serd_node_malloc(total_len, flags | SERD_HAS_LANGUAGE, SERD_LITERAL);
    node->length = length;
    memcpy(serd_node_buffer(node), str.buf, length);

    SerdNode* lang_node = node + 1 + (len / sizeof(SerdNode));
    lang_node->type     = SERD_LITERAL;
    lang_node->length   = lang.len;
    memcpy(serd_node_buffer(lang_node), lang.buf, lang.len);
    serd_node_check_padding(lang_node);

  } else if (datatype_uri.len) {
    const size_t total_len = len + sizeof(SerdNode) + datatype_uri.len;

    node = serd_node_malloc(total_len, flags | SERD_HAS_DATATYPE, SERD_LITERAL);
    node->length = length;
    memcpy(serd_node_buffer(node), str.buf, length);

    SerdNode* datatype_node = node + 1 + (len / sizeof(SerdNode));
    datatype_node->type     = SERD_URI;
    datatype_node->length   = datatype_uri.len;
    memcpy(serd_node_buffer(datatype_node), datatype_uri.buf, datatype_uri.len);
    serd_node_check_padding(datatype_node);

  } else {
    node = serd_node_malloc(length, flags, SERD_LITERAL);
    memcpy(serd_node_buffer(node), str.buf, length);
    node->length = length;
  }

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_blank(const SerdStringView str)
{
  return serd_new_simple_node(SERD_BLANK, str);
}

SerdNode*
serd_new_curie(const SerdStringView str)
{
  return serd_new_simple_node(SERD_CURIE, str);
}

SerdNode*
serd_node_copy(const SerdNode* node)
{
  if (!node) {
    return NULL;
  }

  const size_t size = serd_node_total_size(node);
  SerdNode*    copy = (SerdNode*)serd_calloc_aligned(serd_node_align, size);

  memcpy(copy, node, size);
  return copy;
}

bool
serd_node_equals(const SerdNode* const a, const SerdNode* const b)
{
  if (a == b) {
    return true;
  }

  if (!a || !b) {
    return false;
  }

  const size_t a_size = serd_node_total_size(a);
  return serd_node_total_size(b) == a_size && !memcmp(a, b, a_size);
}

static size_t
serd_uri_string_length(const SerdURIView* const uri)
{
  size_t len = uri->path_prefix.len;

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

SerdNode*
serd_new_uri(const SerdStringView str)
{
  return serd_new_simple_node(SERD_URI, str);
}

SerdNode*
serd_new_parsed_uri(const SerdURIView uri)
{
  const size_t    len        = serd_uri_string_length(&uri);
  SerdNode* const node       = serd_node_malloc(len, 0, SERD_URI);
  char*           ptr        = serd_node_buffer(node);
  const size_t    actual_len = serd_write_uri(uri, string_sink, &ptr);

  serd_node_buffer(node)[actual_len] = '\0';
  node->length                       = actual_len;

  serd_node_check_padding(node);
  return node;
}

static SerdNode*
serd_new_from_uri(const SerdURIView uri, const SerdURIView base)
{
  const SerdURIView abs_uri    = serd_resolve_uri(uri, base);
  const size_t      len        = serd_uri_string_length(&abs_uri);
  SerdNode*         node       = serd_node_malloc(len, 0, SERD_URI);
  char*             ptr        = serd_node_buffer(node);
  const size_t      actual_len = serd_write_uri(abs_uri, string_sink, &ptr);

  serd_node_buffer(node)[actual_len] = '\0';
  node->length                       = actual_len;

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_resolved_uri(const SerdStringView string, const SerdURIView base)
{
  if (!string.len || !string.buf[0]) {
    // Empty URI => Base URI, or nothing if no base is given
    return base.scheme.buf ? serd_new_from_uri(base, SERD_URI_NULL) : NULL;
  }

  const SerdURIView uri    = serd_parse_uri(string.buf);
  SerdNode* const   result = serd_new_from_uri(uri, base);

  if (!serd_uri_string_has_scheme(serd_node_string(result))) {
    serd_node_free(result);
    return NULL;
  }

  serd_node_check_padding(result);
  return result;
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
serd_new_file_uri(const SerdStringView path, const SerdStringView hostname)
{
  const bool is_windows = is_windows_path(path.buf);
  size_t     uri_len    = 0;
  char*      uri        = NULL;

  if (path.buf[0] == '/' || is_windows) {
    uri_len = strlen("file://") + hostname.len + is_windows;
    uri     = (char*)calloc(uri_len + 1, 1);

    memcpy(uri, "file://", 7);

    if (hostname.len) {
      memcpy(uri + 7, hostname.buf, hostname.len + 1);
    }

    if (is_windows) {
      uri[7 + hostname.len] = '/';
    }
  }

  SerdBuffer buffer = {uri, uri_len};
  for (size_t i = 0; i < path.len; ++i) {
    if (is_windows && path.buf[i] == '\\') {
      serd_buffer_sink("/", 1, 1, &buffer);
    } else if (path.buf[i] == '%') {
      serd_buffer_sink("%%", 1, 2, &buffer);
    } else if (is_uri_path_char(path.buf[i])) {
      serd_buffer_sink(path.buf + i, 1, 1, &buffer);
    } else {
      char escape_str[10] = {'%', 0, 0, 0, 0, 0, 0, 0, 0, 0};
      snprintf(
        escape_str + 1, sizeof(escape_str) - 1, "%X", (unsigned)path.buf[i]);
      serd_buffer_sink(escape_str, 1, 3, &buffer);
    }
  }
  serd_buffer_sink_finish(&buffer);

  SerdNode* node =
    serd_new_uri(SERD_STRING_VIEW((const char*)buffer.buf, buffer.len - 1));

  free(buffer.buf);
  serd_node_check_padding(node);
  return node;
}

static unsigned
serd_digits(const double abs)
{
  const double lg = ceil(log10(floor(abs) + 1.0));
  return lg < 1.0 ? 1U : (unsigned)lg;
}

SerdNode*
serd_new_decimal(const double          d,
                 const unsigned        frac_digits,
                 const SerdNode* const datatype)
{
  if (isnan(d) || isinf(d)) {
    return NULL;
  }

  const SerdNode* type       = datatype ? datatype : &serd_xsd_decimal.node;
  const double    abs_d      = fabs(d);
  const unsigned  int_digits = serd_digits(abs_d);
  const size_t    len        = int_digits + frac_digits + 3;
  const size_t    type_len   = serd_node_total_size(type);
  const size_t    total_len  = len + type_len;

  SerdNode* const node =
    serd_node_malloc(total_len, SERD_HAS_DATATYPE, SERD_LITERAL);

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

    node->length = (size_t)(s - buf) + 1u;

    // Write digits from last trailing zero to decimal point
    for (; i < frac_digits; ++i) {
      *s-- = (char)('0' + (frac % 10));
      frac /= 10;
    }
  }

  memcpy(serd_node_meta(node), type, type_len);
  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_integer(const int64_t i, const SerdNode* const datatype)
{
  const SerdNode* type      = datatype ? datatype : &serd_xsd_integer.node;
  uint64_t        abs_i     = (uint64_t)((i < 0) ? -i : i);
  const unsigned  digits    = serd_digits((double)abs_i);
  const size_t    type_len  = serd_node_total_size(type);
  const size_t    total_len = digits + 2 + type_len;

  SerdNode* node = serd_node_malloc(total_len, SERD_HAS_DATATYPE, SERD_LITERAL);

  // Point s to the end
  char* buf = serd_node_buffer(node);
  char* s   = buf + digits - 1;
  if (i < 0) {
    *buf = '-';
    ++s;
  }

  node->length = (size_t)(s - buf) + 1u;

  // Write integer part (right to left)
  do {
    *s-- = (char)('0' + (abs_i % 10));
  } while ((abs_i /= 10) > 0);

  memcpy(serd_node_meta(node), type, type_len);
  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_blob(const void* const     buf,
              const size_t          size,
              const bool            wrap_lines,
              const SerdNode* const datatype)
{
  if (!buf || !size) {
    return NULL;
  }

  const SerdNode* type      = datatype ? datatype : &serd_xsd_base64Binary.node;
  const size_t    len       = serd_base64_get_length(size, wrap_lines);
  const size_t    type_len  = serd_node_total_size(type);
  const size_t    total_len = len + 1 + type_len;

  SerdNode* const node =
    serd_node_malloc(total_len, SERD_HAS_DATATYPE, SERD_LITERAL);

  uint8_t* str = (uint8_t*)serd_node_buffer(node);
  if (serd_base64_encode(str, buf, size, wrap_lines)) {
    node->flags |= SERD_HAS_NEWLINE;
  }

  node->length = len;
  memcpy(serd_node_meta(node), type, type_len);
  serd_node_check_padding(node);
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

SerdStringView
serd_node_string_view(const SerdNode* SERD_NONNULL node)
{
  static const SerdStringView empty_string_view = {"", 0};

  if (!node) {
    return empty_string_view;
  }

  const SerdStringView result = {(const char*)(node + 1), node->length};

  return result;
}

SerdURIView
serd_node_uri_view(const SerdNode* SERD_NONNULL node)
{
  return (node->type == SERD_URI) ? serd_parse_uri(serd_node_string(node))
                                  : SERD_URI_NULL;
}

const SerdNode*
serd_node_datatype(const SerdNode* const node)
{
  if (!node || !(node->flags & SERD_HAS_DATATYPE)) {
    return NULL;
  }

  const SerdNode* const datatype = serd_node_meta_c(node);
  assert(datatype->type == SERD_URI || datatype->type == SERD_CURIE);
  return datatype;
}

const SerdNode*
serd_node_language(const SerdNode* const node)
{
  if (!node || !(node->flags & SERD_HAS_LANGUAGE)) {
    return NULL;
  }

  const SerdNode* const lang = serd_node_meta_c(node);
  assert(lang->type == SERD_LITERAL);
  return lang;
}

SerdNodeFlags
serd_node_flags(const SerdNode* const node)
{
  return node->flags;
}

void
serd_node_free(SerdNode* const node)
{
  serd_free_aligned(node);
}
