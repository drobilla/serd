// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BYTE_SOURCE_H
#define SERD_SRC_BYTE_SOURCE_H

#include "caret.h" // IWYU pragma: keep

#include "serd/attributes.h"
#include "serd/byte_source.h"
#include "serd/caret.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/stream.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  FROM_STRING,   ///< Reading from a user-provided buffer
  FROM_FILENAME, ///< Reading from a file we opened
  FROM_FUNCTION, ///< Reading from a user-provided function
} SerdByteSourceType;

struct SerdByteSourceImpl {
  SerdReadFunc        read_func;  ///< Read function (e.g. fread)
  SerdStreamErrorFunc error_func; ///< Error function (e.g. ferror)
  SerdStreamCloseFunc close_func; ///< Function for closing stream
  void*               stream;     ///< Stream (e.g. FILE)
  size_t              page_size;  ///< Number of bytes to read at a time
  size_t              buf_size;   ///< Number of bytes in file_buf
  SerdNode*           name;       ///< Name of stream (referenced by cur)
  SerdCaret           caret;      ///< File position for error reporting
  uint8_t*            file_buf;   ///< Buffer iff reading pages from a file
  const uint8_t*      read_buf;   ///< Pointer to file_buf or read_byte
  size_t              read_head;  ///< Offset into read_buf
  SerdByteSourceType  type;       ///< Type of input
  uint8_t             read_byte;  ///< 1-byte 'buffer' used when not paging
  bool                prepared;   ///< True iff prepared for reading
  bool                eof;        ///< True iff end of file reached
};

SerdStatus
serd_byte_source_prepare(SerdByteSource* source);

SerdStatus
serd_byte_source_page(SerdByteSource* source);

SERD_PURE_FUNC
static inline uint8_t
serd_byte_source_peek(SerdByteSource* source)
{
  assert(source->prepared);
  return source->read_buf[source->read_head];
}

static inline SerdStatus
serd_byte_source_advance(SerdByteSource* source)
{
  SerdStatus st      = SERD_SUCCESS;
  const bool was_eof = source->eof;

  switch (serd_byte_source_peek(source)) {
  case '\n':
    ++source->caret.line;
    source->caret.col = 0;
    break;
  default:
    ++source->caret.col;
  }

  if (source->type != FROM_STRING) {
    if (++source->read_head >= source->buf_size) {
      st = serd_byte_source_page(source);
    }
  } else if (!source->eof) {
    source->eof = source->read_buf[++source->read_head] == '\0';
  }

  return (was_eof && source->eof) ? SERD_FAILURE : st;
}

#endif // SERD_SRC_BYTE_SOURCE_H
