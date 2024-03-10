// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_SERD_INTERNAL_H
#define SERD_SRC_SERD_INTERNAL_H

#include "world.h" // IWYU pragma: keep

#include "serd/error.h"
#include "serd/world.h"

#include <stdio.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

#define SERD_PAGE_SIZE 4096

#ifndef MIN
#  define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* Error reporting */

static inline void
serd_error(const SerdWorld* world, const SerdError* e)
{
  if (world->error_func) {
    world->error_func(world->error_handle, e);
  } else {
    if (e->filename) {
      fprintf(stderr, "error: %s:%u:%u: ", e->filename, e->line, e->col);
    } else {
      fprintf(stderr, "error: ");
    }
    vfprintf(stderr, e->fmt, *e->args);
  }
}

#endif // SERD_SRC_SERD_INTERNAL_H
