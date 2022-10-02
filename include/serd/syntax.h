// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SYNTAX_H
#define SERD_SYNTAX_H

#include "serd/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_syntax Syntax Utilities
   @ingroup serd_utilities
   @{
*/

/// RDF syntax type
typedef enum {
  SERD_TURTLE   = 1U, ///< Terse triples http://www.w3.org/TR/turtle
  SERD_NTRIPLES = 2U, ///< Line-based triples http://www.w3.org/TR/n-triples/
  SERD_NQUADS   = 3U, ///< Line-based quads http://www.w3.org/TR/n-quads/
  SERD_TRIG     = 4U, ///< Terse quads http://www.w3.org/TR/trig/
} SerdSyntax;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_SYNTAX_H
