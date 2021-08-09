// Copyright 2021-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TOOLS_CONSOLE_H
#define SERD_TOOLS_CONSOLE_H

#include <serd/input_stream.h>
#include <serd/syntax.h>

int
serd_print_version(const char* program);

SerdSyntax
serd_choose_syntax(SerdSyntax requested, const char* filename);

SerdInputStream
serd_open_tool_input(const char* filename);

#endif // SERD_TOOLS_CONSOLE_H
