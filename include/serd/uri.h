// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_URI_H
#define SERD_URI_H

#include <serd/attributes.h>
#include <serd/stream.h>
#include <serd/stream_result.h>
#include <serd/string.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_uri URI
   @ingroup serd_data
   @{
*/

/// The number of fields in a #SerdURIView
#define SERD_N_URI_FIELDS 6U

/// A component of a #SerdURIView
typedef enum {
  SERD_URI_SCHEME,      ///< Scheme
  SERD_URI_AUTHORITY,   ///< Authority
  SERD_URI_PATH_PREFIX, ///< Path prefix for resolved paths
  SERD_URI_PATH_SUFFIX, ///< Path suffix
  SERD_URI_QUERY,       ///< Query
  SERD_URI_FRAGMENT,    ///< Fragment
} SerdURIField;

/**
   A parsed view of a URI.

   This is a lightweight view type designed for streaming that allows URIs to
   be parsed, resolved, and made relative without allocation.  Path
   up-references ("../") are specially encoded since these characters aren't in
   the inputs.
*/
typedef struct {
  const char* ZIX_NULLABLE front;     ///< Prefix up to the split (from base)
  const char* ZIX_NULLABLE back;      ///< Suffix after the split
  SerdURIField             split : 8; ///< First field in `back` (not `front`)
  uint8_t                  up;        ///< Number of up-references ("../")

  uint16_t counts[SERD_N_URI_FIELDS]; ///< Counts for each field
} SerdURIView;

/**
   Return an unset/sentinel URI view.

   Note that this is distinct from the "empty" URI reference (written "<>").
   This has all-zero fields, whereas an empty URI reference has `back` pointing
   to some string, even though all counts are zero.
*/
SERD_CONST_API SerdURIView
serd_no_uri(void);

/// Return true iff `uri` is not a view of any URI (not even an empty one)
SERD_CONST_API bool
serd_uri_is_null(SerdURIView uri);

/// Return true iff `string` starts with a valid URI scheme
SERD_PURE_API bool
serd_uri_string_has_scheme(const char* ZIX_NULLABLE string);

/// Return true iff `uri` has a URI scheme (is "absolute")
SERD_CONST_API bool
serd_uri_has_scheme(SerdURIView uri);

/// Return a view of the given field in `uri`
SERD_CONST_API ZixStringView
serd_uri_field(SerdURIView uri, SerdURIField field);

/// Parse `string` and return a URI view that points into it
SERD_PURE_API SerdURIView
serd_parse_uri(const char* ZIX_NULLABLE string);

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
SERD_CONST_API size_t
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
SERD_API SerdStreamResult
serd_write_uri(SerdURIView               uri,
               SerdWriteFunc ZIX_NONNULL sink,
               void* ZIX_UNSPECIFIED     stream);

/**
   Return `uri` flattened into a newly allocated string.

   @return `uri` as a string, or an empty string with NULL `data` on error.
*/
SERD_API SerdString
serd_uri_to_string(ZixAllocator* ZIX_NULLABLE allocator, SerdURIView uri);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_URI_H
