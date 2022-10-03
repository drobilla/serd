// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SYNTAX_H
#define SERD_SYNTAX_H

#include "serd/attributes.h"

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_syntax Syntax Utilities
   @ingroup serd_utilities
   @{
*/

/// Syntax supported by serd
typedef enum {
  SERD_SYNTAX_EMPTY = 0U, ///< Empty syntax
  SERD_TURTLE       = 1U, ///< Terse triples http://www.w3.org/TR/turtle/
  SERD_NTRIPLES     = 2U, ///< Flat triples http://www.w3.org/TR/n-triples/
  SERD_NQUADS       = 3U, ///< Flat quads http://www.w3.org/TR/n-quads/
  SERD_TRIG         = 4U, ///< Terse quads http://www.w3.org/TR/trig/
} SerdSyntax;

/**
   Get a syntax by name.

   Case-insensitive, supports "Turtle", "NTriples", "NQuads", and "TriG".

   @return The syntax with the given name, or the empty syntax if the name is
   unknown.
*/
SERD_PURE_API SerdSyntax
serd_syntax_by_name(const char* SERD_NONNULL name);

/**
   Guess a syntax from a filename.

   This uses the file extension to guess the syntax of a file.  Zero is
   returned if the extension is not recognized.
*/
SERD_PURE_API SerdSyntax
serd_guess_syntax(const char* SERD_NONNULL filename);

/**
   Return whether a syntax can represent multiple graphs in one document.

   @return True for #SERD_NQUADS and #SERD_TRIG, false otherwise.
*/
SERD_CONST_API bool
serd_syntax_has_graphs(SerdSyntax syntax);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_SYNTAX_H
