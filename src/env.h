// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_ENV_H
#define SERD_SRC_ENV_H

#include "serd/attributes.h"
#include "serd/env.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/uri.h"

/**
   Expand `curie`.

   Errors: SERD_ERR_BAD_ARG if `curie` is not valid, or SERD_ERR_BAD_CURIE if
   prefix is not defined in `env`.
*/
SerdStatus
serd_env_expand_in_place(const SerdEnv*  env,
                         SerdStringView  curie,
                         SerdStringView* uri_prefix,
                         SerdStringView* uri_suffix);

SERD_PURE_FUNC
SerdURIView
serd_env_base_uri_view(const SerdEnv* env);

#endif // SERD_SRC_ENV_H
