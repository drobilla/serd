// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"

#include "serd/node.h"
#include "serd/status.h"
#include "serd/string.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

const char*
serd_strerror(const SerdStatus status)
{
  switch (status) {
  case SERD_SUCCESS:
    return "Success";
  case SERD_FAILURE:
    return "Non-fatal failure";
  case SERD_NO_DATA:
    return "Missing input";
  case SERD_NO_SPACE:
    return "Insufficient space";

  case SERD_UNKNOWN_ERROR:
    break;

  case SERD_BAD_SYNTAX:
    return "Invalid syntax";
  case SERD_BAD_ARG:
    return "Invalid argument";
  case SERD_BAD_LABEL:
    return "Clashing blank node label";
  case SERD_BAD_CURIE:
    return "Invalid CURIE or unknown namespace prefix";
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
  case SERD_BAD_TEXT:
    return "Invalid text encoding";
  case SERD_BAD_CALL:
    return "Invalid call";
  case SERD_BAD_EVENT:
    return "Invalid event in stream";
  case SERD_BAD_URI:
    return "Invalid or unresolved URI";
  case SERD_BAD_DATA:
    return "Invalid data";
  case SERD_BAD_LITERAL:
    return "Invalid literal";
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

int
serd_strcasecmp(const char* s1, const char* s2)
{
  while (*s1 && *s2) {
    const char c1 = serd_to_upper(*s1++);
    const char c2 = serd_to_upper(*s2++);
    if (c1 != c2) {
      return (c1 < c2) ? -1 : +1;
    }
  }

  const char c1 = serd_to_upper(*s1);
  const char c2 = serd_to_upper(*s2);
  return (c1 == c2) ? 0 : (c1 < c2) ? -1 : +1;
}
