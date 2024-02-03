// Copyright 2020-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_SYMBOLS_H
#define SERD_SRC_SYMBOLS_H

#include <zix/string_view.h>

#define SERD_N_SYMBOLS 9U

typedef enum {
  RDF_FIRST,
  RDF_LANGSTRING,
  RDF_NIL,
  RDF_REST,
  RDF_TYPE,
  XSD_BOOLEAN,
  XSD_DECIMAL,
  XSD_DOUBLE,
  XSD_INTEGER,
} SerdSymbol;

extern const ZixStringView serd_symbols[SERD_N_SYMBOLS];

#endif // SERD_SRC_SYMBOLS_H
