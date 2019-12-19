/*
  Copyright 2011-2016 David Robillard <http://drobilla.net>

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

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#    ifndef isnan
#        define isnan(x) _isnan(x)
#    endif
#    ifndef isinf
#        define isinf(x) (!_finite(x))
#    endif
#endif

SerdNode*
serd_node_malloc(size_t n_bytes, SerdNodeFlags flags, SerdType type)
{
	SerdNode* node = (SerdNode*)calloc(1, sizeof(SerdNode) + n_bytes + 1);
	node->n_bytes = 0;
	node->flags   = flags;
	node->type    = type;
	return node;
}

char*
serd_node_buffer(SerdNode* node)
{
	return (char*)(node + 1);
}

void
serd_node_set(SerdNode** dst, const SerdNode* src)
{
	if (src) {
		if (!(*dst) || (*dst)->n_bytes < src->n_bytes) {
			(*dst) = (SerdNode*)realloc(*dst, sizeof(SerdNode) + src->n_bytes + 1);
		}

		memcpy(*dst, src, sizeof(SerdNode) + src->n_bytes + 1);
	} else if (*dst) {
		memset(*dst, 0, sizeof(SerdNode));
	}
}

SerdNode*
serd_node_new_string(SerdType type, const char* str)
{
	if (!str) {
		return NULL;
	}

	uint32_t     flags   = 0;
	const size_t n_bytes = serd_strlen(str, &flags);
	SerdNode*    node    = serd_node_malloc(n_bytes, flags, type);
	memcpy(serd_node_buffer(node), str, n_bytes);
	node->n_bytes = n_bytes;
	return node;
}

SerdNode*
serd_node_new_substring(SerdType type, const char* str, const size_t len)
{
	if (!str) {
		return NULL;
	}

	uint32_t     flags   = 0;
	const size_t n_bytes = serd_substrlen(str, len, &flags);
	SerdNode*    node    = serd_node_malloc(n_bytes, flags, type);
	memcpy(serd_node_buffer(node), str, n_bytes);
	node->n_bytes = n_bytes;
	return node;
}

SerdNode*
serd_node_copy(const SerdNode* node)
{
	if (!node) {
		return NULL;
	}

	const size_t size = sizeof(SerdNode) + node->n_bytes + 1;
	SerdNode*    copy = (SerdNode*)malloc(size);
	memcpy(copy, node, size);
	return copy;
}

bool
serd_node_equals(const SerdNode* a, const SerdNode* b)
{
	return (a == b) ||
	       (a && b && a->type == b->type && a->n_bytes == b->n_bytes &&
	        !memcmp(serd_node_get_string(a),
	                serd_node_get_string(b),
	                a->n_bytes));
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
	char** ptr = (char**)stream;
	memcpy(*ptr, buf, len);
	*ptr += len;
	return len;
}

SerdNode*
serd_node_new_uri_from_node(const SerdNode* uri_node,
                            const SerdURI*  base,
                            SerdURI*        out)
{
	const char* uri_str = serd_node_get_string(uri_node);
	return (uri_node->type == SERD_URI && uri_str)
		? serd_node_new_uri_from_string(uri_str, base, out)
		: NULL;
}

SerdNode*
serd_node_new_uri_from_string(const char*    str,
                              const SerdURI* base,
                              SerdURI*       out)
{
	if (!str || str[0] == '\0') {
		// Empty URI => Base URI, or nothing if no base is given
		return base ? serd_node_new_uri(base, NULL, out) : NULL;
	}

	SerdURI uri;
	serd_uri_parse(str, &uri);
	return serd_node_new_uri(&uri, base, out);  // Resolve/Serialise
}

static inline bool
is_uri_path_char(const char c)
{
	if (is_alpha(c) || is_digit(c)) {
		return true;
	}
	switch (c) {
	case '-': case '.': case '_': case '~':	 // unreserved
	case ':': case '@':	 // pchar
	case '/':  // separator
	// sub-delims
	case '!': case '$': case '&': case '\'': case '(': case ')':
	case '*': case '+': case ',': case ';': case '=':
		return true;
	default:
		return false;
	}
}

SerdNode*
serd_node_new_file_uri(const char* path,
                       const char* hostname,
                       SerdURI*    out,
                       bool        escape)
{
	const size_t path_len     = strlen(path);
	const size_t hostname_len = hostname ? strlen(hostname) : 0;
	const bool   evil         = is_windows_path(path);
	size_t       uri_len      = 0;
	char*        uri          = NULL;

	if (path[0] == '/' || is_windows_path(path)) {
		uri_len = strlen("file://") + hostname_len + evil;
		uri = (char*)malloc(uri_len + 1);
		snprintf(uri, uri_len + 1, "file://%s%s",
		         hostname ? hostname : "", evil ? "/" : "");
	}

	SerdBuffer buffer = { uri, uri_len };
	for (size_t i = 0; i < path_len; ++i) {
		if (evil && path[i] == '\\') {
			serd_buffer_sink("/", 1, &buffer);
		} else if (path[i] == '%') {
			serd_buffer_sink("%%", 2, &buffer);
		} else if (!escape || is_uri_path_char(path[i])) {
			serd_buffer_sink(path + i, 1, &buffer);
		} else {
			char escape_str[4] = { '%', 0, 0, 0 };
			snprintf(escape_str + 1, sizeof(escape_str) - 1, "%X", path[i]);
			serd_buffer_sink(escape_str, 3, &buffer);
		}
	}
	serd_buffer_sink_finish(&buffer);

	SerdNode* node = serd_node_new_substring(
		SERD_URI, (const char*)buffer.buf, buffer.len);
	if (out) {
		serd_uri_parse(serd_node_buffer(node), out);
	}

	free(buffer.buf);
	return node;
}

SerdNode*
serd_node_new_uri(const SerdURI* uri, const SerdURI* base, SerdURI* out)
{
	SerdURI abs_uri = *uri;
	if (base) {
		serd_uri_resolve(uri, base, &abs_uri);
	}

	const size_t len        = serd_uri_string_length(&abs_uri);
	SerdNode*    node       = serd_node_malloc(len, 0, SERD_URI);
	char*        ptr        = serd_node_buffer(node);
	const size_t actual_len = serd_uri_serialise(&abs_uri, string_sink, &ptr);

	serd_node_buffer(node)[actual_len] = '\0';
	node->n_bytes = actual_len;

	if (out) {
		serd_uri_parse(serd_node_buffer(node), out);  // TODO: avoid double parse
	}

	return node;
}

SerdNode*
serd_node_new_relative_uri(const SerdURI* uri,
                           const SerdURI* base,
                           const SerdURI* root,
                           SerdURI*       out)
{
	const size_t uri_len    = serd_uri_string_length(uri);
	const size_t base_len   = serd_uri_string_length(base);
	SerdNode*    node       = serd_node_malloc(uri_len + base_len, 0, SERD_URI);
	char*        ptr        = serd_node_buffer(node);
	const size_t actual_len = serd_uri_serialise_relative(
		uri, base, root, string_sink, &ptr);

	serd_node_buffer(node)[actual_len] = '\0';
	node->n_bytes = actual_len;

	if (out) {
		serd_uri_parse(serd_node_buffer(node), out);  // TODO: avoid double parse
	}

	return node;
}

static inline unsigned
serd_digits(double abs)
{
	const double lg = ceil(log10(floor(abs) + 1.0));
	return lg < 1.0 ? 1U : (unsigned)lg;
}

SerdNode*
serd_node_new_decimal(double d, unsigned frac_digits)
{
	if (isnan(d) || isinf(d)) {
		return NULL;
	}

	const double    abs_d      = fabs(d);
	const unsigned  int_digits = serd_digits(abs_d);
	const size_t    len        = int_digits + frac_digits + 3;
	SerdNode* const node       = serd_node_malloc(len, 0, SERD_LITERAL);
	char* const     buf        = serd_node_buffer(node);
	const double    int_part   = floor(abs_d);

	// Point s to decimal point location
	char* s = buf + int_digits;
	if (d < 0.0) {
		*buf = '-';
		++s;
	}

	// Write integer part (right to left)
	char*    t   = s - 1;
	uint64_t dec = (uint64_t)int_part;
	do {
		*t-- = (char)('0' + dec % 10);
	} while ((dec /= 10) > 0);


	*s++ = '.';

	// Write fractional part (right to left)
	double frac_part = fabs(d - int_part);
	if (frac_part < DBL_EPSILON) {
		*s++ = '0';
		node->n_bytes = (size_t)(s - buf);
	} else {
		uint64_t frac = (uint64_t)llround(frac_part * pow(10.0, (int)frac_digits));
		s += frac_digits - 1;
		unsigned i = 0;

		// Skip trailing zeros
		for (; i < frac_digits - 1 && !(frac % 10); ++i, --s, frac /= 10) {}

		node->n_bytes = (size_t)(s - buf) + 1u;

		// Write digits from last trailing zero to decimal point
		for (; i < frac_digits; ++i) {
			*s-- = (char)('0' + (frac % 10));
			frac /= 10;
		}
	}

	return node;
}

SerdNode*
serd_node_new_integer(int64_t i)
{
	int64_t        abs_i  = (i < 0) ? -i : i;
	const unsigned digits = serd_digits((double)abs_i);
	SerdNode*      node   = serd_node_malloc(digits + 2, 0, SERD_LITERAL);
	char*          buf    = serd_node_buffer(node);

	// Point s to the end
	char* s = buf + digits - 1;
	if (i < 0) {
		*buf = '-';
		++s;
	}

	node->n_bytes = (size_t)(s - buf) + 1u;

	// Write integer part (right to left)
	do {
		*s-- = (char)('0' + (abs_i % 10));
	} while ((abs_i /= 10) > 0);

	return node;
}

/**
   Base64 encoding table.
   @see <a href="http://tools.ietf.org/html/rfc3548#section-3">RFC3986 S3</a>.
*/
static const uint8_t b64_map[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
   Encode 3 raw bytes to 4 base64 characters.
*/
static inline void
encode_chunk(uint8_t out[4], const uint8_t in[3], size_t n_in)
{
	out[0] = b64_map[in[0] >> 2];
	out[1] = b64_map[((in[0] & 0x03) << 4) | ((in[1] & 0xF0) >> 4)];
	out[2] = ((n_in > 1)
	          ? (b64_map[((in[1] & 0x0F) << 2) | ((in[2] & 0xC0) >> 6)])
	          : (uint8_t)'=');
	out[3] = ((n_in > 2) ? b64_map[in[2] & 0x3F] : (uint8_t)'=');
}

SerdNode*
serd_node_new_blob(const void* buf, size_t size, bool wrap_lines)
{
	if (!buf || !size) {
		return NULL;
	}

	const size_t len  = (size + 2) / 3 * 4 + (wrap_lines * ((size - 1) / 57));
	SerdNode*    node = serd_node_malloc(len + 1, 0, SERD_LITERAL);
	uint8_t*     str  = (uint8_t*)serd_node_buffer(node);
	for (size_t i = 0, j = 0; i < size; i += 3, j += 4) {
		uint8_t in[4] = { 0, 0, 0, 0 };
		size_t  n_in  = MIN(3, size - i);
		memcpy(in, (const uint8_t*)buf + i, n_in);

		if (wrap_lines && i > 0 && (i % 57) == 0) {
			str[j++] = '\n';
			node->flags |= SERD_HAS_NEWLINE;
		}

		encode_chunk(str + j, in, n_in);
	}
	node->n_bytes = len;
	return node;
}

SerdType
serd_node_get_type(const SerdNode* node)
{
	return node->type;
}

const char*
serd_node_get_string(const SerdNode* node)
{
	return node ? (const char*)(node + 1) : NULL;
}

size_t
serd_node_get_length(const SerdNode* node)
{
	return node ? node->n_bytes : 0;
}

SerdNodeFlags
serd_node_get_flags(const SerdNode* node)
{
	return node->flags;
}

void
serd_node_free(SerdNode* node)
{
	free(node);
}
