// Copyright 2020-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_SYMBOLS_H
#define SERD_SRC_SYMBOLS_H

#include <zix/string_view.h>

#define SERD_N_SYMBOLS 21U

typedef enum {
  EMPTY_SYMBOL,
  RDF_FIRST,
  RDF_LANGSTRING,
  RDF_NIL,
  RDF_REST,
  RDF_TYPE,
  XSD_BASE64BINARY,
  XSD_BOOLEAN,
  XSD_BYTE,
  XSD_DECIMAL,
  XSD_DOUBLE,
  XSD_FLOAT,
  XSD_HEXBINARY,
  XSD_INT,
  XSD_INTEGER,
  XSD_LONG,
  XSD_SHORT,
  XSD_UNSIGNEDBYTE,
  XSD_UNSIGNEDINT,
  XSD_UNSIGNEDLONG,
  XSD_UNSIGNEDSHORT,
} SerdSymbol;

extern const ZixStringView serd_symbols[SERD_N_SYMBOLS];

#endif // SERD_SRC_SYMBOLS_H
