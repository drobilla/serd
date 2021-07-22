// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"

#include "namespaces.h"
#include "node_impl.h"
#include "string_utils.h"

#include "exess/exess.h"
#include "serd/buffer.h"
#include "serd/node.h"
#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/stream_result.h"
#include "serd/uri.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
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
    {NULL, sizeof(NS_XSD #name) - 1U, 0U, SERD_URI}, NS_XSD #name};

DEFINE_XSD_NODE(base64Binary)
DEFINE_XSD_NODE(boolean)
DEFINE_XSD_NODE(decimal)
DEFINE_XSD_NODE(double)
DEFINE_XSD_NODE(float)
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

size_t
serd_node_pad_length(const size_t length)
{
  const size_t terminated = length + 1U;
  const size_t padded     = (terminated + 3U) & ~0x03U;
  assert(padded % 4U == 0U);
  return padded;
}

const SerdNode*
serd_node_meta(const SerdNode* const node)
{
  return node->meta;
}

static ZIX_PURE_FUNC size_t
serd_node_total_size(const SerdNode* const node)
{
  return node ? (sizeof(SerdNode) + serd_node_pad_length(node->length)) : 0U;
}

static SerdNode*
serd_node_malloc(ZixAllocator* const allocator,
                 const size_t        max_length,
                 const SerdNodeFlags flags,
                 const SerdNodeType  type)
{
  const size_t    size = sizeof(SerdNode) + serd_node_pad_length(max_length);
  SerdNode* const node = (SerdNode*)zix_calloc(allocator, 1U, size);

  if (node) {
    node->flags = flags;
    node->type  = type;
  }

  return node;
}

void
serd_node_set_header(SerdNode* const     node,
                     const size_t        length,
                     const SerdNodeFlags flags,
                     const SerdNodeType  type)
{
  node->meta   = NULL;
  node->length = length;
  node->flags  = flags;
  node->type   = type;
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
    if (!(*dst = (SerdNode*)zix_calloc(allocator, 1U, size))) {
      return SERD_BAD_ALLOC;
    }
  }

  assert(*dst);
  memcpy(*dst, src, size);
  return SERD_SUCCESS;
}

static SerdStreamResult
result(const SerdStatus status, const size_t count)
{
  const SerdStreamResult result = {status, count};
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
  }

  return node;
}

ZIX_PURE_FUNC static bool
is_langtag(const SerdNode* const node)
{
  const ZixStringView string = serd_node_string_view(node);

  if (serd_node_type(node) != SERD_LITERAL) {
    return false;
  }

  // First character must be a letter
  size_t i = 0;
  if (!string.length || !is_alpha(string.data[i])) {
    return false;
  }

  // First component must be all letters
  while (++i < string.length && string.data[i] != '-') {
    if (!is_alpha(string.data[i])) {
      return false;
    }
  }

  // Following components can have letters and digits
  while (i < string.length) {
    while (++i < string.length && string.data[i] != '-') {
      const char c = string.data[i];
      if (!is_alpha(c) && !is_digit(c)) {
        return false;
      }
    }
  }

  return true;
}

ZIX_PURE_FUNC static bool
is_datatype(const SerdNode* const node)
{
  const char* const node_str = serd_node_string(node);

  if (serd_node_type(node) != SERD_URI) {
    return false;
  }

  if (!serd_uri_string_has_scheme(node_str)) {
    return false;
  }

  if (!strcmp(node_str, NS_RDF "langString")) {
    return false;
  }

  return true;
}

