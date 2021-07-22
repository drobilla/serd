/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

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
#include <stdlib.h>
#include <string.h>

typedef struct {
  const void* SERD_NULLABLE buf;
  size_t                    len;
} SerdConstBuffer;

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
  SerdNodeFlags flags  = 0u;
  const size_t  length = str.buf ? str.len : 0u;
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
  SerdNodeFlags flags = 0u;
  SerdNode*     node  = serd_node_malloc(str.len, flags, SERD_LITERAL);

  if (node) {
    if (str.buf && str.len) {
      memcpy(serd_node_buffer(node), str.buf, str.len);
    }

    node->length = str.len;
    serd_node_check_padding(node);
  }

  return node;
}

SERD_PURE_FUNC
static bool
is_langtag(const SerdStringView string)
{
  // First character must be a letter
  size_t i = 0;
  if (!string.len || !is_alpha(string.buf[i])) {
    return false;
  }

  // First component must be all letters
  while (++i < string.len && string.buf[i] && string.buf[i] != '-') {
    if (!is_alpha(string.buf[i])) {
      return false;
    }
  }

  // Following components can have letters and digits
  while (i < string.len && string.buf[i] == '-') {
    while (++i < string.len && string.buf[i] && string.buf[i] != '-') {
      const char c = string.buf[i];
      if (!is_alpha(c) && !is_digit(c)) {
        return false;
      }
    }
  }

  return true;
}

SerdNode*
serd_new_literal(const SerdStringView string,
                 const SerdNodeFlags  flags,
                 const SerdStringView meta)
{
  if (!(flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE))) {
    SerdNode* node = serd_node_malloc(string.len, flags, SERD_LITERAL);

    memcpy(serd_node_buffer(node), string.buf, string.len);
    node->length = string.len;
    serd_node_check_padding(node);
    return node;
  }

  if ((flags & SERD_HAS_DATATYPE) && (flags & SERD_HAS_LANGUAGE)) {
    return NULL;
  }

  if (!meta.len) {
    return NULL;
  }

  if (((flags & SERD_HAS_DATATYPE) &&
       (!serd_uri_string_has_scheme(meta.buf) ||
        !strcmp(meta.buf, NS_RDF "langString"))) ||
      ((flags & SERD_HAS_LANGUAGE) && !is_langtag(meta))) {
    return NULL;
  }

  const size_t len       = serd_node_pad_length(string.len);
  const size_t meta_len  = serd_node_pad_length(meta.len);
  const size_t meta_size = sizeof(SerdNode) + meta_len;

  SerdNode* node = serd_node_malloc(len + meta_size, flags, SERD_LITERAL);
  memcpy(serd_node_buffer(node), string.buf, string.len);
  node->length = string.len;

  SerdNode* meta_node = node + 1u + (len / sizeof(SerdNode));
  meta_node->length   = meta.len;
  meta_node->type     = (flags & SERD_HAS_DATATYPE) ? SERD_URI : SERD_LITERAL;
  memcpy(serd_node_buffer(meta_node), meta.buf, meta.len);
  serd_node_check_padding(meta_node);

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_blank(const SerdStringView str)
{
  return serd_new_token(SERD_BLANK, str);
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
    const ExessResult r = {vr.status, 0u};
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
         : r.status                 ? result(SERD_ERR_BAD_SYNTAX, 0u)
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

int
serd_node_compare(const SerdNode* const a, const SerdNode* const b)
{
  assert(a);
  assert(b);

  int cmp = 0;

  if ((cmp = ((int)a->type - (int)b->type)) ||
      (cmp = strcmp(serd_node_string_i(a), serd_node_string_i(b))) ||
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

  return strcmp(serd_node_string_i(ma), serd_node_string_i(mb));
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

SerdNode*
serd_new_file_uri(const SerdStringView path, const SerdStringView hostname)
{
  SerdBuffer buffer = {NULL, 0u};

  serd_write_file_uri(path, hostname, serd_buffer_sink, &buffer);
  serd_buffer_sink_finish(&buffer);

  SerdNode* node =
    serd_new_uri(SERD_SUBSTRING((const char*)buffer.buf, buffer.len - 1));

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
    total_size, datatype ? SERD_HAS_DATATYPE : 0u, SERD_LITERAL);

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
                  : serd_new_literal(SERD_SUBSTRING(buf, r.count),
                                     SERD_HAS_DATATYPE,
                                     SERD_STRING(EXESS_XSD_URI "double"));
}

SerdNode*
serd_new_float(const float f)
{
  char buf[EXESS_MAX_FLOAT_LENGTH + 1] = {0};

  const ExessResult r = exess_write_float(f, sizeof(buf), buf);

  return r.status ? NULL
                  : serd_new_literal(SERD_SUBSTRING(buf, r.count),
                                     SERD_HAS_DATATYPE,
                                     SERD_STRING(EXESS_XSD_URI "float"));
}

SerdNode*
serd_new_boolean(bool b)
{
  return serd_new_literal(b ? SERD_STRING("true") : SERD_STRING("false"),
                          SERD_HAS_DATATYPE,
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
  r = exess_write_long(i, r.count + 1, serd_node_buffer(node));
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
  assert(datatype->type == SERD_URI);
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
