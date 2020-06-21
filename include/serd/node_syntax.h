// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_SYNTAX_H
#define SERD_NODE_SYNTAX_H

#include "serd/attributes.h"
#include "serd/env.h"
#include "serd/node.h"
#include "serd/syntax.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_node_syntax Node Syntax
   @ingroup serd_reading_writing
   @{
*/

/**
   Create a node from a string representation in `syntax`.

   The string should be a node as if written as an object in the given syntax,
   without any extra quoting or punctuation, which is the format returned by
   serd_node_to_syntax().  These two functions, when used with #SERD_TURTLE,
   can be used to round-trip any node to a string and back.

   @param allocator Allocator used for the returned node, and any temporary
   objects if `env` is null.

   @param str String representation of a node.

   @param syntax Syntax to use.  Should be either SERD_TURTLE or SERD_NTRIPLES
   (the others are redundant).  Note that namespaced (CURIE) nodes and relative
   URIs can not be expressed in NTriples.

   @param env Environment of `str`.  This must define any abbreviations needed
   to parse the string.

   @return A newly allocated node that must be freed with serd_node_free()
   using the world allocator.
*/
SERD_API SerdNode* ZIX_ALLOCATED
serd_node_from_syntax(ZixAllocator* ZIX_NULLABLE allocator,
                      const char* ZIX_NONNULL    str,
                      SerdSyntax                 syntax,
                      SerdEnv* ZIX_NULLABLE      env);

/**
   Return a string representation of `node` in `syntax`.

   The returned string represents that node as if written as an object in the
   given syntax, without any extra quoting or punctuation.

   @param allocator Allocator used for the returned node, and any temporary
   objects if `env` is null.

   @param node Node to write as a string.

   @param syntax Syntax to use.  Should be either SERD_TURTLE or SERD_NTRIPLES
   (the others are redundant).  Note that namespaced (CURIE) nodes and relative
   URIs can not be expressed in NTriples.

   @param env Environment for the output string.  This can be used to
   abbreviate things nicely by setting namespace prefixes.

   @return A newly allocated string that must be freed with zix_free() using
   the world allocator.
*/
SERD_API char* ZIX_ALLOCATED
serd_node_to_syntax(ZixAllocator* ZIX_NULLABLE  allocator,
                    const SerdNode* ZIX_NONNULL node,
                    SerdSyntax                  syntax,
                    const SerdEnv* ZIX_NULLABLE env);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_SYNTAX_H
