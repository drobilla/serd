// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NODE_TYPE_H
#define SERD_NODE_TYPE_H

#include "serd/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_node_type Type
   @ingroup serd_node
   @{
*/

/**
   Type of a node.

   Note that this set of types is both more precise than, and extended from,
   the possible types of an abstract RDF node.  Not all types can occur in all
   contexts, for example, a Turtle document can't contain a variable node.

   |           | LITERAL | URI | CURIE | BLANK | VARIABLE |
   |-----------|---------|-----|-------|-------|----------|
   |     Graph |       0 |   1 |     1 |     1 |        1 |
   |   Subject |       0 |   1 |     1 |     1 |        1 |
   | Predicate |       0 |   1 |     1 |     0 |        1 |
   |    Object |       1 |   1 |     1 |     1 |        1 |
   |  Language |       1 |   0 |     0 |     0 |        0 |
   |  Datatype |       0 |   1 |     1 |     0 |        0 |

   The string value of a node never contains quoting or other type indicators.
   For example, the blank node `_:id3` and the plain literal `"id3"` from a
   Turtle document would both have the same string, "id3", returned by
   #serd_node_string.
*/
typedef enum {
  /**
     Literal value.

     A literal is a string that optionally has either a language, or a datatype
     (but never both).  Literals can only occur as the object of a statement,
     never the subject or predicate.
  */
  SERD_LITERAL = 1U,

  /**
     Universal Resource Identifier (URI).

     A URI (more pedantically, a URI reference) is either a relative reference
     with respect to some base URI, like "foo/bar", or an absolute URI with a
     scheme, like "http://example.org/foo".

     @see [RFC3986](http://tools.ietf.org/html/rfc3986)
  */
  SERD_URI,

  /**
     CURIE, a shortened URI.

     Value is an unquoted CURIE string relative to the current environment,
     e.g. "rdf:type".  @see [CURIE Syntax 1.0](http://www.w3.org/TR/curie)
  */
  SERD_CURIE,

  /**
     A blank node.

     A blank node is a resource that has no URI.  The identifier of a blank
     node is local to its context (a document, for example), and so unlike
     URIs, blank nodes can't be used to link data across sources.

     @see [RDF 1.1
     Turtle](http://www.w3.org/TR/turtle/#grammar-production-BLANK_NODE_LABEL)
  */
  SERD_BLANK,

  /**
     Variable node.

     A variable node, like a blank node, is local to its context.  Variables
     don't occur in data but are used in search patterns.

     @see [SPARQL 1.1 Query
     Language](https://www.w3.org/TR/sparql11-query/#rVar)
  */
  SERD_VARIABLE,
} SerdNodeType;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_NODE_TYPE_H
