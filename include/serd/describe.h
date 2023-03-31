// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_DESCRIBE_H
#define SERD_DESCRIBE_H

#include "serd/attributes.h"
#include "serd/cursor.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_range Range
   @ingroup serd_storage
   @{
*/

/// Flags that control the style of a model description
typedef enum {
  SERD_NO_TYPE_FIRST = 1U << 0U, ///< Disable writing rdf:type ("a") first
} SerdDescribeFlag;

/// Bitwise OR of SerdDescribeFlag values
typedef uint32_t SerdDescribeFlags;

/**
   Describe a range of statements by writing to a sink.

   This will consume the given cursor, and emit at least every statement it
   visits.  More statements from the model may be written in order to describe
   anonymous blank nodes that are associated with a subject in the range.

   The default is to write statements in an order suited for pretty-printing
   with Turtle or TriG with as many anonymous nodes as possible.  If
   `SERD_NO_INLINE_OBJECTS` is given, a simple sorted stream is written
   instead, which is faster since no searching is required, but can result in
   ugly output for Turtle or Trig.
*/
SERD_API SerdStatus
serd_describe_range(ZixAllocator* ZIX_NULLABLE     allocator,
                    const SerdCursor* ZIX_NULLABLE range,
                    const SerdSink* ZIX_NONNULL    sink,
                    SerdDescribeFlags              flags);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_DESCRIBE_H
