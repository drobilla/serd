// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_EVENT_H
#define SERD_EVENT_H

#include "serd/attributes.h"
#include "serd/caret_view.h"
#include "serd/node.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "zix/attributes.h"

#include <stdint.h>

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

/// Flags indicating inline abbreviation information for statement events
typedef enum {
  SERD_EMPTY_S = 1U << 0U, ///< Empty blank node subject
  SERD_EMPTY_O = 1U << 1U, ///< Empty blank node object
  SERD_EMPTY_G = 1U << 2U, ///< Empty blank node graph
  SERD_ANON_S  = 1U << 3U, ///< Start of anonymous subject
  SERD_ANON_O  = 1U << 4U, ///< Start of anonymous object
  SERD_LIST_S  = 1U << 5U, ///< Start of list subject
  SERD_LIST_O  = 1U << 6U, ///< Start of list object
  SERD_TERSE_S = 1U << 7U, ///< Start of terse subject
  SERD_TERSE_O = 1U << 8U, ///< Start of terse object
} SerdStatementEventFlag;

/// Bitwise OR of SerdSinkStatementFlag values
typedef uint32_t SerdStatementEventFlags;

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
  SerdEventType           type;      ///< #SERD_STATEMENT
  SerdStatementEventFlags flags;     ///< Flags for pretty-printing
  SerdStatementView       statement; ///< Statement
  SerdCaretView           caret;     ///< Statement origin
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
typedef SerdStatus (*SerdEventFunc)(void* ZIX_UNSPECIFIED        handle,
                                    const SerdEvent* ZIX_NONNULL event);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_EVENT_H
