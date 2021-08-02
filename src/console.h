// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/serd.h"

#include <stdio.h>

void
serd_set_stream_utf8_mode(FILE* stream);

int
serd_print_version(const char* program);

SerdByteSource*
serd_open_input(const char* filename, size_t page_size);
