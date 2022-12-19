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
#include "serd/string.h"
#include "serd/uri.h"
#include "serd/value.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
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

static const ExessDatatype value_type_datatypes[] = {
  EXESS_NOTHING,
  EXESS_BOOLEAN,
  EXESS_DOUBLE,
  EXESS_FLOAT,
  EXESS_LONG,
  EXESS_INT,
  EXESS_SHORT,
  EXESS_BYTE,
  EXESS_ULONG,
  EXESS_UINT,
  EXESS_USHORT,
  EXESS_UBYTE,
};

static const SerdValueType datatype_value_types[] = {
  SERD_NOTHING, ///< EXESS_NOTHING
  SERD_BOOL,    ///< EXESS_BOOLEAN
  SERD_DOUBLE,  ///< EXESS_DECIMAL
  SERD_DOUBLE,  ///< EXESS_DOUBLE
  SERD_FLOAT,   ///< EXESS_FLOAT
  SERD_LONG,    ///< EXESS_INTEGER
  SERD_LONG,    ///< EXESS_NON_POSITIVE_INTEGER
  SERD_LONG,    ///< EXESS_NEGATIVE_INTEGER
  SERD_LONG,    ///< EXESS_LONG
  SERD_INT,     ///< EXESS_INT
  SERD_SHORT,   ///< EXESS_SHORT
  SERD_BYTE,    ///< EXESS_BYTE
  SERD_ULONG,   ///< EXESS_NON_NEGATIVE_INTEGER
  SERD_ULONG,   ///< EXESS_ULONG
  SERD_UINT,    ///< EXESS_UINT
  SERD_USHORT,  ///< EXESS_USHORT
  SERD_UBYTE,   ///< EXESS_UBYTE
  SERD_ULONG,   ///< EXESS_POSITIVE_INTEGER
};

static ExessDatatype
value_type_datatype(const SerdValueType value_type)
{
  return (value_type > SERD_UBYTE) ? EXESS_NOTHING
                                   : value_type_datatypes[value_type];
}

