// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "base64.h"
#include "string_utils.h"
#include "system.h"

#include "serd/attributes.h"
#include "serd/buffer.h"
#include "serd/node.h"
#include "serd/string.h"
#include "serd/string_view.h"
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

static const size_t serd_node_align = 2 * sizeof(uint64_t);

static const SerdNodeFlags meta_mask = (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE);

static size_t
serd_uri_string_length(const SerdURIView* const uri)
{
  size_t len = uri->path_prefix.len;

#define ADD_LEN(field, n_delims)     \
  if ((field).len) {                 \
    len += (field).len + (n_delims); \
  }

  ADD_LEN(uri->path, 1)      // + possible leading '/'
  ADD_LEN(uri->scheme, 1)    // + trailing ':'
  ADD_LEN(uri->authority, 2) // + leading '//'
  ADD_LEN(uri->query, 1)     // + leading '?'
  ADD_LEN(uri->fragment, 1)  // + leading '#'

  return len + 2; // + 2 for authority '//'
}

static size_t
string_sink(const void* const buf, const size_t len, void* const stream)
{
  char** ptr = (char**)stream;
  memcpy(*ptr, buf, len);
  *ptr += len;
  return len;
}

static size_t
serd_node_pad_size(const size_t n_bytes)
{
  const size_t pad = sizeof(SerdNode) - (n_bytes + 2) % sizeof(SerdNode);
  return n_bytes + 2 + pad;
}

static const SerdNode*
serd_node_meta_c(const SerdNode* const node)
{
  return node + 1 + (serd_node_pad_size(node->length) / sizeof(SerdNode));
}

static const SerdNode*
serd_node_maybe_get_meta_c(const SerdNode* const node)
{
  return (node->flags & meta_mask) ? serd_node_meta_c(node) : NULL;
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
    serd_free_aligned(*dst);
    *dst = NULL;
    return;
  }

  const size_t size = serd_node_total_size(src);
  if (!*dst || serd_node_total_size(*dst) < size) {
    serd_free_aligned(*dst);
    *dst = (SerdNode*)serd_calloc_aligned(serd_node_align, size);
  }

  assert(*dst);
  memcpy(*dst, src, size);
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
serd_new_literal(const char* const str,
                 const char* const datatype,
                 const char* const lang)
{
  SerdNodeFlags flags  = 0;
  const size_t  length = serd_strlen(str, &flags);
  const size_t  len    = serd_node_pad_size(length);

  SerdNode* node = NULL;
  if (lang) {
    flags |= SERD_HAS_LANGUAGE;
    const size_t lang_len  = strlen(lang);
    const size_t total_len = len + sizeof(SerdNode) + lang_len;
    node                   = serd_node_malloc(total_len, flags, SERD_LITERAL);
    memcpy(serd_node_buffer(node), str, length);
    node->length = length;

    SerdNode* lang_node = node + 1 + (len / sizeof(SerdNode));
    lang_node->type     = SERD_LITERAL;
    lang_node->length   = lang_len;
    memcpy(serd_node_buffer(lang_node), lang, lang_len);
  } else if (datatype) {
    flags |= SERD_HAS_DATATYPE;
    const size_t datatype_len = strlen(datatype);
    const size_t total_len    = len + sizeof(SerdNode) + datatype_len;
    node = serd_node_malloc(total_len, flags, SERD_LITERAL);
    memcpy(serd_node_buffer(node), str, length);
    node->length = length;

    SerdNode* datatype_node = node + 1 + (len / sizeof(SerdNode));
    datatype_node->type     = SERD_URI;
    datatype_node->length   = datatype_len;
    memcpy(serd_node_buffer(datatype_node), datatype, datatype_len);
  } else {
    node = serd_node_malloc(length, flags, SERD_LITERAL);
    memcpy(serd_node_buffer(node), str, length);
    node->length = length;
  }

  return node;
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
    const SerdNode* const am = serd_node_meta_c(a);
    const SerdNode* const bm = serd_node_meta_c(b);

    return am->length == bm->length && am->type == bm->type &&
           !memcmp(serd_node_string(am), serd_node_string(bm), am->length);
  }

  return true;
}

SerdNode*
serd_new_uri(const char* const str)
{
  const size_t length = strlen(str);
  SerdNode*    node   = serd_node_malloc(length, 0, SERD_URI);
  memcpy(serd_node_buffer(node), str, length);
  node->length = length;
  return node;
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

  return node;
}

SerdNode*
serd_new_resolved_uri(const SerdStringView string, const SerdURIView base)
{
  const SerdURIView uri    = serd_parse_uri(string.buf);
  SerdNode* const   result = serd_new_from_uri(uri, base);

  if (!serd_uri_string_has_scheme(serd_node_string(result))) {
    serd_node_free(result);
    return NULL;
  }

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
    if (path[i] == '%') {
      serd_buffer_sink("%%", 2, &buffer);
    } else if (is_uri_path_char(path[i])) {
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
  if (!buf || !size) {
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
serd_node_string_view(const SerdNode* const node)
{
  const SerdStringView r = {(const char*)(node + 1), node->length};

  return r;
}

SERD_PURE_FUNC
SerdURIView
serd_node_uri_view(const SerdNode* const node)
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

  const size_t          len      = serd_node_pad_size(node->length);
  const SerdNode* const datatype = node + 1 + (len / sizeof(SerdNode));
  assert(datatype->type == SERD_URI || datatype->type == SERD_CURIE);
  return datatype;
}

const SerdNode*
serd_node_language(const SerdNode* const node)
{
  if (!node || !(node->flags & SERD_HAS_LANGUAGE)) {
    return NULL;
  }

  const size_t          len  = serd_node_pad_size(node->length);
  const SerdNode* const lang = node + 1 + (len / sizeof(SerdNode));
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
