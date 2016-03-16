// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"

#include "serd/serd.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void
serd_free(void* const ptr)
{
  free(ptr);
}

const char*
serd_strerror(const SerdStatus status)
{
  switch (status) {
  case SERD_SUCCESS:
    return "Success";
  case SERD_FAILURE:
    return "Non-fatal failure";
  case SERD_ERR_UNKNOWN:
    break;
  case SERD_ERR_BAD_SYNTAX:
    return "Invalid syntax";
  case SERD_ERR_BAD_ARG:
    return "Invalid argument";
  case SERD_ERR_NOT_FOUND:
    return "Not found";
  case SERD_ERR_ID_CLASH:
    return "Blank node ID clash";
  case SERD_ERR_BAD_CURIE:
    return "Invalid CURIE";
  case SERD_ERR_INTERNAL:
    return "Internal error";
  case SERD_ERR_BAD_WRITE:
    return "Error writing to file/stream";
  case SERD_ERR_BAD_TEXT:
    return "Invalid text encoding";
  }

  return "Unknown error";
}

static void
serd_update_flags(const char c, SerdNodeFlags* const flags)
{
  if (c == '\r' || c == '\n') {
    *flags |= SERD_HAS_NEWLINE;
  } else if (c == '"') {
    *flags |= SERD_HAS_QUOTE;
  }
}

size_t
serd_substrlen(const char* const    str,
               const size_t         len,
               SerdNodeFlags* const flags)
{
  assert(str);
  assert(flags);

  size_t i = 0;
  *flags   = 0;
  for (; i < len && str[i]; ++i) {
    serd_update_flags(str[i], flags);
  }

  return i;
}

size_t
serd_strlen(const char* const str, SerdNodeFlags* const flags)
{
  if (flags) {
    size_t i = 0;
    *flags   = 0;
    for (; str[i]; ++i) {
      serd_update_flags(str[i], flags);
    }
    return i;
  }

  return strlen(str);
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
