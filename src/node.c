// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "node.h"
#include "node_impl.h" // IWYU pragma: keep
#include "symbols.h"

#include <exess/exess.h>
#include <serd/file_uri.h>
#include <serd/node_args.h>
#include <serd/node_flags.h>
#include <serd/node_id.h>
#include <serd/node_type.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <serd/token_view.h>
#include <serd/uri.h>
#include <serd/value.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SERD_MAX_STRING_LENGTH (UINT32_MAX - 1U)

#ifndef NDEBUG
#  define MUST_SUCCEED(status) assert(!(status))
#else
#  define MUST_SUCCEED(status) ((void)(status))
#endif

typedef SerdStreamResult (*ConstructFunc)(size_t                  buf_size,
                                          void*                   buf,
                                          const SerdNodeArgsData* data);

typedef ExessResult (*BinaryWriteFunc)(size_t, const void*, size_t, char*);

static const SerdSymbol value_datatype_symbols[] = {
  RDF_NIL, // SERD_NO_VALUE (unused)
  XSD_BOOLEAN,
  XSD_DOUBLE,
  XSD_FLOAT,
  XSD_LONG,
  XSD_INT,
  XSD_SHORT,
  XSD_BYTE,
  XSD_UNSIGNEDLONG,
  XSD_UNSIGNEDINT,
  XSD_UNSIGNEDSHORT,
  XSD_UNSIGNEDBYTE,
};

SerdTokenView
serd_node_token_view(const SerdNode* const node)
{
  const SerdTokenView view = {node->type, serd_node_string_view(node)};
  return view;
}

// Internal node functions

static char*
serd_node_buffer(SerdNode* const node)
{
  return (char*)(node + 1U);
}

ZIX_CONST_FUNC static size_t
serd_node_pad_length(const size_t length)
{
  const size_t terminated = length + 1U;
  const size_t padded     = (terminated + 3U) & ~0x03U;
  assert(padded % 4U == 0U);
  return padded;
}

static ZIX_CONST_FUNC size_t
serd_node_size_for_length(const size_t length)
{
  return (length > SERD_MAX_STRING_LENGTH)
           ? 0U
           : (sizeof(SerdNode) + serd_node_pad_length(length));
}

static SerdNode*
serd_node_malloc(ZixAllocator* const allocator, const size_t max_length)
{
  return (SerdNode*)zix_calloc(
    allocator, 1U, serd_node_size_for_length(max_length));
}

static void
serd_node_set_header(SerdNode* const     node,
                     const size_t        length,
                     const SerdNodeFlags flags,
                     const SerdNodeType  type)
{
  assert(length <= SERD_MAX_STRING_LENGTH);

  node->type   = type;
  node->flags  = (uint16_t)flags;
  node->meta   = 0U;
  node->length = (uint32_t)length;
}

// Construction

typedef struct {
  char*  buf;
  size_t len;
  size_t offset;
} ConstructWriteHead;

static SerdStreamResult
result(const SerdStatus status, const size_t count)
{
  const SerdStreamResult result = {status, count};
  return result;
}

/// Write bytes to a stream during node construction
static size_t
construct_write(void* const stream, const size_t n_bytes, const void* const buf)
{
  ConstructWriteHead* const head = (ConstructWriteHead*)stream;

  if (head->offset + n_bytes <= head->len) {
    assert(head->buf);
    memcpy(head->buf + head->offset, buf, n_bytes);
  }

  head->offset += n_bytes;

  return n_bytes;
}

/// Write the header of a SerdNode to a stream (convenience wrapper)
static size_t
construct_write_header(ConstructWriteHead* const stream,
                       const size_t              length,
                       const SerdNodeFlags       flags,
                       const SerdNodeType        type)
{
  assert(length < SERD_MAX_STRING_LENGTH);
  const SerdNode header = {type, (uint16_t)flags, 0U, (uint32_t)length};
  return construct_write(stream, sizeof(header), &header);
}

/// Write string-terminating null bytes after a node's string
static size_t
construct_terminate(void* const stream, const size_t length)
{
  static const char pad[8U]       = {0, 0, 0, 0, 0, 0, 0, 0};
  const size_t      padded_length = serd_node_pad_length(length);
  const size_t      n_null_bytes  = padded_length - length;

  assert(n_null_bytes < 8U);
  return construct_write(stream, n_null_bytes, pad);
}

