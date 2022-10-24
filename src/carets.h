// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_CARETS_H
#define SERD_SRC_CARETS_H

#include "statement.h"

#include <serd/caret_view.h>
#include <serd/nodes.h>
#include <serd/status.h>
#include <zix/allocator.h>
#include <zix/attributes.h>

struct ZixHashImpl* ZIX_ALLOCATED
serd_carets_new(ZixAllocator* ZIX_NULLABLE allocator);

void
serd_carets_free(struct ZixHashImpl* ZIX_NULLABLE carets,
                 ZixAllocator* ZIX_NULLABLE       allocator);

SerdStatus
serd_carets_insert(struct ZixHashImpl* ZIX_NONNULL  carets,
                   ZixAllocator* ZIX_NULLABLE       allocator,
                   SerdNodes* ZIX_NONNULL           nodes,
                   const SerdStatement* ZIX_NONNULL statement,
                   SerdCaretView                    caret);

SerdStatus
serd_carets_remove(struct ZixHashImpl* ZIX_NONNULL  carets,
                   ZixAllocator* ZIX_NULLABLE       allocator,
                   const SerdStatement* ZIX_NONNULL statement);

SerdCaretView
serd_carets_get(const struct ZixHashImpl* ZIX_NONNULL carets,
                const SerdNodes* ZIX_NONNULL          nodes,
                const SerdStatement* ZIX_NONNULL      statement);

#endif // SERD_SRC_CARETS_H