static inline SerdValueType
datatype_value_type(const ExessDatatype datatype)
{
  return (datatype > EXESS_POSITIVE_INTEGER) ? SERD_NOTHING
                                             : datatype_value_types[datatype];
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

char*
serd_node_buffer(SerdNode* const node)
{
  return (char*)(node + 1U);
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

// Dynamic allocation

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
  SerdNodeFlags flags  = 0;
  const size_t  length = serd_substrlen(str.data, str.length, &flags);
  SerdNode*     node = serd_node_malloc(allocator, length, flags, SERD_LITERAL);

  if (node) {
    node->length = length;
    memcpy(serd_node_buffer(node), str.data, str.length);
    serd_node_buffer(node)[str.length] = '\0';
  }

  return node;
}

/// Internal pre-measured implementation of serd_new_plain_literal
static SerdNode*
serd_new_plain_literal_i(ZixAllocator* const   allocator,
                         const ZixStringView   str,
                         SerdNodeFlags         flags,
                         const SerdNode* const lang)
{
  assert(str.length);
  assert(serd_node_length(lang));

  flags |= SERD_HAS_LANGUAGE;

  SerdNode* const node =
    serd_node_malloc(allocator, str.length, flags, SERD_LITERAL);

  memcpy(serd_node_buffer(node), str.data, str.length);
  node->meta   = lang;
  node->length = str.length;

  return node;
}

SerdNode*
serd_new_plain_literal(ZixAllocator* const   allocator,
                       const ZixStringView   str,
                       const SerdNode* const lang)
{
  if (!lang) {
    return serd_new_string(allocator, str);
  }

  if (serd_node_type(lang) != SERD_LITERAL) {
    return NULL;
  }

  SerdNodeFlags flags = 0;
  serd_strlen(str.data, &flags);

  return serd_new_plain_literal_i(allocator, str, flags, lang);
}

SerdNode*
serd_new_typed_literal(ZixAllocator* const   allocator,
                       const ZixStringView   str,
                       const SerdNode* const datatype_uri)
{
  if (!datatype_uri) {
    return serd_new_string(allocator, str);
  }

  if (serd_node_type(datatype_uri) != SERD_URI ||
      !strcmp(serd_node_string(datatype_uri), NS_RDF "langString")) {
    return NULL;
  }

  SerdNodeFlags flags = 0U;
  serd_strlen(str.data, &flags);

  flags |= SERD_HAS_DATATYPE;

  SerdNode* const node =
    serd_node_malloc(allocator, str.length, flags, SERD_LITERAL);

  memcpy(serd_node_buffer(node), str.data, str.length);
  node->meta   = datatype_uri;
  node->length = str.length;

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

SerdNode*
serd_new_boolean(ZixAllocator* const allocator, bool b)
{
  static const ZixStringView true_string  = ZIX_STATIC_STRING("true");
  static const ZixStringView false_string = ZIX_STATIC_STRING("false");

  return serd_new_typed_literal(
    allocator, b ? true_string : false_string, &serd_xsd_boolean.node);
}

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
                  : serd_new_typed_literal(allocator,
                                           zix_substring(buf, r.count),
                                           &serd_xsd_double.node);
}

SerdNode*
serd_new_float(ZixAllocator* const allocator, const float f)
{
  char buf[EXESS_MAX_FLOAT_LENGTH + 1] = {0};

  const ExessResult r = exess_write_float(f, sizeof(buf), buf);

  return r.status ? NULL
                  : serd_new_typed_literal(allocator,
                                           zix_substring(buf, r.count),
                                           &serd_xsd_float.node);
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

void
serd_node_free(ZixAllocator* const allocator, SerdNode* const node)
{
  zix_aligned_free(allocator, node);
}

// Operators

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

// Accessors

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
  return (const char*)(node + 1U);
}

ZixStringView
serd_node_string_view(const SerdNode* const node)
{
  assert(node);
  const ZixStringView r = {(const char*)(node + 1U), node->length};
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

SerdValue
serd_node_value(const SerdNode* const node)
{
  assert(node);

  const SerdNode* const datatype_node = serd_node_datatype(node);

  const ExessDatatype datatype =
    datatype_node ? exess_datatype_from_uri(serd_node_string(datatype_node))
                  : EXESS_NOTHING;

  const SerdValueType value_type = datatype_value_type(datatype);
  if (value_type == SERD_NOTHING) {
    return serd_nothing();
  }

  ExessValue                value = {false};
  const ExessVariableResult vr =
    exess_read_value(datatype, sizeof(value), &value, serd_node_string(node));

  if (vr.status) {
    return serd_nothing();
  }

  SerdValue result = {value_type, {false}};
  memcpy(&result.data, &value, vr.write_count);

  return result;
}

SerdValue
serd_node_value_as(const SerdNode* const node,
                   const SerdValueType   type,
                   const bool            lossy)
{
  // Get the value as it is
  const SerdValue value = serd_node_value(node);
  if (!value.type || value.type == type) {
    return value;
  }

  const ExessCoercions coercions =
    lossy ? (EXESS_REDUCE_PRECISION | EXESS_ROUND | EXESS_TRUNCATE)
          : EXESS_LOSSLESS;

  const ExessDatatype node_datatype = value_type_datatype(value.type);
  const ExessDatatype datatype      = value_type_datatype(type);
  SerdValueData       data          = {false};

  // Coerce to the desired type
  const ExessResult r = exess_value_coerce(coercions,
                                           node_datatype,
                                           exess_value_size(node_datatype),
                                           &value.data,
                                           datatype,
                                           exess_value_size(datatype),
                                           &data);

  const SerdValue result = {r.status ? SERD_NOTHING : type, data};
  return result;
}

size_t
serd_node_decoded_size(const SerdNode* const node)
{
  const SerdNode* const datatype = serd_node_datatype(node);

  return !datatype ? 0U
         : !strcmp(serd_node_string(datatype), NS_XSD "hexBinary")
           ? exess_hex_decoded_size(serd_node_length(node))
         : !strcmp(serd_node_string(datatype), NS_XSD "base64Binary")
           ? exess_base64_decoded_size(serd_node_length(node))
           : 0U;
}

SerdStreamResult
serd_node_decode(const SerdNode* const node,
                 const size_t          buf_size,
                 void* const           buf)
{
  const SerdNode* const datatype = serd_node_datatype(node);
  if (!datatype) {
    return result(SERD_BAD_ARG, 0U);
  }

  ExessVariableResult r = {EXESS_UNSUPPORTED, 0U, 0U};
  if (!strcmp(serd_node_string(datatype), NS_XSD "hexBinary")) {
    r = exess_read_hex(buf_size, buf, serd_node_string(node));
  } else if (!strcmp(serd_node_string(datatype), NS_XSD "base64Binary")) {
    r = exess_read_base64(buf_size, buf, serd_node_string(node));
  } else {
    return result(SERD_BAD_ARG, 0U);
  }

  return r.status == EXESS_NO_SPACE ? result(SERD_NO_SPACE, r.write_count)
         : r.status                 ? result(SERD_BAD_SYNTAX, 0U)
                                    : result(SERD_SUCCESS, r.write_count);
}
