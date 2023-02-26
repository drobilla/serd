// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"
#include "uri_utils.h"
#include "warnings.h"

#include "serd/buffer.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/string_view.h"
#include "serd/uri.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char*
serd_file_uri_parse(const char* const uri, char** const hostname)
{
  assert(uri);

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
        serd_buffer_sink("%", 1, &buffer);
        ++s;
      } else if (is_hexdig(*(s + 1)) && is_hexdig(*(s + 2))) {
        const uint8_t hi = hex_digit_value((const uint8_t)s[1]);
        const uint8_t lo = hex_digit_value((const uint8_t)s[2]);
        const char    c  = (char)((hi << 4U) | lo);
        serd_buffer_sink(&c, 1, &buffer);
        s += 2;
      } else {
        s += 2; // Junk escape, ignore
      }
    } else {
      serd_buffer_sink(s, 1, &buffer);
    }
  }
  return serd_buffer_sink_finish(&buffer);
}

bool
serd_uri_string_has_scheme(const char* utf8)
{
  // RFC3986: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
  if (!utf8 || !is_alpha(utf8[0])) {
    return false; // Invalid scheme initial character, URI is relative
  }

  for (char c = 0; (c = *++utf8) != '\0';) {
    if (!is_uri_scheme_char(c)) {
      return false;
    }

    if (c == ':') {
      return true; // End of scheme
    }
  }

  return false;
}

