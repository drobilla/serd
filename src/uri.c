/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#include "string_utils.h"
#include "uri_utils.h"

#include "serd/serd.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char*
serd_parse_file_uri(const char* const uri, char** const hostname)
{
  const char* path = uri;
  if (hostname) {
    *hostname = NULL;
  }

  if (!strncmp(uri, "file://", 7)) {
    const char* auth = uri + 7;
    if (*auth == '/') { // No hostname
      path = auth;
    } else { // Has hostname
      if (!(path = strchr(auth, '/'))) {
        return NULL;
      }

      if (hostname) {
        const size_t len = (size_t)(path - auth);
        *hostname        = (char*)calloc(len + 1, 1);
        memcpy(*hostname, auth, len);
      }
    }
  }

  if (is_windows_path(path + 1)) {
    ++path;
  }

  SerdBuffer buffer = {NULL, 0};
  for (const char* s = path; *s; ++s) {
    if (*s == '%') {
      if (*(s + 1) == '%') {
        serd_buffer_sink("%", 1, 1, &buffer);
        ++s;
      } else if (is_hexdig(*(s + 1)) && is_hexdig(*(s + 2))) {
        const char code[3] = {*(s + 1), *(s + 2), 0};
        const char c       = (char)strtoul(code, NULL, 16);
        serd_buffer_sink(&c, 1, 1, &buffer);
        s += 2;
      } else {
        s += 2; // Junk escape, ignore
      }
    } else {
      serd_buffer_sink(s, 1, 1, &buffer);
    }
  }

  return serd_buffer_sink_finish(&buffer);
}

/// RFC3986: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
bool
serd_uri_string_has_scheme(const char* const string)
{
  if (is_alpha(string[0])) {
    for (size_t i = 1; string[i]; ++i) {
      if (!is_uri_scheme_char(string[i])) {
        return false; // Non-scheme character before a ':'
      }

      if (string[i] == ':') {
        return true; // Valid scheme terminated by a ':'
      }
    }
  }

  return false; // String doesn't start with a scheme
}

