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
#include "string_utils.h"
#include "system.h"

#include "exess/exess.h"
#include "serd/serd.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Round size up to an even multiple of the node alignment
static size_t
serd_node_pad_size(const size_t size)
{
  const size_t n_trailing = size % serd_node_align;
  const size_t n_pad      = n_trailing ? (serd_node_align - n_trailing) : 0u;

  return size + n_pad;
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

    serd_node_check_padding(serd_node_datatype(node));
    serd_node_check_padding(serd_node_language(node));
  }
#endif
}

size_t
serd_node_total_size(const SerdNode* const node)
{
  const size_t padded_length = serd_node_pad_length(node->length);
  const size_t base_size     = sizeof(SerdNode) + padded_length;

  const bool has_meta = (node->flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE));

  return base_size +
         (has_meta ? serd_node_total_size(serd_node_meta_c(node)) : 0u);
}

SerdNode*
serd_node_malloc(const size_t size)
{
  SerdNode* const node =
    (SerdNode*)serd_calloc_aligned(serd_node_align, serd_node_pad_size(size));

  assert((uintptr_t)node % serd_node_align == 0);
  return node;
}

SerdNode*
serd_node_try_malloc(const SerdWriteResult r)
{
  return (r.status && r.status != SERD_ERR_OVERFLOW)
           ? NULL
           : serd_node_malloc(r.count);
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

static SerdWriteResult
serd_node_construct_simple(const size_t         buf_size,
                           void* const          buf,
                           const SerdNodeType   type,
                           const SerdNodeFlags  flags,
                           const SerdStringView string)
{
  const size_t total_size = sizeof(SerdNode) + serd_node_pad_length(string.len);
  if (!buf || total_size > buf_size) {
    return result(SERD_ERR_OVERFLOW, total_size);
  }

  SerdNode* const node = (SerdNode*)buf;

  node->length = string.len;
  node->flags  = flags;
  node->type   = type;

  if (string.buf) {
    memcpy(serd_node_buffer(node), string.buf, string.len);
  }

  serd_node_zero_pad(node);
  return result(SERD_SUCCESS, total_size);
}

SerdWriteResult
serd_node_construct_token(const size_t         buf_size,
                          void* const          buf,
                          const SerdNodeType   type,
                          const SerdStringView string)
{
  return serd_node_construct_simple(buf_size, buf, type, 0u, string);
}

bool
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
  while (i < string.len && string.buf[i] && string.buf[i] == '-') {
    while (++i < string.len && string.buf[i] && string.buf[i] != '-') {
      const char c = string.buf[i];
      if (!is_alpha(c) && !is_digit(c)) {
        return false;
      }
    }
  }

  return true;
}

SerdWriteResult
serd_node_construct_literal(const size_t         buf_size,
                            void* const          buf,
                            const SerdStringView string,
                            const SerdNodeFlags  flags,
                            const SerdStringView meta)
{
  if (!(flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE))) {
    return serd_node_construct_simple(
      buf_size, buf, SERD_LITERAL, flags, string);
  }

  if ((flags & SERD_HAS_DATATYPE) && (flags & SERD_HAS_LANGUAGE)) {
    return result(SERD_ERR_BAD_ARG, 0);
  }

  if (!meta.len) {
    return result(SERD_ERR_BAD_ARG, 0);
  }

  if (((flags & SERD_HAS_DATATYPE) &&
       (!serd_uri_string_has_scheme(meta.buf) ||
        !strcmp(meta.buf, NS_RDF "langString"))) ||
      ((flags & SERD_HAS_LANGUAGE) && !is_langtag(meta))) {
    return result(SERD_ERR_BAD_ARG, 0);
  }

  const size_t padded_length = serd_node_pad_length(string.len);

  const size_t meta_size  = sizeof(SerdNode) + serd_node_pad_length(meta.len);
  const size_t total_size = sizeof(SerdNode) + padded_length + meta_size;
  if (!buf || total_size > buf_size) {
    return result(SERD_ERR_OVERFLOW, total_size);
  }

  SerdNode* const node = (SerdNode*)buf;

  node->length = string.len;
  node->flags  = flags;
  node->type   = SERD_LITERAL;

  memcpy(serd_node_buffer(node), string.buf, string.len);

  SerdNode* meta_node = node + 1 + (padded_length / sizeof(SerdNode));
  meta_node->type     = (flags & SERD_HAS_DATATYPE) ? SERD_URI : SERD_LITERAL;
  meta_node->length   = meta.len;
  memcpy(serd_node_buffer(meta_node), meta.buf, meta.len);

  serd_node_zero_pad(node);
  return result(SERD_SUCCESS, total_size);
}

