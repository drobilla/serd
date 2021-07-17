// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STREAM_RESULT_H
#define SERD_STREAM_RESULT_H

#include "serd/attributes.h"
#include "serd/status.h"

#include <stddef.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_stream_result Write Result
   @ingroup serd_utilities
   @{
*/

/**
   A status code with an associated byte count.

   This is returned by functions which write to a buffer to inform the caller
   about the size written, or in case of overflow, size required.
*/
typedef struct {
  /**
     Status code.

     This reports the status of the operation as usual, and also dictates the
     meaning of `count`.
  */
  SerdStatus status;

  /**
     Number of bytes written or required.

     On success, this is the total number of bytes written.  On #SERD_NO_SPACE,
     this is the number of bytes of output space that are required for success.
  */
  size_t count;
} SerdStreamResult;

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STREAM_RESULT_H
