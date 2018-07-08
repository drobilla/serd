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

#include "serd_internal.h"
#include "system.h"

#include "serd/serd.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct SerdByteSinkImpl {
	SerdWriteFunc sink;
	void*         stream;
	char*         buf;
	size_t        size;
	size_t        block_size;
};

SerdByteSink*
serd_byte_sink_new(SerdWriteFunc write_func, void* stream, size_t block_size)
{
	SerdByteSink* sink = (SerdByteSink*)calloc(1, sizeof(SerdByteSink));

	sink->sink       = write_func;
	sink->stream     = stream;
	sink->block_size = block_size;
	sink->buf =
		((block_size > 1) ? (char*)serd_allocate_buffer(block_size) : NULL);
	return sink;
}

size_t
serd_byte_sink_write(const void*   buf,
                     size_t        size,
                     size_t        nmemb,
                     SerdByteSink* sink)
{
	assert(size == 1);
	(void)size;

	if (nmemb == 0) {
		return 0;
	} else if (sink->block_size == 1) {
		return sink->sink(buf, 1, nmemb, sink->stream);
	}

	const size_t orig_len = nmemb;
	while (nmemb) {
		const size_t space = sink->block_size - sink->size;
		const size_t n     = MIN(space, nmemb);

		// Write as much as possible into the remaining buffer space
		memcpy(sink->buf + sink->size, buf, n);
		sink->size += n;
		buf = (const char*)buf + n;
		nmemb -= n;

		// Flush page if buffer is full
		if (sink->size == sink->block_size) {
			sink->sink(sink->buf, 1, sink->block_size, sink->stream);
			sink->size = 0;
		}
	}

	return orig_len;
}

void
serd_byte_sink_flush(SerdByteSink* sink)
{
	if (sink->block_size > 1 && sink->size > 0) {
		sink->sink(sink->buf, 1, sink->size, sink->stream);
		sink->size = 0;
	}
}

void
serd_byte_sink_free(SerdByteSink* sink)
{
	serd_byte_sink_flush(sink);
	free(sink->buf);
	free(sink);
}
