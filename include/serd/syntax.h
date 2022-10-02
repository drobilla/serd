// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SYNTAX_H
#define SERD_SYNTAX_H

#include "serd/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup syntax Syntax Type
   @ingroup serd
   @{
*/

/// RDF syntax type
typedef enum {
  SERD_TURTLE   = 1, ///< Terse triples http://www.w3.org/TR/turtle
  SERD_NTRIPLES = 2, ///< Line-based triples http://www.w3.org/TR/n-triples/
  SERD_NQUADS   = 3, ///< Line-based quads http://www.w3.org/TR/n-quads/
  SERD_TRIG     = 4, ///< Terse quads http://www.w3.org/TR/trig/
} SerdSyntax;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_SYNTAX_H
