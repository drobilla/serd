// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "namespaces.h"
#include "string_utils.h"

#include "exess/exess.h"
#include "serd/buffer.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/uri.h"
#include "serd/write_result.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
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

static SerdNode*
serd_new_from_uri(ZixAllocator* allocator, SerdURIView uri, SerdURIView base);

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

ZIX_PURE_FUNC static size_t
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
    const size_t padded_length = serd_node_pad_length(node->length);
    for (size_t i = 0; i < padded_length - node->length; ++i) {
      assert(serd_node_buffer_c(node)[node->length + i] == '\0');
    }

    serd_node_check_padding(serd_node_maybe_get_meta_c(node));
  }
#endif
}

static ZIX_PURE_FUNC size_t
serd_node_total_size(const SerdNode* const node)
{
  return node ? (sizeof(SerdNode) + serd_node_pad_length(node->length) +
                 serd_node_total_size(serd_node_maybe_get_meta_c(node)))
              : 0;
}

SerdNode*
serd_node_malloc(ZixAllocator* const allocator,
                 const size_t        length,
                 const SerdNodeFlags flags,
                 const SerdNodeType  type)
{
  const size_t size = sizeof(SerdNode) + serd_node_pad_length(length);

  SerdNode* const node =
    (SerdNode*)zix_aligned_alloc(allocator, serd_node_align, size);

  if (node) {
    memset(node, 0, size);
  }

  if (node) {
    node->length = 0;
    node->flags  = flags;
    node->type   = type;
  }

  assert((uintptr_t)node % serd_node_align == 0U);
  return node;
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
    if (!(*dst =
            (SerdNode*)zix_aligned_alloc(allocator, serd_node_align, size))) {
      return SERD_BAD_ALLOC;
    }
  }

  assert(*dst);
  memcpy(*dst, src, size);
  return SERD_SUCCESS;
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

  serd_node_check_padding(node);

  return node;
}

SerdNode*
serd_new_string(ZixAllocator* const allocator, const ZixStringView str)
{
  SerdNodeFlags flags = 0U;
  SerdNode* node = serd_node_malloc(allocator, str.length, flags, SERD_LITERAL);

  if (node) {
    if (str.data && str.length) {
      memcpy(serd_node_buffer(node), str.data, str.length);
    }

    node->length = str.length;
    serd_node_check_padding(node);
  }

  return node;
}

ZIX_PURE_FUNC static bool
is_langtag(const ZixStringView string)
{
  // First character must be a letter
  size_t i = 0;
  if (!string.length || !is_alpha(string.data[i])) {
    return false;
  }

  // First component must be all letters
  while (++i < string.length && string.data[i] && string.data[i] != '-') {
    if (!is_alpha(string.data[i])) {
      return false;
    }
  }

  // Following components can have letters and digits
  while (i < string.length && string.data[i] == '-') {
    while (++i < string.length && string.data[i] && string.data[i] != '-') {
      const char c = string.data[i];
      if (!is_alpha(c) && !is_digit(c)) {
        return false;
      }
    }
  }

  return true;
}

SerdNode*
serd_new_literal(ZixAllocator* const allocator,
                 const ZixStringView string,
                 const SerdNodeFlags flags,
                 const ZixStringView meta)
{
  if (!(flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE))) {
    SerdNode* node =
      serd_node_malloc(allocator, string.length, flags, SERD_LITERAL);

    memcpy(serd_node_buffer(node), string.data, string.length);
    node->length = string.length;
    serd_node_check_padding(node);
    return node;
  }

  if ((flags & SERD_HAS_DATATYPE) && (flags & SERD_HAS_LANGUAGE)) {
    return NULL;
  }

  if (!meta.length) {
    return NULL;
  }

  if (((flags & SERD_HAS_DATATYPE) &&
       (!serd_uri_string_has_scheme(meta.data) ||
        !strcmp(meta.data, NS_RDF "langString"))) ||
      ((flags & SERD_HAS_LANGUAGE) && !is_langtag(meta))) {
    return NULL;
  }

  const size_t len       = serd_node_pad_length(string.length);
  const size_t meta_len  = serd_node_pad_length(meta.length);
  const size_t meta_size = sizeof(SerdNode) + meta_len;

  SerdNode* node =
    serd_node_malloc(allocator, len + meta_size, flags, SERD_LITERAL);
  memcpy(serd_node_buffer(node), string.data, string.length);
  node->length = string.length;

  SerdNode* meta_node = node + 1U + (len / sizeof(SerdNode));
  meta_node->length   = meta.length;
  meta_node->type     = (flags & SERD_HAS_DATATYPE) ? SERD_URI : SERD_LITERAL;
  memcpy(serd_node_buffer(meta_node), meta.data, meta.length);
  serd_node_check_padding(meta_node);

  serd_node_check_padding(node);
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
  assert(node);

  bool value = false;
  serd_node_get_value_as(node, EXESS_BOOLEAN, sizeof(value), &value);

  return value;
}

