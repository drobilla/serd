// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

/// @file serd.h API for Serd, a lightweight RDF syntax library

#ifndef SERD_SERD_H
#define SERD_SERD_H

// IWYU pragma: begin_exports

/**
   @defgroup serd Serd C API
   @{
*/

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

#include "serd/log.h"
#include "serd/status.h"

/**
   @}
   @defgroup serd_memory Memory Management
   @{
*/

#include "serd/buffer.h"
#include "serd/memory.h"

/**
   @}
   @defgroup serd_utilities Utilities
   @{
*/

#include "serd/string.h"
#include "serd/syntax.h"

/**
   @}
   @defgroup serd_data Data
   @{
*/

#include "serd/caret.h"
#include "serd/node.h"
#include "serd/statement.h"
#include "serd/uri.h"
#include "serd/value.h"

/**
   @}
   @defgroup serd_streaming Streaming
   @{
*/

#include "serd/canon.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/filter.h"
#include "serd/sink.h"

/**
   @}
   @defgroup serd_reading_writing Reading and Writing
   @{
*/

#include "serd/input_stream.h"
#include "serd/node_syntax.h"
#include "serd/output_stream.h"
#include "serd/reader.h"
#include "serd/stream.h"
#include "serd/writer.h"

/**
   @}
   @defgroup serd_storage Storage
   @{
*/

#include "serd/cursor.h"
#include "serd/inserter.h"
#include "serd/model.h"
#include "serd/nodes.h"
#include "serd/range.h"

/**
   @}
*/

/**
   @}
*/

// IWYU pragma: end_exports

#endif /* SERD_SERD_H */
