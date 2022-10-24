// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_CURSOR_INTERNAL_H
#define SERD_SRC_CURSOR_INTERNAL_H

#include "cursor_impl.h"
#include "statement.h"

#include <serd/cursor.h>
#include <serd/model.h>
#include <serd/node_id.h>
#include <serd/status.h>
#include <zix/btree.h>

#include <stdbool.h>

const SerdStatement*
serd_cursor_get_internal(const SerdCursor* cursor);

SerdCursor
serd_cursor_make(const SerdModel* model,
                 ZixBTreeIter     iter,
                 const SerdNodeID pattern[static 4],
                 ScanStrategy     strategy);

SerdStatus
serd_cursor_scan_next(SerdCursor* cursor);

bool
serd_iter_in_range(ZixBTreeIter     iter,
                   const SerdNodeID pattern[static 4],
                   ScanStrategy     strategy);

#endif // SERD_SRC_CURSOR_INTERNAL_H
