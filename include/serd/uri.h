// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_URI_H
#define SERD_URI_H

#include "serd/attributes.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/string_view.h"

#include <stdbool.h>
#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_uri URI
   @ingroup serd_data
   @{
*/

/**
   A parsed URI.

   This struct directly refers to slices in other strings, it does not own any
   memory itself.  This allows some URI operations like resolution to be done
   in-place without allocating memory.
*/
typedef struct {
  SerdStringView scheme;    ///< Scheme
  SerdStringView authority; ///< Authority
  SerdStringView path_base; ///< Path prefix if relative
  SerdStringView path;      ///< Path suffix
  SerdStringView query;     ///< Query
  SerdStringView fragment;  ///< Fragment
} SerdURIView;

static const SerdURIView SERD_URI_NULL =
  {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}};

/**
   Get the unescaped path and hostname from a file URI.

   The returned path and `*hostname` must be freed with serd_free().

   @param uri A file URI.
   @param hostname If non-NULL, set to the hostname, if present.
   @return The path component of the URI.
*/
SERD_API char* SERD_NULLABLE
serd_file_uri_parse(const char* SERD_NONNULL              uri,
                    char* SERD_UNSPECIFIED* SERD_NULLABLE hostname);

/// Return true iff `utf8` starts with a valid URI scheme
SERD_PURE_API
bool
serd_uri_string_has_scheme(const char* SERD_NULLABLE utf8);

/// Parse `utf8`, writing result to `out`
SERD_API SerdStatus
serd_uri_parse(const char* SERD_NONNULL utf8, SerdURIView* SERD_NONNULL out);

/**
   Set target `t` to reference `r` resolved against `base`.

   @see [RFC3986 5.2.2](http://tools.ietf.org/html/rfc3986#section-5.2.2)
*/
SERD_API void
serd_uri_resolve(const SerdURIView* SERD_NONNULL r,
                 const SerdURIView* SERD_NONNULL base,
                 SerdURIView* SERD_NONNULL       t);

/// Serialise `uri` with a series of calls to `sink`
SERD_API size_t
serd_uri_serialise(const SerdURIView* SERD_NONNULL uri,
                   SerdWriteFunc SERD_NONNULL      sink,
                   void* SERD_UNSPECIFIED          stream);

/**
   Serialise `uri` relative to `base` with a series of calls to `sink`

   The `uri` is written as a relative URI iff if it a child of `base` and
   `root`.  The optional `root` parameter must be a prefix of `base` and can be
   used keep up-references ("../") within a certain namespace.
*/
SERD_API size_t
serd_uri_serialise_relative(const SerdURIView* SERD_NONNULL  uri,
                            const SerdURIView* SERD_NULLABLE base,
                            const SerdURIView* SERD_NULLABLE root,
                            SerdWriteFunc SERD_NONNULL       sink,
                            void* SERD_UNSPECIFIED           stream);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_URI_H
