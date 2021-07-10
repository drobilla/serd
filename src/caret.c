// Copyright 2018-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "caret.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

SerdCaret*
serd_caret_new(const SerdNode* name, unsigned line, unsigned col)
{
  SerdCaret* caret = (SerdCaret*)malloc(sizeof(SerdCaret));

  caret->file = name;
  caret->line = line;
  caret->col  = col;
  return caret;
}

SerdCaret*
serd_caret_copy(const SerdCaret* caret)
{
  if (!caret) {
    return NULL;
  }

  SerdCaret* copy = (SerdCaret*)malloc(sizeof(SerdCaret));
  memcpy(copy, caret, sizeof(SerdCaret));
  return copy;
}

void
serd_caret_free(SerdCaret* caret)
{
  free(caret);
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
  return caret->file;
}

unsigned
serd_caret_line(const SerdCaret* caret)
{
  return caret->line;
}

unsigned
serd_caret_column(const SerdCaret* caret)
{
  return caret->col;
}