SerdStatus
serd_uri_parse(const char* const utf8, SerdURIView* const out)
{
  assert(utf8);
  assert(out);

  *out = SERD_URI_NULL;

  const char* ptr = utf8;

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
        ptr = utf8;
        goto path; // Relative URI (starts with path by definition)
      case ':':
        out->scheme.data   = utf8;
        out->scheme.length = (size_t)((ptr++) - utf8);
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
    out->authority.data = ptr;
    for (char c = 0; (c = *ptr) != '\0'; ++ptr) {
      switch (c) {
      case '/':
        goto path;
      case '?':
        goto query;
      case '#':
        goto fragment;
      default:
        ++out->authority.length;
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
  out->path.data   = ptr;
  out->path.length = 0;
  for (char c = 0; (c = *ptr) != '\0'; ++ptr) {
    switch (c) {
    case '?':
      goto query;
    case '#':
      goto fragment;
    default:
      ++out->path.length;
    }
  }

  /* RFC3986 S3.4: The query component is indicated by the first question
     mark ("?") character and terminated by a number sign ("#") character
     or by the end of the URI.
  */
query:
  if (*ptr == '?') {
    out->query.data = ++ptr;
    for (char c = 0; (c = *ptr) != '\0'; ++ptr) {
      if (c == '#') {
        goto fragment;
      }
      ++out->query.length;
    }
  }

  /* RFC3986 S3.5: A fragment identifier component is indicated by the
     presence of a number sign ("#") character and terminated by the end
     of the URI.
  */
fragment:
  if (*ptr == '#') {
    out->fragment.data = ptr;
    while (*ptr++ != '\0') {
      ++out->fragment.length;
    }
  }

end:
  return SERD_SUCCESS;
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
    if (!strcmp(p, ".")) {
      ++i; // Chop input "."
    } else if (!strcmp(p, "..")) {
      ++*up;
      i += 2; // Chop input ".."
    } else if (!strncmp(p, "./", 2) || !strncmp(p, "/./", 3)) {
      i += 2; // Chop leading "./", or replace leading "/./" with "/"
    } else if (!strncmp(p, "../", 3) || !strncmp(p, "/../", 4)) {
      ++*up;
      i += 3; // Chop leading "../", or replace "/../" with "/"
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
  const char* begin = remove_dot_segments(path->data, path->length, &up);
  const char* end   = path->data + path->length;

  if (base->length) {
    // Find the up'th last slash
    const char* base_last = (base->data + base->length - 1);
    ++up;
    do {
      if (*base_last == '/') {
        --up;
      }
    } while (up > 0 && (--base_last > base->data));

    // Set path prefix
    base->length = (size_t)(base_last - base->data + 1);
  }

  // Set path suffix
  path->data   = begin;
  path->length = (size_t)(end - begin);
}

/// See http://tools.ietf.org/html/rfc3986#section-5.2.2
void
serd_uri_resolve(const SerdURIView* const r,
                 const SerdURIView* const base,
                 SerdURIView* const       t)
{
  assert(r);
  assert(base);
  assert(t);

  if (!base->scheme.length) {
    *t = *r; // Don't resolve against non-absolute URIs
    return;
  }

  t->path_base.data   = "";
  t->path_base.length = 0;
  if (r->scheme.length) {
    *t = *r;
  } else {
    if (r->authority.length) {
      t->authority = r->authority;
      t->path      = r->path;
      t->query     = r->query;
    } else {
      t->path = r->path;
      if (!r->path.length) {
        t->path_base = base->path;
        if (r->query.length) {
          t->query = r->query;
        } else {
          t->query = base->query;
        }
      } else {
        if (r->path.data[0] != '/') {
          t->path_base = base->path;
        }
        merge(&t->path_base, &t->path);
        t->query = r->query;
      }
      t->authority = base->authority;
    }
    t->scheme   = base->scheme;
    t->fragment = r->fragment;
  }
}

/** Write the path of `uri` starting at index `i` */
static size_t
write_path_tail(SerdSink                 sink,
                void* const              stream,
                const SerdURIView* const uri,
                const size_t             i)
{
  SERD_DISABLE_NULL_WARNINGS

  size_t len = 0;
  if (i < uri->path_base.length) {
    len += sink(uri->path_base.data + i, uri->path_base.length - i, stream);
  }

  if (uri->path.data) {
    if (i < uri->path_base.length) {
      len += sink(uri->path.data, uri->path.length, stream);
    } else {
      const size_t j = (i - uri->path_base.length);
      len += sink(uri->path.data + j, uri->path.length - j, stream);
    }
  }

  return len;

  SERD_RESTORE_WARNINGS
}

/** Write the path of `uri` relative to the path of `base`. */
static size_t
write_rel_path(SerdSink                 sink,
               void* const              stream,
               const SerdURIView* const uri,
               const SerdURIView* const base)
{
  const size_t path_len = uri_path_len(uri);
  const size_t base_len = uri_path_len(base);
  const size_t min_len  = (path_len < base_len) ? path_len : base_len;

  // Find the last separator common to both paths
  size_t last_shared_sep = 0;
  size_t i               = 0;
  for (; i < min_len && uri_path_at(uri, i) == uri_path_at(base, i); ++i) {
    if (uri_path_at(uri, i) == '/') {
      last_shared_sep = i;
    }
  }

  if (i == path_len && i == base_len) { // Paths are identical
    return 0;
  }

  // Find the number of up references ("..") required
  size_t up = 0;
  for (size_t s = last_shared_sep + 1; s < base_len; ++s) {
    if (uri_path_at(base, s) == '/') {
      ++up;
    }
  }

  // Write up references
  size_t len = 0;
  for (size_t u = 0; u < up; ++u) {
    len += sink("../", 3, stream);
  }

  // Write suffix
  return len + write_path_tail(sink, stream, uri, last_shared_sep + 1);
}

static uint8_t
serd_uri_path_starts_without_slash(const SerdURIView* uri)
{
  return ((uri->path_base.length || uri->path.length) &&
          ((!uri->path_base.length || uri->path_base.data[0] != '/') &&
           (!uri->path.length || uri->path.data[0] != '/')));
}

/// See http://tools.ietf.org/html/rfc3986#section-5.3
size_t
serd_uri_serialise_relative(const SerdURIView* const uri,
                            const SerdURIView* const base,
                            const SerdURIView* const root,
                            SerdSink                 sink,
                            void* const              stream)
{
  assert(uri);
  assert(sink);

  size_t     len = 0;
  const bool relative =
    root ? uri_is_under(uri, root) : uri_is_related(uri, base);

  if (relative) {
    len = write_rel_path(sink, stream, uri, base);
  }

  SERD_DISABLE_NULL_WARNINGS

  if (!relative || (!len && base->query.data)) {
    if (uri->scheme.data) {
      len += sink(uri->scheme.data, uri->scheme.length, stream);
      len += sink(":", 1, stream);
    }
    if (uri->authority.data) {
      len += sink("//", 2, stream);
      len += sink(uri->authority.data, uri->authority.length, stream);

      const bool authority_ends_with_slash =
        (uri->authority.length > 0 &&
         uri->authority.data[uri->authority.length - 1] == '/');

      if (!authority_ends_with_slash &&
          serd_uri_path_starts_without_slash(uri)) {
        // Special case: ensure path begins with a slash
        // https://tools.ietf.org/html/rfc3986#section-3.2
        len += sink("/", 1, stream);
      }
    }
    len += write_path_tail(sink, stream, uri, 0);
  }

  if (uri->query.data) {
    len += sink("?", 1, stream);
    len += sink(uri->query.data, uri->query.length, stream);
  }

  if (uri->fragment.data) {
    // Note uri->fragment.data includes the leading '#'
    len += sink(uri->fragment.data, uri->fragment.length, stream);
  }

  SERD_RESTORE_WARNINGS

  return len;
}

/// See http://tools.ietf.org/html/rfc3986#section-5.3
size_t
serd_uri_serialise(const SerdURIView* const uri,
                   SerdSink                 sink,
                   void* const              stream)
{
  assert(uri);
  assert(sink);
  return serd_uri_serialise_relative(uri, NULL, NULL, sink, stream);
}
