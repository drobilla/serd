// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"

#include <serd/status.h>
#include <serd/string.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <math.h>
#include <string.h>

const char*
serd_strerror(const SerdStatus status)
{
  switch (status) {
  case SERD_SUCCESS:
    return "Success";
  case SERD_FAILURE:
    return "Non-fatal failure";
  case SERD_NO_CHANGE:
    return "No change";
  case SERD_NO_DATA:
    return "Missing input";
  case SERD_NO_SPACE:
    return "Insufficient space";

  case SERD_UNKNOWN_ERROR:
    break;
  case SERD_BAD_ALLOC:
    return "Memory allocation failed";
  case SERD_BAD_READ:
    return "Error reading from file";
  case SERD_BAD_WRITE:
    return "Error writing to file";
  case SERD_BAD_STREAM:
    return "File or stream error";
  case SERD_BAD_STACK:
    return "Stack overflow";
  case SERD_BAD_CALL:
    return "Invalid call";
  case SERD_BAD_ARG:
    return "Invalid argument";
  case SERD_BAD_EVENT:
    return "Invalid event in stream";

  case SERD_BAD_SYNTAX:
    return "Invalid syntax";
  case SERD_BAD_LABEL:
    return "Clashing blank node label";
  case SERD_BAD_CURIE:
    return "Invalid CURIE or unknown namespace prefix";
  case SERD_BAD_TEXT:
    return "Invalid text encoding";
  case SERD_BAD_URI:
    return "Invalid or unresolved URI";
  case SERD_BAD_DATA:
    return "Invalid data";
  case SERD_BAD_LITERAL:
    return "Invalid literal";
  }

  return "Unknown error";
}

SerdString
serd_string_new(ZixAllocator* const allocator, const ZixStringView contents)
{
  SerdString string = {0U, NULL};
  if (contents.length &&
      (string.data = (char*)zix_calloc(allocator, contents.length + 1U, 1U))) {
    memcpy(string.data, contents.data, contents.length);
    string.length = contents.length;
  }
  return string;
}

int
serd_strcasecmp(const char* lhs, const char* rhs)
{
  while (*lhs && *rhs) {
    const char c1 = serd_to_upper(*lhs++);
    const char c2 = serd_to_upper(*rhs++);
    if (c1 != c2) {
      return (c1 < c2) ? -1 : +1;
    }
  }

  const char c1 = serd_to_upper(*lhs);
  const char c2 = serd_to_upper(*rhs);
  return (c1 == c2) ? 0 : (c1 < c2) ? -1 : +1;
}

static double
read_sign(const char** const sptr)
{
  double sign = 1.0;

  if (**sptr == '-') {
    sign = -1.0;
    ++(*sptr);
  } else if (**sptr == '+') {
    ++(*sptr);
  }

  return sign;
}

double
serd_strtod(const char* const str, char** const endptr)
{
  assert(str);

  double result = 0.0;

  // Point s at the first non-whitespace character
  const char* s = str;
  while (is_space(*s)) {
    ++s;
  }

  // Read leading sign if necessary
  const double sign = read_sign(&s);

  // Parse integer part
  for (; is_digit(*s); ++s) {
    result = (result * 10.0) + (*s - '0');
  }

  // Parse fractional part
  if (*s == '.') {
    double denom = 10.0;
    for (++s; is_digit(*s); ++s) {
      result += (*s - '0') / denom;
      denom *= 10.0;
    }
  }

  // Parse exponent
  if (*s == 'e' || *s == 'E') {
    ++s;
    double expt      = 0.0;
    double expt_sign = read_sign(&s);
    for (; is_digit(*s); ++s) {
      expt = (expt * 10.0) + (*s - '0');
    }
    result *= pow(10, expt * expt_sign);
  }

  if (endptr) {
    *endptr = (char*)s;
  }

  return result * sign;
}
