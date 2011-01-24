/* Serd, an RDF serialisation library.
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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "serd_internal.h"

#if 0
static inline size_t
utf8_strlen(const uint8_t* utf8, size_t* out_n_bytes)
{
	size_t n_chars = 0;
	size_t i       = 0;
	for (; utf8[i]; ++i) {
		if ((utf8[i] & 0xC0) != 0x80) {
			// Does not start with `10', start of a new character
			++n_chars;
		}
	}
	if (out_n_bytes) {
		*out_n_bytes = i + 1;
	}
	return n_chars;
}
#endif

static SerdString*
serd_string_new_measured(const uint8_t* utf8, size_t n_bytes, size_t n_chars)
{
	SerdString* const str = malloc(sizeof(SerdString) + n_bytes);
	str->n_bytes = n_bytes;
	str->n_chars = n_chars;
	memcpy(str->buf, utf8, n_bytes);
	return str;
}

#if 0
SERD_API
SerdString*
serd_string_new(const uint8_t* utf8)
{
	size_t n_bytes;
	size_t n_chars = utf8_strlen(utf8, &n_bytes);
	return serd_string_new_measured(utf8, n_bytes, n_chars);
}
#endif

SERD_API
SerdString*
serd_string_new_from_node(const SerdNode* node)
{
	return serd_string_new_measured(node->buf, node->n_bytes, node->n_chars);
}

#if 0
SERD_API
SerdString*
serd_string_copy(const SerdString* s)
{
	if (s) {
		SerdString* const copy = malloc(sizeof(SerdString) + s->n_bytes);
		memcpy(copy, s, sizeof(SerdString) + s->n_bytes);
		return copy;
	}
	return NULL;
}
#endif

SERD_API
void
serd_string_free(SerdString* str)
{
	free(str);
}
