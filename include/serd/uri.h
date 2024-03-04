// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_URI_H
#define SERD_URI_H

#include "serd/attributes.h"
#include "serd/stream.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <stdbool.h>
#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_uri URI
   @ingroup serd_data
   @{
*/

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
  ZixStringView scheme;      ///< Scheme
  ZixStringView authority;   ///< Authority
  ZixStringView path_prefix; ///< Path prefix for relative/resolved paths
  ZixStringView path;        ///< Path suffix
  ZixStringView query;       ///< Query
  ZixStringView fragment;    ///< Fragment
} SerdURIView;

static const SerdURIView SERD_URI_NULL =
  {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}};

/**
   Get the unescaped path and hostname from a file URI.

   Both the returned path and `*hostname` must be freed with zix_free() using
   the same allocator.

   @param allocator Allocator for the returned string.
   @param uri A file URI.
   @param hostname If non-NULL, set to the newly allocated hostname, if present.

   @return A newly allocated path string.
*/
SERD_API char* ZIX_ALLOCATED
serd_parse_file_uri(ZixAllocator* ZIX_NULLABLE          allocator,
                    const char* ZIX_NONNULL             uri,
                    char* ZIX_UNSPECIFIED* ZIX_NULLABLE hostname);

/// Return true iff `string` starts with a valid URI scheme
SERD_PURE_API bool
serd_uri_string_has_scheme(const char* ZIX_NULLABLE string);

/// Parse `string` and return a URI view that points into it
SERD_PURE_API SerdURIView
serd_parse_uri(const char* ZIX_NONNULL string);

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
   to base, but the path prefix is already being used (most likely because `r`
   was previously a relative URI reference that was resolved against some
   base).
*/
SERD_PURE_API SerdURIView
serd_relative_uri(SerdURIView r, SerdURIView base);

/**
   Return whether `r` can be written as a reference relative to `base`.

   For example, with `base` "http://example.org/base/", this returns true if
   `r` is also "http://example.org/base/", or something like
   "http://example.org/base/child" ("child")
   "http://example.org/base/child/grandchild#fragment"
   ("child/grandchild#fragment"),
   "http://example.org/base/child/grandchild?query" ("child/grandchild?query"),
   and so on.

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
   Write `uri` as a string to `sink`.

   This will call `sink` several times to emit the URI.

   @param uri URI to write as a string.
   @param sink Sink to write string output to.
   @param stream Opaque user argument to pass to `sink`.

   @return The length of the written URI string (not including a null
   terminator, which is not written), which may be less than
   `serd_uri_string_length(uri)` on error.
*/
SERD_API size_t
serd_write_uri(SerdURIView               uri,
               SerdWriteFunc ZIX_NONNULL sink,
               void* ZIX_UNSPECIFIED     stream);

/**
   Write a file URI to `sink` from a path and optional hostname.

   Backslashes in Windows paths will be converted, and other characters will be
   percent encoded as necessary.

   If `path` is relative, `hostname` is ignored.

   @param path File system path.
   @param hostname Optional hostname.
   @param sink Sink to write string output to.
   @param stream Opaque user argument to pass to `sink`.

   @return The length of the written URI string (not including a null
   terminator, which is not written)
*/
SERD_API size_t
serd_write_file_uri(ZixStringView             path,
                    ZixStringView             hostname,
                    SerdWriteFunc ZIX_NONNULL sink,
                    void* ZIX_UNSPECIFIED     stream);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_URI_H
