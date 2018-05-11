/*
  Copyright 2011-2018 David Robillard <http://drobilla.net>

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

#ifndef SERD_URI_UTILS_H
#define SERD_URI_UTILS_H

#include "string_utils.h"

#include "serd/serd.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool
slice_equals(const SerdStringView* a, const SerdStringView* b)
{
	return a->len == b->len
		&& !strncmp((const char*)a->buf, (const char*)b->buf, a->len);
}

static inline size_t
uri_path_len(const SerdURI* uri)
{
	return uri->path_base.len + uri->path.len;
}

static inline char
uri_path_at(const SerdURI* uri, size_t i)
{
	if (i < uri->path_base.len) {
		return uri->path_base.buf[i];
	} else {
		return uri->path.buf[i - uri->path_base.len];
	}
}

/**
   Return the index of the first differing character after the last root slash,
   or zero if `uri` is not under `root`.
*/
static inline size_t
uri_rooted_index(const SerdURI* uri, const SerdURI* root)
{
	if (!root || !root->scheme.len ||
	    !slice_equals(&root->scheme, &uri->scheme) ||
	    !slice_equals(&root->authority, &uri->authority)) {
		return 0;
	}

	bool         differ          = false;
	const size_t path_len        = uri_path_len(uri);
	const size_t root_len        = uri_path_len(root);
	size_t       last_root_slash = 0;
	for (size_t i = 0; i < path_len && i < root_len; ++i) {
		const char u = uri_path_at(uri, i);
		const char r = uri_path_at(root, i);

		differ = differ || u != r;
		if (r == '/') {
			last_root_slash = i;
			if (differ) {
				return 0;
			}
		}
	}

	return last_root_slash + 1;
}

/** Return true iff `uri` shares path components with `root` */
static inline bool
uri_is_related(const SerdURI* uri, const SerdURI* root)
{
	return uri_rooted_index(uri, root) > 0;
}

/** Return true iff `uri` is within the base of `root` */
static inline bool
uri_is_under(const SerdURI* uri, const SerdURI* root)
{
	const size_t index = uri_rooted_index(uri, root);
	return index > 0 && uri->path.len > index;
}

static inline bool
is_uri_scheme_char(const int c)
{
	switch (c) {
	case ':': case '+': case '-': case '.':
		return true;
	default:
		return is_alpha(c) || is_digit(c);
	}
}

#endif  // SERD_URI_UTILS_H
