// Copyright 2024-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRING_PAIR_VIEW_H
#define SERD_STRING_PAIR_VIEW_H

#include <serd/attributes.h>
#include <zix/string_view.h>

#include <stdbool.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_string_pair_view String Pair View
   @ingroup serd_utilities
   @{
*/

/**
   A view of a string prefix and suffix.

   This is a general view type for two strings, the precise meaning of the
   fields depends on the context.  For example, this is used to represent
   CURIE-style prefixed names (name and URI suffix), and expanded URIs (URI
   prefix and suffix.
*/
typedef struct {
  ZixStringView prefix; ///< First or prefix string
  ZixStringView suffix; ///< Second or suffix string
} SerdStringPairView;

/// Return whether two string pairs (as if concatenated) are the same string
SERD_PURE_API bool
serd_string_pair_view_equals(SerdStringPairView lhs, SerdStringPairView rhs);

/// Return whether a string pair (as if concatenated) matches a string
SERD_PURE_API bool
serd_string_pair_view_equals_string(SerdStringPairView pair,
                                    ZixStringView      string);

/// Return whether a string pair (as if concatenated) has a given prefix
SERD_PURE_API bool
serd_string_pair_view_starts_with(SerdStringPairView pair,
                                  ZixStringView      string);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_STRING_PAIR_VIEW_H
