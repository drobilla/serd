// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "symbols.h"

#include <exess/exess.h>
#include <serd/binary.h>
#include <serd/node_flags.h>
#include <serd/object_view.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <zix/string_view.h>

#include <stddef.h>

static SerdStreamResult
result(const SerdStatus status, const size_t count)
{
  const SerdStreamResult result = {status, count};
  return result;
}

size_t
serd_binary_decoded_size(const SerdObjectView node)
{
  return !(node.flags & SERD_HAS_DATATYPE) ? 0U
         : zix_string_view_equals(node.meta.string, serd_symbols[XSD_HEXBINARY])
           ? exess_decoded_hex_size(node.string.length)
         : zix_string_view_equals(node.meta.string,
                                  serd_symbols[XSD_BASE64BINARY])
           ? exess_decoded_base64_size(node.string.length)
           : 0U;
}

SerdStreamResult
serd_binary_decode(const SerdObjectView node,
                   const size_t         buf_size,
                   void* const          buf)
{
  if (!(node.flags & SERD_HAS_DATATYPE)) {
    return result(SERD_BAD_ARG, 0U);
  }

  ExessVariableResult r = {EXESS_UNSUPPORTED, 0U, 0U};
  if (zix_string_view_equals(node.meta.string, serd_symbols[XSD_HEXBINARY])) {
    r = exess_read_hex(node.string.data, buf_size, buf);
  } else if (zix_string_view_equals(node.meta.string,
                                    serd_symbols[XSD_BASE64BINARY])) {
    r = exess_read_base64(node.string.data, buf_size, buf);
  } else {
    return result(SERD_BAD_ARG, 0U);
  }

  return r.status == EXESS_NO_SPACE ? result(SERD_NO_SPACE, r.write_count)
         : r.status                 ? result(SERD_BAD_SYNTAX, 0U)
                                    : result(SERD_SUCCESS, r.write_count);
}
