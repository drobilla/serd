// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"

#include "serd/memory.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/string.h"

#include <assert.h>
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
  }

  return "Unknown error";
}

static void
serd_update_flags(const char c, SerdNodeFlags* const flags)
{
  switch (c) {
  case '\r':
  case '\n':
    *flags |= SERD_HAS_NEWLINE;
    break;
  case '"':
    *flags |= SERD_HAS_QUOTE;
    break;
  default:
    break;
  }
}

size_t
serd_substrlen(const char* const    str,
               const size_t         len,
               SerdNodeFlags* const flags)
{
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
