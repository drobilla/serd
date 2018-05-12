/*
  Copyright 2011-2018 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef SERD_MODEL_H
#define SERD_MODEL_H

#include "serd/serd.h"
#include "zix/btree.h"

#include <stdint.h>

struct SerdModelImpl
{
	SerdWorld*     world;       ///< World this model is a part of
	ZixBTree*      indices[12]; ///< Trees of SordQuad
	SerdIter*      end;         ///< End iterator (always the same)
	uint64_t       version;     ///< Version incremented on every change
	SerdModelFlags flags;       ///< Active indices and features
};

SerdStatus
serd_model_add_internal(SerdModel*        model,
                        const SerdCursor* cursor,
                        const SerdNode*   s,
                        const SerdNode*   p,
                        const SerdNode*   o,
                        const SerdNode*   g);

#endif // SERD_MODEL_H
