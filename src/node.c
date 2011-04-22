/*
  Copyright 2011 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED  AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/

#include <stdlib.h>
#include <string.h>

#include "serd_internal.h"

SERD_API
SerdNode
serd_node_from_string(SerdType type, const uint8_t* buf)
{
	size_t buf_n_bytes;
	const size_t buf_n_chars = serd_strlen(buf, &buf_n_bytes);
	SerdNode ret = { type, buf_n_bytes, buf_n_chars, buf };
	return ret;
}

SERD_API
SerdNode
serd_node_copy(const SerdNode* node)
{
	SerdNode copy = *node;
	uint8_t* buf  = malloc(copy.n_bytes);
	memcpy(buf, node->buf, copy.n_bytes);
	copy.buf = buf;
	return copy;
}

static size_t
serd_uri_string_length(const SerdURI* uri)
{
	size_t len = uri->path_base.len;

#define ADD_LEN(field, n_delims) \
	if ((field).len) { len += (field).len + (n_delims); }

	ADD_LEN(uri->path,      1);  // + possible leading `/'
	ADD_LEN(uri->scheme,    1);  // + trailing `:'
	ADD_LEN(uri->authority, 2);  // + leading `//'
	ADD_LEN(uri->query,     1);  // + leading `?'
	ADD_LEN(uri->fragment,  1);  // + leading `#'

	// Add 2 for authority // prefix (added even though authority.len = 0)
	return len + 2; // + 2 for authority //
}

static size_t
string_sink(const void* buf, size_t len, void* stream)
{
	uint8_t** ptr = (uint8_t**)stream;
	memcpy(*ptr, buf, len);
	*ptr += len;
	return len;
}

SERD_API
SerdNode
serd_node_new_uri_from_node(const SerdNode* uri_node,
                            const SerdURI*  base,
                            SerdURI*        out)
{
	return serd_node_new_uri_from_string(uri_node->buf, base, out);
}

SERD_API
SerdNode
serd_node_new_uri_from_string(const uint8_t* str,
                              const SerdURI* base,
                              SerdURI*       out)
{
	if (str[0] == '\0') {
		return serd_node_new_uri(base, NULL, out);  // Empty URI => Base URI
	} else {
		SerdURI uri;
		if (serd_uri_parse(str, &uri)) {
			return serd_node_new_uri(&uri, base, out);  // Resolve/Serialise
		}
	}
	return SERD_NODE_NULL;
}

SERD_API
SerdNode
serd_node_new_uri(const SerdURI* uri, const SerdURI* base, SerdURI* out)
{
	SerdURI abs_uri = *uri;
	if (base) {
		serd_uri_resolve(uri, base, &abs_uri);
	}

	const size_t len = serd_uri_string_length(&abs_uri);
	uint8_t*     buf = malloc(len + 1);

	SerdNode node = { SERD_URI, len + 1, len, buf };  // FIXME: UTF-8

	uint8_t*     ptr        = buf;
	const size_t actual_len = serd_uri_serialise(&abs_uri, string_sink, &ptr);

	buf[actual_len] = '\0';
	node.n_bytes    = actual_len + 1;
	node.n_chars    = actual_len;

	// FIXME: double parse
	if (!serd_uri_parse(buf, out)) {
		fprintf(stderr, "error parsing URI\n");
		return SERD_NODE_NULL;
	}

	return node;
}

SERD_API
void
serd_node_free(SerdNode* node)
{
	free((uint8_t*)node->buf);
}
