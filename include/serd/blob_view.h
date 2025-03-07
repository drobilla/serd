// Copyright 2024-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_BLOB_VIEW_H
#define SERD_BLOB_VIEW_H

#include <serd/attributes.h>
#include <zix/attributes.h>

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_blob_view Blob View
   @ingroup serd_data
   @{
*/

/// A view of an opaque blob of binary data
typedef struct {
  size_t                  size; ///< Size of `data` in bytes
  const void* ZIX_NONNULL data; ///< Opaque data
} SerdBlobView;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_BLOB_VIEW_H