double
serd_get_double(const SerdNode* const node)
{
  assert(node);

  double value = (double)NAN; // NOLINT(google-readability-casting)
  serd_node_get_value_as(node, EXESS_DOUBLE, sizeof(value), &value);

  return value;
}

float
serd_get_float(const SerdNode* const node)
{
  assert(node);

  float value = (float)NAN; // NOLINT(google-readability-casting)
  serd_node_get_value_as(node, EXESS_FLOAT, sizeof(value), &value);

  return value;
}

int64_t
serd_get_integer(const SerdNode* const node)
{
  assert(node);

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

  return r.status == EXESS_NO_SPACE ? result(SERD_OVERFLOW, max_size)
         : r.status                 ? result(SERD_BAD_SYNTAX, 0U)
                                    : result(SERD_SUCCESS, r.write_count);
}

SerdNode*
serd_node_copy(ZixAllocator* const allocator, const SerdNode* node)
{
  if (!node) {
    return NULL;
  }

  const size_t size = serd_node_total_size(node);
  SerdNode*    copy =
    (SerdNode*)zix_aligned_alloc(allocator, serd_node_align, size);

  if (copy) {
    memcpy(copy, node, size);
  }

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

  serd_node_check_padding(node);
  return node;
}

static SerdNode*
serd_new_from_uri(ZixAllocator* const allocator,
                  const SerdURIView   uri,
                  const SerdURIView   base)
{
  const SerdURIView abs_uri = serd_resolve_uri(uri, base);
  const size_t      len     = serd_uri_string_length(abs_uri);
  SerdNode*         node    = serd_node_malloc(allocator, len, 0, SERD_URI);
  if (node) {
    char*        ptr        = serd_node_buffer(node);
    const size_t actual_len = serd_write_uri(abs_uri, string_sink, &ptr);
    assert(actual_len == len);

    node->length                         = actual_len;
    serd_node_buffer(node)[node->length] = '\0';
    serd_node_check_padding(node);
  }

  return node;
}

SerdNode*
serd_new_resolved_uri(ZixAllocator* const allocator,
                      const ZixStringView string,
                      const SerdURIView   base)
{
  const SerdURIView uri    = serd_parse_uri(string.data);
  SerdNode* const   result = serd_new_from_uri(allocator, uri, base);

  if (result) {
    if (!serd_uri_string_has_scheme(serd_node_string(result))) {
      serd_node_free(allocator, result);
      return NULL;
    }

    serd_node_check_padding(result);
  }

  return result;
}

SerdNode*
serd_new_file_uri(ZixAllocator* const allocator,
                  const ZixStringView path,
                  const ZixStringView hostname)
{
  SerdBuffer buffer = {NULL, NULL, 0U};

  serd_write_file_uri(path, hostname, serd_buffer_write, &buffer);
  serd_buffer_close(&buffer);

  const size_t      length = buffer.len;
  const char* const string = (char*)buffer.buf;
  SerdNode* const   node =
    serd_new_string(allocator, zix_substring(string, length));

  zix_free(buffer.allocator, buffer.buf);
  serd_node_check_padding(node);
  return node;
}

typedef size_t (*SerdWriteLiteralFunc)(const void* user_data,
                                       size_t      buf_size,
                                       char*       buf);

