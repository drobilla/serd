// Copyright 2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TOOLS_CONSOLE_H
#define SERD_TOOLS_CONSOLE_H

#include "serd/env.h"
#include "serd/input_stream.h"
#include "serd/output_stream.h"
#include "serd/reader.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/writer.h"
#include "zix/string_view.h"

#include <stdio.h>

void
serd_set_stream_utf8_mode(FILE* stream);

int
serd_print_version(const char* program);

SerdStatus
serd_set_base_uri_from_path(SerdEnv* env, const char* path);

SerdSyntax
serd_choose_syntax(SerdSyntax requested, const char* filename);

SerdStatus
serd_set_input_option(ZixStringView    name,
                      SerdSyntax*      syntax,
                      SerdReaderFlags* flags);

SerdStatus
serd_set_output_option(ZixStringView    name,
                       SerdSyntax*      syntax,
                       SerdWriterFlags* flags);

SerdInputStream
serd_open_tool_input(const char* filename);

SerdOutputStream
serd_open_tool_output(const char* filename);

#endif // SERD_TOOLS_CONSOLE_H