static SerdStreamResult
construct_token(const size_t                  buf_size,
                void* const                   buf,
                const SerdNodeArgsData* const data)
{
  const SerdNodeType  type       = data->as_token.type;
  const ZixStringView string     = data->as_token.string;
  const size_t        total_size = serd_node_size_for_length(string.length);
  if (!total_size || total_size > buf_size) {
    return result(SERD_NO_SPACE, total_size);
  }

  SerdNode* const node = (SerdNode*)buf;

  node->meta   = 0U;
  node->length = (uint32_t)string.length;
  node->flags  = 0U;
  node->type   = type;

  memcpy(serd_node_buffer(node), string.data, string.length);
  serd_node_buffer(node)[string.length] = '\0';
  return result(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_literal_helper(const size_t        buf_size,
                         void* const         buf,
                         const ZixStringView string,
                         const SerdNodeFlags flags,
                         const SerdNodeID    meta)
{
  // Calculate total node size
  const size_t total_size = serd_node_size_for_length(string.length);
  if (!total_size || total_size > buf_size) {
    return result(SERD_NO_SPACE, total_size);
  }

  // Write node header and copy string to node body
  SerdNode* const node = (SerdNode*)buf;
  serd_node_set_header(node, string.length, flags, SERD_LITERAL);
  memcpy(serd_node_buffer(node), string.data, string.length);
  serd_node_buffer(node)[string.length] = '\0';
  node->meta                            = meta;
  return result(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_literal(const size_t                  buf_size,
                  void* const                   buf,
                  const SerdNodeArgsData* const data)
{
  return construct_literal_helper(buf_size,
                                  buf,
                                  data->as_literal.string,
                                  data->as_literal.flags,
                                  data->as_literal.meta);
}

static ExessResult
write_value_string(const size_t    buf_size,
                   char* const     buf,
                   const SerdValue value)
{
  switch (value.type) {
  case SERD_NOTHING:
    break;
  case SERD_BOOL:
    return exess_write_boolean(value.data.as_bool, buf_size, buf);
  case SERD_DOUBLE:
    return exess_write_double(value.data.as_double, buf_size, buf);
  case SERD_FLOAT:
    return exess_write_float(value.data.as_float, buf_size, buf);
  case SERD_LONG:
    return exess_write_long(value.data.as_long, buf_size, buf);
  case SERD_INT:
    return exess_write_int(value.data.as_int, buf_size, buf);
  case SERD_SHORT:
    return exess_write_short(value.data.as_short, buf_size, buf);
  case SERD_BYTE:
    return exess_write_byte(value.data.as_byte, buf_size, buf);
  case SERD_ULONG:
    return exess_write_ulong(value.data.as_ulong, buf_size, buf);
  case SERD_UINT:
    return exess_write_uint(value.data.as_uint, buf_size, buf);
  case SERD_USHORT:
    return exess_write_ushort(value.data.as_ushort, buf_size, buf);
  case SERD_UBYTE:
    return exess_write_ubyte(value.data.as_ubyte, buf_size, buf);
  }

  const ExessResult r = {EXESS_UNSUPPORTED, 0U};
  return r;
}

static SerdStreamResult
construct_primitive(const size_t                  buf_size,
                    void* const                   buf,
                    const SerdNodeArgsData* const data)
{
  char temp[EXESS_MAX_DOUBLE_LENGTH + 1U] = {0};

  const SerdValue   value = data->as_value;
  const ExessResult r     = write_value_string(sizeof(temp), temp, value);

  return r.status
           ? result(SERD_BAD_ARG, 0U)
           : construct_literal_helper(buf_size,
                                      buf,
                                      zix_substring(temp, r.count),
                                      SERD_HAS_DATATYPE,
                                      value_datatype_symbols[value.type]);
}

static SerdStreamResult
construct_decimal(const size_t                  buf_size,
                  void* const                   buf,
                  const SerdNodeArgsData* const data)
{
  char temp[EXESS_MAX_DECIMAL_LENGTH + 1U] = {0};

  const double      value = data->as_value.data.as_double;
  const ExessResult r     = exess_write_decimal(value, sizeof(temp), temp);
  MUST_SUCCEED(r.status); // The only error is buffer overrun

  return construct_literal_helper(buf_size,
                                  buf,
                                  zix_substring(temp, r.count),
                                  SERD_HAS_DATATYPE,
                                  XSD_DECIMAL);
}

static SerdStreamResult
construct_integer(const size_t                  buf_size,
                  void* const                   buf,
                  const SerdNodeArgsData* const data)
{
  char temp[EXESS_MAX_LONG_LENGTH + 1U] = {0};

  const int64_t     value = data->as_value.data.as_long;
  const ExessResult r     = exess_write_long(value, sizeof(temp), temp);
  MUST_SUCCEED(r.status); // The only error is buffer overrun

  return construct_literal_helper(buf_size,
                                  buf,
                                  zix_substring(temp, r.count),
                                  SERD_HAS_DATATYPE,
                                  XSD_INTEGER);
}

static SerdStreamResult
construct_binary_helper(const size_t      buf_size,
                        void* const       buf,
                        const size_t      value_size,
                        const void* const value,
                        const SerdSymbol  datatype_symbol,
                        BinaryWriteFunc   write_func)
{
  // Verify argument sanity
  assert(value);
  if (!value_size) {
    return result(SERD_BAD_ARG, 0);
  }

  // Find the length of the encoded string (a simple function of the size)
  ExessResult r = write_func(value_size, value, 0, NULL);

  // Check that the provided buffer is large enough
  const size_t total_size = serd_node_size_for_length(r.count);
  if (!total_size || total_size > buf_size) {
    return result(SERD_NO_SPACE, total_size);
  }

  // Write node header
  SerdNode* const node = (SerdNode*)buf;
  serd_node_set_header(node, r.count, SERD_HAS_DATATYPE, SERD_LITERAL);
  node->meta = datatype_symbol;

  // Write the encoded string into the node body
  char* const buffer = serd_node_buffer(node);
  r = write_func(value_size, value, total_size - sizeof(SerdNode), buffer);
  MUST_SUCCEED(r.status);

  return result(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_hex(const size_t                  buf_size,
              void* const                   buf,
              const SerdNodeArgsData* const data)
{
  return construct_binary_helper(buf_size,
                                 buf,
                                 data->as_blob.size,
                                 data->as_blob.data,
                                 XSD_HEXBINARY,
                                 exess_write_hex);
}

static SerdStreamResult
construct_base64(const size_t                  buf_size,
                 void* const                   buf,
                 const SerdNodeArgsData* const data)
{
  return construct_binary_helper(buf_size,
                                 buf,
                                 data->as_blob.size,
                                 data->as_blob.data,
                                 XSD_BASE64BINARY,
                                 exess_write_base64);
}

static SerdStreamResult
string_sink(void* const stream, const size_t len, const void* const buf)
{
  char** const ptr = (char**)stream;
  memcpy(*ptr, buf, len);
  *ptr += len;
  const SerdStreamResult r = {SERD_SUCCESS, len};
  return r;
}

static SerdStreamResult
construct_uri(const size_t                  buf_size,
              void* const                   buf,
              const SerdNodeArgsData* const data)
{
  const SerdURIView uri        = data->as_uri;
  const size_t      length     = serd_uri_string_length(uri);
  const size_t      total_size = serd_node_size_for_length(length);
  if (!total_size || total_size > buf_size) {
    return result(SERD_NO_SPACE, total_size);
  }

  // Write node header
  SerdNode* const node = (SerdNode*)buf;
  node->meta           = 0U;
  node->length         = (uint32_t)length;
  node->flags          = 0U;
  node->type           = SERD_URI;

  // Write URI string to node body
  char*                  ptr = serd_node_buffer(node);
  const SerdStreamResult r   = serd_write_uri(uri, string_sink, &ptr);
  assert(r.count == length);

  serd_node_buffer(node)[r.count] = '\0';
  return result(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_write_stream(void* const       stream,
                       const size_t      n_bytes,
                       const void* const buf)
{
  return result(SERD_SUCCESS, construct_write(stream, n_bytes, buf));
}

static SerdStreamResult
construct_host_path(const size_t                  buf_size,
                    void* const                   buf,
                    const SerdNodeArgsData* const data)
{
  const ZixStringView hostname = data->as_string_pair.prefix;
  const ZixStringView path     = data->as_string_pair.suffix;

  // Write node header
  ConstructWriteHead head     = {(char*)buf, buf_size, 0U};
  const size_t       n_header = construct_write_header(&head, 0U, 0U, SERD_URI);

  // Write URI string node body
  const SerdStreamResult br =
    serd_write_file_uri(path, hostname, construct_write_stream, &head);

  // Write terminating null byte(s)
  const size_t n_null = construct_terminate(&head, br.count);
  const size_t count  = n_header + br.count + n_null;
  if (count > buf_size || br.count > SERD_MAX_STRING_LENGTH) {
    return result(SERD_NO_SPACE, count);
  }

  SerdNode* const node = (SerdNode*)buf;
  assert(node);
  node->length = (uint32_t)br.count;
  assert(node->length == strlen(serd_node_string(node)));
  return result(SERD_SUCCESS, count);
}

static SerdStreamResult
construct_split_string_helper(const size_t        buf_size,
                              void* const         buf,
                              const SerdNodeType  type,
                              const ZixStringView prefix,
                              const char          sep,
                              const ZixStringView suffix)
{
  const size_t length     = prefix.length + (bool)sep + suffix.length;
  const size_t total_size = serd_node_size_for_length(length);
  if (!total_size || total_size > buf_size) {
    return result(SERD_NO_SPACE, total_size);
  }

  // Write node header
  ConstructWriteHead head  = {(char*)buf, buf_size, 0U};
  size_t             count = construct_write_header(&head, length, 0U, type);

  // Write prefix, separator if given, and suffix
  count += construct_write(&head, prefix.length, prefix.data);
  if (sep) {
    count += construct_write(&head, 1U, &sep);
  }
  count += construct_write(&head, suffix.length, suffix.data);
  count += construct_terminate(&head, length);
  assert(count <= buf_size);
  return result(SERD_SUCCESS, count);
}

static SerdStreamResult
construct_prefixed_name(const size_t                  buf_size,
                        void* const                   buf,
                        const SerdNodeArgsData* const data)
{
  return construct_split_string_helper(buf_size,
                                       buf,
                                       SERD_CURIE,
                                       data->as_string_pair.prefix,
                                       ':',
                                       data->as_string_pair.suffix);
}

static SerdStreamResult
construct_joined_uri(const size_t                  buf_size,
                     void* const                   buf,
                     const SerdNodeArgsData* const data)
{
  return construct_split_string_helper(buf_size,
                                       buf,
                                       SERD_URI,
                                       data->as_string_pair.prefix,
                                       '\0',
                                       data->as_string_pair.suffix);
}

SerdStreamResult
serd_node_construct(const size_t       buf_size,
                    void* const        buf,
                    const SerdNodeArgs args)
{
  // Note this is an internal API, some arg types aren't constructed as nodes
  assert(!buf_size || buf);
  assert(args.type > SERD_NODE_ARGS_NODE_ID);
  assert(args.type != SERD_NODE_ARGS_OBJECT);
  assert(args.type <= SERD_NODE_ARGS_BASE64);

  static const ConstructFunc funcs[13] = {
    NULL, // SERD_NODE_ARGS_NODE_ID
    construct_uri,
    construct_host_path,
    construct_prefixed_name,
    construct_joined_uri,
    construct_token,
    NULL, // SERD_NODE_ARGS_OBJECT
    construct_literal,
    construct_primitive,
    construct_decimal,
    construct_integer,
    construct_hex,
    construct_base64,
  };

  return funcs[args.type](buf_size, buf, &args.data);
}

// Dynamic allocation

SerdNode*
serd_node_new(ZixAllocator* const allocator, const SerdNodeArgs args)
{
  SerdStreamResult r = serd_node_construct(0, NULL, args);
  assert(r.status == SERD_NO_SPACE); // Caller has checked args first

  SerdNode* const node =
    serd_node_malloc(allocator, sizeof(SerdNode) + r.count);

  if (node) {
    r = serd_node_construct(r.count, node, args);
    MUST_SUCCEED(r.status); // Any error should have been reported above
  }

  return node;
}

SerdNode*
serd_node_copy(ZixAllocator* const allocator, const SerdNode* node)
{
  assert(node);

  const size_t    size = serd_node_size_for_length(node->length);
  SerdNode* const copy = (SerdNode*)zix_calloc(allocator, 1U, size);
  if (copy) {
    memcpy(copy, node, size);
  }

  return copy;
}

// Accessors

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

#undef MUST_SUCCEED
