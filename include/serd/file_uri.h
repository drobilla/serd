// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_FILE_URI_H
#define SERD_FILE_URI_H

#include <serd/attributes.h>
#include <serd/stream.h>
#include <serd/string.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_file_uri File URI
   @ingroup serd_data

   API for writing or allocating file URIs from path and hostname strings.

   @{
*/

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
   Return a file URI as a newly allocated string.

   @return A file URI with the given `path` and `hostname` as a string, or an
   empty string with NULL `data` on error.
*/
SERD_API SerdString
serd_file_uri_to_string(ZixAllocator* ZIX_NULLABLE allocator,
                        ZixStringView              path,
                        ZixStringView              hostname);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_FILE_URI_H
