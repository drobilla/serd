// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

/// @file serd.h API for Serd, a lightweight RDF syntax library

#ifndef SERD_SERD_H
#define SERD_SERD_H

/**
   @defgroup serd Serd C API
   @{
*/

// IWYU pragma: begin_exports

/**
   @defgroup serd_library Library
   @{
*/

#include "serd/attributes.h"
#include "serd/version.h"
#include "serd/world.h"

/**
   @}
   @defgroup serd_errors Error Handling
   @{
*/

#include "serd/error.h"
#include "serd/status.h"

/**
   @}
   @defgroup serd_memory Memory Management
   @{
*/

#include "serd/buffer.h"

/**
   @}
   @defgroup serd_utilities Utilities
   @{
*/

#include "serd/stream_result.h"
#include "serd/string.h"
#include "serd/syntax.h"

/**
   @}
   @defgroup serd_data Data
   @{
*/

#include "serd/field.h"
#include "serd/node.h"
#include "serd/uri.h"
#include "serd/value.h"

/**
   @}
   @defgroup serd_streaming Streaming
   @{
*/

#include "serd/caret_view.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/tee.h"

/**
   @}
   @defgroup serd_reading_writing Reading and Writing
   @{
*/

#include "serd/input_stream.h"
#include "serd/output_stream.h"
#include "serd/reader.h"
#include "serd/stream.h"
#include "serd/writer.h"

/**
   @}
*/

// IWYU pragma: end_exports

/**
   @}
*/

#endif /* SERD_SERD_H */
