// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"

#include "serd/syntax.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
  const char* name;
  const char* extension;
  SerdSyntax  syntax;
} Syntax;

static const Syntax syntaxes[] = {
  {"turtle", ".ttl", SERD_TURTLE},
  {"ntriples", ".nt", SERD_NTRIPLES},
  {"nquads", ".nq", SERD_NQUADS},
  {"trig", ".trig", SERD_TRIG},
  {NULL, NULL, SERD_SYNTAX_EMPTY},
};

SerdSyntax
serd_syntax_by_name(const char* const name)
{
  for (const Syntax* s = syntaxes; s->name; ++s) {
    if (!serd_strncasecmp(s->name, name, strlen(name))) {
      return s->syntax;
    }
  }
  return SERD_SYNTAX_EMPTY;
}

SerdSyntax
serd_guess_syntax(const char* const filename)
{
  const char* ext = strrchr(filename, '.');
  if (ext) {
    for (const Syntax* s = syntaxes; s->name; ++s) {
      if (s->extension && !serd_strncasecmp(s->extension, ext, strlen(ext))) {
        return s->syntax;
      }
    }
  }
  return SERD_SYNTAX_EMPTY;
}

bool
serd_syntax_has_graphs(const SerdSyntax syntax)
{
  return syntax == SERD_NQUADS || syntax == SERD_TRIG;
}
