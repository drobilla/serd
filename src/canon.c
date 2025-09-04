// Copyright 2019-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log_internal.h"
#include "string_utils.h"
#include "symbols.h"

#include <exess/exess.h>
#include <serd/canon.h>
#include <serd/caret_view.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/handler.h>
#include <serd/log.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/object_view.h>
#include <serd/sink.h>
#include <serd/statement_view.h>
#include <serd/status.h>
#include <serd/string_pair_view.h>
#include <serd/world.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define MAX_LANG_LEN 48U // RFC5646 requires 35, RFC4646 recommends 42

typedef struct {
  size_t         string_size; ///< Size of `string`
  char*          string;      ///< Allocated string or null
  SerdObjectView node;        ///< View of canonical node using `string`
} SerdCanonicalNode;

typedef struct {
  const SerdWorld*  world;
  const SerdEnv*    env;
  const SerdSink*   target;
  SerdCanonicalNode node;
  SerdCanonFlags    flags;
} SerdCanonData;

static ExessVariableResult
build_typed(ZixAllocator* const      allocator,
            const SerdEnv* const     env,
            const SerdObjectView     node,
            SerdCanonicalNode* const out)
{
  const char* const   str = node.string.data;
  ExessVariableResult r   = {EXESS_SUCCESS, 0U, 0U};

  out->node = node;

  // Expand the datatype into a full URI (in-place as a string pair view)
  SerdStringPairView datatype = {zix_empty_string(), zix_empty_string()};
  if (serd_env_resolve(env, node.meta, &datatype)) {
    r.status = EXESS_UNSUPPORTED;
    return r;
  }

  // If datatype is rdf:langString, just strip the datatype
  if (serd_string_pair_view_equals_string(datatype,
                                          serd_symbols[RDF_LANGSTRING])) {
    out->node.flags       = node.flags & ~(SerdNodeFlags)SERD_HAS_DATATYPE;
    out->node.meta.type   = SERD_NOTHING;
    out->node.meta.string = zix_empty_string();
    return r;
  }

  // Do nothing if the datatype isn't in the XSD namespace
  if (datatype.prefix.length > sizeof(EXESS_XSD_URI) ||
      !serd_string_pair_view_starts_with(datatype, zix_string(EXESS_XSD_URI))) {
    return r;
  }

  // Measure canonical form to know how much space to allocate
  const size_t offset = sizeof(EXESS_XSD_URI) - datatype.prefix.length - 1U;
  const char* const   datatype_name = datatype.suffix.data + offset;
  const ExessDatatype value_type    = exess_datatype_from_name(datatype_name);
  if ((value_type == EXESS_NOTHING) ||
      (r = exess_write_canonical(str, value_type, 0, NULL)).status) {
    return r;
  }

  // Ensure there's no unexpected trailing garbage
  for (size_t i = r.read_count; i < node.string.length; ++i) {
    if (!is_space(str[i])) {
      r.status = EXESS_BAD_VALUE;
      return r;
    }
  }

  // Allocate string
  const size_t buf_size = r.write_count + 1U;
  if (out->string_size < buf_size) {
    char* const buf = (char*)zix_realloc(allocator, out->string, buf_size);
    if (!buf) {
      r.status = EXESS_NO_SPACE;
      return r;
    }

    buf[r.write_count] = '\0';
    out->string        = buf;
    out->string_size   = buf_size;
  }

  // Write canonical form with absolute URI datatype
  exess_write_canonical(str, value_type, buf_size, out->string);
  out->node.string      = zix_substring(out->string, r.write_count);
  out->node.meta.type   = SERD_URI;
  out->node.meta.string = zix_string(exess_datatype_uri(value_type));
  return r;
}

static ExessVariableResult
build_tagged(const SerdObjectView     node,
             char                     lang_buf[static MAX_LANG_LEN],
             SerdCanonicalNode* const out)
{
  ExessVariableResult r = {EXESS_SUCCESS, 0U, 0U};

  out->node = node;

  // Get the language tag and ensure its a reasonable size
  const ZixStringView language = node.meta.string;
  if (language.length > MAX_LANG_LEN) {
    r.status = EXESS_NO_SPACE;
    return r;
  }

  // Convert language tag to lower-case
  for (size_t i = 0U; i < language.length; ++i) {
    lang_buf[i] = serd_to_lower(language.data[i]);
  }

  // Return the same node except with the new language tag
  out->node.meta.string = zix_substring(lang_buf, language.length);
  return r;
}

static SerdStatus
serd_canon_on_statement(SerdCanonData* const    data,
                        const SerdEventFlags    flags,
                        const SerdStatementView statement,
                        SerdCaretView           caret)
{
  ZixAllocator* const  allocator = serd_world_allocator(data->world);
  const SerdObjectView object    = statement.object;

  // If the object is a simple token, pass the statement through as-is
  if (!(object.flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE))) {
    return serd_sink_event(
      data->target,
      serd_cite_event(serd_statement_event(flags, statement), caret));
  }

  char                      lang_buf[MAX_LANG_LEN] = {0};
  const ExessVariableResult r =
    (object.flags & SERD_HAS_DATATYPE)
      ? build_typed(allocator, data->env, object, &data->node)
      : build_tagged(object, lang_buf, &data->node);

  const size_t node_length = r.write_count;
  if (r.status) {
    const bool lax = (data->flags & SERD_CANON_LAX);

    if (caret.document.length) {
      // Adjust column to point at the error within the literal
      caret.column = caret.column + 1U + (unsigned)node_length;
    }

    serd_logf(data->world,
              lax ? SERD_LOG_LEVEL_WARNING : SERD_LOG_LEVEL_ERROR,
              caret,
              "invalid literal (%s)",
              exess_strerror(r.status));

    if (!lax) {
      return r.status == EXESS_NO_SPACE ? SERD_BAD_ALLOC : SERD_BAD_LITERAL;
    }
  }

  return serd_sink_event(
    data->target,
    serd_cite_event(serd_statement_event(flags,
                                         serd_quad_view(statement.subject,
                                                        statement.predicate,
                                                        data->node.node,
                                                        statement.graph)),
                    caret));
}

static SerdStatus
serd_canon_on_event(void* const handle, const SerdEvent* const event)
{
  SerdCanonData* const data = (SerdCanonData*)handle;

  return (event->type == SERD_EVENT_STATEMENT)
           ? serd_canon_on_statement(
               data, event->flags, event->body.statement, event->caret)
           : serd_sink_event(data->target, *event);
}

static void
serd_canon_destroy_data(void* const handle)
{
  const SerdCanonData* const data = (const SerdCanonData*)handle;

  zix_free(serd_world_allocator(data->world), data->node.string);
}

SerdHandler*
serd_canon_new(const SerdWorld* const world,
               const SerdEnv* const   env,
               const SerdSink* const  target,
               const SerdCanonFlags   flags)
{
  assert(world);
  assert(target);

  SerdHandler* const canon = serd_handler_new(serd_world_allocator(world),
                                              serd_canon_on_event,
                                              serd_canon_destroy_data,
                                              sizeof(SerdCanonData));

  if (canon) {
    SerdCanonData* const data = (SerdCanonData*)serd_handler_data(canon);

    data->world  = world;
    data->env    = env;
    data->target = target;
    data->flags  = flags;
  }

  return canon;
}

#undef MAX_LANG_LEN
