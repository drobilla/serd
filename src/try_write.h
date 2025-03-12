// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_TRY_WRITE_H
#define SERD_SRC_TRY_WRITE_H

#define TRY_WRITE(wr, exp)             \
  do {                                 \
    const SerdStreamResult er = (exp); \
    (wr).count += er.count;            \
    if (((wr).status = er.status)) {   \
      return (wr);                     \
    }                                  \
  } while (0)

#endif // SERD_SRC_TRY_WRITE_H
