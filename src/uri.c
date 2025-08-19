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
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef uint16_t FieldCount;

typedef struct {
  const SerdURIView* uri;
  unsigned           offset;
  SerdURIField       field;
} PathIterator;

static const SerdURIView null_uri = {NULL,
                                     NULL,
                                     SERD_URI_SCHEME,
                                     0U,
                                     {0U, 0U, 0U, 0U, 0U, 0U}};

static unsigned
field_offset(const SerdURIView  uri,
             const SerdURIField field,
             const SerdURIField current)
{
  return ((field < uri.split) != (current < uri.split) ? 0U
          : (field < current)                          ? (uri.counts[field])
                                                       : 0U);
}

static unsigned
start_offset(const SerdURIView uri, const SerdURIField field)
{
  return field_offset(uri, SERD_URI_SCHEME, field) +
         field_offset(uri, SERD_URI_AUTHORITY, field) +
         field_offset(uri, SERD_URI_PATH_PREFIX, field) +
         field_offset(uri, SERD_URI_PATH_SUFFIX, field) +
         field_offset(uri, SERD_URI_QUERY, field);
}

static const char*
field_data(const SerdURIView* const uri, const SerdURIField field)
{
  return (field < uri->split ? uri->front : uri->back);
}

static const char*
field_start(const SerdURIView* const uri, const SerdURIField field)
{
  return field_data(uri, field) + start_offset(*uri, field);
}

static PathIterator
path_begin(const SerdURIView* const uri)
{
  const SerdURIField field = uri->counts[SERD_URI_PATH_PREFIX]
                               ? SERD_URI_PATH_PREFIX
                               : SERD_URI_PATH_SUFFIX;

  const PathIterator iter = {uri, 0U, field};
  return iter;
}

static char
path_get(const PathIterator iter)
{
  if (iter.offset >= iter.uri->counts[iter.field]) {
    return '\0';
  }

  return field_start(iter.uri, iter.field)[iter.offset];
}

static PathIterator
path_next(const PathIterator iter)
{
  PathIterator result = iter;

  ++result.offset;
  if (iter.field == SERD_URI_PATH_PREFIX &&
      result.offset >= iter.uri->counts[SERD_URI_PATH_PREFIX]) {
    result.offset = 0U;
    result.field  = SERD_URI_PATH_SUFFIX;
  }

  return result;
}

static bool
field_equals(const SerdURIField field,
             const SerdURIView  lhs,
             const SerdURIView  rhs)
{
  return lhs.counts[field] && (lhs.counts[field] == rhs.counts[field]) &&
         zix_string_view_equals(serd_uri_field(lhs, field),
                                serd_uri_field(rhs, field));
}

static bool
has_relative_path(const SerdURIView uri)
{
  const char f = path_get(path_begin(&uri));
  return f && f != '/';
}

SerdURIView
serd_no_uri(void)
{
  return null_uri;
}

bool
serd_uri_is_null(const SerdURIView uri)
{
  return !uri.front && !uri.back;
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
  return !!uri.counts[SERD_URI_SCHEME];
}

ZixStringView
serd_uri_field(const SerdURIView uri, const SerdURIField field)
{
  static const uint8_t pre_offsets[]  = {0U, 2U, 0U, 0U, 1U, 1U};
  static const uint8_t post_offsets[] = {1U, 0U, 0U, 0U, 0U, 0U};

  const uint16_t    count = uri.counts[field];
  const uint16_t    pre   = pre_offsets[field];
  const uint16_t    post  = post_offsets[field];
  const char* const data  = field_start(&uri, field);
  assert(data);
  assert(!count || count >= pre + post);

  return !count ? zix_empty_string()
                : zix_substring(data + pre, (size_t)(count - pre - post));
}

static const char*
append_until(const char      end_chars[static 4],
             const char*     ptr,
             uint16_t* const count)
{
  while (*ptr != end_chars[0] && *ptr != end_chars[1] && *ptr != end_chars[2] &&
         *ptr != end_chars[3]) {
    ++*count;
    ++ptr;
  }

  return ptr;
}

