// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_WORLD_INTERNAL_H
#define SERD_SRC_WORLD_INTERNAL_H

#include "serd/world.h"

#include <stdint.h>

uint32_t
serd_world_next_document_id(SerdWorld* world);

#endif // SERD_SRC_WORLD_INTERNAL_H
