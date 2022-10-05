// Copyright 2019-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "caret.h" // IWYU pragma: keep
#include "namespaces.h"
#include "node.h"
#include "statement.h" // IWYU pragma: keep
#include "string_utils.h"

#include "exess/exess.h"
#include "serd/attributes.h"
#include "serd/canon.h"
#include "serd/caret.h"
#include "serd/event.h"
#include "serd/log.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/world.h"

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
build_typed(SerdNode** const                   out,
            const SerdNode* const SERD_NONNULL node,
            const SerdNode* const SERD_NONNULL datatype)
{
  *out = NULL;

  const char* str          = serd_node_string(node);
  const char* datatype_uri = serd_node_string(datatype);
  ExessResult r            = {EXESS_SUCCESS, 0};

  if (!strcmp(datatype_uri, NS_RDF "langString")) {
    *out = serd_new_string(serd_node_string_view(node));
    return r;
  }

  const ExessDatatype value_type = exess_datatype_from_uri(datatype_uri);
  if (value_type == EXESS_NOTHING) {
    return r;
  }

  // Measure canonical form to know how much space to allocate for node
  if ((r = exess_write_canonical(str, value_type, 0, NULL)).status) {
    return r;
  }

  // Allocate node
  const size_t    datatype_uri_len = serd_node_length(datatype);
  const size_t    datatype_size    = serd_node_total_size(datatype);
  const size_t    len              = serd_node_pad_length(r.count);
  const size_t    total_len        = sizeof(SerdNode) + len + datatype_size;
  SerdNode* const result           = serd_node_malloc(total_len);

  result->length = r.count;
  result->flags  = SERD_HAS_DATATYPE;
  result->type   = SERD_LITERAL;

  // Write canonical form directly into node
  exess_write_canonical(str, value_type, r.count + 1, serd_node_buffer(result));

  SerdNode* const datatype_node = result + 1 + (len / sizeof(SerdNode));
  char* const     datatype_buf  = serd_node_buffer(datatype_node);

  datatype_node->length = datatype_uri_len;
  datatype_node->type   = SERD_URI;
  memcpy(datatype_buf, datatype_uri, datatype_uri_len + 1);

  *out = result;
  return r;
}

static ExessResult
build_tagged(SerdNode** const                   out,
             const SerdNode* const SERD_NONNULL node,
             const SerdNode* const SERD_NONNULL language)
{
#define MAX_LANG_LEN 48 // RFC5646 requires 35, RFC4646 recommends 42

  const size_t      node_len = serd_node_length(node);
  const char* const lang     = serd_node_string(language);
  const size_t      lang_len = serd_node_length(language);
  if (lang_len > MAX_LANG_LEN) {
    const ExessResult r = {EXESS_NO_SPACE, node_len};
    return r;
  }

  // Convert language tag to lower-case
  char canonical_lang[MAX_LANG_LEN] = {0};
  for (size_t i = 0U; i < lang_len; ++i) {
    canonical_lang[i] = serd_to_lower(lang[i]);
  }

  // Make a new literal that is otherwise identical
  *out = serd_new_literal(serd_node_string_view(node),
                          serd_node_flags(node),
                          serd_substring(canonical_lang, lang_len));

  const ExessResult r = {EXESS_SUCCESS, node_len};
  return r;

#undef MAX_LANG_LEN
}

static SerdStatus
serd_canon_on_statement(SerdCanonData* const       data,
                        const SerdStatementFlags   flags,
                        const SerdStatement* const statement)
{
  const SerdNode* const object   = serd_statement_object(statement);
  const SerdNode* const datatype = serd_node_datatype(object);
  const SerdNode* const language = serd_node_language(object);
  if (!datatype && !language) {
    return serd_sink_write_statement(data->target, flags, statement);
  }

  SerdNode*         normo = NULL;
  const ExessResult r     = datatype ? build_typed(&normo, object, datatype)
                                     : build_tagged(&normo, object, language);

  if (r.status) {
    SerdCaret  caret = {NULL, 0U, 0U};
    const bool lax   = (data->flags & SERD_CANON_LAX);

    if (statement->caret) {
      // Adjust column to point at the error within the literal
      caret.document = statement->caret->document;
      caret.line     = statement->caret->line;
      caret.col      = statement->caret->col + 1 + (unsigned)r.count;
    }

    serd_logf_at(data->world,
                 lax ? SERD_LOG_LEVEL_WARNING : SERD_LOG_LEVEL_ERROR,
                 statement->caret ? &caret : NULL,
                 "invalid literal (%s)",
                 exess_strerror(r.status));

    if (!lax) {
      return SERD_BAD_LITERAL;
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
  assert(target);

  SerdCanonData* const data = (SerdCanonData*)calloc(1, sizeof(SerdCanonData));

  data->world  = world;
  data->target = target;
  data->flags  = flags;

  return serd_sink_new(world, data, (SerdEventFunc)serd_canon_on_event, free);
}
