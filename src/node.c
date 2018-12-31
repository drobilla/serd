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

#include "node.h"

#include "serd_internal.h"
#include "static_nodes.h"
#include "string_utils.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t serd_node_align = sizeof(SerdNode);

static SerdNode*
serd_new_from_uri(const SerdURI* uri, const SerdURI* base);

static size_t
serd_node_pad_size(const size_t n_bytes)
{
	const size_t pad = serd_node_align - (n_bytes + 2) % serd_node_align;
	const size_t size = n_bytes + 2 + pad;
	assert(size % serd_node_align == 0);
	return size;
}

static const SerdNode*
serd_node_get_meta_c(const SerdNode* node)
{
	return node + 1 + (serd_node_pad_size(node->n_bytes) / serd_node_align);
}

static SerdNode*
serd_node_get_meta(SerdNode* node)
{
	return node + 1 + (serd_node_pad_size(node->n_bytes) / serd_node_align);
}

static const SerdNode*
serd_node_maybe_get_meta_c(const SerdNode* node)
{
	return (node->flags & (SERD_HAS_LANGUAGE | SERD_HAS_DATATYPE))
		? (node + 1 + (serd_node_pad_size(node->n_bytes) / serd_node_align))
		: NULL;
}

static void
serd_node_check_padding(const SerdNode* node)
{
	(void)node;
#ifndef NDEBUG
	if (node) {
		const size_t unpadded_size = node->n_bytes;
		const size_t padded_size   = serd_node_pad_size(node->n_bytes);
		for (size_t i = 0; i < padded_size - unpadded_size; ++i) {
			assert(serd_node_buffer_c(node)[unpadded_size + i] == '\0');
		}

		serd_node_check_padding(serd_node_maybe_get_meta_c(node));
	}
#endif
}

size_t
serd_node_total_size(const SerdNode* node)
{
	return node ? (sizeof(SerdNode) + serd_node_pad_size(node->n_bytes) +
	               serd_node_total_size(serd_node_maybe_get_meta_c(node)))
	            : 0;
}

SerdNode*
serd_node_malloc(size_t n_bytes, SerdNodeFlags flags, SerdNodeType type)
{
	const size_t size = sizeof(SerdNode) + serd_node_pad_size(n_bytes);
	SerdNode*    node = (SerdNode*)calloc(1, size);
	node->n_bytes = 0;
	node->flags   = flags;
	node->type    = type;
	assert((intptr_t)node % serd_node_align == 0);
	return node;
}

void
serd_node_set(SerdNode** dst, const SerdNode* src)
{
	if (src) {
		const size_t size = serd_node_total_size(src);
		if (!(*dst) || serd_node_total_size(*dst) < size) {
			(*dst) = (SerdNode*)realloc(*dst, size);
		}

		memcpy(*dst, src, size);
	} else if (*dst) {
		memset(*dst, 0, sizeof(SerdNode));
	}
}

SerdNode*
serd_new_simple_node(SerdNodeType type, const char* str, const size_t len)
{
	if (!str) {
		return NULL;
	} else if (type != SERD_BLANK && type != SERD_CURIE && type != SERD_URI) {
		return NULL;
	}

	SerdNode* node = serd_node_malloc(len, 0, type);
	memcpy(serd_node_buffer(node), str, len);
	node->n_bytes = len;
	serd_node_check_padding(node);
	return node;
}

SerdNode*
serd_new_string(const char* str)
{
	if (!str) {
		return NULL;
	}

	SerdNodeFlags flags   = 0;
	const size_t  n_bytes = serd_strlen(str, &flags);
	SerdNode*     node    = serd_node_malloc(n_bytes, flags, SERD_LITERAL);
	memcpy(serd_node_buffer(node), str, n_bytes);
	node->n_bytes = n_bytes;
	serd_node_check_padding(node);
	return node;
}

SerdNode*
serd_new_substring(const char* str, const size_t len)
{
	if (!str) {
		return NULL;
	}

	SerdNodeFlags flags   = 0;
	const size_t  n_bytes = serd_substrlen(str, len, &flags);
	SerdNode*     node    = serd_node_malloc(n_bytes, flags, SERD_LITERAL);
	memcpy(serd_node_buffer(node), str, n_bytes);
	node->n_bytes = n_bytes;
	return node;
}

