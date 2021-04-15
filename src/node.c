// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "namespaces.h"
#include "static_nodes.h"
#include "string_utils.h"
#include "system.h"

#include "exess/exess.h"
#include "serd/serd.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const void* SERD_NULLABLE buf;
  size_t                    len;
} SerdConstBuffer;

static const SerdNodeFlags meta_mask = (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE);

static SerdNode*
serd_new_from_uri(SerdURIView uri, SerdURIView base);

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

static size_t
serd_node_pad_length(const size_t n_bytes)
{
  const size_t pad  = sizeof(SerdNode) - (n_bytes + 2) % sizeof(SerdNode);
  const size_t size = n_bytes + 2 + pad;
  assert(size % sizeof(SerdNode) == 0);
  return size;
}

static const SerdNode*
serd_node_meta_c(const SerdNode* const node)
{
  return node + 1 + (serd_node_pad_length(node->length) / sizeof(SerdNode));
}

static SerdNode*
serd_node_meta(SerdNode* const node)
{
  return node + 1 + (serd_node_pad_length(node->length) / sizeof(SerdNode));
}

static const SerdNode*
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
    const size_t padded_length = serd_node_pad_length(node->length);
    for (size_t i = 0; i < padded_length - node->length; ++i) {
      assert(serd_node_buffer_c(node)[node->length + i] == '\0');
    }

    serd_node_check_padding(serd_node_maybe_get_meta_c(node));
  }
#endif
}

size_t
serd_node_total_size(const SerdNode* const node)
{
  return node ? (sizeof(SerdNode) + serd_node_pad_length(node->length) +
                 serd_node_total_size(serd_node_maybe_get_meta_c(node)))
              : 0;
}

SerdNode*
serd_node_malloc(const size_t        length,
                 const SerdNodeFlags flags,
                 const SerdNodeType  type)
{
  const size_t size = sizeof(SerdNode) + serd_node_pad_length(length);
  SerdNode*    node = (SerdNode*)serd_calloc_aligned(serd_node_align, size);

  node->length = 0;
  node->flags  = flags;
  node->type   = type;

  assert((uintptr_t)node % serd_node_align == 0);
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

/**
   Zero node padding.

   This is used for nodes which live in re-used stack memory during reading,
   which must be normalized before being passed to a sink so comparison will
   work correctly.
*/
void
serd_node_zero_pad(SerdNode* node)
{
  char*        buf           = serd_node_buffer(node);
  const size_t padded_length = serd_node_pad_length(node->length);

  memset(buf + node->length, 0, padded_length - node->length);

  if (node->flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE)) {
    serd_node_zero_pad(serd_node_meta(node));
  }
}

static SerdWriteResult
result(const SerdStatus status, const size_t count)
{
  const SerdWriteResult result = {status, count};
  return result;
}

