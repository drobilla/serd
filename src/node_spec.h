/*
  Copyright 2021 David Robillard <d@drobilla.net>

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

#ifndef SERD_NODE_SPEC_H
#define SERD_NODE_SPEC_H

#include "serd/serd.h"

/**
   A lightweight "specification" of a node.

   This is essentially the arguments needed to construct any node combined into
   a single structure.  Since it only refers to strings elsewhere, it is
   convenient as a way to completely specify a node without having to actually
   allocate one.
*/
typedef struct {
  SerdNodeType   type;   ///< Basic type of this node
  SerdStringView string; ///< String contents of this node
  SerdNodeFlags  flags;  ///< Additional node flags
  SerdStringView meta;   ///< String contents of datatype or language node
} NodeSpec;

static inline NodeSpec
token_spec(const SerdNodeType type, const SerdStringView string)
{
  NodeSpec spec = {type, string, 0u, serd_empty_string()};
  return spec;
}

static inline NodeSpec
literal_spec(const SerdStringView string,
             const SerdNodeFlags  flags,
             const SerdStringView meta)
{
  NodeSpec spec = {SERD_LITERAL, string, flags, meta};
  return spec;
}

#endif // SERD_NODE_SPEC_H
