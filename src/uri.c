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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "serd_internal.h"

// #define URI_DEBUG 1

SERD_API
bool
serd_uri_string_has_scheme(const uint8_t* utf8)
{
	// RFC3986: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
	if (!is_alpha(utf8[0])) {
		return false;  // Invalid scheme initial character, URI is relative
	}
	for (uint8_t c = *++utf8; (c = *utf8) != '\0'; ++utf8) {
		switch (c) {
		case ':':
			return true;  // End of scheme
		case '+': case '-': case '.':
			break;  // Valid scheme character, continue
		default:
			if (!is_alpha(c) && !is_digit(c)) {
				return false;  // Invalid scheme character
			}
		}
	}

	return false;
}

#ifdef URI_DEBUG
static void
serd_uri_dump(const SerdURI* uri, FILE* file)
{
#define PRINT_PART(range, name) \
	if (range.buf) { \
		fprintf(stderr, "  " name " = "); \
		fwrite((range).buf, 1, (range).len, stderr); \
		fprintf(stderr, "\n"); \
	}

	PRINT_PART(uri->scheme,    "scheme");
	PRINT_PART(uri->authority, "authority");
	PRINT_PART(uri->path_base, "path_base");
	PRINT_PART(uri->path,      "path");
	PRINT_PART(uri->query,     "query");
	PRINT_PART(uri->fragment,  "fragment");
}
#endif

SERD_API
bool
serd_uri_parse(const uint8_t* utf8, SerdURI* uri)
{
	*uri = SERD_URI_NULL;
	assert(uri->path_base.buf == NULL);
	assert(uri->path_base.len == 0);
	assert(uri->authority.len == 0);

	const uint8_t* ptr = utf8;

	/* See http://tools.ietf.org/html/rfc3986#section-3
	   URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
	*/

	/* S3.1: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) */
	if (is_alpha(*ptr)) {
		for (uint8_t c = *++ptr; true; c = *++ptr) {
			switch (c) {
			case '\0': case '/': case '?': case '#':
				ptr = utf8;
				goto path;  // Relative URI (starts with path by definition)
			case ':':
				uri->scheme.buf = utf8;
				uri->scheme.len = (ptr++) - utf8;
				goto maybe_authority;  // URI with scheme
			case '+': case '-': case '.':
				continue;
			default:
				if (is_alpha(c) || is_digit(c)) {
					continue;
				}
			}
		}
	}

	/* S3.2: The authority component is preceded by a double slash ("//")
	   and is terminated by the next slash ("/"), question mark ("?"),
	   or number sign ("#") character, or by the end of the URI.
	*/
maybe_authority:
	if (*ptr == '/' && *(ptr + 1) == '/') {
		ptr += 2;
		uri->authority.buf = ptr;
		assert(uri->authority.len == 0);
		for (uint8_t c = *ptr; (c = *ptr) != '\0'; ++ptr) {
			switch (c) {
			case '/': goto path;
			case '?': goto query;
			case '#': goto fragment;
			default:
				++uri->authority.len;
			}
		}
	}

	/* RFC3986 S3.3: The path is terminated by the first question mark ("?")
	   or number sign ("#") character, or by the end of the URI.
	*/
path:
	switch (*ptr) {
	case '?':  goto query;
	case '#':  goto fragment;
	case '\0': goto end;
	default:  break;
	}
	uri->path.buf = ptr;
	uri->path.len = 0;
	for (uint8_t c = *ptr; (c = *ptr) != '\0'; ++ptr) {
		switch (c) {
		case '?': goto query;
		case '#': goto fragment;
		default:
			++uri->path.len;
		}
	}

	/* RFC3986 S3.4: The query component is indicated by the first question
	   mark ("?") character and terminated by a number sign ("#") character
	   or by the end of the URI.
	*/
query:
	if (*ptr == '?') {
		uri->query.buf = ++ptr;
		for (uint8_t c = *ptr; (c = *ptr) != '\0'; ++ptr) {
			switch (c) {
			case '#':
				goto fragment;
			default:
				++uri->query.len;
			}
		}
	}

	/* RFC3986 S3.5: A fragment identifier component is indicated by the
	   presence of a number sign ("#") character and terminated by the end
	   of the URI.
	*/
fragment:
	if (*ptr == '#') {
		uri->fragment.buf = ptr;
		while (*ptr++ != '\0') {
			++uri->fragment.len;
		}
	}

end:
	#ifdef URI_DEBUG
	fprintf(stderr, "PARSE URI <%s>\n", utf8);
	serd_uri_dump(uri, stderr);
	fprintf(stderr, "\n");
	#endif

	return true;
}

