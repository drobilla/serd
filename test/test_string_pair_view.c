// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include <serd/string_pair_view.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>

static bool
check_equals(const char* const lhs_prefix_str,
             const char* const lhs_suffix_str,
             const char* const rhs_prefix_str,
             const char* const rhs_suffix_str)
{
  const SerdStringPairView lhs = {zix_string(lhs_prefix_str),
                                  zix_string(lhs_suffix_str)};
  const SerdStringPairView rhs = {zix_string(rhs_prefix_str),
                                  zix_string(rhs_suffix_str)};

  return serd_string_pair_view_equals(lhs, rhs);
}

static void
test_string_pair_view_equals(void)
{
  // Equal with various splits
  assert(check_equals("", "abcdef", "", "abcdef"));
  assert(check_equals("a", "bcdef", "a", "bcdef"));
  assert(check_equals("ab", "cdef", "ab", "cdef"));
  assert(check_equals("abc", "def", "abc", "def"));
  assert(check_equals("abcd", "ef", "abcd", "ef"));
  assert(check_equals("abcde", "f", "abcde", "f"));
  assert(check_equals("abcdef", "", "abcdef", ""));

  // Not equal due to differing characters
  assert(!check_equals("", "Xbcdef", "a", "bcdef"));
  assert(!check_equals("X", "bcdef", "ab", "cdef"));
  assert(!check_equals("aX", "cdef", "abc", "def"));
  assert(!check_equals("Xa", "cdef", "abc", "def"));
  assert(!check_equals("ab", "Xdef", "abcd", "ef"));
  assert(!check_equals("ab", "cXef", "abcde", "f"));
  assert(!check_equals("abc", "deX", "abcdef", ""));

  // Not equal due to length
  assert(!check_equals("", "abcde", "", "abcdef"));
  assert(!check_equals("a", "bcde", "a", "bcdef"));
  assert(!check_equals("abcde", "f", "", "abcde"));
  assert(!check_equals("a", "bcdef", "a", "bcde"));
}

static bool
check_equals_string(const char* const prefix_str,
                    const char* const suffix_str,
                    const char* const string_str)
{
  const SerdStringPairView pair = {zix_string(prefix_str),
                                   zix_string(suffix_str)};

  return serd_string_pair_view_equals_string(pair, zix_string(string_str));
}

static void
test_string_pair_view_equals_string(void)
{
  // Equal with various splits
  assert(check_equals_string("", "abcdef", "abcdef"));
  assert(check_equals_string("a", "bcdef", "abcdef"));
  assert(check_equals_string("ab", "cdef", "abcdef"));
  assert(check_equals_string("abc", "def", "abcdef"));
  assert(check_equals_string("abcd", "ef", "abcdef"));
  assert(check_equals_string("abcde", "f", "abcdef"));
  assert(check_equals_string("abcdef", "", "abcdef"));

  // Not equal due to differing characters
  assert(!check_equals_string("", "Xbcdef", "abcdef"));
  assert(!check_equals_string("X", "bcdef", "abcdef"));
  assert(!check_equals_string("aX", "cdef", "abcdef"));
  assert(!check_equals_string("abc", "dXf", "abcdef"));
  assert(!check_equals_string("abc", "deX", "abcdef"));

  // Not equal due to length
  assert(!check_equals_string("", "abcde", "abcdef"));
  assert(!check_equals_string("", "abcdef", "abcde"));
}

static bool
check_starts_with(const char* const prefix_str,
                  const char* const suffix_str,
                  const char* const string_str)
{
  const SerdStringPairView pair = {zix_string(prefix_str),
                                   zix_string(suffix_str)};

  return serd_string_pair_view_starts_with(pair, zix_string(string_str));
}

static void
test_string_pair_view_starts_with(void)
{
  assert(check_starts_with("", "abcdef", "a"));
  assert(check_starts_with("", "abcdef", "ab"));
  assert(check_starts_with("", "abcdef", "abcdef"));
  assert(check_starts_with("a", "bcdef", "a"));
  assert(check_starts_with("a", "bcdef", "ab"));
  assert(check_starts_with("ab", "cdef", "a"));
  assert(check_starts_with("ab", "cdef", "ab"));
  assert(check_starts_with("ab", "cdef", "abc"));

  assert(!check_starts_with("", "abcdef", "b"));
  assert(!check_starts_with("", "abcdef", "abcdefg"));
  assert(!check_starts_with("a", "bcdef", "abce"));
  assert(!check_starts_with("a", "bcdef", "bbc"));
  assert(!check_starts_with("ab", "cdef", "b"));
  assert(!check_starts_with("ab", "cdef", "abd"));
  assert(!check_starts_with("ab", "cdef", "abcdefg"));
}

ZIX_PURE_FUNC int
main(void)
{
  test_string_pair_view_equals();
  test_string_pair_view_equals_string();
  test_string_pair_view_starts_with();
  return 0;
}
