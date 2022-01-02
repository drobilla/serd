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

#include "caret.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

SerdCaret*
serd_caret_new(const SerdNode* name, unsigned line, unsigned col)
{
  assert(name);

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