static SerdNode*
serd_new_custom_literal(ZixAllocator* const        allocator,
                        const void* const          user_data,
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
    allocator, total_size, datatype ? SERD_HAS_DATATYPE : 0U, SERD_LITERAL);

  node->length = write(user_data, len + 1, serd_node_buffer(node));

  if (datatype) {
    memcpy(serd_node_meta(node), datatype, datatype_size);
  }

  return node;
}

SerdNode*
serd_new_double(ZixAllocator* const allocator, const double d)
{
  char buf[EXESS_MAX_DOUBLE_LENGTH + 1] = {0};

  const ExessResult r = exess_write_double(d, sizeof(buf), buf);

  return r.status ? NULL
                  : serd_new_literal(allocator,
                                     zix_substring(buf, r.count),
                                     SERD_HAS_DATATYPE,
                                     zix_string(EXESS_XSD_URI "double"));
}

SerdNode*
serd_new_float(ZixAllocator* const allocator, const float f)
{
  char buf[EXESS_MAX_FLOAT_LENGTH + 1] = {0};

  const ExessResult r = exess_write_float(f, sizeof(buf), buf);

  return r.status ? NULL
                  : serd_new_literal(allocator,
                                     zix_substring(buf, r.count),
                                     SERD_HAS_DATATYPE,
                                     zix_string(EXESS_XSD_URI "float"));
}

SerdNode*
serd_new_boolean(ZixAllocator* const allocator, bool b)
{
  return serd_new_literal(allocator,
                          b ? zix_string("true") : zix_string("false"),
                          SERD_HAS_DATATYPE,
                          serd_node_string_view(&serd_xsd_boolean.node));
}

SerdNode*
serd_new_decimal(ZixAllocator* const   allocator,
                 const double          d,
                 const SerdNode* const datatype)
{
  // Use given datatype, or xsd:decimal as a default if it is null
  const SerdNode* type      = datatype ? datatype : &serd_xsd_decimal.node;
  const size_t    type_size = serd_node_total_size(type);

  // Measure integer string to know how much space the node will need
  ExessResult r = exess_write_decimal(d, 0, NULL);
  assert(!r.status);

  // Allocate node with enough space for value and datatype URI
  SerdNode* const node =
    serd_node_malloc(allocator,
                     serd_node_pad_length(r.count) + type_size,
                     SERD_HAS_DATATYPE,
                     SERD_LITERAL);

  // Write string directly into node
  r = exess_write_decimal(d, r.count + 1, serd_node_buffer(node));
  assert(!r.status);

  node->length = r.count;
  memcpy(serd_node_meta(node), type, type_size);
  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_integer(ZixAllocator* const allocator, const int64_t i)
{
  // Use given datatype, or xsd:integer as a default if it is null
  const SerdNode* datatype      = &serd_xsd_integer.node;
  const size_t    datatype_size = serd_node_total_size(datatype);

  // Measure integer string to know how much space the node will need
  ExessResult r = exess_write_long(i, 0, NULL);
  assert(!r.status);

  // Allocate node with enough space for value and datatype URI
  SerdNode* const node =
    serd_node_malloc(allocator,
                     serd_node_pad_length(r.count) + datatype_size,
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
serd_new_base64(ZixAllocator* const allocator, const void* buf, size_t size)
{
  const size_t    len  = exess_write_base64(size, buf, 0, NULL).count;
  SerdConstBuffer blob = {buf, size};

  return serd_new_custom_literal(
    allocator, &blob, len, write_base64_literal, &serd_xsd_base64Binary.node);
}

SerdNodeType
serd_node_type(const SerdNode* const node)
{
  assert(node);

  return node->type;
}

const char*
serd_node_string(const SerdNode* const node)
{
  assert(node);

  return (const char*)(node + 1);
}

size_t
serd_node_length(const SerdNode* const node)
{
  assert(node);

  return node->length;
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

SerdNodeFlags
serd_node_flags(const SerdNode* const node)
{
  return node->flags;
}

void
serd_node_free(ZixAllocator* const allocator, SerdNode* const node)
{
  zix_aligned_free(allocator, node);
}