/// Internal pre-measured implementation of serd_new_plain_literal
static SerdNode*
serd_new_plain_literal_i(const char*   str,
                         const size_t  str_len,
                         SerdNodeFlags flags,
                         const char*   lang,
                         const size_t  lang_len)
{
	assert(str);
	assert(lang);

	flags |= SERD_HAS_LANGUAGE;

	const size_t len       = serd_node_pad_size(str_len);
	const size_t total_len = len + sizeof(SerdNode) + lang_len;

	SerdNode* node = serd_node_malloc(total_len, flags, SERD_LITERAL);
	memcpy(serd_node_buffer(node), str, str_len);
	node->n_bytes = str_len;

	SerdNode* lang_node = node + 1 + (len / serd_node_align);
	lang_node->type     = SERD_LITERAL;
	lang_node->n_bytes  = lang_len;
	memcpy(serd_node_buffer(lang_node), lang, lang_len);
	serd_node_check_padding(lang_node);

	serd_node_check_padding(node);
	return node;
}

/// Internal pre-measured implementation of serd_new_typed_literal
static SerdNode*
serd_new_typed_literal_i(const char*   str,
                         const size_t  str_len,
                         SerdNodeFlags flags,
                         const char*   datatype_uri,
                         const size_t  datatype_uri_len)
{
	assert(str);
	assert(datatype_uri);
	assert(strcmp(datatype_uri, NS_RDF "langString"));

	flags |= SERD_HAS_DATATYPE;

	const size_t len       = serd_node_pad_size(str_len);
	const size_t total_len = len + sizeof(SerdNode) + datatype_uri_len;

	SerdNode* node = serd_node_malloc(total_len, flags, SERD_LITERAL);
	memcpy(serd_node_buffer(node), str, str_len);
	node->n_bytes = str_len;

	SerdNode* datatype_node = node + 1 + (len / serd_node_align);
	datatype_node->n_bytes  = datatype_uri_len;
	datatype_node->type     = SERD_URI;
	memcpy(serd_node_buffer(datatype_node), datatype_uri, datatype_uri_len);
	serd_node_check_padding(datatype_node);

	serd_node_check_padding(node);
	return node;
}

SerdNode*
serd_new_literal(const char*  str,
                 const size_t str_len,
                 const char*  datatype_uri,
                 const size_t datatype_uri_len,
                 const char*  lang,
                 const size_t lang_len)
{
	if (!str ||
	    (lang && datatype_uri && strcmp(datatype_uri, NS_RDF "langString"))) {
		return NULL;
	}

	SerdNodeFlags flags = 0;
	serd_substrlen(str, str_len, &flags);

	if (lang) {
		return serd_new_plain_literal_i(str, str_len, flags, lang, lang_len);
	} else if (datatype_uri) {
		return serd_new_typed_literal_i(
		        str, str_len, flags, datatype_uri, datatype_uri_len);
	} else {
		return serd_new_substring(str, str_len);
	}
}

SerdNode*
serd_new_plain_literal(const char* str, const char* lang)
{
	if (!str) {
		return NULL;
	} else if (!lang) {
		return serd_new_string(str);
	}

	SerdNodeFlags flags    = 0;
	const size_t  str_len  = serd_strlen(str, &flags);
	const size_t  lang_len = strlen(lang);

	return serd_new_plain_literal_i(str, str_len, flags, lang, lang_len);
}

SerdNode*
serd_new_typed_literal(const char* str, const SerdNode* datatype)
{
	if (!str) {
		return NULL;
	} else if (!datatype) {
		return serd_new_string(str);
	} else if (!strcmp(serd_node_buffer_c(datatype), NS_RDF "langString") ||
	           serd_node_get_type(datatype) != SERD_URI) {
		return NULL;
	}

	SerdNodeFlags flags   = 0;
	const size_t  str_len = serd_strlen(str, &flags);

	return serd_new_typed_literal_i(str,
	                                str_len,
	                                flags,
	                                serd_node_get_string(datatype),
	                                serd_node_get_length(datatype));
}

SerdNode*
serd_new_blank(const char* str)
{
	return str ? serd_new_simple_node(SERD_BLANK, str, strlen(str)) : NULL;
}

SerdNode*
serd_new_curie(const char* str)
{
	return str ? serd_new_simple_node(SERD_CURIE, str, strlen(str)) : NULL;
}

SerdNode*
serd_new_uri(const char* str)
{
	return str ? serd_new_simple_node(SERD_URI, str, strlen(str)) : NULL;
}

/**
   Zero node padding.

   This is used for nodes which live in re-used stack memory during reading,
   which must be normalized before being passed to a sink so comparison will
   work correctly.
*/
void
serd_node_zero_pad(SerdNode* node)
{
	char*        buf         = serd_node_buffer(node);
	const size_t size        = node->n_bytes;
	const size_t padded_size = serd_node_pad_size(node->n_bytes);

	memset(buf + size, 0, padded_size - size);

	if (node->flags & (SERD_HAS_DATATYPE|SERD_HAS_LANGUAGE)) {
		serd_node_zero_pad(serd_node_get_meta(node));
	}

	if (node->flags & (SERD_HAS_DATATYPE|SERD_HAS_LANGUAGE)) {
		serd_node_zero_pad(serd_node_get_meta(node));
	}
}

