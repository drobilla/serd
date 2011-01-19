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
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"

typedef struct {
	SerdString* name;
	SerdString* uri;
} SerdNamespace;

struct SerdNamespacesImpl {
	SerdNamespace* namespaces;
	size_t         n_namespaces;
};

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

SERD_API
SerdString*
serd_string_new(const uint8_t* utf8)
{
	size_t n_bytes;
	size_t n_chars = utf8_strlen(utf8, &n_bytes);
	SerdString* const str = malloc(sizeof(SerdString) + n_bytes);
	str->n_bytes = n_bytes;
	str->n_chars = n_chars;
	memcpy(str->buf, utf8, str->n_bytes);
	return str;
}

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

SERD_API
SerdNamespaces
serd_namespaces_new()
{
	SerdNamespaces ns = malloc(sizeof(struct SerdNamespacesImpl));
	ns->namespaces   = NULL;
	ns->n_namespaces = 0;
	return ns;
}

SERD_API
void
serd_namespaces_free(SerdNamespaces ns)
{
	for (size_t i = 0; i < ns->n_namespaces; ++i) {
		free(ns->namespaces[i].name);
		free(ns->namespaces[i].uri);
	}
	free(ns->namespaces);
	free(ns);
}

static inline SerdNamespace*
serd_namespaces_find(SerdNamespaces    ns,
                     const uint8_t*    name,
                     size_t            name_len)
{
	for (size_t i = 0; i < ns->n_namespaces; ++i) {
		const SerdString* ns_name = ns->namespaces[i].name;
		if (ns_name->n_bytes == name_len + 1) {
			if (!memcmp(ns_name->buf, name, name_len)) {
				return &ns->namespaces[i];
			}
		}
	}
	return NULL;
}

SERD_API
void
serd_namespaces_add(SerdNamespaces    ns,
                    const SerdString* name,
                    const SerdString* uri)
{
	assert(name);
	assert(uri);
	SerdNamespace* const record = serd_namespaces_find(ns, name->buf, name->n_chars);
	if (record) {
		free(record->uri);
		record->uri = serd_string_copy(uri);
	} else {
		++ns->n_namespaces;
		ns->namespaces = realloc(ns->namespaces,
		                         ns->n_namespaces * sizeof(SerdNamespace));
		ns->namespaces[ns->n_namespaces - 1].name = serd_string_copy(name);
		ns->namespaces[ns->n_namespaces - 1].uri  = serd_string_copy(uri);
	}
}

SERD_API
bool
serd_namespaces_expand(SerdNamespaces    ns,
                       const SerdString* qname,
                       SerdRange*        uri_prefix,
                       SerdRange*        uri_suffix)
{
	const uint8_t* colon = memchr((const char*)qname->buf, ':', qname->n_bytes);
	if (!colon) {
		return false;  // Illegal qname
	}
	
	SerdNamespace* const record = serd_namespaces_find(ns, qname->buf, colon - qname->buf);
	if (record) {
		uri_prefix->buf = record->uri->buf;
		uri_prefix->len = record->uri->n_bytes;
		uri_suffix->buf = colon + 1;
		uri_suffix->len = qname->n_bytes - (colon - qname->buf) - 1;
		return true;
	}
	return false;
}
