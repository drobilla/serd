// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_IMPL_H
#define SERD_SRC_WORLD_IMPL_H

#include "node_impl.h"

#include "serd/world.h"

#include <stdint.h>

struct SerdWorldImpl {
  SerdLimits      limits;
  ZixAllocator*   allocator;
  SerdLog         log;
  SerdNodes*      nodes;
  const SerdNode* rdf_first;
  const SerdNode* rdf_nil;
  const SerdNode* rdf_rest;
  const SerdNode* rdf_type;
  const SerdNode* xsd_boolean;
  const SerdNode* xsd_decimal;
  const SerdNode* xsd_integer;
  uint32_t        next_blank_id;
  uint32_t        next_document_id;
  uint64_t        blank_buf[4U];
};

#endif // SERD_SRC_WORLD_IMPL_H
