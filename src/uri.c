// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"
#include "uri_utils.h"

#include "serd/buffer.h"
#include "serd/output_stream.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/uri.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SerdStatus
write_file_uri_char(const char c, SerdOutputStream* const out)
{
  return (out->write(&c, 1, 1, out->stream) == 1) ? SERD_SUCCESS
                                                  : SERD_BAD_ALLOC;
}

static char*
parse_hostname(const char* const authority, char** const hostname)
{
  char* const path = strchr(authority, '/');
  if (!path) {
    return NULL;
  }

  if (hostname) {
    const size_t len = (size_t)(path - authority);
    if ((*hostname = (char*)calloc(len + 1, 1))) {
      memcpy(*hostname, authority, len);
    }
  }

  return path;
}

char*
serd_parse_file_uri(const char* const uri, char** const hostname)
{
  assert(uri);

  SerdStatus st = SERD_SUCCESS;

  const char* path = uri;
  if (hostname) {
    *hostname = NULL;
  }

  if (!strncmp(uri, "file://", 7)) {
    const char* auth = uri + 7;
    if (*auth == '/') { // No hostname
      path = auth;
    } else if (!(path = parse_hostname(auth, hostname))) {
      return NULL;
    }
  }

  if (is_windows_path(path + 1)) {
    ++path;
  }

  SerdBuffer       buffer = {NULL, 0};
  SerdOutputStream out    = serd_open_output_buffer(&buffer);
  for (const char* s = path; !st && *s; ++s) {
    if (*s == '%') {
      if (is_hexdig(*(s + 1)) && is_hexdig(*(s + 2))) {
        const uint8_t hi = hex_digit_value((const uint8_t)s[1]);
        const uint8_t lo = hex_digit_value((const uint8_t)s[2]);
        const char    c  = (char)((hi << 4U) | lo);

        st = write_file_uri_char(c, &out);
        s += 2;
      } else {
        st = SERD_BAD_SYNTAX;
      }
    } else {
      st = write_file_uri_char(*s, &out);
    }
  }

  const SerdStatus cst = serd_close_output(&out);
  if (st || cst) {
    free(buffer.buf);
    return NULL;
  }

  return (char*)buffer.buf;
}

/// RFC3986: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
bool
serd_uri_string_has_scheme(const char* const string)
{
  if (string && is_alpha(string[0])) {
    for (size_t i = 1; string[i]; ++i) {
      if (!is_uri_scheme_char(string[i])) {
        return false; // Non-scheme character before a ':'
      }

      if (string[i] == ':') {
        return true; // Valid scheme terminated by a ':'
      }
    }
  }

  return false;
}

static inline bool
is_uri_authority_char(const char c)
{
  return c && c != '/' && c != '?' && c != '#';
}

static inline bool
is_uri_path_char(const char c)
{
  return c && c != '?' && c != '#';
}

static inline bool
is_uri_query_char(const char c)
{
  return c && c != '#';
}

