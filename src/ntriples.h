// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_NTRIPLES_H
#define SERD_SRC_NTRIPLES_H

#include "string_utils.h"

#include <stdbool.h>

static inline bool
is_PN_CHARS_BASE(const int c)
{
  return (is_alpha(c) || in_range(c, 0x000C0U, 0x000D6U) ||
          in_range(c, 0x000D8U, 0x000F6U) || in_range(c, 0x000F8U, 0x002FFU) ||
          in_range(c, 0x00370U, 0x0037DU) || in_range(c, 0x0037FU, 0x01FFFU) ||
          in_range(c, 0x0200CU, 0x0200DU) || in_range(c, 0x02070U, 0x0218FU) ||
          in_range(c, 0x02C00U, 0x02FEFU) || in_range(c, 0x03001U, 0x0D7FFU) ||
          in_range(c, 0x0F900U, 0x0FDCFU) || in_range(c, 0x0FDF0U, 0x0FFFDU) ||
          in_range(c, 0x10000U, 0xEFFFFU));
}

#endif // SERD_SRC_NTRIPLES_H
