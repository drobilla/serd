/* Serd, a lightweight RDF syntax library.
 * Copyright 2011 David Robillard <d@drobilla.net>
 *
 * Serd is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Serd is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include "serd_internal.h"

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

	return len;
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
serd_node_new_uri(const SerdURI* uri, SerdURI* out)
{
	const size_t len = serd_uri_string_length(uri);
	uint8_t*     buf = malloc(len + 1);

	SerdNode node = { SERD_URI, len + 1, len, buf };  // FIXME: UTF-8

	uint8_t*     ptr        = buf;
	const size_t actual_len = serd_uri_serialise(uri, string_sink, &ptr);

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
	free((uint8_t*)node->buf);  // FIXME: ick, const cast
}
