/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

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

#ifndef SERD_COMPARE_H
#define SERD_COMPARE_H

#include "serd/serd.h"

/// Compare statements lexicographically, ignoring graph
SERD_PURE_FUNC
int
serd_triple_compare(const void* x, const void* y, const void* user_data);

/**
   Compare statments with statement patterns lexicographically, ignoring graph.

   Null nodes in the second argument are treated as wildcards, always less than
   any node.
*/
SERD_PURE_FUNC
int
serd_triple_compare_pattern(const void* x,
                            const void* y,
                            const void* user_data);

/// Compare statements lexicographically
SERD_PURE_FUNC
int
serd_quad_compare(const void* x, const void* y, const void* user_data);

/**
   Compare statments with statement patterns lexicographically.

   Null nodes in the second argument are treated as wildcards, always less than
   any node.
*/
SERD_PURE_FUNC
int
serd_quad_compare_pattern(const void* x, const void* y, const void* user_data);

#endif // SERD_COMPARE_H
