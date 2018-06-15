/*
  Copyright 2011-2018 David Robillard <http://drobilla.net>

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

#ifndef SERD_BYTE_SINK_H
#define SERD_BYTE_SINK_H

#include <stddef.h>
#include <string.h>

#include "serd/serd.h"

typedef struct SerdByteSinkImpl {
	SerdWriteFunc sink;
	void*         stream;
	char*         buf;
	size_t        size;
	size_t        block_size;
} SerdByteSink;

static inline SerdByteSink
serd_byte_sink_new(SerdWriteFunc sink, void* stream, size_t block_size)
{
	SerdByteSink bsink;
	bsink.sink       = sink;
	bsink.stream     = stream;
	bsink.size       = 0;
	bsink.block_size = block_size;
	bsink.buf        = ((block_size > 1)
	                    ? (char*)serd_bufalloc(block_size)
	                    : NULL);
	return bsink;
}

static inline void
serd_byte_sink_flush(SerdByteSink* bsink)
{
	if (bsink->block_size > 1 && bsink->size > 0) {
		bsink->sink(bsink->buf, 1, bsink->size, bsink->stream);
		bsink->size = 0;
	}
}

static inline void
serd_byte_sink_free(SerdByteSink* bsink)
{
	serd_byte_sink_flush(bsink);
	free(bsink->buf);
	bsink->buf = NULL;
}

static inline size_t
serd_byte_sink_write(const void* buf, size_t len, SerdByteSink* bsink)
{
	if (len == 0) {
		return 0;
	} else if (bsink->block_size == 1) {
		return bsink->sink(buf, 1, len, bsink->stream);
	}

	const size_t orig_len = len;
	while (len) {
		const size_t space = bsink->block_size - bsink->size;
		const size_t n     = MIN(space, len);

		// Write as much as possible into the remaining buffer space
		memcpy(bsink->buf + bsink->size, buf, n);
		bsink->size += n;
		buf          = (const char*)buf + n;
		len         -= n;

		// Flush page if buffer is full
		if (bsink->size == bsink->block_size) {
			bsink->sink(bsink->buf, 1, bsink->block_size, bsink->stream);
			bsink->size = 0;
		}
	}
	return orig_len;
}

#endif  // SERD_BYTE_SINK_H
