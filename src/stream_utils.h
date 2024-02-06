// Copyright 2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_STREAM_UTILS_H
#define SERD_SRC_STREAM_UTILS_H

#include <stdio.h>

typedef enum {
  SERD_FILE_MODE_READ,
  SERD_FILE_MODE_WRITE,
} SerdFileMode;

FILE*
serd_fopen_wrapper(const char* path, SerdFileMode mode);

#endif // SERD_SRC_STREAM_UTILS_H
