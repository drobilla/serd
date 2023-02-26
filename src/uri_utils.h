// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_URI_UTILS_H
#define SERD_SRC_URI_UTILS_H

#include "serd/attributes.h"
#include "serd/string_view.h"
#include "serd/uri.h"

#include "string_utils.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  size_t shared;
  size_t root;
} SlashIndexes;

static inline bool
slice_equals(const SerdStringView* a, const SerdStringView* b)
{
  return a->length == b->length && !strncmp(a->data, b->data, a->length);
}

static inline size_t
uri_path_len(const SerdURIView* uri)
{
  return uri->path_base.length + uri->path.length;
}

static inline char
uri_path_at(const SerdURIView* uri, size_t i)
{
  if (i < uri->path_base.length) {
    return uri->path_base.data[i];
  }

  return uri->path.data[i - uri->path_base.length];
}

/**
   Return the index of the last slash shared with the root, or `SIZE_MAX`.

   The index of the next slash found in the root is also returned, so the two
   can be compared to determine if the URI is within the root (if the shared
   slash is the last in the root, then the URI is a child of the root,
   otherwise it may merely share some leading path components).
*/
static inline SERD_PURE_FUNC SlashIndexes
uri_rooted_index(const SerdURIView* uri, const SerdURIView* root)
{
  SlashIndexes indexes = {SIZE_MAX, SIZE_MAX};

  if (!root || !root->scheme.length ||
      !slice_equals(&root->scheme, &uri->scheme) ||
      !slice_equals(&root->authority, &uri->authority)) {
    return indexes;
  }

  const size_t path_len = uri_path_len(uri);
  const size_t root_len = uri_path_len(root);
  const size_t min_len  = path_len < root_len ? path_len : root_len;
  for (size_t i = 0; i < min_len; ++i) {
    const char u = uri_path_at(uri, i);
    const char r = uri_path_at(root, i);

    if (u == r) {
      if (u == '/') {
        indexes.root = indexes.shared = i;
      }
    } else {
      for (size_t j = i; j < root_len; ++j) {
        if (uri_path_at(root, j) == '/') {
          indexes.root = j;
          break;
        }
      }

      return indexes;
    }
  }

  return indexes;
}

/** Return true iff `uri` shares path components with `root` */
static inline SERD_PURE_FUNC bool
uri_is_related(const SerdURIView* uri, const SerdURIView* root)
{
  return uri_rooted_index(uri, root).shared != SIZE_MAX;
}

/** Return true iff `uri` is within the base of `root` */
static inline SERD_PURE_FUNC bool
uri_is_under(const SerdURIView* uri, const SerdURIView* root)
{
  const SlashIndexes indexes = uri_rooted_index(uri, root);
  return indexes.shared && indexes.shared != SIZE_MAX &&
         indexes.shared == indexes.root;
}

static inline bool
is_uri_scheme_char(const int c)
{
  return c == '+' || c == '-' || c == '.' || c == ':' || is_alpha(c) ||
         is_digit(c);
}

#endif // SERD_SRC_URI_UTILS_H
