// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_URI_H
#define SERD_URI_H

#include <serd/attributes.h>
#include <serd/stream.h>

#include <stdbool.h>
#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_uri URI
   @ingroup serd_data
   @{
*/

/**
   A component of a URI.

   This is like a string view, but the pointer may be null to distinguish
   between empty and missing components (an empty component indicates that the
   URI has the corresponding delimiter).
*/
typedef struct {
  const char* SERD_NULLABLE data;   ///< Start of string, or null
  size_t                    length; ///< Length of string in bytes
} SerdURIComponent;

/**
   A parsed view of a URI.

   This representation is designed for fast streaming.  It makes it possible to
   create relative URI references or resolve them into absolute URIs in-place
   without any string allocation.

   Each component refers to slices in other strings, so a URI view must outlive
   any strings it was parsed from.  Note that the components are not
   necessarily null-terminated.

   The scheme, authority, path, query, and fragment simply point to the string
   value of those components, not including any delimiters.  The path_prefix is
   a special component for storing relative or resolved paths.  If it points to
   a string (usually a base URI the URI was resolved against), then this string
   is prepended to the path.  Otherwise, the length is interpreted as the
   number of up-references ("../") that must be prepended to the path.
*/
typedef struct {
  SerdURIComponent scheme;      ///< Scheme
  SerdURIComponent authority;   ///< Authority
  SerdURIComponent path_prefix; ///< Path prefix for relative/resolved paths
  SerdURIComponent path;        ///< Path suffix
  SerdURIComponent query;       ///< Query
  SerdURIComponent fragment;    ///< Fragment
} SerdURIView;

static const SerdURIView SERD_URI_NULL =
  {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}};

/**
   Get the unescaped path and hostname from a file URI.

   The returned path and `*hostname` must be freed with serd_free().

   @param uri A file URI.
   @param hostname If non-NULL, set to the hostname, if present.
   @return A newly-allocated filesystem path.
*/
SERD_API char* SERD_ALLOCATED
serd_parse_file_uri(const char* SERD_NONNULL              uri,
                    char* SERD_UNSPECIFIED* SERD_NULLABLE hostname);

/// Return true iff `string` starts with a valid URI scheme
SERD_PURE_API bool
serd_uri_string_has_scheme(const char* SERD_NULLABLE string);

/// Return true iff `uri` has a URI scheme (is "absolute")
SERD_CONST_API bool
serd_uri_has_scheme(SerdURIView uri);

/// Parse `string` and return a URI view that points into it
SERD_PURE_API SerdURIView
serd_parse_uri(const char* SERD_NULLABLE string);

/**
   Return reference `r` resolved against `base`.

   This will make `r` an absolute URI if possible.

   @see [RFC3986 5.2.2](http://tools.ietf.org/html/rfc3986#section-5.2.2)

   @param r URI reference to make absolute, for example "child/path".

   @param base Base URI, for example "http://example.org/base/".

   @return An absolute URI, for example "http://example.org/base/child/path",
   or `r` if it is not a URI reference that can be resolved against `base`.
*/
SERD_PURE_API SerdURIView
serd_resolve_uri(SerdURIView r, SerdURIView base);

/**
   Return `r` as a reference relative to `base` if possible.

   @see [RFC3986 5.2.2](http://tools.ietf.org/html/rfc3986#section-5.2.2)

   @param r URI to make relative, for example
   "http://example.org/base/child/path".

   @param base Base URI, for example "http://example.org/base".

   @return A relative URI reference, for example "child/path", `r` if it can
   not be made relative to `base`, or a null URI if `r` could be made relative
   to base, but it has already been resolved and there's not enough space for
   all the required components.
*/
SERD_PURE_API SerdURIView
serd_relative_uri(SerdURIView r, SerdURIView base);

/**
   Return whether `r` can be written as a reference relative to `base`.

   For example, with `base` "http://example.org/base/", this returns true if
   `r` is also "http://example.org/base/", or something like
   "http://example.org/base/child",
   "http://example.org/base/child/grandchild#fragment",
   "http://example.org/base/child/grandchild?query", and so on.

   @return True if `r` and `base` are equal or if `r` is a child of `base`.
*/
SERD_PURE_API bool
serd_uri_is_within(SerdURIView r, SerdURIView base);

/**
   Return the length of `uri` as a string.

   This can be used to get the expected number of bytes that will be written by
   serd_write_uri().

   @return A string length in bytes, not including the null terminator.
*/
SERD_PURE_API size_t
serd_uri_string_length(SerdURIView uri);

/**
   Write `uri` string characters to `sink`.

   This will call `sink` several times to emit the URI.

   @param uri URI to write as a string.
   @param sink Sink to write string output to.
   @param stream Opaque user argument to pass to `sink`.

   @return The length of the written URI string (not including a null
   terminator, which isn't written), which may be less than
   `serd_uri_string_length(uri)` on error.
*/
SERD_API size_t
serd_write_uri(SerdURIView                uri,
               SerdWriteFunc SERD_NONNULL sink,
               void* SERD_UNSPECIFIED     stream);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_URI_H