SerdNode*
serd_node_copy(const SerdNode* node)
{
	if (!node) {
		return NULL;
	}

	const size_t size = serd_node_total_size(node);
	serd_node_check_padding(node);
	SerdNode*    copy = (SerdNode*)calloc(1, size + 3);
	memcpy(copy, node, size);
	return copy;
}

bool
serd_node_equals(const SerdNode* a, const SerdNode* b)
{
	if (a == b) {
		return true;
	} else if (!a || !b) {
		return false;
	}

	const size_t a_size = serd_node_total_size(a);
	return serd_node_total_size(b) == a_size && !memcmp(a, b, a_size);
}

int
serd_node_compare(const SerdNode* a, const SerdNode* b)
{
	if (a == b) {
		return 0;
	} else if (!a) {
		return -1;
	} else if (!b) {
		return 1;
	} else if (a->type != b->type) {
		return (a->type < b->type) ? -1 : 1;
	}

	const int cmp = strcmp(serd_node_get_string(a), serd_node_get_string(b));
	return cmp ? cmp
	           : serd_node_compare(serd_node_maybe_get_meta_c(a),
	                               serd_node_maybe_get_meta_c(b));
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
string_sink(const void* buf, size_t size, size_t nmemb, void* stream)
{
	char** ptr = (char**)stream;
	memcpy(*ptr, buf, size * nmemb);
	*ptr += size * nmemb;
	return nmemb;
}

SerdNode*
serd_new_resolved_uri(const char* str, const SerdNode* base)
{
	if (!base || base->type != SERD_URI) {
		return NULL;
	}

	SerdURI base_uri;
	serd_uri_parse(serd_node_get_string(base), &base_uri);
	return serd_new_resolved_uri_i(str, &base_uri);
}

SerdNode*
serd_node_resolve(const SerdNode* node, const SerdNode* base)
{
	if (!node || !base || node->type != SERD_URI || base->type != SERD_URI) {
		return NULL;
	}

	SerdURI uri;
	SerdURI base_uri;
	serd_uri_parse(serd_node_get_string(node), &uri);
	serd_uri_parse(serd_node_get_string(base), &base_uri);

	return serd_new_from_uri(&uri, &base_uri);
}

SerdNode*
serd_new_resolved_uri_i(const char* str, const SerdURI* base)
{
	SerdNode* result = NULL;
	if (!str || str[0] == '\0') {
		// Empty URI => Base URI, or nothing if no base is given
		result = base ? serd_new_from_uri(base, NULL) : NULL;
	} else {
		SerdURI uri;
		serd_uri_parse(str, &uri);
		result = serd_new_from_uri(&uri, base);
	}

	if (!serd_uri_string_has_scheme(serd_node_get_string(result))) {
		serd_node_free(result);
		return NULL;
	}

	return result;
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
serd_new_file_uri(const char* path, const char* hostname)
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
			serd_buffer_sink("/", 1, 1, &buffer);
		} else if (path[i] == '%') {
			serd_buffer_sink("%%", 1, 2, &buffer);
		} else if (is_uri_path_char(path[i])) {
			serd_buffer_sink(path + i, 1, 1, &buffer);
		} else {
			char escape_str[4] = { '%', 0, 0, 0 };
			snprintf(escape_str + 1, sizeof(escape_str) - 1, "%X", path[i]);
			serd_buffer_sink(escape_str, 1, 3, &buffer);
		}
	}
	serd_buffer_sink_finish(&buffer);

	SerdNode* node = serd_new_uri((const char*)buffer.buf);
	free(buffer.buf);
	serd_node_check_padding(node);
	return node;
}

static SerdNode*
serd_new_from_uri(const SerdURI* uri, const SerdURI* base)
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

	serd_node_check_padding(node);
	return node;
}

