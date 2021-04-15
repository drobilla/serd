// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "node_impl.h"
#include "serd_internal.h"
#include "string_utils.h"
#include "system.h"

#include "exess/exess.h"
#include "serd/buffer.h"
#include "serd/node.h"
#include "serd/string.h"
#include "serd/uri.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const void* ZIX_NULLABLE buf;
  size_t                   len;
} SerdConstBuffer;

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

typedef struct StaticNode {
  SerdNode node;
  char     buf[sizeof(NS_XSD "base64Binary")];
} StaticNode;

#define DEFINE_XSD_NODE(name)                 \
  static const StaticNode serd_xsd_##name = { \
    {sizeof(NS_XSD #name) - 1, 0, SERD_URI}, NS_XSD #name};

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
  return (char*)(node + 1);
}

const char*
serd_node_buffer_c(const SerdNode* const node)
{
  return (const char*)(node + 1);
}

ZIX_PURE_FUNC static size_t
serd_node_pad_size(const size_t n_bytes)
{
  const size_t pad  = sizeof(SerdNode) - (n_bytes + 2) % sizeof(SerdNode);
  const size_t size = n_bytes + 2 + pad;
  assert(size % sizeof(SerdNode) == 0);
  return size;
}

ZIX_PURE_FUNC static SerdNode*
serd_node_meta(SerdNode* const node)
{
  return node + 1 + (serd_node_pad_size(node->length) / sizeof(SerdNode));
}

ZIX_PURE_FUNC static const SerdNode*
serd_node_meta_c(const SerdNode* const node)
{
  return node + 1 + (serd_node_pad_size(node->length) / sizeof(SerdNode));
}

ZIX_PURE_FUNC static const SerdNode*
serd_node_maybe_get_meta_c(const SerdNode* const node)
{
  return (node->flags & meta_mask) ? serd_node_meta_c(node) : NULL;
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

static ZIX_PURE_FUNC size_t
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

  assert((uintptr_t)node % serd_node_align == 0U);
  return node;
}

void
serd_node_set(SerdNode** const dst, const SerdNode* const src)
{
  assert(dst);
  assert(src);

  const size_t size = serd_node_total_size(src);
  if (!*dst || serd_node_total_size(*dst) < size) {
    serd_free_aligned(*dst);
    *dst = (SerdNode*)serd_calloc_aligned(serd_node_align, size);
  }

  assert(*dst);
  memcpy(*dst, src, size);
}

void
serd_node_reset(SerdNode* const node)
{
  memset(node, 0, sizeof(SerdNode));
}

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
serd_new_token(const SerdNodeType type, const ZixStringView str)
{
  SerdNodeFlags flags  = 0U;
  const size_t  length = str.data ? str.length : 0U;
  SerdNode*     node   = serd_node_malloc(length, flags, type);

  if (node) {
    if (str.data) {
      memcpy(serd_node_buffer(node), str.data, length);
    }

    node->length = length;
  }

  serd_node_check_padding(node);

  return node;
}

SerdNode*
serd_new_string(const ZixStringView str)
{
  SerdNodeFlags flags  = 0;
  const size_t  length = serd_substrlen(str.data, str.length, &flags);
  SerdNode*     node   = serd_node_malloc(length, flags, SERD_LITERAL);

  memcpy(serd_node_buffer(node), str.data, str.length);
  node->length = length;

  serd_node_check_padding(node);
  return node;
}

/// Internal pre-measured implementation of serd_new_plain_literal
static SerdNode*
serd_new_plain_literal_i(const ZixStringView str,
                         SerdNodeFlags       flags,
                         const ZixStringView lang)
{
  assert(str.length);
  assert(lang.length);

  flags |= SERD_HAS_LANGUAGE;

  const size_t len       = serd_node_pad_size(str.length);
  const size_t total_len = len + sizeof(SerdNode) + lang.length;

  SerdNode* node = serd_node_malloc(total_len, flags, SERD_LITERAL);
  memcpy(serd_node_buffer(node), str.data, str.length);
  node->length = str.length;

  SerdNode* lang_node = node + 1 + (len / sizeof(SerdNode));
  lang_node->type     = SERD_LITERAL;
  lang_node->length   = lang.length;
  memcpy(serd_node_buffer(lang_node), lang.data, lang.length);
  serd_node_check_padding(lang_node);

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_plain_literal(const ZixStringView str, const ZixStringView lang)
{
  if (!lang.length) {
    return serd_new_string(str);
  }

  SerdNodeFlags flags = 0;
  serd_strlen(str.data, &flags);

  return serd_new_plain_literal_i(str, flags, lang);
}

SerdNode*
serd_new_typed_literal(const ZixStringView str,
                       const ZixStringView datatype_uri)
{
  if (!datatype_uri.length) {
    return serd_new_string(str);
  }

  if (!strcmp(datatype_uri.data, NS_RDF "langString")) {
    return NULL;
  }

  SerdNodeFlags flags = 0U;
  serd_strlen(str.data, &flags);

  flags |= SERD_HAS_DATATYPE;

  const size_t len       = serd_node_pad_size(str.length);
  const size_t total_len = len + sizeof(SerdNode) + datatype_uri.length;

  SerdNode* node = serd_node_malloc(total_len, flags, SERD_LITERAL);
  memcpy(serd_node_buffer(node), str.data, str.length);
  node->length = str.length;

  SerdNode* datatype_node = node + 1 + (len / sizeof(SerdNode));
  datatype_node->length   = datatype_uri.length;
  datatype_node->type     = SERD_URI;
  memcpy(
    serd_node_buffer(datatype_node), datatype_uri.data, datatype_uri.length);
  serd_node_check_padding(datatype_node);

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_blank(const ZixStringView str)
{
  return serd_new_token(SERD_BLANK, str);
}

SerdNode*
serd_new_curie(const ZixStringView str)
{
  return serd_new_token(SERD_CURIE, str);
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

int
serd_node_compare(const SerdNode* const a, const SerdNode* const b)
{
  assert(a);
  assert(b);

  int cmp = 0;

  const char* const a_str = serd_node_string(a);
  const char* const b_str = serd_node_string(b);
  if ((cmp = ((int)a->type - (int)b->type)) || (cmp = strcmp(a_str, b_str)) ||
      (cmp = ((int)a->flags - (int)b->flags)) ||
      !(a->flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE))) {
    return cmp;
  }

  assert(a->flags == b->flags);
  assert(a->flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE));
  assert(b->flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE));
  const SerdNode* const ma = serd_node_meta_c(a);
  const SerdNode* const mb = serd_node_meta_c(b);

  assert(ma->type == mb->type);
  assert(ma->flags == mb->flags);

  const char* const ma_str = serd_node_string(ma);
  const char* const mb_str = serd_node_string(mb);
  return strcmp(ma_str, mb_str);
}

SerdNode*
serd_new_uri(const ZixStringView string)
{
  return serd_new_token(SERD_URI, string);
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

  serd_node_check_padding(node);
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
serd_new_file_uri(const ZixStringView path, const ZixStringView hostname)
{
  const bool is_windows = is_windows_path(path.data);
  size_t     uri_len    = 0;
  char*      uri        = NULL;

  if (is_dir_sep(path.data[0]) || is_windows) {
    uri_len = strlen("file://") + hostname.length + is_windows;
    uri     = (char*)calloc(uri_len + 1, 1);

    memcpy(uri, "file://", 7);

    if (hostname.length) {
      memcpy(uri + 7, hostname.data, hostname.length + 1);
    }

    if (is_windows) {
      uri[7 + hostname.length] = '/';
    }
  }

  SerdBuffer buffer = {uri, uri_len};
  for (size_t i = 0; i < path.length; ++i) {
    if (is_uri_path_char(path.data[i])) {
      serd_buffer_sink(path.data + i, 1, 1, &buffer);
#ifdef _WIN32
    } else if (path.data[i] == '\\') {
      serd_buffer_sink("/", 1, 1, &buffer);
#endif
    } else {
      char escape_str[10] = {'%', 0, 0, 0, 0, 0, 0, 0, 0, 0};
      snprintf(
        escape_str + 1, sizeof(escape_str) - 1, "%X", (unsigned)path.data[i]);
      serd_buffer_sink(escape_str, 1, 3, &buffer);
    }
  }

  const size_t      length = buffer.len;
  const char* const string = serd_buffer_sink_finish(&buffer);
  SerdNode* const   node   = serd_new_string(zix_substring(string, length));

  free(buffer.buf);
  serd_node_check_padding(node);
  return node;
}

typedef size_t (*SerdWriteLiteralFunc)(const void* user_data,
                                       size_t      buf_size,
                                       char*       buf);

SerdNode*
serd_new_boolean(bool b)
{
  static const ZixStringView true_string  = ZIX_STATIC_STRING("true");
  static const ZixStringView false_string = ZIX_STATIC_STRING("false");

  return serd_new_typed_literal(b ? true_string : false_string,
                                serd_node_string_view(&serd_xsd_boolean.node));
}

static SerdNode*
serd_new_custom_literal(const void* const          user_data,
                        const size_t               len,
                        const SerdWriteLiteralFunc write,
                        const SerdNode* const      datatype)
{
  if (len == 0 || !write) {
    return NULL;
  }

  const size_t datatype_size = serd_node_total_size(datatype);
  const size_t total_size    = serd_node_pad_size(len + 1) + datatype_size;

  SerdNode* const node = serd_node_malloc(
    total_size, datatype ? SERD_HAS_DATATYPE : 0U, SERD_LITERAL);

  node->length = write(user_data, len + 1, serd_node_buffer(node));

  if (datatype) {
    memcpy(serd_node_meta(node), datatype, datatype_size);
  }

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_decimal(const double d)
{
  static const SerdNode* const datatype = &serd_xsd_decimal.node;

  const size_t datatype_size = serd_node_total_size(datatype);

  // Measure integer string to know how much space the node will need
  ExessResult r = exess_write_decimal(d, 0, NULL);
  assert(!r.status);

  // Allocate node with enough space for value and datatype URI
  SerdNode* const node =
    serd_node_malloc(serd_node_pad_size(r.count + 1) + datatype_size,
                     SERD_HAS_DATATYPE,
                     SERD_LITERAL);

  // Write string directly into node
  r = exess_write_decimal(d, r.count + 1, serd_node_buffer(node));
  assert(!r.status);

  node->length = r.count;
  memcpy(serd_node_meta(node), datatype, datatype_size);
  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_integer(const int64_t i)
{
  static const SerdNode* const datatype = &serd_xsd_integer.node;

  const size_t datatype_size = serd_node_total_size(datatype);

  // Measure integer string to know how much space the node will need
  ExessResult r = exess_write_long(i, 0, NULL);
  assert(!r.status);

  // Allocate node with enough space for value and datatype URI
  SerdNode* const node =
    serd_node_malloc(serd_node_pad_size(r.count + 1) + datatype_size,
                     SERD_HAS_DATATYPE,
                     SERD_LITERAL);

  // Write string directly into node
  r = exess_write_long(i, r.count + 1U, serd_node_buffer(node));
  assert(!r.status);

  node->length = r.count;
  memcpy(serd_node_meta(node), datatype, datatype_size);
  serd_node_check_padding(node);
  return node;
}

static size_t
write_base64_literal(const void* const user_data,
                     const size_t      buf_size,
                     char* const       buf)
{
  const SerdConstBuffer blob = *(const SerdConstBuffer*)user_data;

  const ExessResult r =
    exess_write_base64(blob.len, (const void*)blob.buf, buf_size, buf);

  return r.status ? 0 : r.count;
}

SerdNode*
serd_new_base64(const void* buf, size_t size)
{
  assert(buf);

  static const SerdNode* const datatype = &serd_xsd_base64Binary.node;

  const size_t    len  = exess_write_base64(size, buf, 0, NULL).count;
  SerdConstBuffer blob = {buf, size};

  return serd_new_custom_literal(&blob, len, write_base64_literal, datatype);
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

const SerdNode*
serd_node_datatype(const SerdNode* const node)
{
  assert(node);

  if (!(node->flags & SERD_HAS_DATATYPE)) {
    return NULL;
  }

  const SerdNode* const datatype = serd_node_meta_c(node);
  assert(datatype->type == SERD_URI || datatype->type == SERD_CURIE);
  return datatype;
}

const SerdNode*
serd_node_language(const SerdNode* const node)
{
  assert(node);

  if (!(node->flags & SERD_HAS_LANGUAGE)) {
    return NULL;
  }

  const SerdNode* const lang = serd_node_meta_c(node);
  assert(lang->type == SERD_LITERAL);
  return lang;
}
