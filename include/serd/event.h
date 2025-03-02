// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_EVENT_H
#define SERD_EVENT_H

#include <serd/attributes.h>
#include <serd/caret_view.h>
#include <serd/statement_view.h>
#include <serd/string_pair_view.h>
#include <zix/string_view.h>

#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_event Events
   @ingroup serd_streaming
   @{
*/

/// Type of a #SerdEvent
typedef enum {
  /**
     Set the base URI.

     The `uri` body is the new base URI string.
  */
  SERD_EVENT_BASE = 1U,

  /**
     Set a namespace prefix.

     The `prefix` body is the name (`prefix`) and URI (`suffix`) of the new
     namespace.
  */
  SERD_EVENT_PREFIX = 2U,

  /**
     Add a statement.

     The `statement` body is the new statement.
  */
  SERD_EVENT_STATEMENT = 3U,

  /**
     End an anonymous node.

     The `label` is the label of the anonymous blank node.
  */
  SERD_EVENT_END = 4U,
} SerdEventType;

/// Flags indicating inline abbreviation information for statement events
typedef enum {
  SERD_EMPTY_S = 1U << 0U, ///< Empty blank node subject
  SERD_EMPTY_O = 1U << 1U, ///< Empty blank node object
  SERD_ANON_S  = 1U << 2U, ///< Start of anonymous subject
  SERD_ANON_O  = 1U << 3U, ///< Start of anonymous object
  SERD_LIST_S  = 1U << 4U, ///< Start of list subject
  SERD_LIST_O  = 1U << 5U, ///< Start of list object
} SerdEventFlag;

/// Bitwise OR of #SerdEventFlag values
typedef uint16_t SerdEventFlags;

/**
   An event in a data stream.

   Events represent everything that can occur in a document, and are used to
   plumb together different components.  For example, when parsing a document,
   the reader emits a stream of events which can be sent to a writer to rewrite
   the document in a different syntax.
*/
typedef struct {
  SerdEventType  type : 16; ///< Event type, the valid field of `body`
  SerdEventFlags flags;     ///< Flags for streaming pretty-printing
  SerdCaretView  caret;     ///< Event origin
  union {
    ZixStringView      uri;       ///< Base URI for #SERD_EVENT_BASE
    SerdStringPairView prefix;    ///< Name and URI for #SERD_EVENT_PREFIX
    SerdStatementView  statement; ///< Statement for #SERD_EVENT_STATEMENT
    ZixStringView      label;     ///< Node label for #SERD_EVENT_END
  } body;                         ///< Event body (depending on `type`)
} SerdEvent;

/**
   @defgroup serd_event_constructors Constructors
   @{
*/

/// Construct a #SERD_EVENT_BASE event
SERD_CONST_API SerdEvent
serd_base_event(ZixStringView uri);

/// Construct a #SERD_EVENT_PREFIX event
SERD_CONST_API SerdEvent
serd_prefix_event(ZixStringView name, ZixStringView uri);

/// Construct a #SERD_EVENT_STATEMENT event
SERD_CONST_API SerdEvent
serd_statement_event(SerdEventFlags flags, SerdStatementView statement);

/// Construct a #SERD_EVENT_END event
SERD_CONST_API SerdEvent
serd_end_event(ZixStringView label);

/// Return an event tagged with its origin in a document
SERD_CONST_API SerdEvent
serd_cite_event(SerdEvent event, SerdCaretView caret);

/**
   @}
   @}
*/

SERD_END_DECLS

#endif // SERD_EVENT_H
