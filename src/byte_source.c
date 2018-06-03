/*
  Copyright 2011-2017 David Robillard <http://drobilla.net>

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

#include "byte_source.h"

#include "system.h"

#include "serd/serd.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

SerdStatus
serd_byte_source_page(SerdByteSource* source)
{
	source->read_head = 0;
	const size_t n_read = source->read_func(
		source->file_buf, 1, source->page_size, source->stream);
	if (n_read == 0) {
		source->file_buf[0] = '\0';
		source->eof         = true;
		return (source->error_func(source->stream)
		        ? SERD_ERR_UNKNOWN : SERD_FAILURE);
	} else if (n_read < source->page_size) {
		source->file_buf[n_read] = '\0';
		source->buf_size         = n_read;
	}
	return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_open_source(SerdByteSource*     source,
                             SerdReadFunc        read_func,
                             SerdStreamErrorFunc error_func,
                             SerdStreamCloseFunc close_func,
                             void*               stream,
                             const SerdNode*     name,
                             size_t              page_size)
{
	memset(source, '\0', sizeof(*source));
	source->read_func   = read_func;
	source->error_func  = error_func;
	source->close_func  = close_func;
	source->stream      = stream;
	source->page_size   = page_size;
	source->buf_size    = page_size;
	source->name        = serd_node_copy(name);
	source->from_stream = true;

	const SerdCursor cur = { source->name, 1, 1 };
	source->cur = cur;

	if (page_size > 1) {
		source->file_buf = (uint8_t*)serd_allocate_buffer(page_size);
		source->read_buf = source->file_buf;
		memset(source->file_buf, '\0', page_size);
	} else {
		source->read_buf = &source->read_byte;
	}

	return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_prepare(SerdByteSource* source)
{
	source->prepared = true;
	if (source->from_stream) {
		if (source->page_size > 1) {
			return serd_byte_source_page(source);
		} else if (source->from_stream) {
			return serd_byte_source_advance(source);
		}
	}
	return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_open_string(SerdByteSource* source,
                             const char*     utf8,
                             const SerdNode* name)
{
	memset(source, '\0', sizeof(*source));

	source->name     = name ? serd_node_copy(name) : serd_new_string("string");
	source->read_buf = (const uint8_t*)utf8;

	const SerdCursor cur = {source->name, 1, 1};
	source->cur          = cur;

	return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_close(SerdByteSource* source)
{
	SerdStatus st = SERD_SUCCESS;
	if (source->close_func) {
		st = source->close_func(source->stream) ? SERD_ERR_UNKNOWN
		                                        : SERD_SUCCESS;
	}
	if (source->page_size > 1) {
		free(source->file_buf);
	}
	serd_node_free(source->name);
	memset(source, '\0', sizeof(*source));
	return st;
}
