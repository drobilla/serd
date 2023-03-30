// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TOOLS_CONSOLE_H
#define SERD_TOOLS_CONSOLE_H

#include "serd/input_stream.h"

#include <stdio.h>

void
serd_set_stream_utf8_mode(FILE* stream);

int
serd_print_version(const char* program);

SerdInputStream
serd_open_tool_input(const char* filename);

#endif // SERD_TOOLS_CONSOLE_H
