// Copyright 2018-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "caret_impl.h" // IWYU pragma: keep

#include "serd/caret.h"
#include "serd/caret_view.h"
#include "serd/node.h"
#include "serd/token_view.h"
#include "zix/allocator.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

SerdCaret*
serd_caret_new(ZixAllocator* const   allocator,
               const SerdNode* const document,
               const unsigned        line,
               const unsigned        column)
{
  assert(document);

  SerdCaret* const caret = (SerdCaret*)zix_malloc(allocator, sizeof(SerdCaret));

  if (caret) {
    caret->document = document;
    caret->line     = line;
    caret->col      = column;
  }

  return caret;
}

SerdCaret*
serd_caret_copy(ZixAllocator* const allocator, const SerdCaret* const caret)
{
  if (!caret) {
    return NULL;
  }

  SerdCaret* const copy = (SerdCaret*)zix_malloc(allocator, sizeof(SerdCaret));

  if (copy) {
    memcpy(copy, caret, sizeof(SerdCaret));
  }

  return copy;
}

void
serd_caret_free(ZixAllocator* const allocator, SerdCaret* const caret)
{
  zix_free(allocator, caret);
}

SerdCaretView
serd_caret_caret_view(const SerdCaret* const caret)
{
  SerdTokenView document = {{"", 0U}, SERD_LITERAL};
  if (!caret) {
    const SerdCaretView view = {document, 0U, 0U};
    return view;
  }

  if (caret->document) {
    document = serd_node_token_view(caret->document);
  }

  const SerdCaretView view = {document, caret->line, caret->col};
  return view;
}

bool
serd_caret_equals(const SerdCaret* const l, const SerdCaret* const r)
{
  return (l == r || (l && r && serd_node_equals(l->document, r->document) &&
                     l->line == r->line && l->col == r->col));
}

const SerdNode*
serd_caret_document(const SerdCaret* const caret)
{
  assert(caret);
  assert(caret->document);

  return caret->document;
}

unsigned
serd_caret_line(const SerdCaret* const caret)
{
  assert(caret);
  return caret->line;
}

unsigned
serd_caret_column(const SerdCaret* const caret)
{
  assert(caret);
  return caret->col;
}
