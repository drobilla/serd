/*
  Copyright 2011 David Robillard <http://drobilla.net>

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

#include "serd_internal.h"

#include <stdlib.h>
#include <string.h>

#ifndef MIN
#    define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

struct SerdBulkSinkImpl {
	SerdSink sink;
	void*    stream;
	uint8_t* buf;
	size_t   size;
	size_t   block_size;
};

SERD_API
SerdBulkSink*
serd_bulk_sink_new(SerdSink sink, void* stream, size_t block_size)
{
	SerdBulkSink* bsink = (SerdBulkSink*)malloc(sizeof(SerdBulkSink));
	bsink->sink       = sink;
	bsink->stream     = stream;
	bsink->size       = 0;
	bsink->block_size = block_size;
	bsink->buf        = serd_bufalloc(block_size);
	return bsink;
}

SERD_API
void
serd_bulk_sink_free(SerdBulkSink* bsink)
{
	if (bsink) {
		// Flush any remaining output
		if (bsink->size > 0) {
			bsink->sink(bsink->buf, bsink->size, bsink->stream);
		}
		free(bsink->buf);
		free(bsink);
	}
}

SERD_API
size_t
serd_bulk_sink_write(const void* buf, size_t len, SerdBulkSink* bsink)
{
	const size_t orig_len = len;
	while (len > 0) {
		const size_t space = bsink->block_size - bsink->size;
		const size_t n     = MIN(space, len);

		// Write as much as possible into the remaining buffer space
		memcpy(bsink->buf + bsink->size, buf, n);
		bsink->size += n;
		buf           = (uint8_t*)buf + n;
		len          -= n;

		// Flush page if buffer is full
		if (bsink->size == bsink->block_size) {
			bsink->sink(bsink->buf, bsink->block_size, bsink->stream);
			bsink->size = 0;
		}
	}
	return orig_len;
}
