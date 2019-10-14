/*
  Copyright 2019-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "cursor.h"
#include "namespaces.h"
#include "node.h"
#include "statement.h"
#include "world.h"

#include "exess/exess.h"
#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const SerdWorld* world;
  const SerdSink*  target;
  SerdCanonFlags   flags;
} SerdCanonData;

static ExessResult
make_canonical(SerdNode** const out, const SerdNode* const SERD_NONNULL node)
{
  *out = NULL;

  const char*     str      = serd_node_string(node);
  const SerdNode* datatype = serd_node_datatype(node);
  ExessResult     r        = {EXESS_SUCCESS, 0};
  assert(datatype);

  const char* datatype_uri = serd_node_string(datatype);
  if (!strcmp(datatype_uri, NS_RDF "langString")) {
    *out = serd_new_string(serd_node_string_view(node));
    return r;
  }

  const ExessDatatype value_type = exess_datatype_from_uri(datatype_uri);
  if (value_type == EXESS_NOTHING) {
    return r;
  }

  // Measure canonical form to know how much space to allocate for node
  ExessVariant variant = exess_make_nothing(EXESS_SUCCESS);
  if (exess_datatype_is_bounded(value_type)) {
    r = exess_read_variant(&variant, value_type, str);
    if (!r.status) {
      r = exess_write_variant(variant, 0, NULL);
    }
  } else {
    r = exess_write_canonical(str, value_type, 0, NULL);
  }

  if (r.status) {
    return r;
  }

  // Allocate node
  const size_t datatype_uri_len = serd_node_length(datatype);
  const size_t len              = serd_node_pad_size(r.count);
  const size_t total_len        = sizeof(SerdNode) + len + datatype_uri_len;

  SerdNode* const result =
    serd_node_malloc(total_len, SERD_HAS_DATATYPE, SERD_LITERAL);

  // Write canonical form directly into node
  char* buf = serd_node_buffer(result);
  if (exess_datatype_is_bounded(value_type)) {
    r              = exess_write_variant(variant, r.count + 1, buf);
    result->length = r.count;
  } else {
    r              = exess_write_canonical(str, value_type, r.count + 1, buf);
    result->length = r.count;
  }

  SerdNode* const datatype_node = result + 1 + (len / sizeof(SerdNode));
  char* const     datatype_buf  = serd_node_buffer(datatype_node);

  datatype_node->length = datatype_uri_len;
  datatype_node->type   = SERD_URI;
  memcpy(datatype_buf, datatype_uri, datatype_uri_len + 1);

  *out = result;
  return r;
}

static SerdStatus
serd_canon_on_statement(SerdCanonData* const       data,
                        const SerdStatementFlags   flags,
                        const SerdStatement* const statement)
{
  const SerdNode* object = serd_statement_object(statement);
  if (serd_node_type(object) != SERD_LITERAL || !serd_node_datatype(object)) {
    return serd_sink_write_statement(data->target, flags, statement);
  }

  SerdNode*   normo = NULL;
  ExessResult r     = make_canonical(&normo, object);
  if (r.status) {
    SerdCursor cursor = {NULL, 0, 0};
    const bool lax    = (data->flags & SERD_CANON_LAX);

    if (statement->cursor) {
      // Adjust column to point at the error within the literal
      cursor.file = statement->cursor->file;
      cursor.line = statement->cursor->line;
      cursor.col  = statement->cursor->col + 1 + (unsigned)r.count;
    }

    serd_world_logf_internal(data->world,
                             SERD_ERR_INVALID,
                             lax ? SERD_LOG_LEVEL_WARNING
                                 : SERD_LOG_LEVEL_ERROR,
                             statement->cursor ? &cursor : NULL,
                             "invalid literal (%s)",
                             exess_strerror(r.status));

    if (!lax) {
      return SERD_ERR_INVALID;
    }
  }

  if (!normo) {
    return serd_sink_write_statement(data->target, flags, statement);
  }

  const SerdStatus st = serd_sink_write(data->target,
                                        flags,
                                        statement->nodes[0],
                                        statement->nodes[1],
                                        normo,
                                        statement->nodes[3]);
  serd_node_free(normo);
  return st;
}

static SerdStatus
serd_canon_on_event(SerdCanonData* const data, const SerdEvent* const event)
{
  return (event->type == SERD_STATEMENT)
           ? serd_canon_on_statement(
               data, event->statement.flags, event->statement.statement)
           : serd_sink_write_event(data->target, event);
}

SerdSink*
serd_canon_new(const SerdWorld* const world,
               const SerdSink* const  target,
               const SerdCanonFlags   flags)
{
  SerdCanonData* const data = (SerdCanonData*)calloc(1, sizeof(SerdCanonData));

  data->world  = world;
  data->target = target;
  data->flags  = flags;

  return serd_sink_new(data, (SerdEventFunc)serd_canon_on_event, free);
}
