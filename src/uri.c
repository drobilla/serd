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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// #define URI_DEBUG 1

SERD_API
const uint8_t*
serd_uri_to_path(const uint8_t* uri)
{
	const uint8_t* path = NULL;
	if (serd_uri_string_has_scheme(uri)) {
		if (strncmp((const char*)uri, "file:", 5)) {
			fprintf(stderr, "Non-file URI `%s'\n", uri);
			return NULL;
		} else if (!strncmp((const char*)uri, "file://localhost/", 17)) {
			path = uri + 16;
		} else if (!strncmp((const char*)uri, "file://", 7)) {
			path = uri + 7;
		} else {
			fprintf(stderr, "Invalid file URI `%s'\n", uri);
			return NULL;
		}
		// Special case for awful Windows file URIs
		if (is_alpha(path[1]) && path[2] == ':' && path[3] == '/') {
			++path;  // Special case for terrible Windows file URIs
		}
	} else {
		path = uri;
	}
	return path;
}

SERD_API
bool
serd_uri_string_has_scheme(const uint8_t* utf8)
{
	// RFC3986: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
	if (!is_alpha(utf8[0])) {
		return false;  // Invalid scheme initial character, URI is relative
	}
	for (uint8_t c; (c = *++utf8) != '\0';) {
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
SerdStatus
serd_uri_parse(const uint8_t* utf8, SerdURI* uri)
{
	*uri = SERD_URI_NULL;

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
		for (uint8_t c; (c = *ptr) != '\0'; ++ptr) {
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
	for (uint8_t c; (c = *ptr) != '\0'; ++ptr) {
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
		for (uint8_t c; (c = *ptr) != '\0'; ++ptr) {
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

	return SERD_SUCCESS;
}

/**
   Remove leading dot components from @c path.
   See http://tools.ietf.org/html/rfc3986#section-5.2.3
   @param up Set to the number of up-references (e.g. "../") trimmed
   @return A pointer to the new start of @path
*/
static const uint8_t*
remove_dot_segments(const uint8_t* path, size_t len, size_t* up)
{
	const uint8_t*       begin = path;
	const uint8_t* const end   = path + len;

	*up = 0;
	while (begin < end) {
		switch (begin[0]) {
		case '.':
			switch (begin[1]) {
			case '/':
				begin += 2;  // Chop leading "./"
				break;
			case '.':
				switch (begin[2]) {
				case '\0':
					++*up;
					begin += 2;  // Chop input ".."
					break;
				case '/':
					++*up;
					begin += 3;  // Chop leading "../"
					break;
				default:
					return begin;
				}
				break;
			case '\0':
				++begin;  // Chop input "." (and fall-through)
			default:
				return begin;
			}
			break;
		case '/':
			switch (begin[1]) {
			case '.':
				switch (begin[2]) {
				case '/':
					begin += 2;  // Leading "/./" => "/"
					break;
				case '.':
					switch (begin[3]) {
					case '/':
						++*up;
						begin += 3;  // Leading "/../" => "/"
					}
					break;
				default:
					return begin;
				}
			} // else fall through
		default:
			return begin;  // Finished chopping dot components
		}
	}

	return begin;
}

SERD_API
void
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
		t->scheme   = base->scheme;
		t->fragment = r->fragment;
	}

	#ifdef URI_DEBUG
	fprintf(stderr, "RESOLVE URI\nBASE:\n");
	serd_uri_dump(base, stderr);
	fprintf(stderr, "URI:\n");
	serd_uri_dump(r, stderr);
	fprintf(stderr, "RESULT:\n");
	serd_uri_dump(t, stderr);
	fprintf(stderr, "\n");
	#endif
}

SERD_API
size_t
serd_uri_serialise(const SerdURI* uri, SerdSink sink, void* stream)
{
	// See http://tools.ietf.org/html/rfc3986#section-5.3

	size_t write_size = 0;
#define WRITE(buf, len) \
	write_size += len; \
	sink((const uint8_t*)buf, len, stream);

	if (uri->scheme.buf) {
		WRITE(uri->scheme.buf, uri->scheme.len);
		WRITE(":", 1);
	}
	if (uri->authority.buf) {
		WRITE("//", 2);
		WRITE(uri->authority.buf, uri->authority.len);
	}
	if (!uri->path.buf) {
		WRITE(uri->path_base.buf, uri->path_base.len);
	} else {
		const uint8_t*       begin = uri->path.buf;
		const uint8_t* const end   = uri->path.buf + uri->path.len;

		size_t up;
		begin = remove_dot_segments(uri->path.buf, uri->path.len, &up);

		if (uri->path_base.buf) {
			// Find the up'th last slash
			const uint8_t* base_last = (uri->path_base.buf
			                            + uri->path_base.len - 1);
			++up;
			do {
				if (*base_last == '/') {
					--up;
				}
			} while (up > 0 && (--base_last > uri->path_base.buf));

			// Write base URI prefix
			const size_t base_len = base_last - uri->path_base.buf + 1;
			WRITE(uri->path_base.buf, base_len);

		} else {
			// Relative path is just query or fragment, append to base URI
			WRITE(uri->path_base.buf, uri->path_base.len);
		}

		// Write URI suffix
		WRITE(begin, end - begin);
	}
	if (uri->query.buf) {
		WRITE("?", 1);
		WRITE(uri->query.buf, uri->query.len);
	}
	if (uri->fragment.buf) {
		// Note uri->fragment.buf includes the leading `#'
		WRITE(uri->fragment.buf, uri->fragment.len);
	}
	return write_size;
}