SerdWriteResult
serd_node_construct(const size_t         buf_size,
                    void* const          buf,
                    const SerdNodeType   type,
                    const SerdStringView string,
                    const SerdNodeFlags  flags,
                    const SerdStringView meta)
{
  return ((type == SERD_LITERAL)
            ? serd_node_construct_literal(buf_size, buf, string, flags, meta)
          : (meta.len > 0u)
            ? result(SERD_ERR_BAD_ARG, 0)
            : serd_node_construct_token(buf_size, buf, type, string));
}

SerdWriteResult
serd_node_construct_boolean(const size_t buf_size,
                            void* const  buf,
                            const bool   value)
{
  char temp[EXESS_MAX_BOOLEAN_LENGTH + 1] = {0};

  const ExessResult r = exess_write_boolean(value, sizeof(temp), temp);
  assert(!r.status); // The only error is buffer overrun

  return serd_node_construct_literal(buf_size,
                                     buf,
                                     SERD_SUBSTRING(temp, r.count),
                                     SERD_HAS_DATATYPE,
                                     SERD_STRING(EXESS_XSD_URI "boolean"));
}

SerdWriteResult
serd_node_construct_decimal(const size_t buf_size,
                            void* const  buf,
                            const double value)
{
  char temp[EXESS_MAX_DECIMAL_LENGTH + 1] = {0};

  const ExessResult r = exess_write_decimal(value, sizeof(temp), temp);
  assert(!r.status); // The only error is buffer overrun

  return serd_node_construct_literal(buf_size,
                                     buf,
                                     SERD_SUBSTRING(temp, r.count),
                                     SERD_HAS_DATATYPE,
                                     SERD_STRING(EXESS_XSD_URI "decimal"));
}

SerdWriteResult
serd_node_construct_double(const size_t buf_size,
                           void* const  buf,
                           const double value)
{
  char temp[EXESS_MAX_DOUBLE_LENGTH + 1] = {0};

  const ExessResult r = exess_write_double(value, sizeof(temp), temp);
  assert(!r.status); // The only error is buffer overrun

  return serd_node_construct_literal(buf_size,
                                     buf,
                                     SERD_SUBSTRING(temp, r.count),
                                     SERD_HAS_DATATYPE,
                                     SERD_STRING(EXESS_XSD_URI "double"));
}

SerdWriteResult
serd_node_construct_float(const size_t buf_size,
                          void* const  buf,
                          const float  value)
{
  char temp[EXESS_MAX_FLOAT_LENGTH + 1] = {0};

  const ExessResult r = exess_write_float(value, sizeof(temp), temp);
  assert(!r.status); // The only error is buffer overrun

  return serd_node_construct_literal(buf_size,
                                     buf,
                                     SERD_SUBSTRING(temp, r.count),
                                     SERD_HAS_DATATYPE,
                                     SERD_STRING(EXESS_XSD_URI "float"));
}

