// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_UTF8_H
#define SERD_SRC_READ_UTF8_H

#include "serd/node.h"
#include "serd/reader.h"
#include "serd/status.h"

#include <stdint.h>

/// Read a UTF-8 character continuation (starting after the lead byte)
SerdStatus
read_utf8_continuation(SerdReader* reader, SerdNode* dest, uint8_t lead);

/// Read a single UTF-8 character and parse it to a code point
SerdStatus
read_utf8_code_point(SerdReader* reader,
                     SerdNode*   dest,
                     uint32_t*   code,
                     uint8_t     lead);

#endif // SERD_SRC_READ_UTF8_H
