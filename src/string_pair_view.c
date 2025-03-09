// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <serd/string_pair_view.h>
#include <zix/string_view.h>

#include <stddef.h>

static char
string_pair_at(const SerdStringPairView pair, const size_t index)
{
  if (index < pair.prefix.length) {
    return pair.prefix.data[index];
  }

  return pair.suffix.data[index - pair.prefix.length];
}

bool
serd_string_pair_view_equals(const SerdStringPairView lhs,
                             const SerdStringPairView rhs)
{
  const size_t length = lhs.prefix.length + lhs.suffix.length;
  if (rhs.prefix.length + rhs.suffix.length != length) {
    return false;
  }

  for (size_t i = 0U; i < length; ++i) {
    if (string_pair_at(lhs, i) != string_pair_at(rhs, i)) {
      return false;
    }
  }

  return true;
}

bool
serd_string_pair_view_equals_string(const SerdStringPairView pair,
                                    const ZixStringView      string)
{
  return (pair.prefix.length + pair.suffix.length != string.length)
           ? false
           : serd_string_pair_view_starts_with(pair, string);
}

bool
serd_string_pair_view_starts_with(const SerdStringPairView pair,
                                  const ZixStringView      string)
{
  if (pair.prefix.length + pair.suffix.length < string.length) {
    return false;
  }

  size_t i = 0U;

  for (; i < pair.prefix.length && i < string.length; ++i) {
    if (pair.prefix.data[i] != string.data[i]) {
      return false;
    }
  }

  for (; i < string.length; ++i) {
    if (pair.suffix.data[i - pair.prefix.length] != string.data[i]) {
      return false;
    }
  }

  return true;
}