SerdURIView
serd_parse_uri(const char* const string)
{
  assert(string);

  SerdURIView result = SERD_URI_NULL;
  const char* ptr    = string;

  /* See http://tools.ietf.org/html/rfc3986#section-3
     URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ] */

  /* S3.1: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) */
  if (is_alpha(*ptr)) {
    for (char c = *++ptr; true; c = *++ptr) {
      if (c == ':') {
        result.scheme.data   = string;
        result.scheme.length = (size_t)(ptr++ - string);
        break;
      }

      if (!is_uri_scheme_char(c)) {
        ptr = string;
        break;
      }
    }
  }

  /* S3.2: The authority component is preceded by "//" and is terminated by the
     next '/', '?', or '#', or by the end of the URI. */
  if (*ptr == '/' && *(ptr + 1) == '/') {
    ptr += 2;
    result.authority.data = ptr;
    while (is_uri_authority_char(*ptr)) {
      ++result.authority.length;
      ++ptr;
    }
  }

  /* S3.3: The path is terminated by the first '?' or '#', or by the end of the
     URI. */
  if (is_uri_path_char(*ptr)) {
    result.path.data   = ptr++;
    result.path.length = 1U;
    while (is_uri_path_char(*ptr)) {
      ++result.path.length;
      ++ptr;
    }
  }

  /* S3.4: The query component is indicated by the first '?' and terminated by
     a '#' or by the end of the URI. */
  if (*ptr == '?') {
    result.query.data = ++ptr;
    while (is_uri_query_char(*ptr)) {
      ++result.query.length;
      ++ptr;
    }
  }

  /* S3.5: A fragment identifier component is indicated by the presence of a
     '#' and terminated by the end of the URI. */
  if (*ptr == '#') {
    result.fragment.data = ptr;
    while (*ptr++) {
      ++result.fragment.length;
    }
  }

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
merge(ZixStringView* const base, ZixStringView* const path)
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
SerdURIView
serd_resolve_uri(const SerdURIView r, const SerdURIView base)
{
  if (r.scheme.length || !base.scheme.length) {
    return r; // No resolution necessary || possible (respectively)
  }

  SerdURIView t = SERD_URI_NULL;

  if (r.authority.length) {
    t.authority = r.authority;
    t.path      = r.path;
    t.query     = r.query;
  } else {
    t.path = r.path;
    if (!r.path.length) {
      t.path_prefix = base.path;
      t.query       = r.query.length ? r.query : base.query;
    } else {
      if (r.path.data[0] != '/') {
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
    result.path.data   = uri.path.data;
    result.path.length = 0;
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
    if (last_shared_sep < uri.path_prefix.length) {
      return SERD_URI_NULL;
    }

    // Special representation: NULL buffer and len set to the depth
    result.path_prefix.length = up;
  }

  if (last_shared_sep < uri.path_prefix.length) {
    result.path_prefix.data   = uri.path_prefix.data + last_shared_sep + 1;
    result.path_prefix.length = uri.path_prefix.length - last_shared_sep - 1;
    result.path               = uri.path;
  } else {
    result.path.data   = uri.path.data + last_shared_sep + 1;
    result.path.length = uri.path.length - last_shared_sep - 1;
  }

  return result;
}

bool
serd_uri_is_within(const SerdURIView uri, const SerdURIView base)
{
  if (!base.scheme.length || !slice_equals(&base.scheme, &uri.scheme) ||
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

size_t
serd_uri_string_length(const SerdURIView uri)
{
  size_t len = 0;

  if (uri.scheme.data) {
    len += uri.scheme.length + 1;
  }

  if (uri.authority.data) {
    const bool needs_extra_slash =
      (uri.authority.length && uri_path_len(&uri) &&
       uri_path_at(&uri, 0) != '/');

    len += 2 + uri.authority.length + needs_extra_slash;
  }

  if (uri.path_prefix.data) {
    len += uri.path_prefix.length;
  } else if (uri.path_prefix.length) {
    len += 3 * uri.path_prefix.length;
  }

  if (uri.path.data) {
    len += uri.path.length;
  }

  if (uri.query.data) {
    len += uri.query.length + 1;
  }

  if (uri.fragment.data) {
    len += uri.fragment.length;
  }

  return len;
}

/// See http://tools.ietf.org/html/rfc3986#section-5.3
size_t
serd_write_uri(const SerdURIView uri, SerdWriteFunc sink, void* const stream)
{
  assert(sink);

  size_t len = 0;

  if (uri.scheme.data) {
    len += sink(uri.scheme.data, 1, uri.scheme.length, stream);
    len += sink(":", 1, 1, stream);
  }

  if (uri.authority.data) {
    len += sink("//", 1, 2, stream);
    len += sink(uri.authority.data, 1, uri.authority.length, stream);

    if (uri.authority.length && uri_path_len(&uri) &&
        uri_path_at(&uri, 0) != '/') {
      // Special case: ensure path begins with a slash
      // https://tools.ietf.org/html/rfc3986#section-3.2
      len += sink("/", 1, 1, stream);
    }
  }

  if (uri.path_prefix.data) {
    len += sink(uri.path_prefix.data, 1, uri.path_prefix.length, stream);
  } else if (uri.path_prefix.length) {
    for (size_t i = 0; i < uri.path_prefix.length; ++i) {
      len += sink("../", 1, 3, stream);
    }
  }

  if (uri.path.data) {
    len += sink(uri.path.data, 1, uri.path.length, stream);
  }

  if (uri.query.data) {
    len += sink("?", 1, 1, stream);
    len += sink(uri.query.data, 1, uri.query.length, stream);
  }

  if (uri.fragment.data) {
    // Note that uri.fragment.data includes the leading '#'
    len += sink(uri.fragment.data, 1, uri.fragment.length, stream);
  }

  return len;
}

static bool
is_unescaped_uri_path_char(const char c)
{
  return is_alpha(c) || is_digit(c) || strchr("!$&\'()*+,-./:;=@_~", c);
}

static bool
is_dir_sep(const char c)
{
#ifdef _WIN32
  return c == '\\' || c == '/';
#else
  return c == '/';
#endif
}

size_t
serd_write_file_uri(const ZixStringView path,
                    const ZixStringView hostname,
                    const SerdWriteFunc sink,
                    void* const         stream)
{
  assert(sink);
  assert(stream);

  const bool is_windows = is_windows_path(path.data);
  size_t     len        = 0U;

  if (is_dir_sep(path.data[0]) || is_windows) {
    len += sink("file://", 1, strlen("file://"), stream);
    if (hostname.length) {
      len += sink(hostname.data, 1, hostname.length, stream);
    }

    if (is_windows) {
      len += sink("/", 1, 1, stream);
    }
  }

  for (size_t i = 0; i < path.length; ++i) {
    if (is_unescaped_uri_path_char(path.data[i])) {
      len += sink(path.data + i, 1, 1, stream);
#ifdef _WIN32
    } else if (path.data[i] == '\\') {
      len += sink("/", 1, 1, stream);
#endif
    } else {
      char escape_str[10] = {'%', 0, 0, 0, 0, 0, 0, 0, 0, 0};
      snprintf(
        escape_str + 1, sizeof(escape_str) - 1, "%X", (unsigned)path.data[i]);
      len += sink(escape_str, 1, 3, stream);
    }
  }

  return len;
}
