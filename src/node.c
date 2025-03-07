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

#ifdef __cplusplus
#  define SRESULT(s, c) (SerdStreamResult{s, c})
#else
#  define SRESULT(s, c) ((SerdStreamResult){s, c})
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

// Node layout utilities

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

// Construction buffer writing utilities (with measuring support)

typedef struct {
  char*  buf;
  size_t len;
  size_t offset;
} WriteState;

/// Skip ahead in construction output without writing anything
static size_t
construct_skip(void* const stream, const size_t length)
{
  ((WriteState*)stream)->offset += length;
  return length;
}

/// Write bytes to construction output
static size_t
construct_write(void* const stream, const size_t n_bytes, const void* const buf)
{
  WriteState* const state = (WriteState*)stream;

  if (state->offset + n_bytes <= state->len) {
    assert(state->buf);
    memcpy(state->buf + state->offset, buf, n_bytes);
  }

  return construct_skip(stream, n_bytes);
}

/// Write a node header at the start of construction output
static size_t
construct_write_head(WriteState* const   stream,
                     const SerdNodeType  type,
                     const SerdNodeFlags flags,
                     const SerdNodeID    meta,
                     const size_t        length)
{
  assert(length < SERD_MAX_STRING_LENGTH);
  const SerdNode header = {type, (uint16_t)flags, meta, (uint32_t)length};
  return construct_write(stream, sizeof(header), &header);
}

/// Write string-terminating null bytes at the end of construction output
static size_t
construct_terminate(void* const stream, const size_t length)
{
  static const char pad[8U]       = {0, 0, 0, 0, 0, 0, 0, 0};
  const size_t      padded_length = serd_node_pad_length(length);
  const size_t      n_null_bytes  = padded_length - length;

  assert(n_null_bytes < 8U);
  return construct_write(stream, n_null_bytes, pad);
}

// Node constructors

