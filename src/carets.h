// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_CARETS_H
#define SERD_SRC_CARETS_H

#include "statement.h"

#include <serd/model_caret.h>
#include <serd/node_id.h>
#include <zix/allocator.h>
#include <zix/attributes.h>
#include <zix/status.h>

#include <stddef.h>

struct ZixHashImpl* ZIX_ALLOCATED
serd_carets_new(ZixAllocator* ZIX_NULLABLE allocator);

void
serd_carets_free(struct ZixHashImpl* ZIX_NULLABLE carets,
                 ZixAllocator* ZIX_NULLABLE       allocator);

ZixStatus
serd_carets_insert(struct ZixHashImpl* ZIX_NULLABLE carets,
                   ZixAllocator* ZIX_NULLABLE       allocator,
                   const SerdStatement* ZIX_NONNULL statement,
                   SerdNodeID                       doc,
                   size_t                           line,
                   size_t                           column);

ZixStatus
serd_carets_remove(struct ZixHashImpl* ZIX_NULLABLE carets,
                   ZixAllocator* ZIX_NULLABLE       allocator,
                   const SerdStatement* ZIX_NONNULL statement);

SerdModelCaret
serd_carets_get(const struct ZixHashImpl* ZIX_NONNULL carets,
                const SerdStatement* ZIX_NONNULL      statement);

#endif // SERD_SRC_CARETS_H