SerdNode*
serd_new_literal(ZixAllocator* const   allocator,
                 const ZixStringView   string,
                 const SerdNodeFlags   flags,
                 const SerdNode* const meta)
{
  if (((flags & SERD_HAS_DATATYPE) && !is_datatype(meta)) ||
      ((flags & SERD_HAS_LANGUAGE) && !is_langtag(meta))) {
    return NULL;
  }

  SerdNode* node =
    serd_node_malloc(allocator, string.length, flags, SERD_LITERAL);

  memcpy(serd_node_buffer(node), string.data, string.length);
  node->meta   = meta;
  node->length = string.length;
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

SerdStreamResult
serd_get_base64(const SerdNode* const node,
                const size_t          buf_size,
                void* const           buf)
{
  const size_t              max_size = serd_get_base64_size(node);
  const ExessVariableResult r =
    exess_read_base64(buf_size, buf, serd_node_string(node));

  return r.status == EXESS_NO_SPACE ? result(SERD_NO_SPACE, max_size)
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
  SerdNode*    copy = (SerdNode*)zix_calloc(allocator, 1U, size);

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
    const SerdNode* const am = serd_node_meta(a);
    const SerdNode* const bm = serd_node_meta(b);

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
  const SerdNode* const ma = serd_node_meta(a);
  const SerdNode* const mb = serd_node_meta(b);

  assert(ma->type == mb->type);
  assert(ma->flags == mb->flags);

  const char* const ma_str = serd_node_string(ma);
  const char* const mb_str = serd_node_string(mb);
  return strcmp(ma_str, mb_str);
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

  return node;
}

SerdNode*
serd_new_file_uri(ZixAllocator* const allocator,
                  const ZixStringView path,
                  const ZixStringView hostname)
{
  SerdBuffer       buffer = {NULL, NULL, 0U};
  SerdOutputStream out    = serd_open_output_buffer(&buffer);

  serd_write_file_uri(path, hostname, out.write, out.stream);
  serd_close_output(&out);

  const size_t      length = buffer.len;
  const char* const string = (char*)buffer.buf;
  if (!string) {
    return NULL;
  }

  SerdNode* const node =
    serd_new_string(allocator, zix_substring(string, length));

  zix_free(buffer.allocator, buffer.buf);
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

  SerdNode* const node = serd_node_malloc(
    allocator, len, datatype ? SERD_HAS_DATATYPE : 0U, SERD_LITERAL);

  node->meta   = datatype;
  node->length = write(user_data, len + 1, serd_node_buffer(node));
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
                                     &serd_xsd_double.node);
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
                                     &serd_xsd_float.node);
}

SerdNode*
serd_new_boolean(ZixAllocator* const allocator, bool b)
{
  return serd_new_literal(allocator,
                          b ? zix_string("true") : zix_string("false"),
                          SERD_HAS_DATATYPE,
                          &serd_xsd_boolean.node);
}

SerdNode*
serd_new_decimal(ZixAllocator* const allocator, const double d)
{
  // Measure integer string to know how much space the node will need
  ExessResult r = exess_write_decimal(d, 0, NULL);
  assert(!r.status);

  // Allocate node with enough space for value and datatype URI
  SerdNode* const node =
    serd_node_malloc(allocator, r.count, SERD_HAS_DATATYPE, SERD_LITERAL);

  // Write string directly into node
  r = exess_write_decimal(d, r.count + 1U, serd_node_buffer(node));
  assert(!r.status);

  node->meta   = &serd_xsd_decimal.node;
  node->length = r.count;
  return node;
}

SerdNode*
serd_new_integer(ZixAllocator* const allocator, const int64_t i)
{
  // Measure integer string to know how much space the node will need
  ExessResult r = exess_write_long(i, 0, NULL);
  assert(!r.status);

  // Allocate node with enough space for value and datatype URI
  SerdNode* const node =
    serd_node_malloc(allocator, r.count, SERD_HAS_DATATYPE, SERD_LITERAL);

  // Write string directly into node
  r = exess_write_long(i, r.count + 1U, serd_node_buffer(node));
  assert(!r.status);

  node->meta   = &serd_xsd_integer.node;
  node->length = r.count;
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
  assert(buf);

  static const SerdNode* const datatype = &serd_xsd_base64Binary.node;

  const size_t    len  = exess_write_base64(size, buf, 0, NULL).count;
  SerdConstBuffer blob = {buf, size};

  return serd_new_custom_literal(
    allocator, &blob, len, write_base64_literal, datatype);
}

void
serd_node_free(ZixAllocator* const allocator, SerdNode* const node)
{
  zix_aligned_free(allocator, node);
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
  return (node->flags & SERD_HAS_DATATYPE) ? node->meta : NULL;
}

const SerdNode*
serd_node_language(const SerdNode* const node)
{
  assert(node);
  return (node->flags & SERD_HAS_LANGUAGE) ? node->meta : NULL;
}
