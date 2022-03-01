// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_NTRIPLES_H
#define SERD_NTRIPLES_H

#include "string_utils.h"

#include <stdbool.h>

static inline bool
is_PN_CHARS_BASE(const int c)
{
  return (is_alpha(c) || in_range(c, 0x000C0u, 0x000D6u) ||
          in_range(c, 0x000D8u, 0x000F6u) || in_range(c, 0x000F8u, 0x002FFu) ||
          in_range(c, 0x00370u, 0x0037Du) || in_range(c, 0x0037Fu, 0x01FFFu) ||
          in_range(c, 0x0200Cu, 0x0200Du) || in_range(c, 0x02070u, 0x0218Fu) ||
          in_range(c, 0x02C00u, 0x02FEFu) || in_range(c, 0x03001u, 0x0D7FFu) ||
          in_range(c, 0x0F900u, 0x0FDCFu) || in_range(c, 0x0FDF0u, 0x0FFFDu) ||
          in_range(c, 0x10000u, 0xEFFFFu));
}

#endif // SERD_NTRIPLES_H
