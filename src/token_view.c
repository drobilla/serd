// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/token_view.h>
#include <zix/string_view.h>

bool
serd_token_view_equals(const SerdTokenView lhs, const SerdTokenView rhs)
{
  return lhs.type == rhs.type && zix_string_view_equals(lhs.string, rhs.string);
}
