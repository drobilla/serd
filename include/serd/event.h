// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_EVENT_H
#define SERD_EVENT_H

#include "serd/attributes.h"
#include "serd/node.h"
#include "serd/statement.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_event Events
   @ingroup serd_streaming
   @{
*/

/// Type of a SerdEvent
typedef enum {
  SERD_BASE      = 1, ///< Base URI changed
  SERD_PREFIX    = 2, ///< New URI prefix
  SERD_STATEMENT = 3, ///< Statement
  SERD_END       = 4, ///< End of anonymous node
} SerdEventType;

/**
   Event for base URI changes.

   Emitted whenever the base URI changes.
*/
typedef struct {
  SerdEventType               type; ///< #SERD_BASE
  const SerdNode* ZIX_NONNULL uri;  ///< Base URI
} SerdBaseEvent;

/**
   Event for namespace definitions.

   Emitted whenever a prefix is defined.
*/
typedef struct {
  SerdEventType               type; ///< #SERD_PREFIX
  const SerdNode* ZIX_NONNULL name; ///< Prefix name
  const SerdNode* ZIX_NONNULL uri;  ///< Namespace URI
} SerdPrefixEvent;

/**
   Event for statements.

   Emitted for every statement.
*/
typedef struct {
  SerdEventType                    type;      ///< #SERD_STATEMENT
  SerdStatementFlags               flags;     ///< Flags for pretty-printing
  const SerdStatement* ZIX_NONNULL statement; ///< Statement
} SerdStatementEvent;

/**
   Event for the end of anonymous node descriptions.

   This is emitted to indicate that the given anonymous node will no longer be
   described.  This is used by the writer which may, for example, need to
   write a delimiter.
*/
typedef struct {
  SerdEventType               type; ///< #SERD_END
  const SerdNode* ZIX_NONNULL node; ///< Anonymous node that is finished
} SerdEndEvent;

/**
   An event in a data stream.

   Streams of data are represented as a series of events.  Events represent
   everything that can occur in an RDF document, and are used to plumb together
   different components.  For example, when parsing a document, a reader emits
   a stream of events which can be sent to a writer to rewrite a document, or
   to an inserter to build a model in memory.
*/
typedef union {
  SerdEventType      type;      ///< Event type (always set)
  SerdBaseEvent      base;      ///< Base URI changed
  SerdPrefixEvent    prefix;    ///< New namespace prefix
  SerdStatementEvent statement; ///< Statement
  SerdEndEvent       end;       ///< End of anonymous node
} SerdEvent;

/// Function for handling events
typedef SerdStatus (*SerdEventFunc)(void* ZIX_NULLABLE           handle,
                                    const SerdEvent* ZIX_NONNULL event);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_EVENT_H
