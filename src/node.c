// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node_internal.h"

#include "namespaces.h"
#include "node_impl.h"
#include "string_utils.h"
#include "warnings.h"

#include "exess/exess.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/stream_result.h"
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

#ifndef NDEBUG
#  define MUST_SUCCEED(status) assert(!(status))
#else
#  define MUST_SUCCEED(status) ((void)(status))
#endif

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

typedef struct StaticNode {
  SerdNode node;
  char     buf[sizeof(NS_XSD "unsignedShort")];
} StaticNode;

#define DEFINE_XSD_NODE(name)                 \
  static const StaticNode serd_xsd_##name = { \
    {NULL, sizeof(NS_XSD #name) - 1U, 0U, SERD_URI}, NS_XSD #name};

DEFINE_XSD_NODE(boolean)
DEFINE_XSD_NODE(decimal)
DEFINE_XSD_NODE(double)
DEFINE_XSD_NODE(float)
DEFINE_XSD_NODE(integer)
DEFINE_XSD_NODE(long)
DEFINE_XSD_NODE(int)
DEFINE_XSD_NODE(short)
DEFINE_XSD_NODE(byte)
DEFINE_XSD_NODE(unsignedLong)
DEFINE_XSD_NODE(unsignedInt)
DEFINE_XSD_NODE(unsignedShort)
DEFINE_XSD_NODE(unsignedByte)
DEFINE_XSD_NODE(hexBinary)
DEFINE_XSD_NODE(base64Binary)

static const SerdNode* const serd_value_datatype_node[] = {
  NULL,
  &serd_xsd_boolean.node,
  &serd_xsd_double.node,
  &serd_xsd_float.node,
  &serd_xsd_long.node,
  &serd_xsd_int.node,
  &serd_xsd_short.node,
  &serd_xsd_byte.node,
  &serd_xsd_unsignedLong.node,
  &serd_xsd_unsignedInt.node,
  &serd_xsd_unsignedShort.node,
  &serd_xsd_unsignedByte.node,
};

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

// Argument constructors

SerdNodeArgs
serd_a_token(const SerdNodeType type, const ZixStringView string)
{
  const SerdNodeArgs args = {SERD_NODE_ARGS_TOKEN, {{type, string}}};
  return args;
}

SerdNodeArgs
serd_a_parsed_uri(const SerdURIView uri)
{
  SerdNodeArgs args;
  args.type                   = SERD_NODE_ARGS_PARSED_URI;
  args.data.as_parsed_uri.uri = uri;
  return args;
}

SerdNodeArgs
serd_a_file_uri(const ZixStringView path, const ZixStringView hostname)
{
  SerdNodeArgs args;
  args.type                      = SERD_NODE_ARGS_FILE_URI;
  args.data.as_file_uri.path     = path;
  args.data.as_file_uri.hostname = hostname;
  return args;
}

SerdNodeArgs
serd_a_prefixed_name(const ZixStringView prefix, const ZixStringView name)
{
  SerdNodeArgs args;
  args.type                         = SERD_NODE_ARGS_PREFIXED_NAME;
  args.data.as_prefixed_name.prefix = prefix;
  args.data.as_prefixed_name.name   = name;
  return args;
}

SerdNodeArgs
serd_a_joined_uri(const ZixStringView prefix, const ZixStringView suffix)
{
  SerdNodeArgs args;
  args.type                      = SERD_NODE_ARGS_JOINED_URI;
  args.data.as_joined_uri.prefix = prefix;
  args.data.as_joined_uri.suffix = suffix;
  return args;
}

SerdNodeArgs
serd_a_literal(const ZixStringView   string,
               const SerdNodeFlags   flags,
               const SerdNode* const meta)
{
  SerdNodeArgs args;
  args.type                   = SERD_NODE_ARGS_LITERAL;
  args.data.as_literal.string = string;
  args.data.as_literal.flags  = flags;
  args.data.as_literal.meta   = meta;
  return args;
}

SerdNodeArgs
serd_a_primitive(const SerdValue value)
{
  SerdNodeArgs args;
  args.type                    = SERD_NODE_ARGS_PRIMITIVE;
  args.data.as_primitive.value = value;
  return args;
}

SerdNodeArgs
serd_a_decimal(const double value)
{
  SerdNodeArgs args;
  args.type                  = SERD_NODE_ARGS_DECIMAL;
  args.data.as_decimal.value = value;
  return args;
}

SerdNodeArgs
serd_a_integer(const int64_t value)
{
  SerdNodeArgs args;
  args.type                  = SERD_NODE_ARGS_INTEGER;
  args.data.as_integer.value = value;
  return args;
}

SerdNodeArgs
serd_a_hex(const size_t size, const void* const data)
{
  SerdNodeArgs args;
  args.type              = SERD_NODE_ARGS_HEX;
  args.data.as_blob.size = size;
  args.data.as_blob.data = data;
  return args;
}

SerdNodeArgs
serd_a_base64(size_t size, const void* const data)
{
  SerdNodeArgs args;
  args.type              = SERD_NODE_ARGS_BASE64;
  args.data.as_blob.size = size;
  args.data.as_blob.data = data;
  return args;
}

// Node functions

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

static ZIX_CONST_FUNC size_t
serd_node_size_for_length(const size_t length)
{
  return sizeof(SerdNode) + serd_node_pad_length(length);
}

size_t
serd_node_total_size(const SerdNode* const node)
{
  return node ? serd_node_size_for_length(node->length) : 0U;
}

SerdNode*
serd_node_malloc(ZixAllocator* const allocator, const size_t max_length)
{
  return (SerdNode*)zix_calloc(
    allocator, 1U, serd_node_size_for_length(max_length));
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
    if (!(*dst = serd_node_malloc(allocator, size))) {
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

static SerdStreamResult
serd_node_construct_simple(const size_t        buf_size,
                           void* const         buf,
                           const SerdNodeType  type,
                           const SerdNodeFlags flags,
                           const ZixStringView string)
{
  const size_t total_size = serd_node_size_for_length(string.length);

  if (!buf || total_size > buf_size) {
    return result(SERD_NO_SPACE, total_size);
  }

  SerdNode* const node = (SerdNode*)buf;

  node->meta   = NULL;
  node->length = string.length;
  node->flags  = flags;
  node->type   = type;

  memcpy(serd_node_buffer(node), string.data, string.length);
  serd_node_buffer(node)[string.length] = '\0';
  return result(SERD_SUCCESS, total_size);
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

static SerdStreamResult
serd_node_construct_literal(const size_t          buf_size,
                            void* const           buf,
                            const ZixStringView   string,
                            const SerdNodeFlags   flags,
                            const SerdNode* const meta)
{
  if (!(flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE))) {
    return serd_node_construct_simple(
      buf_size, buf, SERD_LITERAL, flags, string);
  }

  if ((flags & SERD_HAS_DATATYPE) && (flags & SERD_HAS_LANGUAGE)) {
    return result(SERD_BAD_ARG, 0);
  }

  if (!meta || !serd_node_length(meta)) {
    return result(SERD_BAD_ARG, 0);
  }

  if (((flags & SERD_HAS_DATATYPE) && !is_datatype(meta)) ||
      ((flags & SERD_HAS_LANGUAGE) && !is_langtag(meta))) {
    return result(SERD_BAD_ARG, 0);
  }

  // Calculate total node size
  const size_t total_size = serd_node_size_for_length(string.length);
  if (!buf || total_size > buf_size) {
    return result(SERD_NO_SPACE, total_size);
  }

  // Write node header
  SerdNode* const node = (SerdNode*)buf;
  node->meta           = meta;
  node->length         = string.length;
  node->flags          = flags;
  node->type           = SERD_LITERAL;

  // Copy string to node body
  memcpy(serd_node_buffer(node), string.data, string.length);
  serd_node_buffer(node)[string.length] = '\0';
  return result(SERD_SUCCESS, total_size);
}

static ExessDatatype
value_type_datatype(const SerdValueType value_type)
{
  return (value_type > SERD_UBYTE) ? EXESS_NOTHING
                                   : value_type_datatypes[value_type];
}

static inline SerdValueType
datatype_value_type(const ExessDatatype datatype)
{
  switch (datatype) {
  case EXESS_NOTHING:
    return SERD_NOTHING;
  case EXESS_BOOLEAN:
    return SERD_BOOL;
  case EXESS_DECIMAL:
  case EXESS_DOUBLE:
    return SERD_DOUBLE;
  case EXESS_FLOAT:
    return SERD_FLOAT;
  case EXESS_INTEGER:
  case EXESS_NON_POSITIVE_INTEGER:
  case EXESS_NEGATIVE_INTEGER:
  case EXESS_LONG:
    return SERD_LONG;
  case EXESS_INT:
    return SERD_INT;
  case EXESS_SHORT:
    return SERD_SHORT;
  case EXESS_BYTE:
    return SERD_BYTE;
  case EXESS_NON_NEGATIVE_INTEGER:
  case EXESS_ULONG:
    return SERD_ULONG;
  case EXESS_UINT:
    return SERD_UINT;
  case EXESS_USHORT:
    return SERD_USHORT;
  case EXESS_UBYTE:
    return SERD_UBYTE;
  case EXESS_POSITIVE_INTEGER:
    return SERD_ULONG;

  case EXESS_DURATION:
  case EXESS_DATETIME:
  case EXESS_TIME:
  case EXESS_DATE:
  case EXESS_HEX:
  case EXESS_BASE64:
    break;
  }

  return SERD_NOTHING;
}

static SerdStreamResult
serd_node_construct_value(const size_t    buf_size,
                          void* const     buf,
                          const SerdValue value)
{
  char        temp[EXESS_MAX_DOUBLE_LENGTH + 1] = {0};
  ExessResult r                                 = {EXESS_UNSUPPORTED, 0U};
  switch (value.type) {
  case SERD_NOTHING:
    return result(SERD_BAD_ARG, 0U);
  case SERD_BOOL:
    r = exess_write_boolean(value.data.as_bool, sizeof(temp), temp);
    break;
  case SERD_DOUBLE:
    r = exess_write_double(value.data.as_double, sizeof(temp), temp);
    break;
  case SERD_FLOAT:
    r = exess_write_float(value.data.as_float, sizeof(temp), temp);
    break;
  case SERD_LONG:
    r = exess_write_long(value.data.as_long, sizeof(temp), temp);
    break;
  case SERD_INT:
    r = exess_write_int(value.data.as_int, sizeof(temp), temp);
    break;
  case SERD_SHORT:
    r = exess_write_short(value.data.as_short, sizeof(temp), temp);
    break;
  case SERD_BYTE:
    r = exess_write_byte(value.data.as_byte, sizeof(temp), temp);
    break;
  case SERD_ULONG:
    r = exess_write_ulong(value.data.as_ulong, sizeof(temp), temp);
    break;
  case SERD_UINT:
    r = exess_write_uint(value.data.as_uint, sizeof(temp), temp);
    break;
  case SERD_USHORT:
    r = exess_write_ushort(value.data.as_ushort, sizeof(temp), temp);
    break;
  case SERD_UBYTE:
    r = exess_write_ubyte(value.data.as_ubyte, sizeof(temp), temp);
    break;
  }

  MUST_SUCCEED(r.status); // The only error is buffer overrun

  const SerdNode* const datatype_uri = serd_value_datatype_node[value.type];
  assert(datatype_uri);

  return serd_node_construct_literal(buf_size,
                                     buf,
                                     zix_substring(temp, r.count),
                                     SERD_HAS_DATATYPE,
                                     datatype_uri);
}

static SerdStreamResult
serd_node_construct_decimal(const size_t buf_size,
                            void* const  buf,
                            const double value)
{
  char temp[EXESS_MAX_DECIMAL_LENGTH + 1] = {0};

  const ExessResult r = exess_write_decimal(value, sizeof(temp), temp);
  MUST_SUCCEED(r.status); // The only error is buffer overrun

  return serd_node_construct_literal(buf_size,
                                     buf,
                                     zix_substring(temp, r.count),
                                     SERD_HAS_DATATYPE,
                                     &serd_xsd_decimal.node);
}

static SerdStreamResult
serd_node_construct_integer(const size_t  buf_size,
                            void* const   buf,
                            const int64_t value)
{
  char              temp[24] = {0};
  const ExessResult r        = exess_write_long(value, sizeof(temp), temp);
  MUST_SUCCEED(r.status); // The only error is buffer overrun

  return serd_node_construct_literal(buf_size,
                                     buf,
                                     zix_substring(temp, r.count),
                                     SERD_HAS_DATATYPE,
                                     &serd_xsd_integer.node);
}

static SerdStreamResult
serd_node_construct_binary(
  const size_t          buf_size,
  void* const           buf,
  const size_t          value_size,
  const void* const     value,
  const SerdNode* const datatype_uri,
  ExessResult (*write_func)(size_t, const void*, size_t, char*))
{
  // Verify argument sanity
  if (!value || !value_size) {
    return result(SERD_BAD_ARG, 0);
  }

  // Find the length of the encoded string (a simple function of the size)
  ExessResult r = write_func(value_size, value, 0, NULL);

  // Check that the provided buffer is large enough
  const size_t total_size = serd_node_size_for_length(r.count);
  if (!buf || total_size > buf_size) {
    return result(SERD_NO_SPACE, total_size);
  }

  // Write node header
  SerdNode* const node = (SerdNode*)buf;
  node->meta           = datatype_uri;
  node->length         = r.count;
  node->flags          = SERD_HAS_DATATYPE;
  node->type           = SERD_LITERAL;

  // Write the encoded string into the node body
  char* const buffer = serd_node_buffer(node);
  r = write_func(value_size, value, total_size - sizeof(SerdNode), buffer);

  MUST_SUCCEED(r.status);
  (void)r;

  return result(SERD_SUCCESS, total_size);
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

static SerdStreamResult
serd_node_construct_uri(const size_t      buf_size,
                        void* const       buf,
                        const SerdURIView uri)
{
  const size_t length        = serd_uri_string_length(uri);
  const size_t required_size = serd_node_size_for_length(length);
  if (!buf || buf_size < required_size) {
    return result(SERD_NO_SPACE, required_size);
  }

  // Write node header
  SerdNode* const node = (SerdNode*)buf;
  node->meta           = NULL;
  node->length         = length;
  node->flags          = 0U;
  node->type           = SERD_URI;

  // Write URI string to node body
  char*        ptr           = serd_node_buffer(node);
  const size_t actual_length = serd_write_uri(uri, string_sink, &ptr);
  assert(actual_length == length);

  serd_node_buffer(node)[actual_length] = '\0';
  return result(SERD_SUCCESS, required_size);
}

SerdNode*
serd_node_new(ZixAllocator* const allocator, const SerdNodeArgs args)
{
  SerdStreamResult r = serd_node_construct(0, NULL, args);
  if (r.status != SERD_NO_SPACE) {
    return NULL;
  }

  SerdNode* const node =
    serd_node_malloc(allocator, sizeof(SerdNode) + r.count);

  if (node) {
    r = serd_node_construct(r.count, node, args);
    MUST_SUCCEED(r.status); // Any error should have been reported above
  }

  return node;
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
  SerdValue           result        = {type, {false}};

  // Coerce to the desired type
  const ExessResult r = exess_value_coerce(coercions,
                                           node_datatype,
                                           exess_value_size(node_datatype),
                                           &value.data,
                                           datatype,
                                           exess_value_size(datatype),
                                           &result.data);

  if (r.status) {
    result.type = SERD_NOTHING;
  }

  return result;
}

size_t
serd_node_decoded_size(const SerdNode* const node)
{
  const SerdNode* const datatype = serd_node_datatype(node);
  if (!datatype) {
    return 0U;
  }

  if (!strcmp(serd_node_string(datatype), NS_XSD "hexBinary")) {
    return exess_hex_decoded_size(serd_node_length(node));
  }

  if (!strcmp(serd_node_string(datatype), NS_XSD "base64Binary")) {
    return exess_base64_decoded_size(serd_node_length(node));
  }

  return 0U;
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

SerdNode*
serd_node_copy(ZixAllocator* const allocator, const SerdNode* node)
{
  if (!node) {
    return NULL;
  }

  SERD_DISABLE_NULL_WARNINGS
  const size_t size = serd_node_total_size(node);
  SERD_RESTORE_WARNINGS

  SerdNode* const copy = (SerdNode*)zix_calloc(allocator, 1U, size);

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

typedef struct {
  char*  buf;
  size_t len;
  size_t offset;
} ConstructWriteHead;

/// Write bytes to a stream during node construction
static size_t
construct_write(const void* const buf,
                const size_t      size,
                const size_t      nmemb,
                void* const       stream)
{
  const size_t              n_bytes = size * nmemb;
  ConstructWriteHead* const head    = (ConstructWriteHead*)stream;

  if (head->buf && head->offset + n_bytes <= head->len) {
    memcpy(head->buf + head->offset, buf, n_bytes);
  }

  head->offset += n_bytes;
  return n_bytes;
}

/// Write string-terminating null bytes after a node's string
static size_t
construct_terminate(const size_t length, void* const stream)
{
  const size_t padded_length = serd_node_pad_length(length);

  size_t count = 0U;
  for (size_t p = 0U; p < padded_length - length; ++p) {
    count += construct_write("", 1, 1, stream);
  }

  return count;
}

static SerdStreamResult
serd_node_construct_file_uri(const size_t        buf_size,
                             void* const         buf,
                             const ZixStringView path,
                             const ZixStringView hostname)
{
  SerdNode* const    node  = (SerdNode*)buf;
  ConstructWriteHead head  = {(char*)buf, buf_size, 0U};
  size_t             count = 0U;

  // Write node header
  SerdNode header = {NULL, 0U, 0U, SERD_URI};
  count += construct_write(&header, sizeof(header), 1, &head);

  // Write URI string node body
  const size_t length =
    serd_write_file_uri(path, hostname, construct_write, &head);

  // Write terminating null byte(s)
  count += length;
  count += construct_terminate(length, &head);

  if (!buf || count > buf_size) {
    return result(SERD_NO_SPACE, count);
  }

  node->length = length;
  assert(node->length == strlen(serd_node_string(node)));
  return result(SERD_SUCCESS, count);
}

static SerdStreamResult
serd_node_construct_prefixed_name(const size_t        buf_size,
                                  void* const         buf,
                                  const ZixStringView prefix,
                                  const ZixStringView name)
{
  const size_t length        = prefix.length + 1U + name.length;
  const size_t required_size = serd_node_size_for_length(length);
  if (!buf || buf_size < required_size) {
    return result(SERD_NO_SPACE, required_size);
  }

  ConstructWriteHead head  = {(char*)buf, buf_size, 0U};
  size_t             count = 0U;

  const SerdNode header = {NULL, length, 0U, SERD_CURIE};
  count += construct_write(&header, sizeof(header), 1, &head);
  count += construct_write(prefix.data, 1U, prefix.length, &head);
  count += construct_write(":", 1U, 1U, &head);
  count += construct_write(name.data, 1U, name.length, &head);
  count += construct_terminate(length, &head);

  if (!buf || count > buf_size) {
    return result(SERD_NO_SPACE, count);
  }

  return result(SERD_SUCCESS, count);
}

static SerdStreamResult
serd_node_construct_joined_uri(const size_t        buf_size,
                               void* const         buf,
                               const ZixStringView prefix,
                               const ZixStringView suffix)
{
  const size_t length        = prefix.length + suffix.length;
  const size_t required_size = serd_node_size_for_length(length);
  if (!buf || buf_size < required_size) {
    return result(SERD_NO_SPACE, required_size);
  }

  ConstructWriteHead head  = {(char*)buf, buf_size, 0U};
  size_t             count = 0U;

  const SerdNode header = {NULL, length, 0U, SERD_URI};
  count += construct_write(&header, sizeof(header), 1, &head);
  count += construct_write(prefix.data, 1U, prefix.length, &head);
  count += construct_write(suffix.data, 1U, suffix.length, &head);
  count += construct_terminate(length, &head);

  if (!buf || count > buf_size) {
    return result(SERD_NO_SPACE, count);
  }

  return result(SERD_SUCCESS, count);
}

SerdStreamResult
serd_node_construct(const size_t       buf_size,
                    void* const        buf,
                    const SerdNodeArgs args)
{
  switch (args.type) {
  case SERD_NODE_ARGS_TOKEN:
    return serd_node_construct_simple(
      buf_size, buf, args.data.as_token.type, 0U, args.data.as_token.string);

  case SERD_NODE_ARGS_PARSED_URI:
    return serd_node_construct_uri(buf_size, buf, args.data.as_parsed_uri.uri);

  case SERD_NODE_ARGS_FILE_URI:
    return serd_node_construct_file_uri(buf_size,
                                        buf,
                                        args.data.as_file_uri.path,
                                        args.data.as_file_uri.hostname);

  case SERD_NODE_ARGS_PREFIXED_NAME:
    return serd_node_construct_prefixed_name(buf_size,
                                             buf,
                                             args.data.as_prefixed_name.prefix,
                                             args.data.as_prefixed_name.name);

  case SERD_NODE_ARGS_JOINED_URI:
    return serd_node_construct_joined_uri(buf_size,
                                          buf,
                                          args.data.as_joined_uri.prefix,
                                          args.data.as_joined_uri.suffix);

  case SERD_NODE_ARGS_LITERAL:
    return serd_node_construct_literal(buf_size,
                                       buf,
                                       args.data.as_literal.string,
                                       args.data.as_literal.flags,
                                       args.data.as_literal.meta);

  case SERD_NODE_ARGS_PRIMITIVE:
    return serd_node_construct_value(
      buf_size, buf, args.data.as_primitive.value);

  case SERD_NODE_ARGS_DECIMAL:
    return serd_node_construct_decimal(
      buf_size, buf, args.data.as_decimal.value);

  case SERD_NODE_ARGS_INTEGER:
    return serd_node_construct_integer(
      buf_size, buf, args.data.as_integer.value);

  case SERD_NODE_ARGS_HEX:
    return serd_node_construct_binary(buf_size,
                                      buf,
                                      args.data.as_blob.size,
                                      args.data.as_blob.data,
                                      &serd_xsd_hexBinary.node,
                                      exess_write_hex);

  case SERD_NODE_ARGS_BASE64:
    return serd_node_construct_binary(buf_size,
                                      buf,
                                      args.data.as_blob.size,
                                      args.data.as_blob.data,
                                      &serd_xsd_base64Binary.node,
                                      exess_write_base64);
  }

  return result(SERD_BAD_ARG, 0U);
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

#undef MUST_SUCCEED