SerdURIView
serd_parse_uri(const char* const string)
{
  //                           Auth Path Query
  static const char ends[6] = {'/', '?', '#', 0, 0, 0};

  SerdURIView result = {
    NULL, string, SERD_URI_SCHEME, 0U, {0U, 0U, 0U, 0U, 0U, 0U}};

  const char* ptr = string;
  if (!ptr) {
    return result;
  }

  /* See http://tools.ietf.org/html/rfc3986#section-3
     URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ] */

  /* S3.1: scheme ::= ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) */
  if (is_alpha(*ptr)) {
    for (char c = *++ptr; true; c = *++ptr) {
      if (c == ':') {
        result.counts[SERD_URI_SCHEME] = (uint16_t)(ptr++ - string + 1U);
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
    ptr += 2U;
    result.counts[SERD_URI_AUTHORITY] = 2U;
    ptr = append_until(ends, ptr, &result.counts[SERD_URI_AUTHORITY]);
  }

  /* S3.3: The path is terminated by the first '?' or '#', or by the end of the
     URI. */
  if (*ptr && *ptr != '?' && *ptr != '#') {
    ++ptr;
    result.counts[SERD_URI_PATH_SUFFIX] = 1U;
    ptr = append_until(&ends[1], ptr, &result.counts[SERD_URI_PATH_SUFFIX]);
  }

  /* S3.4: The query component is indicated by the first '?' and terminated by
     a '#' or by the end of the URI. */
  if (*ptr == '?') {
    ptr = append_until(&ends[2], ptr, &result.counts[SERD_URI_QUERY]);
  }

  /* S3.5: A fragment identifier component is indicated by the presence of a
     '#' and terminated by the end of the URI. */
  if (*ptr == '#') {
    while (*ptr++) {
      ++result.counts[SERD_URI_FRAGMENT];
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
static unsigned
remove_dot_segments(const char* const path,
                    const unsigned    len,
                    unsigned* const   up)
{
  *up = 0;

  for (unsigned i = 0U; i < len;) {
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

/// Drop everything before `offset` bytes into `field`
static void
drop_everything_before(SerdURIView* const uri,
                       const SerdURIField field,
                       const uint16_t     offset)
{
  assert(offset <= uri->counts[field]);

  const unsigned new_field_count = (unsigned)(uri->counts[field] - offset);

  for (unsigned i = 0U; i < field; ++i) {
    if (uri->split <= i) {
      assert(uri->back);
      uri->back += uri->counts[i];
    } else {
      assert(uri->front);
      uri->front += uri->counts[i];
    }
    uri->counts[i] = 0U;
  }

  if (uri->split <= field) {
    uri->back += offset;
  } else {
    uri->front += offset;
  }

  uri->counts[field] = (uint16_t)new_field_count;
}

/// Merge path prefix and suffix in-place
static void
merge_paths(SerdURIView* const uri)
{
  const char* const path_data = field_start(uri, SERD_URI_PATH_SUFFIX);
  const FieldCount  path_len  = uri->counts[SERD_URI_PATH_SUFFIX];
  unsigned          up        = 0;
  const unsigned    start     = remove_dot_segments(path_data, path_len, &up);
  const char* const base_data = field_start(uri, SERD_URI_PATH_PREFIX);
  const size_t      base_len  = uri->counts[SERD_URI_PATH_PREFIX];
  if (base_len) {
    // Find the up'th last slash
    const char* base_last = (base_data + base_len - 1);
    ++up;
    do {
      if (*base_last == '/') {
        --up;
      }
    } while (up > 0 && (--base_last > base_data));

    // Set path prefix
    uri->counts[SERD_URI_PATH_PREFIX] = (FieldCount)(base_last - base_data + 1);
  }

  // Set path suffix
  uri->back += start;
  uri->counts[SERD_URI_PATH_SUFFIX] = (FieldCount)(path_len - start);
}

/// See http://tools.ietf.org/html/rfc3986#section-5.2.2
SerdURIView
serd_resolve_uri(const SerdURIView r, const SerdURIView base)
{
  if (serd_uri_has_scheme(r) || !serd_uri_has_scheme(base)) {
    return r; // No resolution necessary || possible (respectively)
  }

  SerdURIView t = {base.back,
                   r.back,
                   SERD_URI_SCHEME,
                   0U,
                   {base.counts[SERD_URI_SCHEME],
                    0U,
                    0U,
                    r.counts[SERD_URI_PATH_SUFFIX],
                    0U,
                    r.counts[SERD_URI_FRAGMENT]}};

  if (r.counts[SERD_URI_AUTHORITY]) {
    t.split                      = SERD_URI_AUTHORITY;
    t.counts[SERD_URI_AUTHORITY] = r.counts[SERD_URI_AUTHORITY];
    t.counts[SERD_URI_QUERY]     = r.counts[SERD_URI_QUERY];
  } else {
    t.counts[SERD_URI_AUTHORITY] = base.counts[SERD_URI_AUTHORITY];
    if (!r.counts[SERD_URI_PATH_SUFFIX]) {
      t.counts[SERD_URI_PATH_SUFFIX] = base.counts[SERD_URI_PATH_SUFFIX];
      if (r.counts[SERD_URI_QUERY]) {
        t.split                  = SERD_URI_QUERY;
        t.counts[SERD_URI_QUERY] = r.counts[SERD_URI_QUERY];
      } else {
        t.split                  = SERD_URI_FRAGMENT;
        t.counts[SERD_URI_QUERY] = base.counts[SERD_URI_QUERY];
      }
    } else {
      t.split = SERD_URI_PATH_SUFFIX;
      if (has_relative_path(r)) {
        t.counts[SERD_URI_PATH_PREFIX] = base.counts[SERD_URI_PATH_SUFFIX];
      }

      merge_paths(&t);
      t.counts[SERD_URI_QUERY] = r.counts[SERD_URI_QUERY];
    }
  }

  return t;
}

SerdURIView
serd_relative_uri(const SerdURIView uri, const SerdURIView base)
{
  // Return nothing if either URI is relative, or if authorities don't match
  if (!field_equals(SERD_URI_SCHEME, base, uri) ||
      !field_equals(SERD_URI_AUTHORITY, base, uri)) {
    return serd_no_uri();
  }

  // Find the last separator (s) common to both paths
  PathIterator u = path_begin(&uri);
  PathIterator b = path_begin(&base);
  PathIterator s = b;
  for (char c = '\0'; (c = path_get(u)) && c == path_get(b);) {
    if (c == '/') {
      s = b;
    }
    u = path_next(u);
    b = path_next(b);
  }

  // If the URI and base URI have identical paths, the relative path is empty
  SerdURIView result = uri;
  if (!path_get(u) && !path_get(b)) {
    drop_everything_before(&result, SERD_URI_QUERY, 0U);
    return result;
  }

  // Otherwise, we need to build a relative path view
  // Find the number of up-references ("..") required
  for (PathIterator t = path_next(s); path_get(t); t = path_next(t)) {
    if (path_get(t) == '/') {
      if (result.up == UINT8_MAX) {
        return serd_no_uri();
      }

      ++result.up;
    }
  }

  if (s.offset < uri.counts[SERD_URI_PATH_PREFIX]) {
    drop_everything_before(
      &result, SERD_URI_PATH_PREFIX, (uint16_t)(s.offset + 1U));
  } else {
    drop_everything_before(
      &result,
      SERD_URI_PATH_SUFFIX,
      (uint16_t)(s.offset + 1U - uri.counts[SERD_URI_PATH_PREFIX]));
  }

  return result;
}

bool
serd_uri_is_within(const SerdURIView uri, const SerdURIView base)
{
  if (!field_equals(SERD_URI_SCHEME, base, uri) ||
      !field_equals(SERD_URI_AUTHORITY, base, uri)) {
    return false;
  }

  PathIterator u      = path_begin(&uri);
  PathIterator b      = path_begin(&base);
  PathIterator s      = b; // Last '/' in base
  bool         differ = false;
  for (char uc = '\0', bc = '\0'; (uc = path_get(u)) && (bc = path_get(b));) {
    differ = differ || uc != bc;
    if (bc == '/') {
      s = b;
      if (differ) {
        return false;
      }
    }

    u = path_next(u);
    b = path_next(b);
  }

  for (s = path_next(s); path_get(s); s = path_next(s)) {
    if (path_get(s) == '/') {
      return false;
    }
  }

  return true;
}

size_t
serd_uri_string_length(const SerdURIView uri)
{
  const bool needs_extra_slash =
    uri.counts[SERD_URI_AUTHORITY] && has_relative_path(uri);

  return (uri.counts[SERD_URI_SCHEME] +
          (uri.counts[SERD_URI_AUTHORITY] + (size_t)needs_extra_slash) +
          (uri.counts[SERD_URI_PATH_PREFIX]) + (3U * (size_t)uri.up) +
          (uri.counts[SERD_URI_PATH_SUFFIX]) + (uri.counts[SERD_URI_QUERY]) +
          (uri.counts[SERD_URI_FRAGMENT]));
}

static SerdStreamResult
write_component(const SerdURIView   uri,
                const SerdURIField  field,
                const SerdWriteFunc sink,
                void* const         stream)
{
  const ZixStringView string = serd_uri_field(uri, field);
  return sink(stream, string.length, string.data);
}

/// See http://tools.ietf.org/html/rfc3986#section-5.3
SerdStreamResult
serd_write_uri(const SerdURIView   uri,
               const SerdWriteFunc sink,
               void* const         stream)
{
  assert(sink);

  SerdStreamResult wr = {SERD_SUCCESS, 0U};

  if (uri.counts[SERD_URI_SCHEME]) {
    TRY_WRITE(wr, write_component(uri, SERD_URI_SCHEME, sink, stream));
    TRY_WRITE(wr, sink(stream, 1, ":"));
  }

  if (uri.counts[SERD_URI_AUTHORITY]) {
    TRY_WRITE(wr, sink(stream, 2, "//"));
    TRY_WRITE(wr, write_component(uri, SERD_URI_AUTHORITY, sink, stream));

    if (uri.counts[SERD_URI_AUTHORITY] && has_relative_path(uri)) {
      // Special case: ensure path begins with a slash
      // https://tools.ietf.org/html/rfc3986#section-3.2
      TRY_WRITE(wr, sink(stream, 1, "/"));
    }
  }

  if (uri.up) {
    for (uint8_t i = 0U; i < uri.up; ++i) {
      TRY_WRITE(wr, sink(stream, 3, "../"));
    }
  }

  if (uri.counts[SERD_URI_PATH_PREFIX]) {
    TRY_WRITE(wr, write_component(uri, SERD_URI_PATH_PREFIX, sink, stream));
  }

  if (uri.counts[SERD_URI_PATH_SUFFIX]) {
    TRY_WRITE(wr, write_component(uri, SERD_URI_PATH_SUFFIX, sink, stream));
  }

  if (uri.counts[SERD_URI_QUERY]) {
    TRY_WRITE(wr, sink(stream, 1, "?"));
    TRY_WRITE(wr, write_component(uri, SERD_URI_QUERY, sink, stream));
  }

  if (uri.counts[SERD_URI_FRAGMENT]) {
    TRY_WRITE(wr, sink(stream, 1, "#"));
    TRY_WRITE(wr, write_component(uri, SERD_URI_FRAGMENT, sink, stream));
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
