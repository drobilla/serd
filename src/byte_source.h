/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#ifndef SERD_BYTE_SOURCE_H
#define SERD_BYTE_SOURCE_H

#include "cursor.h"

#include "serd/serd.h"

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
  SerdCursor          cur;        ///< Cursor for error reporting
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
    ++source->cur.line;
    source->cur.col = 0;
    break;
  default:
    ++source->cur.col;
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

#endif // SERD_BYTE_SOURCE_H