SerdNode*
serd_new_token(const SerdNodeType type, const SerdStringView str)
{
  SerdNodeFlags flags  = 0U;
  const size_t  length = str.buf ? str.len : 0U;
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

/// Internal pre-measured implementation of serd_new_plain_literal
static SerdNode*
serd_new_plain_literal_i(const SerdStringView str,
                         SerdNodeFlags        flags,
                         const SerdStringView lang)
{
  assert(str.len);
  assert(lang.len);

  flags |= SERD_HAS_LANGUAGE;

  const size_t len       = serd_node_pad_length(str.len);
  const size_t total_len = len + sizeof(SerdNode) + lang.len;

  SerdNode* node = serd_node_malloc(total_len, flags, SERD_LITERAL);
  memcpy(serd_node_buffer(node), str.buf, str.len);
  node->length = str.len;

  SerdNode* lang_node = node + 1 + (len / sizeof(SerdNode));
  lang_node->type     = SERD_LITERAL;
  lang_node->length   = lang.len;
  memcpy(serd_node_buffer(lang_node), lang.buf, lang.len);
  serd_node_check_padding(lang_node);

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_plain_literal(const SerdStringView str, const SerdStringView lang)
{
  if (!lang.len) {
    return serd_new_string(str);
  }

  SerdNodeFlags flags = 0;
  serd_strlen(str.buf, &flags);

  return serd_new_plain_literal_i(str, flags, lang);
}

SerdNode*
serd_new_typed_literal(const SerdStringView str,
                       const SerdStringView datatype_uri)
{
  if (!datatype_uri.len) {
    return serd_new_string(str);
  }

  if (!strcmp(datatype_uri.buf, NS_RDF "langString")) {
    return NULL;
  }

  SerdNodeFlags flags = 0U;
  serd_strlen(str.buf, &flags);

  flags |= SERD_HAS_DATATYPE;

  const size_t len       = serd_node_pad_length(str.len);
  const size_t total_len = len + sizeof(SerdNode) + datatype_uri.len;

  SerdNode* node = serd_node_malloc(total_len, flags, SERD_LITERAL);
  memcpy(serd_node_buffer(node), str.buf, str.len);
  node->length = str.len;

  SerdNode* datatype_node = node + 1 + (len / sizeof(SerdNode));
  datatype_node->length   = datatype_uri.len;
  datatype_node->type     = SERD_URI;
  memcpy(serd_node_buffer(datatype_node), datatype_uri.buf, datatype_uri.len);
  serd_node_check_padding(datatype_node);

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_blank(const SerdStringView str)
{
  return serd_new_token(SERD_BLANK, str);
}

SerdNode*
serd_new_curie(const SerdStringView str)
{
  return serd_new_token(SERD_CURIE, str);
}

ExessResult
serd_node_get_value_as(const SerdNode* const node,
                       const ExessDatatype   value_type,
                       const size_t          value_size,
                       void* const           value)
{
  const SerdNode* const datatype_node = serd_node_datatype(node);

  const ExessDatatype node_type =
    datatype_node ? exess_datatype_from_uri(serd_node_string(datatype_node))
                  : EXESS_NOTHING;

  if (node_type == EXESS_NOTHING ||
      (node_type == EXESS_HEX && value_type == EXESS_BASE64) ||
      (node_type == EXESS_BASE64 && value_type == EXESS_HEX)) {
    // Try to read the large or untyped node string directly into the result
    const ExessVariableResult vr =
      exess_read_value(value_type, value_size, value, serd_node_string(node));

    const ExessResult r = {vr.status, vr.write_count};
    return r;
  }

  // Read the (smallish) value from the node
  ExessValue                node_value = {false};
  const ExessVariableResult vr         = exess_read_value(
    node_type, sizeof(node_value), &node_value, serd_node_string(node));

  if (vr.status) {
    const ExessResult r = {vr.status, 0U};
    return r;
  }

  // Coerce value to the desired type if possible
  return exess_value_coerce(EXESS_REDUCE_PRECISION,
                            node_type,
                            vr.write_count,
                            &node_value,
                            value_type,
                            value_size,
                            value);
}

bool
serd_get_boolean(const SerdNode* const node)
{
  bool value = false;
  serd_node_get_value_as(node, EXESS_BOOLEAN, sizeof(value), &value);

  return value;
}

double
serd_get_double(const SerdNode* const node)
{
  double value = (double)NAN; // NOLINT(google-readability-casting)
  serd_node_get_value_as(node, EXESS_DOUBLE, sizeof(value), &value);

  return value;
}

float
serd_get_float(const SerdNode* const node)
{
  float value = (float)NAN; // NOLINT(google-readability-casting)
  serd_node_get_value_as(node, EXESS_FLOAT, sizeof(value), &value);

  return value;
}

int64_t
serd_get_integer(const SerdNode* const node)
{
  int64_t value = 0;
  serd_node_get_value_as(node, EXESS_LONG, sizeof(value), &value);

  return value;
}

size_t
serd_get_base64_size(const SerdNode* const node)
{
  return exess_base64_decoded_size(serd_node_length(node));
}

SerdWriteResult
serd_get_base64(const SerdNode* const node,
                const size_t          buf_size,
                void* const           buf)
{
  const size_t              max_size = serd_get_base64_size(node);
  const ExessVariableResult r =
    exess_read_base64(buf_size, buf, serd_node_string(node));

  return r.status == EXESS_NO_SPACE ? result(SERD_ERR_OVERFLOW, max_size)
         : r.status                 ? result(SERD_ERR_BAD_SYNTAX, 0U)
                                    : result(SERD_SUCCESS, r.write_count);
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
serd_new_uri(const SerdStringView str)
{
  return serd_new_token(SERD_URI, str);
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

static SerdNode*
serd_new_from_uri(const SerdURIView uri, const SerdURIView base)
{
  const SerdURIView abs_uri    = serd_resolve_uri(uri, base);
  const size_t      len        = serd_uri_string_length(abs_uri);
  SerdNode*         node       = serd_node_malloc(len, 0, SERD_URI);
  char*             ptr        = serd_node_buffer(node);
  const size_t      actual_len = serd_write_uri(abs_uri, string_sink, &ptr);

  assert(actual_len == len);

  serd_node_buffer(node)[actual_len] = '\0';
  node->length                       = actual_len;

  serd_node_check_padding(node);
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
    serd_new_uri(serd_substring((const char*)buffer.buf, buffer.len - 1));

  free(buffer.buf);
  serd_node_check_padding(node);
  return node;
}

typedef size_t (*SerdWriteLiteralFunc)(const void* user_data,
                                       size_t      buf_size,
                                       char*       buf);

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
  const size_t total_size    = serd_node_pad_length(len) + datatype_size;

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
serd_new_double(const double d)
{
  char buf[EXESS_MAX_DOUBLE_LENGTH + 1] = {0};

  const ExessResult r = exess_write_double(d, sizeof(buf), buf);

  return r.status ? NULL
                  : serd_new_typed_literal(serd_substring(buf, r.count),
                                           serd_string(EXESS_XSD_URI "double"));
}

SerdNode*
serd_new_float(const float f)
{
  char buf[EXESS_MAX_FLOAT_LENGTH + 1] = {0};

  const ExessResult r = exess_write_float(f, sizeof(buf), buf);

  return r.status ? NULL
                  : serd_new_typed_literal(serd_substring(buf, r.count),
                                           serd_string(EXESS_XSD_URI "float"));
}

SerdNode*
serd_new_boolean(bool b)
{
  return serd_new_typed_literal(b ? serd_string("true") : serd_string("false"),
                                serd_node_string_view(&serd_xsd_boolean.node));
}

SerdNode*
serd_new_decimal(const double d, const SerdNode* const datatype)
{
  // Use given datatype, or xsd:decimal as a default if it is null
  const SerdNode* type      = datatype ? datatype : &serd_xsd_decimal.node;
  const size_t    type_size = serd_node_total_size(type);

  // Measure integer string to know how much space the node will need
  ExessResult r = exess_write_decimal(d, 0, NULL);
  assert(!r.status);

  // Allocate node with enough space for value and datatype URI
  SerdNode* const node = serd_node_malloc(
    serd_node_pad_length(r.count) + type_size, SERD_HAS_DATATYPE, SERD_LITERAL);

  // Write string directly into node
  r = exess_write_decimal(d, r.count + 1, serd_node_buffer(node));
  assert(!r.status);

  node->length = r.count;
  memcpy(serd_node_meta(node), type, type_size);
  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_integer(const int64_t i, const SerdNode* const datatype)
{
  // Use given datatype, or xsd:integer as a default if it is null
  const SerdNode* type      = datatype ? datatype : &serd_xsd_integer.node;
  const size_t    type_size = serd_node_total_size(type);

  // Measure integer string to know how much space the node will need
  ExessResult r = exess_write_long(i, 0, NULL);
  assert(!r.status);

  // Allocate node with enough space for value and datatype URI
  SerdNode* const node = serd_node_malloc(
    serd_node_pad_length(r.count) + type_size, SERD_HAS_DATATYPE, SERD_LITERAL);

  // Write string directly into node
  r = exess_write_long(i, r.count + 1U, serd_node_buffer(node));
  assert(!r.status);

  node->length = r.count;
  memcpy(serd_node_meta(node), type, type_size);
  serd_node_check_padding(node);
  return node;
}

static size_t
write_base64_literal(const void* const user_data,
                     const size_t      buf_size,
                     char* const       buf)
{
  const SerdConstBuffer blob = *(const SerdConstBuffer*)user_data;

  const ExessResult r = exess_write_base64(blob.len, blob.buf, buf_size, buf);

  return r.status ? 0 : r.count;
}

SerdNode*
serd_new_base64(const void* buf, size_t size, const SerdNode* datatype)
{
  const size_t    len  = exess_write_base64(size, buf, 0, NULL).count;
  const SerdNode* type = datatype ? datatype : &serd_xsd_base64Binary.node;
  SerdConstBuffer blob = {buf, size};

  return serd_new_custom_literal(&blob, len, write_base64_literal, type);
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
