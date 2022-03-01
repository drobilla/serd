// Copyright 2018-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "caret.h"

#include "memory.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

SerdCaret*
serd_caret_new(SerdAllocator* const  allocator,
               const SerdNode* const name,
               const unsigned        line,
               const unsigned        col)
{
  assert(name);

  SerdCaret* const caret =
    (SerdCaret*)serd_amalloc(allocator, sizeof(SerdCaret));

  if (caret) {
    caret->file = name;
    caret->line = line;
    caret->col  = col;
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
                     l->line == r->line && l->col == r->col));
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
  return caret->col;
}
