// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_ENV_H
#define SERD_SRC_ENV_H

#include "serd/env.h"
#include "serd/node.h"
#include "serd/status.h"
#include "serd/uri.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <stdbool.h>

/// Qualify `uri` into a CURIE if possible
bool
serd_env_qualify_in_place(const SerdEnv*   env,
                          const SerdNode*  uri,
                          const SerdNode** prefix,
                          ZixStringView*   suffix);

/**
   Expand `curie`.

   Errors: SERD_BAD_ARG if `curie` is not valid, or SERD_BAD_CURIE if prefix is
   not defined in `env`.
*/
SerdStatus
serd_env_expand_in_place(const SerdEnv* env,
                         ZixStringView  curie,
                         ZixStringView* uri_prefix,
                         ZixStringView* uri_suffix);

ZIX_PURE_FUNC SerdURIView
serd_env_base_uri_view(const SerdEnv* env);

#endif // SERD_SRC_ENV_H
