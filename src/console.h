// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "serd/env.h"
#include "serd/input_stream.h"
#include "serd/output_stream.h"
#include "serd/status.h"

#include <stdio.h>

void
serd_set_stream_utf8_mode(FILE* stream);

int
serd_print_version(const char* program);

SerdInputStream
serd_open_tool_input(const char* filename);

SerdOutputStream
serd_open_tool_output(const char* filename);

SerdStatus
serd_set_base_uri_from_path(SerdEnv* env, const char* path);
