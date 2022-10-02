// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_BYTE_SOURCE_H
#define SERD_SRC_BYTE_SOURCE_H

#include "serd/status.h"
#include "serd/stream.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int (*SerdCloseFunc)(void*);

typedef struct {
  const char* filename;
  unsigned    line;
  unsigned    col;
} Cursor;

typedef struct {
  SerdReadFunc   read_func;   ///< Read function (e.g. fread)
  SerdErrorFunc  error_func;  ///< Error function (e.g. ferror)
  SerdCloseFunc  close_func;  ///< Function for closing stream
  void*          stream;      ///< Stream (e.g. FILE)
  size_t         page_size;   ///< Number of bytes to read at a time
  size_t         buf_size;    ///< Number of bytes in file_buf
  Cursor         cur;         ///< Cursor for error reporting
  uint8_t*       file_buf;    ///< Buffer iff reading pages from a file
  const uint8_t* read_buf;    ///< Pointer to file_buf or read_byte
  size_t         read_head;   ///< Offset into read_buf
  uint8_t        read_byte;   ///< 1-byte 'buffer' used when not paging
  bool           from_stream; ///< True iff reading from `stream`
  bool           prepared;    ///< True iff prepared for reading
  bool           eof;         ///< True iff end of file reached
} SerdByteSource;

SerdStatus
serd_byte_source_open_string(SerdByteSource* source, const char* utf8);

SerdStatus
serd_byte_source_open_source(SerdByteSource* source,
                             SerdReadFunc    read_func,
                             SerdErrorFunc   error_func,
                             SerdCloseFunc   close_func,
                             void*           stream,
                             const char*     name,
                             size_t          page_size);

SerdStatus
serd_byte_source_close(SerdByteSource* source);

SerdStatus
serd_byte_source_prepare(SerdByteSource* source);

SerdStatus
serd_byte_source_page(SerdByteSource* source);

static inline ZIX_PURE_FUNC uint8_t
serd_byte_source_peek(SerdByteSource* source)
{
  assert(source->prepared);
  return source->read_buf[source->read_head];
}

static inline SerdStatus
serd_byte_source_advance(SerdByteSource* source)
{
  SerdStatus st = SERD_SUCCESS;

  if (serd_byte_source_peek(source) == '\n') {
    ++source->cur.line;
    source->cur.col = 0;
  } else {
    ++source->cur.col;
  }

  const bool was_eof = source->eof;
  if (source->from_stream) {
    source->eof = false;
    if (source->page_size > 1) {
      if (++source->read_head == source->page_size) {
        st = serd_byte_source_page(source);
      } else if (source->read_head == source->buf_size) {
        source->eof = true;
      }
    } else {
      if (!source->read_func(&source->read_byte, 1, 1, source->stream)) {
        source->eof = true;
        st =
          source->error_func(source->stream) ? SERD_BAD_STREAM : SERD_FAILURE;
      }
    }
  } else if (!source->eof) {
    ++source->read_head; // Move to next character in string
    if (source->read_buf[source->read_head] == '\0') {
      source->eof = true;
    }
  }

  return (was_eof && source->eof) ? SERD_FAILURE : st;
}

#endif // SERD_SRC_BYTE_SOURCE_H
