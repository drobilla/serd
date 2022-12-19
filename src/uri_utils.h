// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_URI_UTILS_H
#define SERD_SRC_URI_UTILS_H

#include "string_utils.h"

#include "serd/attributes.h"
#include "serd/uri.h"
#include "zix/string_view.h"

#include <stdbool.h>
#include <string.h>

static inline bool
slice_equals(const ZixStringView* a, const ZixStringView* b)
{
  return a->length == b->length && !strncmp(a->data, b->data, a->length);
}

static inline size_t
uri_path_len(const SerdURIView* uri)
{
  return uri->path_prefix.length + uri->path.length;
}

static inline char
uri_path_at(const SerdURIView* uri, size_t i)
{
  if (i < uri->path_prefix.length) {
    return uri->path_prefix.data[i];
  }

  return uri->path.data[i - uri->path_prefix.length];
}

/**
   Return the index of the first differing character after the last root slash,
   or zero if `uri` is not under `root`.
*/
static inline SERD_PURE_FUNC
size_t
uri_rooted_index(const SerdURIView* uri, const SerdURIView* root)
{
  if (!root || !root->scheme.length ||
      !slice_equals(&root->scheme, &uri->scheme) ||
      !slice_equals(&root->authority, &uri->authority)) {
    return 0;
  }

  bool         differ   = false;
  const size_t path_len = uri_path_len(uri);
  const size_t root_len = uri_path_len(root);

  size_t last_root_slash = 0;
  for (size_t i = 0; i < path_len && i < root_len; ++i) {
    const char u = uri_path_at(uri, i);
    const char r = uri_path_at(root, i);

    differ = differ || u != r;
    if (r == '/') {
      last_root_slash = i;
      if (differ) {
        return 0;
      }
    }
  }

  return last_root_slash + 1;
}

/** Return true iff `uri` shares path components with `root` */
static inline SERD_PURE_FUNC
bool
uri_is_related(const SerdURIView* uri, const SerdURIView* root)
{
  return root && root->scheme.length &&
         slice_equals(&root->scheme, &uri->scheme) &&
         slice_equals(&root->authority, &uri->authority);
}

/** Return true iff `uri` is within the base of `root` */
static inline SERD_PURE_FUNC
bool
uri_is_under(const SerdURIView* uri, const SerdURIView* root)
{
  const size_t index = uri_rooted_index(uri, root);
  return index > 0 && uri->path.length > index;
}

static inline bool
is_uri_scheme_char(const int c)
{
  switch (c) {
  case ':':
  case '+':
  case '-':
  case '.':
    return true;
  default:
    return is_alpha(c) || is_digit(c);
  }
}

#endif // SERD_SRC_URI_UTILS_H
