/*
  Copyright 2019 David Robillard <http://drobilla.net>

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

#include "namespaces.h"
#include "node.h"
#include "sink.h"
#include "statement.h"
#include "string_utils.h"

#include "serd/serd.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
	const SerdSink* target;
} SerdNormaliserData;

struct SerdNormaliserImpl
{
	SerdSink           sink;
	SerdNormaliserData data;
};

/// Return true iff `c` is "+" or "-"
static inline bool
is_sign(const int c)
{
	return c == '+' || c == '-';
}

/// Return true iff `c` is "0"
static inline bool
is_zero(const int c)
{
	return c == '0';
}

/// Return true iff `c` is "."
static inline bool
is_point(const int c)
{
	return c == '.';
}

/// Return a view of `buf` with leading and trailing whitespace trimmed
static SerdStringView
trim(const char* buf, const size_t len)
{
	SerdStringView view = {buf, len};

	while (view.len > 0 && is_space(*view.buf)) {
		++view.buf;
		--view.len;
	}

	while (is_space(view.buf[view.len - 1])) {
		--view.len;
	}

	return view;
}

/// Scan `s` forwards as long as `pred` is true for the character it points at
static inline const char*
scan(const char** s, bool (*pred)(const int))
{
	while (pred(**s)) {
		++(*s);
	}

	return *s;
}

/// Skip `s` forward once if `pred` is true for the character it points at
static inline const char**
skip(const char** s, bool (*pred)(const int))
{
	*s += pred(**s);
	return s;
}

static SerdNode*
serd_normalise_decimal(const char* str)
{
	const char* s     = str;                                // Cursor
	const char* sign  = scan(&s, is_space);                 // Sign
	const char* first = scan(skip(&s, is_sign), is_zero);   // First non-zero
	const char* point = scan(&s, is_digit);                 // Decimal point
	const char* last  = scan(skip(&s, is_point), is_digit); // Last digit
	const char* end   = scan(&s, is_space);                 // Last non-space

	if (*end != '\0') {
		return NULL;
	} else if (*point == '.') {
		while (*(last - 1) == '0') {
			--last;
		}
	}

	char* buf = (char*)calloc(1, (size_t)(end - sign) + 4u);
	char* b   = buf;
	if (*sign == '-') {
		*b++ = '-';
	}

	if (*first == '.' || first == last) {
		*b++ = '0'; // Add missing leading zero (before point)
	}

	memcpy(b, first, (size_t)(last - first));
	b += last - first;

	if (*point != '.') {
		*b++ = '.';
		*b++ = '0';
	} else if (point == last - 1) {
		*b++ = '0'; // Add missing trailing zero (after point)
	}

	const char* const datatype = NS_XSD "decimal";
	SerdNode*         node     = serd_new_literal(
            buf, (size_t)(b - buf), datatype, strlen(datatype), NULL, 0);

	free(buf);
	return node;
}

static SerdNode*
serd_normalise_integer(const char* str, const SerdNode* datatype)
{
	const char* s     = str;                              // Cursor
	const char* sign  = scan(&s, is_space);               // Sign
	const char* first = scan(skip(&s, is_sign), is_zero); // First non-zero
	const char* last  = scan(&s, is_digit);               // Last digit
	const char* end   = scan(&s, is_space);               // Last non-space

	if (*end != '\0') {
		return NULL;
	}

	char* const buf = (char*)calloc(1, (size_t)(end - sign) + 2u);
	char*       b   = buf;
	if (*sign == '-') {
		*b++ = '-';
	}

	if (first == last) {
		*b = '0';
	} else {
		memcpy(b, first, (size_t)(last - first));
	}

	SerdNode* node = serd_new_typed_literal(buf, datatype);

	free(buf);
	return node;
}

SerdNode*
serd_node_normalise(const SerdEnv* env, const SerdNode* const node)
{
#define INTEGER_TYPE_LEN 19

	static const char int_types[13][INTEGER_TYPE_LEN] = {"byte",
	                                                     "int",
	                                                     "integer",
	                                                     "long",
	                                                     "negativeInteger",
	                                                     "nonNegativeInteger",
	                                                     "nonPositiveInteger",
	                                                     "positiveInteger",
	                                                     "short",
	                                                     "unsignedByte",
	                                                     "unsignedInt",
	                                                     "unsignedLong",
	                                                     "unsignedShort"};

	const char* str      = serd_node_get_string(node);
	SerdNode*   datatype = serd_env_expand(env, serd_node_get_datatype(node));
	if (node->type != SERD_LITERAL || !datatype) {
		return NULL;
	}

	const char* datatype_uri = serd_node_get_string(datatype);
	SerdNode*   result       = NULL;
	if (!strcmp(datatype_uri, NS_XSD "boolean")) {
		const SerdStringView trimmed = trim(str, serd_node_get_length(node));
		if (trimmed.len) {
			if (!strncmp(trimmed.buf, "false", trimmed.len) ||
			    !strncmp(trimmed.buf, "0", trimmed.len)) {
				result = serd_new_boolean(false);
			} else if (!strncmp(trimmed.buf, "true", trimmed.len) ||
			           !strncmp(trimmed.buf, "1", trimmed.len)) {
				result = serd_new_boolean(true);
			}
		}
	} else if (!strcmp(datatype_uri, NS_XSD "float")) {
		result = serd_new_float((float)serd_strtod(str, NULL));
	} else if (!strcmp(datatype_uri, NS_XSD "double")) {
		result = serd_new_double(serd_strtod(str, NULL));
	} else if (!strcmp(datatype_uri, NS_XSD "decimal")) {
		result = serd_normalise_decimal(str);
	} else if (!strncmp(datatype_uri, NS_XSD, strlen(NS_XSD)) &&
	           bsearch(datatype_uri + strlen(NS_XSD),
	                   &int_types,
	                   sizeof(int_types) / INTEGER_TYPE_LEN,
	                   INTEGER_TYPE_LEN,
	                   (int (*)(const void*, const void*))strcmp)) {
		result = serd_normalise_integer(str, datatype);
	}

	serd_node_free(datatype);
	return result;
}

static SerdStatus
serd_normaliser_on_base(void* handle, const SerdNode* uri)
{
	const SerdSink* target = ((SerdNormaliserData*)handle)->target;

	return serd_sink_write_base(target, uri);
}

static SerdStatus
serd_normaliser_on_prefix(void*           handle,
                          const SerdNode* name,
                          const SerdNode* uri)
{
	const SerdSink* target = ((SerdNormaliserData*)handle)->target;

	return serd_sink_write_prefix(target, name, uri);
}

static SerdStatus
serd_normaliser_on_statement(void*                handle,
                             SerdStatementFlags   flags,
                             const SerdStatement* statement)
{
	const SerdSink* target = ((SerdNormaliserData*)handle)->target;
	const SerdNode* object = serd_statement_get_object(statement);
	SerdNode*       normo  = serd_node_normalise(target->env, object);

	if (normo) {
		const SerdStatus st = serd_sink_write(target,
		                                      flags,
		                                      statement->nodes[0],
		                                      statement->nodes[1],
		                                      normo,
		                                      statement->nodes[3]);

		serd_node_free(normo);
		return st;
	}

	return serd_sink_write_statement(target, flags, statement);
}

static SerdStatus
serd_normaliser_on_end(void* handle, const SerdNode* node)
{
	const SerdSink* target = ((SerdNormaliserData*)handle)->target;

	return serd_sink_write_end(target, node);
}

SerdNormaliser*
serd_normaliser_new(const SerdSink* target)
{
	SerdNormaliser* normaliser =
	        (SerdNormaliser*)calloc(1, sizeof(SerdNormaliser));

	SerdNormaliserData* data = &normaliser->data;
	SerdSink*           sink = &normaliser->sink;

	sink->handle      = data;
	sink->free_handle = NULL;
	sink->env         = target->env;
	data->target      = target;

	serd_sink_set_base_func(sink, serd_normaliser_on_base);
	serd_sink_set_prefix_func(sink, serd_normaliser_on_prefix);
	serd_sink_set_statement_func(sink, serd_normaliser_on_statement);
	serd_sink_set_end_func(sink, serd_normaliser_on_end);

	return normaliser;
}

void
serd_normaliser_free(SerdNormaliser* normaliser)
{
	free(normaliser);
}

const SerdSink*
serd_normaliser_get_sink(const SerdNormaliser* normaliser)
{
	return &normaliser->sink;
}
