// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/serd.h"

#include <stdio.h>

void
serd_set_stream_utf8_mode(FILE* stream);

int
serd_print_version(const char* program);

SerdStatus
serd_set_input_option(SerdStringView   name,
                      SerdSyntax*      syntax,
                      SerdReaderFlags* flags);

SerdStatus
serd_set_output_option(SerdStringView   name,
                       SerdSyntax*      syntax,
                       SerdWriterFlags* flags);

SerdByteSource*
serd_open_input(const char* filename, size_t block_size);

SerdByteSink*
serd_open_output(const char* filename, size_t block_size);

SerdStatus
serd_set_base_uri_from_path(SerdEnv* env, const char* path);