SerdURIView
serd_parse_uri(const char* const string)
{
  SerdURIView result = SERD_URI_NULL;
  const char* ptr    = string;

  /* See http://tools.ietf.org/html/rfc3986#section-3
     URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
  */

  /* S3.1: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) */
  if (is_alpha(*ptr)) {
    for (char c = *++ptr; true; c = *++ptr) {
      switch (c) {
      case '\0':
      case '/':
      case '?':
      case '#':
        ptr = string;
        goto path; // Relative URI (starts with path by definition)
      case ':':
        result.scheme.buf = string;
        result.scheme.len = (size_t)((ptr++) - string);
        goto maybe_authority; // URI with scheme
      case '+':
      case '-':
      case '.':
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
    result.authority.buf = ptr;
    for (char c = 0; (c = *ptr) != '\0'; ++ptr) {
      switch (c) {
      case '/':
        goto path;
      case '?':
        goto query;
      case '#':
        goto fragment;
      default:
        ++result.authority.len;
      }
    }
  }

  /* RFC3986 S3.3: The path is terminated by the first question mark ("?")
     or number sign ("#") character, or by the end of the URI.
  */
path:
  switch (*ptr) {
  case '?':
    goto query;
  case '#':
    goto fragment;
  case '\0':
    goto end;
  default:
    break;
  }
  result.path.buf = ptr;
  result.path.len = 0;
  for (char c = 0; (c = *ptr) != '\0'; ++ptr) {
    switch (c) {
    case '?':
      goto query;
    case '#':
      goto fragment;
    default:
      ++result.path.len;
    }
  }

  /* RFC3986 S3.4: The query component is indicated by the first question
     mark ("?") character and terminated by a number sign ("#") character
     or by the end of the URI.
  */
query:
  if (*ptr == '?') {
    result.query.buf = ++ptr;
    for (char c = 0; (c = *ptr) != '\0'; ++ptr) {
      if (c == '#') {
        goto fragment;
      }
      ++result.query.len;
    }
  }

  /* RFC3986 S3.5: A fragment identifier component is indicated by the
     presence of a number sign ("#") character and terminated by the end
     of the URI.
  */
fragment:
  if (*ptr == '#') {
    result.fragment.buf = ptr;
    while (*ptr++ != '\0') {
      ++result.fragment.len;
    }
  }

end:
  return result;
}

/**
   Remove leading dot components from `path`.
   See http://tools.ietf.org/html/rfc3986#section-5.2.3
   @param up Set to the number of up-references (e.g. "../") trimmed
   @return A pointer to the new start of `path`
*/
static const char*
remove_dot_segments(const char* const path, const size_t len, size_t* const up)
{
  *up = 0;

  for (size_t i = 0; i < len;) {
    const char* const p = path + i;
    if (!strncmp(p, "./", 2)) {
      i += 2; // Chop leading "./"
    } else if (!strcmp(p, "..")) {
      ++*up;
      i += 2; // Chop input ".."
    } else if (!strncmp(p, "../", 3)) {
      ++*up;
      i += 3; // Chop leading "../"
    } else if (!strcmp(p, ".")) {
      ++i; // Chop input "."
    } else if (!strncmp(p, "/./", 3)) {
      i += 2; // Leading "/./" => "/"
    } else if (!strncmp(p, "/../", 4)) {
      ++*up;
      i += 3; // Leading "/../" => "/"
    } else {
      return p;
    }
  }

  return path + len;
}

/// Merge `base` and `path` in-place
static void
merge(SerdStringView* const base, SerdStringView* const path)
{
  size_t      up    = 0;
  const char* begin = remove_dot_segments(path->buf, path->len, &up);
  const char* end   = path->buf + path->len;

  if (base->len) {
    // Find the up'th last slash
    const char* base_last = (base->buf + base->len - 1);
    ++up;
    do {
      if (*base_last == '/') {
        --up;
      }
    } while (up > 0 && (--base_last > base->buf));

    // Set path prefix
    base->len = (size_t)(base_last - base->buf + 1);
  }

  // Set path suffix
  path->buf = begin;
  path->len = (size_t)(end - begin);
}

/// See http://tools.ietf.org/html/rfc3986#section-5.2.2
SerdURIView
serd_resolve_uri(const SerdURIView r, const SerdURIView base)
{
  if (r.scheme.len || !base.scheme.len) {
    return r; // No resolution necessary || possible (respectively)
  }

  SerdURIView t = SERD_URI_NULL;

  if (r.authority.len) {
    t.authority = r.authority;
    t.path      = r.path;
    t.query     = r.query;
  } else {
    t.path = r.path;
    if (!r.path.len) {
      t.path_prefix = base.path;
      t.query       = r.query.len ? r.query : base.query;
    } else {
      if (r.path.buf[0] != '/') {
        t.path_prefix = base.path;
      }

      merge(&t.path_prefix, &t.path);
      t.query = r.query;
    }

    t.authority = base.authority;
  }

  t.scheme   = base.scheme;
  t.fragment = r.fragment;

  return t;
}

SerdURIView
serd_relative_uri(const SerdURIView uri, const SerdURIView base)
{
  if (!uri_is_related(&uri, &base)) {
    return uri;
  }

  SerdURIView result = SERD_URI_NULL;

  // Regardless of the path, the query and/or fragment come along
  result.query    = uri.query;
  result.fragment = uri.fragment;

  const size_t path_len = uri_path_len(&uri);
  const size_t base_len = uri_path_len(&base);
  const size_t min_len  = (path_len < base_len) ? path_len : base_len;

  // Find the last separator common to both paths
  size_t last_shared_sep = 0;
  size_t i               = 0;
  for (; i < min_len && uri_path_at(&uri, i) == uri_path_at(&base, i); ++i) {
    if (uri_path_at(&uri, i) == '/') {
      last_shared_sep = i;
    }
  }

  // If the URI and base URI have identical paths, the relative path is empty
  if (i == path_len && i == base_len) {
    result.path.buf = uri.path.buf;
    result.path.len = 0;
    return result;
  }

  // Otherwise, we need to build the relative path out of string slices

  // Find the number of up references ("..") required
  size_t up = 0;
  for (size_t s = last_shared_sep + 1; s < base_len; ++s) {
    if (uri_path_at(&base, s) == '/') {
      ++up;
    }
  }

  if (up > 0) {
    if (last_shared_sep < uri.path_prefix.len) {
      return SERD_URI_NULL;
    }

    // Special representation: NULL buffer and len set to the depth
    result.path_prefix.len = up;
  }

  if (last_shared_sep < uri.path_prefix.len) {
    result.path_prefix.buf = uri.path_prefix.buf + last_shared_sep + 1;
    result.path_prefix.len = uri.path_prefix.len - last_shared_sep - 1;
    result.path            = uri.path;
  } else {
    result.path.buf = uri.path.buf + last_shared_sep + 1;
    result.path.len = uri.path.len - last_shared_sep - 1;
  }

  return result;
}

bool
serd_uri_is_within(const SerdURIView uri, const SerdURIView base)
{
  if (!base.scheme.len || !slice_equals(&base.scheme, &uri.scheme) ||
      !slice_equals(&base.authority, &uri.authority)) {
    return false;
  }

  bool         differ   = false;
  const size_t path_len = uri_path_len(&uri);
  const size_t base_len = uri_path_len(&base);

  size_t last_base_slash = 0;
  for (size_t i = 0; i < path_len && i < base_len; ++i) {
    const char u = uri_path_at(&uri, i);
    const char b = uri_path_at(&base, i);

    differ = differ || u != b;
    if (b == '/') {
      last_base_slash = i;
      if (differ) {
        return false;
      }
    }
  }

  for (size_t i = last_base_slash + 1; i < base_len; ++i) {
    if (uri_path_at(&base, i) == '/') {
      return false;
    }
  }

  return true;
}

/// See http://tools.ietf.org/html/rfc3986#section-5.3
size_t
serd_write_uri(const SerdURIView   uri,
               const SerdWriteFunc sink,
               void* const         stream)
{
  size_t len = 0;

  if (uri.scheme.buf) {
    len += sink(uri.scheme.buf, 1, uri.scheme.len, stream);
    len += sink(":", 1, 1, stream);
  }

  if (uri.authority.buf) {
    len += sink("//", 1, 2, stream);
    len += sink(uri.authority.buf, 1, uri.authority.len, stream);

    if (uri.authority.len > 0 && uri_path_len(&uri) > 0 &&
        uri_path_at(&uri, 0) != '/') {
      // Special case: ensure path begins with a slash
      // https://tools.ietf.org/html/rfc3986#section-3.2
      len += sink("/", 1, 1, stream);
    }
  }

  if (uri.path_prefix.buf) {
    len += sink(uri.path_prefix.buf, 1, uri.path_prefix.len, stream);
  } else if (uri.path_prefix.len) {
    for (size_t i = 0; i < uri.path_prefix.len; ++i) {
      len += sink("../", 1, 3, stream);
    }
  }

  if (uri.path.buf) {
    len += sink(uri.path.buf, 1, uri.path.len, stream);
  }

  if (uri.query.buf) {
    len += sink("?", 1, 1, stream);
    len += sink(uri.query.buf, 1, uri.query.len, stream);
  }

  if (uri.fragment.buf) {
    // Note that uri.fragment.buf includes the leading `#'
    len += sink(uri.fragment.buf, 1, uri.fragment.len, stream);
  }
  return len;
}
