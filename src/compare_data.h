// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_COMPARE_DATA_H
#define SERD_SRC_COMPARE_DATA_H

#include <serd/nodes.h>

#include <stdint.h>

typedef struct {
  SerdNodes* nodes;
  uint8_t    ordering[4U];
} CompareData;

#endif // SERD_SRC_COMPARE_DATA_H