SerdNode*
serd_new_relative_uri(const char*     str,
                      const SerdNode* base,
                      const SerdNode* root)
{
	SerdURI uri      = SERD_URI_NULL;
	SerdURI base_uri = SERD_URI_NULL;
	SerdURI root_uri = SERD_URI_NULL;

	serd_uri_parse(str, &uri);
	if (base) {
		serd_uri_parse(serd_node_get_string(base), &base_uri);
	}
	if (root) {
		serd_uri_parse(serd_node_get_string(root), &root_uri);
	}

	const size_t uri_len    = serd_uri_string_length(&uri);
	const size_t base_len   = serd_uri_string_length(&base_uri);
	SerdNode*    node       = serd_node_malloc(uri_len + base_len, 0, SERD_URI);
	char*        ptr        = serd_node_buffer(node);
	const size_t actual_len = serd_uri_serialise_relative(
		&uri, &base_uri, root ? &root_uri : NULL, string_sink, &ptr);

	serd_node_buffer(node)[actual_len] = '\0';
	node->n_bytes = actual_len;

	serd_node_check_padding(node);
	return node;
}

static inline unsigned
serd_digits(double abs)
{
	const double lg = ceil(log10(floor(abs) + 1.0));
	return lg < 1.0 ? 1U : (unsigned)lg;
}

SerdNode*
serd_new_decimal(double d, unsigned frac_digits, const SerdNode* datatype)
{
	if (!isfinite(d)) {
		return NULL;
	}

	const SerdNode* type       = datatype ? datatype : &serd_xsd_decimal.node;
	const double    abs_d      = fabs(d);
	const unsigned  int_digits = serd_digits(abs_d);
	const size_t    len        = int_digits + frac_digits + 3;
	const size_t    type_len   = serd_node_total_size(type);
	const size_t    total_len  = len + type_len;

	SerdNode* const node =
		serd_node_malloc(total_len, SERD_HAS_DATATYPE, SERD_LITERAL);

	// Point s to decimal point location
	char* const  buf      = serd_node_buffer(node);
	const double int_part = floor(abs_d);
	char*        s        = buf + int_digits;
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

	memcpy(serd_node_get_meta(node), type, type_len);
	serd_node_check_padding(node);
	return node;
}

SerdNode*
serd_new_integer(int64_t i, const SerdNode* datatype)
{
	const SerdNode* type      = datatype ? datatype : &serd_xsd_integer.node;
	int64_t         abs_i     = (i < 0) ? -i : i;
	const unsigned  digits    = serd_digits((double)abs_i);
	const size_t    type_len  = serd_node_total_size(type);
	const size_t    total_len = digits + 2 + type_len;

	SerdNode* node =
		serd_node_malloc(total_len, SERD_HAS_DATATYPE, SERD_LITERAL);

	// Point s to the end
	char* buf = serd_node_buffer(node);
	char* s   = buf + digits - 1;
	if (i < 0) {
		*buf = '-';
		++s;
	}

	node->n_bytes = (size_t)(s - buf) + 1u;

	// Write integer part (right to left)
	do {
		*s-- = (char)('0' + (abs_i % 10));
	} while ((abs_i /= 10) > 0);

	memcpy(serd_node_get_meta(node), type, type_len);
	serd_node_check_padding(node);
	return node;
}

SerdNode*
serd_new_boolean(bool b)
{
	return serd_new_typed_literal(b ? "true" : "false", &serd_xsd_boolean.node);
}

SerdNode*
serd_new_blob(const void*     buf,
              size_t          size,
              bool            wrap_lines,
              const SerdNode* datatype)
{
	if (!buf || !size) {
		return NULL;
	}

	const SerdNode* type      = datatype ? datatype : &serd_xsd_base64Binary.node;
	const size_t    len       = serd_base64_encoded_length(size, wrap_lines);
	const size_t    type_len  = serd_node_total_size(type);
	const size_t    total_len = len + 1 + type_len;

	SerdNode* const node =
		serd_node_malloc(total_len, SERD_HAS_DATATYPE, SERD_LITERAL);

	if (serd_base64_encode(serd_node_buffer(node), buf, size, wrap_lines)) {
		node->flags |= SERD_HAS_NEWLINE;
	}

	node->n_bytes = len;
	memcpy(serd_node_get_meta(node), type, type_len);
	serd_node_check_padding(node);
	return node;
}

SerdNodeType
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

const SerdNode*
serd_node_get_datatype(const SerdNode* node)
{
	if (!node || !(node->flags & SERD_HAS_DATATYPE)) {
		return NULL;
	}

	const SerdNode* const datatype = serd_node_get_meta_c(node);
	assert(datatype->type == SERD_URI || datatype->type == SERD_CURIE);
	return datatype;
}

const SerdNode*
serd_node_get_language(const SerdNode* node)
{
	if (!node || !(node->flags & SERD_HAS_LANGUAGE)) {
		return NULL;
	}

	const SerdNode* const lang = serd_node_get_meta_c(node);
	assert(lang->type == SERD_LITERAL);
	return lang;
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
