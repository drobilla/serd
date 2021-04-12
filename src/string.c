// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"

#include "serd/memory.h"
#include "serd/status.h"
#include "serd/string.h"

#include <stdlib.h>

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
  case SERD_ERR_BAD_CURSOR:
    return "Invalid cursor";
  case SERD_ERR_NOT_FOUND:
    return "Not found";
  case SERD_ERR_ID_CLASH:
    return "Blank node ID clash";
  case SERD_ERR_BAD_CURIE:
    return "Invalid CURIE or unknown namespace prefix";
  case SERD_ERR_INTERNAL:
    return "Internal error";
  case SERD_ERR_OVERFLOW:
    return "Stack overflow";
  case SERD_ERR_BAD_TEXT:
    return "Invalid text encoding";
  case SERD_ERR_BAD_WRITE:
    return "Error writing to file";
  case SERD_ERR_NO_DATA:
    return "Unexpected end of input";
  case SERD_ERR_BAD_CALL:
    return "Invalid call";
  case SERD_ERR_BAD_URI:
    return "Invalid or unresolved URI";
  case SERD_ERR_BAD_DATA:
    return "Invalid data";
  case SERD_ERR_BAD_INDEX:
    return "No optimal model index available";
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
