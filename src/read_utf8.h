/*
  Copyright 2011-2021 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef SERD_READ_UTF8_H
#define SERD_READ_UTF8_H

#include "serd/serd.h"

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

#endif // SERD_READ_UTF8_H
