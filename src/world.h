/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#ifndef SERD_WORLD_H
#define SERD_WORLD_H

#include "serd/serd.h"

#include <stdint.h>
#include <stdio.h>

struct SerdWorldImpl {
  SerdNodes*      nodes;
  SerdErrorFunc   error_func;
  void*           error_handle;
  SerdNode*       blank_node;
  const SerdNode* rdf_first;
  const SerdNode* rdf_nil;
  const SerdNode* rdf_rest;
  const SerdNode* rdf_type;
  const SerdNode* xsd_boolean;
  const SerdNode* xsd_decimal;
  const SerdNode* xsd_integer;
  uint32_t        next_blank_id;
};

/// Open a file configured for fast sequential reading
FILE*
serd_world_fopen(SerdWorld* world, const char* path, const char* mode);

SerdStatus
serd_world_error(const SerdWorld* world, const SerdError* e);

SerdStatus
serd_world_errorf(const SerdWorld* world, SerdStatus st, const char* fmt, ...);

#endif // SERD_WORLD_H
