// Copyright 2020-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "symbols.h"

#include "namespaces.h"

#include <zix/string_view.h>

const ZixStringView serd_symbols[SERD_N_SYMBOLS] = {
  ZIX_STATIC_STRING(NS_RDF "first"),
  ZIX_STATIC_STRING(NS_RDF "langString"),
  ZIX_STATIC_STRING(NS_RDF "nil"),
  ZIX_STATIC_STRING(NS_RDF "rest"),
  ZIX_STATIC_STRING(NS_RDF "type"),
  ZIX_STATIC_STRING(NS_XSD "boolean"),
  ZIX_STATIC_STRING(NS_XSD "decimal"),
  ZIX_STATIC_STRING(NS_XSD "double"),
  ZIX_STATIC_STRING(NS_XSD "integer"),
};
