/*
  Copyright 2018-2020 David Robillard <d@drobilla.net>

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

#include "memory.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

SerdCaret*
serd_caret_new(SerdAllocator* const  allocator,
               const SerdNode* const name,
               const unsigned        line,
               const unsigned        column)
{
  assert(name);

  SerdCaret* const caret =
    (SerdCaret*)serd_amalloc(allocator, sizeof(SerdCaret));

  if (caret) {
    caret->file   = name;
    caret->line   = line;
    caret->column = column;
  }

  return caret;
}

SerdCaret*
serd_caret_copy(SerdAllocator* const allocator, const SerdCaret* caret)
{
  if (!caret) {
    return NULL;
  }

  SerdCaret* const copy =
    (SerdCaret*)serd_amalloc(allocator, sizeof(SerdCaret));

  if (copy) {
    memcpy(copy, caret, sizeof(SerdCaret));
  }

  return copy;
}

void
serd_caret_free(SerdAllocator* const allocator, SerdCaret* caret)
{
  serd_afree(allocator, caret);
}

bool
serd_caret_equals(const SerdCaret* l, const SerdCaret* r)
{
  return (l == r || (l && r && serd_node_equals(l->file, r->file) &&
                     l->line == r->line && l->column == r->column));
}

const SerdNode*
serd_caret_name(const SerdCaret* caret)
{
  assert(caret);
  assert(caret->file);
  return caret->file;
}

unsigned
serd_caret_line(const SerdCaret* caret)
{
  assert(caret);
  return caret->line;
}

unsigned
serd_caret_column(const SerdCaret* caret)
{
  assert(caret);
  return caret->column;
}
