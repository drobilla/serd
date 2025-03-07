// Copyright 2020-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "symbols.h"

#include "namespaces.h"

#include <zix/string_view.h>

const ZixStringView serd_symbols[SERD_N_SYMBOLS] = {
  ZIX_STATIC_STRING(""),
  ZIX_STATIC_STRING(NS_RDF "first"),
  ZIX_STATIC_STRING(NS_RDF "langString"),
  ZIX_STATIC_STRING(NS_RDF "nil"),
  ZIX_STATIC_STRING(NS_RDF "rest"),
  ZIX_STATIC_STRING(NS_RDF "type"),
  ZIX_STATIC_STRING(NS_XSD "base64Binary"),
  ZIX_STATIC_STRING(NS_XSD "boolean"),
  ZIX_STATIC_STRING(NS_XSD "byte"),
  ZIX_STATIC_STRING(NS_XSD "decimal"),
  ZIX_STATIC_STRING(NS_XSD "double"),
  ZIX_STATIC_STRING(NS_XSD "float"),
  ZIX_STATIC_STRING(NS_XSD "hexBinary"),
  ZIX_STATIC_STRING(NS_XSD "int"),
  ZIX_STATIC_STRING(NS_XSD "integer"),
  ZIX_STATIC_STRING(NS_XSD "long"),
  ZIX_STATIC_STRING(NS_XSD "short"),
  ZIX_STATIC_STRING(NS_XSD "unsignedByte"),
  ZIX_STATIC_STRING(NS_XSD "unsignedInt"),
  ZIX_STATIC_STRING(NS_XSD "unsignedLong"),
  ZIX_STATIC_STRING(NS_XSD "unsignedShort"),
};
