// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "memory.h"
#include "string_utils.h"

#include "serd/memory.h"
#include "serd/status.h"
#include "serd/string.h"

#include <stdlib.h>

void
serd_free(SerdAllocator* const allocator, void* const ptr)
{
  serd_afree(allocator, ptr);
}

const char*
serd_strerror(const SerdStatus status)
{
  switch (status) {
  case SERD_SUCCESS:
    return "Success";
  case SERD_FAILURE:
    return "Non-fatal failure";
  case SERD_UNKNOWN_ERROR:
    break;
  case SERD_NO_DATA:
    return "Unexpected end of input";
  case SERD_OVERFLOW:
    return "Insufficient space";

  case SERD_BAD_ALLOC:
    return "Memory allocation failed";
  case SERD_BAD_ARG:
    return "Invalid argument";
  case SERD_BAD_CALL:
    return "Invalid call";
  case SERD_BAD_CURIE:
    return "Invalid CURIE or unknown namespace prefix";
  case SERD_BAD_CURSOR:
    return "Invalid cursor";
  case SERD_BAD_EVENT:
    return "Invalid event in stream";
  case SERD_BAD_INDEX:
    return "No optimal model index available";
  case SERD_BAD_LABEL:
    return "Clashing blank node label";
  case SERD_BAD_LITERAL:
    return "Invalid literal";
  case SERD_BAD_PATTERN:
    return "Invalid statement pattern";
  case SERD_BAD_READ:
    return "Error reading from file";
  case SERD_BAD_STACK:
    return "Stack overflow";
  case SERD_BAD_SYNTAX:
    return "Invalid syntax";
  case SERD_BAD_TEXT:
    return "Invalid text encoding";
  case SERD_BAD_URI:
    return "Invalid or unresolved URI";
  case SERD_BAD_WRITE:
    return "Error writing to file";
  case SERD_BAD_DATA:
    return "Invalid data";
  }

  return "Unknown error";
}

int
serd_strncasecmp(const char* s1, const char* s2, size_t n)
{
  for (; n > 0 && *s2; s1++, s2++, --n) {
    if (serd_to_lower(*s1) != serd_to_lower(*s2)) {
      return (*s1 < *s2) ? -1 : +1;
    }
  }

  return 0;
}
