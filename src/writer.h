// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/node.h"
#include "serd/status.h"
#include "serd/writer.h"

SerdStatus
serd_writer_write_node(SerdWriter* writer, const SerdNode* node);
