// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_TEST_EXPECT_STRING_H
#define SERD_TEST_EXPECT_STRING_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <zix/string_view.h>

static inline bool
expect_string(const char* const actual, const char* const expected)
{
  assert(expected);
  const bool equal = actual && !strcmp(actual, expected);
  if (!equal) {
    fprintf(stderr, "Expected:\n%s\n\n", expected);
    fprintf(stderr, "Actual:\n%s\n\n", actual ? actual : "(null)");
  }
  return equal;
}

static inline bool
expect_string_view(const ZixStringView actual, const char* const expected)
{
  assert(expected);
  const size_t length = strlen(expected);
  const bool   equal =
    actual.length == length && !strncmp(actual.data, expected, length);
  if (!equal) {
    fprintf(stderr, "Expected:\n%s\n\n", expected);
    fprintf(stderr, "Actual:\n%s\n\n", actual.data);
  }
  return equal;
}

#endif // SERD_TEST_EXPECT_STRING_H
