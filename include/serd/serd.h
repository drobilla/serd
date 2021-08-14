// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

/// @file serd.h API for Serd, a lightweight RDF syntax library

#ifndef SERD_SERD_H
#define SERD_SERD_H

/**
   @defgroup serd Serd C API
   @{
*/

// IWYU pragma: begin_exports

#include "serd/attributes.h"
#include "serd/statement.h"
#include "serd/syntax.h"
#include "serd/version.h"

#include "serd/buffer.h"
#include "serd/memory.h"
#include "serd/status.h"
#include "serd/stream.h"
#include "serd/string_view.h"

#include "serd/input_stream.h"
#include "serd/output_stream.h"

#include "serd/log.h"
#include "serd/uri.h"

#include "serd/node.h"

#include "serd/caret.h"
#include "serd/event.h"
#include "serd/nodes.h"
#include "serd/string.h"

#include "serd/sink.h"

#include "serd/world.h"

#include "serd/canon.h"
#include "serd/env.h"
#include "serd/filter.h"

#include "serd/node_syntax.h"

#include "serd/reader.h"
#include "serd/writer.h"

#include "serd/cursor.h"
#include "serd/inserter.h"
#include "serd/model.h"
#include "serd/range.h"

// IWYU pragma: end_exports

/**
   @}
*/

#endif /* SERD_SERD_H */
