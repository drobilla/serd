// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/field.h"

#include <stdbool.h>
#include <stdint.h>

bool
serd_field_supports(const SerdField field, const SerdNodeType type)
{
  static const uint8_t table[4U][6U] = {
    /*               (), li, ur, cu, bl, vr */
    /* Subject   */ {0U, 0U, 1U, 1U, 1U, 1U},
    /* Predicate */ {0U, 0U, 1U, 1U, 0U, 1U},
    /* Object    */ {0U, 1U, 1U, 1U, 1U, 1U},
    /* Graph     */ {0U, 0U, 1U, 1U, 1U, 1U},
  };

  return table[field][type];
}
