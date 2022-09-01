// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ATTRIBUTES_H
#define SERD_ATTRIBUTES_H

#ifdef __GNUC__
#  define SERD_MALLOC_FUNC __attribute__((malloc))
#else
#  define SERD_MALLOC_FUNC
#endif

#endif // SERD_ATTRIBUTES_H
