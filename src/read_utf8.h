// Copyright 2011-2026 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_UTF8_H
#define SERD_SRC_READ_UTF8_H

#include "token_header.h"

#include <serd/reader.h>
#include <serd/status.h>

#include <stdint.h>

/// Read a UTF-8 character continuation (starting after the lead byte)
SerdStatus
read_utf8_continuation(SerdReader*  reader,
                       TokenHeader* dest,
                       uint32_t*    code,
                       uint8_t      lead);

#endif // SERD_SRC_READ_UTF8_H