SerdWriteResult
serd_node_construct_integer(const size_t         buf_size,
                            void* const          buf,
                            const int64_t        value,
                            const SerdStringView datatype)
{
  if (datatype.len && !serd_uri_string_has_scheme(datatype.buf)) {
    return result(SERD_ERR_BAD_ARG, 0);
  }

  char              temp[24] = {0};
  const ExessResult r        = exess_write_long(value, sizeof(temp), temp);
  assert(!r.status); // The only error is buffer overrun

  return serd_node_construct_literal(
    buf_size,
    buf,
    SERD_SUBSTRING(temp, r.count),
    SERD_HAS_DATATYPE,
    datatype.len ? datatype : SERD_STRING(NS_XSD "integer"));
}

SerdWriteResult
serd_node_construct_base64(const size_t         buf_size,
                           void* const          buf,
                           const size_t         value_size,
                           const void* const    value,
                           const SerdStringView datatype)
{
  static const SerdStringView xsd_base64Binary =
    SERD_STRING(NS_XSD "base64Binary");

  // Verify argument sanity
  if (!value || !value_size ||
      (datatype.len && !serd_uri_string_has_scheme(datatype.buf))) {
    return result(SERD_ERR_BAD_ARG, 0);
  }

  // Determine the type to use (default to xsd:base64Binary)
  const SerdStringView type        = datatype.len ? datatype : xsd_base64Binary;
  const size_t         type_length = serd_node_pad_length(type.len);
  const size_t         type_size   = sizeof(SerdNode) + type_length;

  // Find the length of the encoded string (just an O(1) arithmetic expression)
  ExessResult r = exess_write_base64(value_size, value, 0, NULL);

  // Check that the provided buffer is large enough
  const size_t padded_length = serd_node_pad_length(r.count);
  const size_t total_size    = sizeof(SerdNode) + padded_length + type_size;
  if (!buf || total_size > buf_size) {
    return result(SERD_ERR_OVERFLOW, total_size);
  }

  SerdNode* const node = (SerdNode*)buf;
  node->length         = r.count;
  node->flags          = SERD_HAS_DATATYPE;
  node->type           = SERD_LITERAL;

  // Write the encoded base64 into the node body
  r = exess_write_base64(
    value_size, value, total_size - sizeof(SerdNode), serd_node_buffer(node));

  assert(!r.status);

  // Append datatype
  SerdNode* meta_node = node + 1 + (padded_length / sizeof(SerdNode));
  meta_node->length   = type.len;
  meta_node->flags    = 0u;
  meta_node->type     = SERD_URI;
  memcpy(serd_node_buffer(meta_node), type.buf, type.len);

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

SerdWriteResult
serd_node_construct_uri(const size_t      buf_size,
                        void* const       buf,
                        const SerdURIView uri)
{
  const size_t length        = serd_uri_string_length(uri);
  const size_t required_size = sizeof(SerdNode) + serd_node_pad_length(length);
  if (!buf || buf_size < required_size) {
    return result(SERD_ERR_OVERFLOW, required_size);
  }

  // Write node header
  SerdNode* const node = (SerdNode*)buf;
  node->length         = length;
  node->flags          = 0u;
  node->type           = SERD_URI;

  // Serialise URI to node body
  char*        ptr           = serd_node_buffer(node);
  const size_t actual_length = serd_write_uri(uri, string_sink, &ptr);
  assert(actual_length == length);

  serd_node_buffer(node)[actual_length] = '\0';
  serd_node_check_padding(node);
  return result(SERD_SUCCESS, required_size);
}

SerdNode*
serd_node_new(const SerdNodeType   type,
              const SerdStringView string,
              const SerdNodeFlags  flags,
              const SerdStringView meta)
{
  SerdWriteResult r = serd_node_construct(0, NULL, type, string, flags, meta);
  if (r.status != SERD_ERR_OVERFLOW) {
    return NULL;
  }

  assert(r.count % sizeof(SerdNode) == 0);

  SerdNode* const node = serd_node_malloc(sizeof(SerdNode) + r.count + 1);

  if (node) {
    r = serd_node_construct(r.count, node, type, string, flags, meta);
    assert(!r.status); // Any error should have been reported above
  }

  return node;
}

SerdNode*
serd_new_token(const SerdNodeType type, const SerdStringView string)
{
  return serd_node_new(type, string, 0u, SERD_EMPTY_STRING());
}

SerdNode*
serd_new_string(const SerdStringView str)
{
  return serd_node_new(SERD_LITERAL, str, 0u, SERD_EMPTY_STRING());
}

SerdNode*
serd_new_literal(const SerdStringView str,
                 const SerdNodeFlags  flags,
                 const SerdStringView meta)
{
  return serd_node_new(SERD_LITERAL, str, flags, meta);
}

ExessVariant
serd_node_get_value_as(const SerdNode* const node,
                       const ExessDatatype   value_type)
{
  const SerdNode* const datatype_node = serd_node_datatype(node);

  const ExessDatatype node_type =
    datatype_node ? exess_datatype_from_uri(serd_node_string(datatype_node))
                  : EXESS_NOTHING;

  if (!node_type) {
    // No datatype, assume it matches and try reading the value directly
    ExessVariant      v = exess_make_nothing(EXESS_SUCCESS);
    const ExessResult r =
      exess_read_variant(&v, value_type, serd_node_string(node));

    return r.status ? exess_make_nothing(r.status) : v;
  }

  // Read the value from the node
  ExessVariant      v = exess_make_nothing(EXESS_SUCCESS);
  const ExessResult r =
    exess_read_variant(&v, node_type, serd_node_string(node));

  // Coerce value to the desired type if possible
  return r.status ? exess_make_nothing(r.status)
                  : exess_coerce(v, value_type, EXESS_REDUCE_PRECISION);
}

bool
serd_get_boolean(const SerdNode* const node)
{
  const ExessVariant variant = serd_node_get_value_as(node, EXESS_BOOLEAN);
  const bool* const  value   = exess_get_boolean(&variant);

  return value ? *value : false;
}

double
serd_get_double(const SerdNode* const node)
{
  const ExessVariant  variant = serd_node_get_value_as(node, EXESS_DOUBLE);
  const double* const value   = exess_get_double(&variant);

  return value ? *value : (double)NAN;
}

float
serd_get_float(const SerdNode* const node)
{
  const ExessVariant variant = serd_node_get_value_as(node, EXESS_FLOAT);
  const float* const value   = exess_get_float(&variant);

  return value ? *value : NAN;
}

int64_t
serd_get_integer(const SerdNode* const node)
{
  const ExessVariant   variant = serd_node_get_value_as(node, EXESS_LONG);
  const int64_t* const value   = exess_get_long(&variant);

  return value ? *value : 0;
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
  const size_t      max_size = serd_get_base64_size(node);
  ExessBlob         blob     = {buf_size, buf};
  const ExessResult r        = exess_read_base64(&blob, serd_node_string(node));

  return r.status == EXESS_NO_SPACE ? result(SERD_ERR_OVERFLOW, max_size)
         : r.status                 ? result(SERD_ERR_BAD_SYNTAX, 0u)
                                    : result(SERD_SUCCESS, blob.size);
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

  if (!a || !b) {
    return false;
  }

  const size_t a_size = serd_node_total_size(a);
  return serd_node_total_size(b) == a_size && !memcmp(a, b, a_size);
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
serd_new_uri(const SerdStringView string)
{
  return serd_new_token(SERD_URI, string);
}

SerdNode*
serd_new_parsed_uri(const SerdURIView uri)
{
  SerdWriteResult r    = serd_node_construct_uri(0u, NULL, uri);
  SerdNode* const node = serd_node_try_malloc(r);

  if (node) {
    r = serd_node_construct_uri(r.count, node, uri);
    assert(!r.status);
  }

  return node;
}

typedef struct {
  char*  buf;
  size_t len;
  size_t offset;
} ConstructWriteHead;

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

SerdWriteResult
serd_node_construct_file_uri(const size_t         buf_size,
                             void* const          buf,
                             const SerdStringView path,
                             const SerdStringView hostname)
{
  SerdNode* const    node  = (SerdNode*)buf;
  ConstructWriteHead head  = {(char*)buf, buf_size, 0u};
  size_t             count = 0u;

  // Write node header
  SerdNode header = {0u, 0u, SERD_URI};
  count += construct_write(&header, 1, sizeof(header), &head);

  // Write URI string node body
  const size_t length =
    serd_write_file_uri(path, hostname, construct_write, &head);

  // Terminate string
  count += length;
  count += construct_write("", 1, 1, &head);

  // Write any additional null bytes needed for padding
  const size_t padded_length = serd_node_pad_length(length);
  for (size_t p = 0u; p < padded_length - length; ++p) {
    count += construct_write("", 1, 1, &head);
  }

  if (!buf || count > buf_size) {
    return result(SERD_ERR_OVERFLOW, count);
  }

  node->length = length;
  assert(node->length == strlen(serd_node_string(node)));

  return result(SERD_SUCCESS, count);
}

SerdNode*
serd_new_file_uri(const SerdStringView path, const SerdStringView hostname)
{
  SerdWriteResult r    = serd_node_construct_file_uri(0, NULL, path, hostname);
  SerdNode* const node = serd_node_try_malloc(r);

  if (node) {
    r = serd_node_construct_file_uri(r.count, node, path, hostname);
    assert(!r.status);
    assert(serd_node_length(node) == strlen(serd_node_string(node)));
  }

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_double(const double d)
{
  SerdWriteResult r    = serd_node_construct_double(0, NULL, d);
  SerdNode* const node = serd_node_try_malloc(r);

  if (node) {
    r = serd_node_construct_double(r.count, node, d);
    assert(!r.status);
    assert(serd_node_length(node) == strlen(serd_node_string(node)));
  }

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_float(const float f)
{
  SerdWriteResult r    = serd_node_construct_float(0, NULL, f);
  SerdNode* const node = serd_node_try_malloc(r);

  if (node) {
    r = serd_node_construct_float(r.count, node, f);
    assert(!r.status);
    assert(serd_node_length(node) == strlen(serd_node_string(node)));
  }

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_boolean(bool b)
{
  SerdWriteResult r    = serd_node_construct_boolean(0, NULL, b);
  SerdNode* const node = serd_node_try_malloc(r);

  if (node) {
    r = serd_node_construct_boolean(r.count, node, b);
    assert(!r.status);
    assert(serd_node_length(node) == strlen(serd_node_string(node)));
  }

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_decimal(const double d)
{
  SerdWriteResult r    = serd_node_construct_decimal(0, NULL, d);
  SerdNode* const node = serd_node_try_malloc(r);

  if (node) {
    r = serd_node_construct_decimal(r.count, node, d);
    assert(!r.status);
    assert(serd_node_length(node) == strlen(serd_node_string(node)));
  }

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_integer(const int64_t i, const SerdStringView datatype)
{
  SerdWriteResult r    = serd_node_construct_integer(0, NULL, i, datatype);
  SerdNode* const node = serd_node_try_malloc(r);

  if (node) {
    r = serd_node_construct_integer(r.count, node, i, datatype);
    assert(!r.status);
    assert(serd_node_length(node) == strlen(serd_node_string(node)));
  }

  serd_node_check_padding(node);
  return node;
}

SerdNode*
serd_new_base64(const void* buf, size_t size, const SerdStringView datatype)
{
  SerdWriteResult r = serd_node_construct_base64(0, NULL, size, buf, datatype);
  SerdNode* const node = serd_node_try_malloc(r);

  if (node) {
    r = serd_node_construct_base64(r.count, node, size, buf, datatype);
    assert(!r.status);
    assert(serd_node_length(node) == strlen(serd_node_string(node)));
  }

  serd_node_check_padding(node);
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
  const SerdStringView result = {(const char*)(node + 1), node->length};

  return result;
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