SERD_API
bool
serd_uri_resolve(const SerdURI* r, const SerdURI* base, SerdURI* t)
{
	// See http://tools.ietf.org/html/rfc3986#section-5.2.2

	t->path_base.buf = NULL;
	t->path_base.len = 0;
	if (r->scheme.len) {
		*t = *r;
	} else {
		if (r->authority.len) {
			t->authority = r->authority;
			t->path      = r->path;
			t->query     = r->query;
		} else {
			t->path = r->path;
			if (!r->path.len) {
				t->path_base = base->path;
				if (r->query.len) {
					t->query = r->query;
				} else {
					t->query = base->query;
				}
			} else {
				if (r->path.buf[0] != '/') {
					t->path_base = base->path;
				}
				t->query = r->query;
			}
			t->authority = base->authority;
		}
		t->scheme = base->scheme;
	}
	t->fragment = r->fragment;

	#ifdef URI_DEBUG
	fprintf(stderr, "RESOLVE URI\nBASE:\n");
	serd_uri_dump(base, stderr);
	fprintf(stderr, "URI:\n");
	serd_uri_dump(r, stderr);
	fprintf(stderr, "RESULT:\n");
	serd_uri_dump(t, stderr);
	fprintf(stderr, "\n");
	#endif
	return true;
}

SERD_API
size_t
serd_uri_serialise(const SerdURI* uri, SerdSink sink, void* stream)
{
	// See http://tools.ietf.org/html/rfc3986#section-5.3

	size_t write_size = 0;
#define WRITE(buf, len) \
	write_size += len; \
	if (len) { \
		sink((const uint8_t*)buf, len, stream); \
	}
#define WRITE_CHAR(c) WRITE(&(c), 1)
#define WRITE_COMPONENT(prefix, field, suffix) \
	if ((field).len) { \
		for (const uint8_t* c = (const uint8_t*)prefix; *c != '\0'; ++c) { \
			WRITE(c, 1); \
		} \
		WRITE((field).buf, (field).len); \
		for (const uint8_t* c = (const uint8_t*)suffix; *c != '\0'; ++c) { \
			WRITE(c, 1); \
		} \
	}

	WRITE_COMPONENT("",   uri->scheme,    ":");
	WRITE_COMPONENT("//", uri->authority, "");
	if (uri->path_base.len) {
		if (!uri->path.buf && (uri->fragment.buf || uri->query.buf)) {
			WRITE_COMPONENT("", uri->path_base, "");
		} else {
			/* Merge paths, removing dot components.
			   See http://tools.ietf.org/html/rfc3986#section-5.2.3
			*/
			const uint8_t* begin = uri->path.buf;
			const uint8_t* end   = begin;
			size_t         up        = 1;
			if (begin) {
				// Count and skip leading dot components
				end = uri->path.buf + uri->path.len;
				for (bool done = false; !done && (begin < end);) {
					switch (begin[0]) {
					case '.':
						switch (begin[1]) {
						case '/':
							begin += 2;  // Chop leading "./"
							break;
						case '.':
							++up;
							switch (begin[2]) {
							case '/':
								begin += 3;  // Chop lading "../"
								break;
							default:
								begin += 2;  // Chop leading ".."
							}
							break;
						default:
							++begin;  // Chop leading "."
						}
						break;
					case '/':
						if (begin[1] == '/') {
							++begin;  // Replace leading "//" with "/"
							break;
						}  // else fall through
					default:
						done = true;  // Finished chopping dot components
					}
				}

				if (uri->path.buf && uri->path_base.buf) {
					// Find the up'th last slash
					const uint8_t* base_last = uri->path_base.buf + uri->path_base.len - 1;
					do {
						if (*base_last == '/') {
							--up;
						}
					} while (up > 0 && (--base_last > uri->path_base.buf));

					// Write base URI prefix
					const size_t base_len = base_last - uri->path_base.buf + 1;
					WRITE(uri->path_base.buf, base_len);

				} else {
					// Relative path is just query or fragment, append it to full base URI
					WRITE_COMPONENT("", uri->path_base, "");
				}

				// Write URI suffix
				WRITE(begin, end - begin);
			}
		}
	} else {
		WRITE_COMPONENT("", uri->path, "");
	}
	WRITE_COMPONENT("?", uri->query, "");
	if (uri->fragment.buf) {
		// Note uri->fragment.buf includes the leading `#'
		WRITE_COMPONENT("", uri->fragment, "");
	}
	return write_size;
}
