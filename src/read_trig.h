// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_TRIG_H
#define SERD_SRC_READ_TRIG_H

#include "serd/attributes.h"
#include "serd/reader.h"
#include "serd/status.h"

/**
   Read a single TriG "chunk" (directive or group of statements).
*/
SERD_NODISCARD SerdStatus
read_trig_chunk(SerdReader* reader);

#endif // SERD_SRC_READ_TRIG_H
