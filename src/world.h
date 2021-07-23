// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_H
#define SERD_SRC_WORLD_H

#include "node.h"

#include "serd/log.h"
#include "serd/node.h"
#include "serd/nodes.h"

#include <stdbool.h>
#include <stdint.h>

struct SerdWorldImpl {
  SerdNodes*      nodes;
  SerdLogFunc     log_func;
  void*           log_handle;
  const SerdNode* rdf_first;
  const SerdNode* rdf_nil;
  const SerdNode* rdf_rest;
  const SerdNode* rdf_type;
  const SerdNode* xsd_boolean;
  const SerdNode* xsd_decimal;
  const SerdNode* xsd_integer;

  struct {
    SerdNode node;
    char     string[16];
  } blank;

  uint32_t next_blank_id;
  uint32_t next_document_id;

  bool stderr_color;
};

#endif // SERD_SRC_WORLD_H
