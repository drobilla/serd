// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_SYNTAX_H
#define SERD_NODE_SYNTAX_H

#include "serd/attributes.h"
#include "serd/env.h"
#include "serd/syntax.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_node_syntax Node Syntax
   @ingroup serd
   @{
*/

/**
   Create a node from a string representation in `syntax`.

   The string should be a node as if written as an object in the given syntax,
   without any extra quoting or punctuation, which is the format returned by
   serd_node_to_syntax().  These two functions, when used with #SERD_TURTLE,
   can be used to round-trip any node to a string and back.

   @param world The world to create sinks in.

   @param str String representation of a node.

   @param syntax Syntax to use.  Should be either SERD_TURTLE or SERD_NTRIPLES
   (the others are redundant).  Note that namespaced (CURIE) nodes and relative
   URIs can not be expressed in NTriples.

   @param env Environment of `str`.  This must define any abbreviations needed
   to parse the string.

   @return A newly allocated node that must be freed with serd_node_free().
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_from_syntax(SerdWorld* SERD_NONNULL  world,
                      const char* SERD_NONNULL str,
                      SerdSyntax               syntax,
                      SerdEnv* SERD_NULLABLE   env);

/**
   Return a string representation of `node` in `syntax`.

   The returned string represents that node as if written as an object in the
   given syntax, without any extra quoting or punctuation.

   @param world The world to create sinks in.

   @param node Node to write as a string.

   @param syntax Syntax to use.  Should be either SERD_TURTLE or SERD_NTRIPLES
   (the others are redundant).  Note that namespaced (CURIE) nodes and relative
   URIs can not be expressed in NTriples.

   @param env Environment for the output string.  This can be used to
   abbreviate things nicely by setting namespace prefixes.

   @return A newly allocated string that must be freed with serd_free().
*/
SERD_API
char* SERD_ALLOCATED
serd_node_to_syntax(SerdWorld* SERD_NONNULL      world,
                    const SerdNode* SERD_NONNULL node,
                    SerdSyntax                   syntax,
                    const SerdEnv* SERD_NULLABLE env);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_SYNTAX_H
