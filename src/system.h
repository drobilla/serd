// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_SYSTEM_H
#define SERD_SRC_SYSTEM_H

#include <stddef.h>

#define SERD_PAGE_SIZE 4096

/// Write the message for a system error code (like errno) to a buffer
int
serd_system_strerror(int errnum, char* buf, size_t buflen);

#endif // SERD_SRC_SYSTEM_H
