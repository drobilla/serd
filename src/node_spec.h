// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NODE_SPEC_H
#define SERD_SRC_NODE_SPEC_H

#include "serd/node.h"
#include "serd/string_view.h"

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
  NodeSpec spec = {type, string, 0U, serd_empty_string()};
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

#endif // SERD_SRC_NODE_SPEC_H
