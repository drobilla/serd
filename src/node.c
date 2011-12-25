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

#include <math.h>
#include <float.h>

SERD_API
SerdNode
serd_node_from_string(SerdType type, const uint8_t* buf)
{
	uint32_t     flags       = 0;
	size_t       buf_n_bytes = 0;
	const size_t buf_n_chars = serd_strlen(buf, &buf_n_bytes, &flags);
	SerdNode ret = { buf, buf_n_bytes, buf_n_chars, flags, type };
	return ret;
}

SERD_API
SerdNode
serd_node_copy(const SerdNode* node)
{
	if (!node || !node->buf) {
		return SERD_NODE_NULL;
	}

	SerdNode copy = *node;
	uint8_t* buf  = malloc(copy.n_bytes + 1);
	memcpy(buf, node->buf, copy.n_bytes + 1);
	copy.buf = buf;
	return copy;
}

SERD_API
bool
serd_node_equals(const SerdNode* a, const SerdNode* b)
{
	return (a == b)
		|| (a->type == b->type
		    && a->n_bytes == b->n_bytes
		    && a->n_chars == b->n_chars
		    && ((a->buf == b->buf) || !memcmp((const char*)a->buf,
		                                      (const char*)b->buf,
		                                      a->n_bytes + 1)));
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

	return len + 2;  // + 2 for authority `//'
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
	if (!str || str[0] == '\0') {
		return serd_node_new_uri(base, NULL, out);  // Empty URI => Base URI
	} else {
		SerdURI uri;
		if (!serd_uri_parse(str, &uri)) {
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

	SerdNode node = { buf, len, len, 0, SERD_URI };  // FIXME: UTF-8

	uint8_t*     ptr        = buf;
	const size_t actual_len = serd_uri_serialise(&abs_uri, string_sink, &ptr);

	buf[actual_len] = '\0';
	node.n_bytes    = actual_len;
	node.n_chars    = actual_len;

	serd_uri_parse(buf, out);  // TODO: cleverly avoid double parse

	return node;
}

SERD_API
SerdNode
serd_node_new_decimal(double d, unsigned frac_digits)
{
	const double abs_d      = fabs(d);
	const long   int_digits = (long)fmax(1.0, ceil(log10(abs_d)));
	char*        buf        = calloc(int_digits + frac_digits + 3, 1);
	SerdNode     node       = { (const uint8_t*)buf, 0, 0, 0, SERD_LITERAL };

	const double int_part  = floor(abs_d);

	// Point s to decimal point location
	char* s = buf + int_digits;
	if (d < 0.0) {
		*buf = '-';
		++s;
	}

	// Write integer part (right to left)
	char* t   = s - 1;
	long  dec = (long)int_part;
	do {
		*t-- = '0' + (dec % 10);
	} while ((dec /= 10) > 0);

	*s++ = '.';

	// Write fractional part (right to left)
	double frac_part = fabs(d - int_part);
	if (frac_part < DBL_EPSILON) {
		*s++ = '0';
		node.n_bytes = node.n_chars = (s - buf);
	} else {
		long frac = lrint(frac_part * pow(10, frac_digits));
		s += frac_digits - 1;
		unsigned i = 0;

		// Skip trailing zeros
		for (; i < frac_digits && (frac % 10 == 0); ++i, --s, frac /= 10) {}

		node.n_bytes = node.n_chars = (s - buf) + 1;

		// Write digits from last trailing zero to decimal point
		for (; i < frac_digits; ++i) {
			*s-- = '0' + (frac % 10);
			frac /= 10;
		}
	}

	return node;
}

SERD_API
SerdNode
serd_node_new_integer(long i)
{
	long       abs_i  = labs(i);
	const long digits = (long)fmax(1.0, ceil(log10((double)abs_i + 1)));
	char*      buf    = calloc(digits + 2, 1);
	SerdNode   node   = { (const uint8_t*)buf, 0, 0, 0, SERD_LITERAL };

	// Point s to the end
	char* s = buf + digits - 1;
	if (i < 0) {
		*buf = '-';
		++s;
	}

	node.n_bytes = node.n_chars = (s - buf) + 1;

	// Write integer part (right to left)
	do {
		*s-- = '0' + (abs_i % 10);
	} while ((abs_i /= 10) > 0);

	return node;
}

SERD_API
void
serd_node_free(SerdNode* node)
{
	if (node->buf) {
		free((uint8_t*)node->buf);
		node->buf = NULL;
	}
}
