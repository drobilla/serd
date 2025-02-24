// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/object_view.h>
#include <zix/string_view.h>

bool
serd_object_view_equals(const SerdObjectView lhs, const SerdObjectView rhs)
{
  return lhs.type == rhs.type && lhs.flags == rhs.flags &&
         zix_string_view_equals(lhs.string, rhs.string) &&
         zix_string_view_equals(lhs.meta.string, rhs.meta.string);
}
