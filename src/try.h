// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_TRY_H
#define SERD_SRC_TRY_H

#define TRY(st, exp)      \
  do {                    \
    if (((st) = (exp))) { \
      return (st);        \
    }                     \
  } while (0)

#endif // SERD_SRC_TRY_H
