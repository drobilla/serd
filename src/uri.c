// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"
#include "try_write.h"

#include <serd/status.h>
#include <serd/stream.h>
#include <serd/stream_result.h>
#include <serd/string.h>
#include <serd/uri.h>
#include <zix/allocator.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static bool
slice_equals(const SerdURIComponent* const a, const SerdURIComponent* const b)
{
  return a->length == b->length && !strncmp(a->data, b->data, a->length);
}

static size_t
uri_path_len(const SerdURIView* const uri)
{
  return uri->path_prefix.length + uri->path.length;
}

static char
uri_path_at(const SerdURIView* const uri, const size_t i)
{
  if (i < uri->path_prefix.length) {
    return uri->path_prefix.data[i];
  }

  const size_t p = i - uri->path_prefix.length;
  assert(p < uri->path.length);
  return uri->path.data[p];
}

SerdURIView
serd_empty_uri(void)
{
  static const SerdURIView empty_uri = {
    {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}};
  return empty_uri;
}

/// RFC3986: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
bool
serd_uri_string_has_scheme(const char* const string)
{
  if (string && is_alpha(string[0])) {
    for (const char* s = string;; ++s) {
      if (*s == ':') {
        return true; // Valid scheme terminated by a ':'
      }

      if (!is_scheme(*s)) {
        return false; // Non-scheme character before a ':'
      }
    }
  }

  return false;
}

bool
serd_uri_has_scheme(const SerdURIView uri)
{
  return !!uri.scheme.length;
}

static const char*
append_until(const char              end_chars[static 4],
             const char*             ptr,
             SerdURIComponent* const dest)
{
  while (*ptr != end_chars[0] && *ptr != end_chars[1] && *ptr != end_chars[2] &&
         *ptr != end_chars[3]) {
    ++dest->length;
    ++ptr;
  }

  return ptr;
}

SerdURIView
serd_parse_uri(const char* const string)
{
  //                           Auth Path Query
  static const char ends[6] = {'/', '?', '#', 0, 0, 0};

  SerdURIView result = serd_empty_uri();
  const char* ptr    = string;
  if (!ptr) {
    return result;
  }

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

      if (!is_scheme(c)) {
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
    ptr                   = append_until(ends, ptr, &result.authority);
  }

  /* S3.3: The path is terminated by the first '?' or '#', or by the end of the
     URI. */
  if (*ptr && *ptr != '?' && *ptr != '#') {
    result.path.data   = ptr++;
    result.path.length = 1U;
    ptr                = append_until(&ends[1], ptr, &result.path);
  }

  /* S3.4: The query component is indicated by the first '?' and terminated by
     a '#' or by the end of the URI. */
  if (*ptr == '?') {
    result.query.data = ++ptr;
    ptr               = append_until(&ends[2], ptr, &result.query);
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
   @return The offset of the new start in `path`
*/
static size_t
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
      return i;
    }
  }

  return len;
}

/// Merge `base` and `path` in-place
static void
merge(SerdURIComponent* const base, SerdURIComponent* const path)
{
  size_t       up    = 0;
  const size_t begin = remove_dot_segments(path->data, path->length, &up);

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
  path->data   = path->data + begin;
  path->length = path->length - begin;
}

/// See http://tools.ietf.org/html/rfc3986#section-5.2.2
SerdURIView
serd_resolve_uri(const SerdURIView r, const SerdURIView base)
{
  if (serd_uri_has_scheme(r) || !serd_uri_has_scheme(base)) {
    return r; // No resolution necessary || possible (respectively)
  }

  SerdURIView t = serd_empty_uri();

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
  // Do nothing if either URI is relative, or if the authorities don't match
  if (!serd_uri_has_scheme(uri) || !slice_equals(&base.scheme, &uri.scheme) ||
      !slice_equals(&base.authority, &uri.authority)) {
    return uri;
  }

  // Regardless of the path, the query and/or fragment come along
  SerdURIView result = serd_empty_uri();
  result.query       = uri.query;
  result.fragment    = uri.fragment;

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

  if (last_shared_sep < uri.path_prefix.length) {
    if (up > 0) {
      return serd_empty_uri();
    }

    result.path_prefix.data   = uri.path_prefix.data + last_shared_sep + 1;
    result.path_prefix.length = uri.path_prefix.length - last_shared_sep - 1;
    result.path               = uri.path;
  } else {
    if (up > 0) {
      // Special representation: NULL buffer and len set to the depth
      result.path_prefix.length = up;
    }

    result.path.data   = uri.path.data + last_shared_sep + 1;
    result.path.length = uri.path.length - last_shared_sep - 1;
  }

  return result;
}

bool
serd_uri_is_within(const SerdURIView uri, const SerdURIView base)
{
  if (!serd_uri_has_scheme(base) || !slice_equals(&base.scheme, &uri.scheme) ||
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
SerdStreamResult
serd_write_uri(const SerdURIView   uri,
               const SerdWriteFunc sink,
               void* const         stream)
{
  assert(sink);

  SerdStreamResult wr = {SERD_SUCCESS, 0U};

  if (uri.scheme.data) {
    const char* const scheme = uri.scheme.data;
    TRY_WRITE(wr, sink(stream, uri.scheme.length, scheme));
    TRY_WRITE(wr, sink(stream, 1, ":"));
  }

  if (uri.authority.data) {
    const char* const authority = uri.authority.data;
    TRY_WRITE(wr, sink(stream, 2, "//"));
    TRY_WRITE(wr, sink(stream, uri.authority.length, authority));

    if (uri.authority.length && uri_path_len(&uri) &&
        uri_path_at(&uri, 0) != '/') {
      // Special case: ensure path begins with a slash
      // https://tools.ietf.org/html/rfc3986#section-3.2
      TRY_WRITE(wr, sink(stream, 1, "/"));
    }
  }

  if (uri.path_prefix.data) {
    const char* const path_prefix = uri.path_prefix.data;
    TRY_WRITE(wr, sink(stream, uri.path_prefix.length, path_prefix));
  } else if (uri.path_prefix.length) {
    for (size_t i = 0; i < uri.path_prefix.length; ++i) {
      TRY_WRITE(wr, sink(stream, 3, "../"));
    }
  }

  if (uri.path.data) {
    const char* const path = uri.path.data;
    TRY_WRITE(wr, sink(stream, uri.path.length, path));
  }

  if (uri.query.data) {
    const char* const query = uri.query.data;
    TRY_WRITE(wr, sink(stream, 1, "?"));
    TRY_WRITE(wr, sink(stream, uri.query.length, query));
  }

  if (uri.fragment.data) {
    // Note that uri.fragment.data includes the leading '#'
    const char* const fragment = uri.fragment.data;
    TRY_WRITE(wr, sink(stream, uri.fragment.length, fragment));
  }

  return wr;
}

static SerdStreamResult
string_sink(void* const stream, const size_t len, const void* const buf)
{
  char** const ptr = (char**)stream;
  memcpy(*ptr, buf, len);
  *ptr += len;
  const SerdStreamResult r = {SERD_SUCCESS, len};
  return r;
}

SerdString
serd_uri_to_string(ZixAllocator* const allocator, const SerdURIView uri)
{
  SerdString   string = {0U, NULL};
  const size_t length = serd_uri_string_length(uri);

  if ((string.data = (char*)zix_calloc(allocator, length + 1U, 1U))) {
    char*        ptr        = string.data;
    const size_t actual_len = serd_write_uri(uri, string_sink, &ptr).count;

    string.length = actual_len;
  }

  return string;
}