static SerdStreamResult
construct_token(const size_t                  buf_size,
                void* const                   buf,
                const SerdNodeArgsData* const data)
{
  const SerdNodeType  type       = data->token.type;
  const ZixStringView string     = data->token.string;
  const size_t        total_size = serd_node_size_for_length(string.length);
  if (!total_size || total_size > buf_size) {
    return SRESULT(SERD_NO_SPACE, total_size);
  }

  WriteState state = {(char*)buf, buf_size, 0U};
  construct_write_head(&state, type, 0U, 0U, string.length);
  construct_write(&state, string.length, string.data);
  construct_terminate(&state, string.length);
  return SRESULT(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_literal_helper(const size_t        buf_size,
                         void* const         buf,
                         const ZixStringView string,
                         const SerdNodeFlags flags,
                         const SerdNodeID    meta)
{
  const size_t total_size = serd_node_size_for_length(string.length);
  if (!total_size || total_size > buf_size) {
    return SRESULT(SERD_NO_SPACE, total_size);
  }

  WriteState state = {(char*)buf, buf_size, 0U};
  construct_write_head(&state, SERD_LITERAL, flags, meta, string.length);
  construct_write(&state, string.length, string.data);
  construct_terminate(&state, string.length);
  return SRESULT(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_literal(const size_t                  buf_size,
                  void* const                   buf,
                  const SerdNodeArgsData* const data)
{
  return construct_literal_helper(buf_size,
                                  buf,
                                  data->literal.string,
                                  data->literal.flags,
                                  data->literal.meta);
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

  const SerdValue   value = data->value;
  const ExessResult r     = write_value_string(sizeof(temp), temp, value);

  return r.status
           ? SRESULT(SERD_BAD_ARG, 0U)
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
  const double        number     = data->value.data.as_double;
  EXESS_ALIGN uint8_t value[24]  = {0};
  char                temp[328U] = {0};

  const ExessResult c = exess_coerce_value(0U,
                                           EXESS_DOUBLE,
                                           sizeof(number),
                                           &number,
                                           EXESS_DECIMAL,
                                           sizeof(value),
                                           value);
  MUST_SUCCEED(c.status); // More than enough space for a double

  const ExessResult w = exess_write_decimal(c.count, value, sizeof(temp), temp);
  MUST_SUCCEED(w.status); // The only error is buffer overrun

  return construct_literal_helper(buf_size,
                                  buf,
                                  zix_substring(temp, w.count),
                                  SERD_HAS_DATATYPE,
                                  XSD_DECIMAL);
}

static SerdStreamResult
construct_integer(const size_t                  buf_size,
                  void* const                   buf,
                  const SerdNodeArgsData* const data)
{
  char temp[EXESS_MAX_LONG_LENGTH + 1U] = {0};

  const int64_t     value = data->value.data.as_long;
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
                        const SerdNodeID  meta,
                        BinaryWriteFunc   write_func)
{
  // Verify argument sanity
  assert(value);
  if (!value_size) {
    return SRESULT(SERD_BAD_ARG, 0);
  }

  // Calculate encoded size (a simple function) and check buffer space
  ExessResult  r          = write_func(value_size, value, 0, NULL);
  const size_t total_size = serd_node_size_for_length(r.count);
  if (!total_size || total_size > buf_size) {
    return SRESULT(SERD_NO_SPACE, total_size);
  }

  // Write encoded string to node body
  WriteState   state = {(char*)buf, buf_size, 0U};
  const size_t space = total_size - sizeof(SerdNode);
  char* const  str   = (char*)buf + sizeof(SerdNode);
  construct_write_head(&state, SERD_LITERAL, SERD_HAS_DATATYPE, meta, r.count);
  r = write_func(value_size, value, space, str);
  construct_skip(&state, r.count);
  construct_terminate(&state, r.count);
  MUST_SUCCEED(r.status);
  return SRESULT(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_hex(const size_t                  buf_size,
              void* const                   buf,
              const SerdNodeArgsData* const data)
{
  return construct_binary_helper(buf_size,
                                 buf,
                                 data->blob.size,
                                 data->blob.data,
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
                                 data->blob.size,
                                 data->blob.data,
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
  // Calculate encoded size (a simple function) and check buffer space
  const SerdURIView uri        = data->uri;
  const size_t      length     = serd_uri_string_length(uri);
  const size_t      total_size = serd_node_size_for_length(length);
  if (!total_size || length > SERD_MAX_STRING_LENGTH || total_size > buf_size) {
    return SRESULT(SERD_NO_SPACE, total_size);
  }

  WriteState state = {(char*)buf, buf_size, 0U};
  construct_write_head(&state, SERD_URI, 0U, 0U, length);

  char*                  ptr = (char*)buf + sizeof(SerdNode);
  const SerdStreamResult r   = serd_write_uri(uri, string_sink, &ptr);
  MUST_SUCCEED(r.status);
  assert(r.count == length);
  construct_skip(&state, r.count);
  construct_terminate(&state, r.count);
  return SRESULT(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_write_stream(void* const       stream,
                       const size_t      n_bytes,
                       const void* const buf)
{
  return SRESULT(SERD_SUCCESS, construct_write(stream, n_bytes, buf));
}

static SerdStreamResult
construct_host_path(const size_t                  buf_size,
                    void* const                   buf,
                    const SerdNodeArgsData* const data)
{
  const ZixStringView hostname = data->string_pair.prefix;
  const ZixStringView path     = data->string_pair.suffix;

  // Write node header
  WriteState   state    = {(char*)buf, buf_size, 0U};
  const size_t n_header = construct_write_head(&state, SERD_URI, 0U, 0U, 0U);

  // Write URI string node body
  const SerdStreamResult br =
    serd_write_file_uri(path, hostname, construct_write_stream, &state);

  // Write terminating null byte(s)
  const size_t n_null = construct_terminate(&state, br.count);
  const size_t count  = n_header + br.count + n_null;
  if (count > buf_size || br.count > SERD_MAX_STRING_LENGTH) {
    return SRESULT(SERD_NO_SPACE, count);
  }

  // Update length in header to true value
  SerdNode* const node = (SerdNode*)buf;
  assert(node);
  node->length = (uint32_t)br.count;
  return SRESULT(SERD_SUCCESS, count);
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
    return SRESULT(SERD_NO_SPACE, total_size);
  }

  WriteState state = {(char*)buf, buf_size, 0U};
  construct_write_head(&state, type, 0U, 0U, length);
  construct_write(&state, prefix.length, prefix.data);
  construct_write(&state, sep ? 1U : 0U, &sep);
  construct_write(&state, suffix.length, suffix.data);
  construct_terminate(&state, length);
  return SRESULT(SERD_SUCCESS, total_size);
}

static SerdStreamResult
construct_prefixed_name(const size_t                  buf_size,
                        void* const                   buf,
                        const SerdNodeArgsData* const data)
{
  return construct_split_string_helper(buf_size,
                                       buf,
                                       SERD_CURIE,
                                       data->string_pair.prefix,
                                       ':',
                                       data->string_pair.suffix);
}

static SerdStreamResult
construct_joined_uri(const size_t                  buf_size,
                     void* const                   buf,
                     const SerdNodeArgsData* const data)
{
  return construct_split_string_helper(buf_size,
                                       buf,
                                       SERD_URI,
                                       data->string_pair.prefix,
                                       '\0',
                                       data->string_pair.suffix);
}

SerdStreamResult
serd_node_construct(const SerdNodeArgs args,
                    const size_t       buf_size,
                    void* const        buf)
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

SerdTokenView
serd_node_token_view(const SerdNode* const node)
{
  const SerdTokenView view = {node->type, serd_node_string_view(node)};
  return view;
}

#undef MUST_SUCCEED
