// Copyright 2019-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "log.h"
#include "namespaces.h"
#include "string_utils.h"

#include "exess/exess.h"
#include "serd/canon.h"
#include "serd/caret_view.h"
#include "serd/event.h"
#include "serd/log.h"
#include "serd/node.h"
#include "serd/object_view.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define MAX_LANG_LEN 48 // RFC5646 requires 35, RFC4646 recommends 42

typedef struct {
  const SerdWorld* world;
  const SerdSink*  target;
  SerdCanonFlags   flags;
} SerdCanonData;

typedef struct {
  ExessResult    result; ///< Result of writing canonical string
  char*          string; ///< Newly allocated string, or NULL
  SerdObjectView node;   ///< View of canonicalized node
} SerdCanonicalNode;

static SerdCanonicalNode
build_typed(ZixAllocator* const ZIX_NONNULL allocator,
            const SerdObjectView            node)
{
  static const ZixStringView rdf_langString =
    ZIX_STATIC_STRING(NS_RDF "langString");

  const ZixStringView datatype = node.meta.string;
  const char* const   str      = node.string.data;
  SerdCanonicalNode   r        = {{EXESS_SUCCESS, 0U}, NULL, node};

  // If datatype is rdf:langString, just strip the datatype
  if (zix_string_view_equals(datatype, rdf_langString)) {
    r.node.flags &= ~(SerdNodeFlags)SERD_HAS_DATATYPE;
    r.node.meta.type   = SERD_LITERAL;
    r.node.meta.string = zix_empty_string();
    return r;
  }

  // Measure canonical form to know how much space to allocate
  const ExessDatatype value_type = exess_datatype_from_uri(datatype.data);
  if ((value_type == EXESS_NOTHING) ||
      (r.result = exess_write_canonical(str, value_type, 0, NULL)).status) {
    return r;
  }

  // Allocate string
  if (!(r.string = (char*)zix_calloc(allocator, r.result.count + 1U, 1U))) {
    r.result.status = EXESS_NO_SPACE;
    return r;
  }

  // Write canonical form
  exess_write_canonical(str, value_type, r.result.count + 1U, r.string);
  r.node.string = zix_substring(r.string, r.result.count);
  return r;
}

static SerdCanonicalNode
build_tagged(const SerdObjectView node, char lang_buf[static MAX_LANG_LEN])
{
  SerdCanonicalNode r = {{EXESS_SUCCESS, node.string.length}, NULL, node};

  const ZixStringView language = node.meta.string;
  if (language.length > MAX_LANG_LEN) {
    r.result.status = EXESS_NO_SPACE;
    return r;
  }

  // Convert language tag to lower-case
  for (size_t i = 0U; i < language.length; ++i) {
    lang_buf[i] = serd_to_lower(language.data[i]);
  }

  // Return the same node except with the new language tag
  r.node.meta.string.data = lang_buf;
  return r;
}

static SerdStatus
serd_canon_on_statement(SerdCanonData* const          data,
                        const SerdStatementEventFlags flags,
                        const SerdStatementView       statement,
                        SerdCaretView                 caret)
{
  ZixAllocator* const  allocator = serd_world_allocator(data->world);
  const SerdObjectView object    = statement.object;

  // If the object is a simple token, pass the statement through as-is
  if (!(object.flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE))) {
    return serd_sink_write_statement(data->target, flags, statement);
  }

  assert(
    ((object.flags & SERD_HAS_DATATYPE) && object.meta.type == SERD_URI) ||
    ((object.flags & SERD_HAS_LANGUAGE) && object.meta.type == SERD_LITERAL));

  char                    lang_buf[MAX_LANG_LEN] = {0};
  const SerdCanonicalNode node = (object.flags & SERD_HAS_DATATYPE)
                                   ? build_typed(allocator, object)
                                   : build_tagged(object, lang_buf);

  const ExessResult r           = node.result;
  const size_t      node_length = node.result.count;
  if (r.status) {
    const bool lax = (data->flags & SERD_CANON_LAX);

    if (caret.document) {
      // Adjust column to point at the error within the literal
      caret.column = caret.column + 1U + (unsigned)node_length;
    }

    serd_logf_at(data->world,
                 lax ? SERD_LOG_LEVEL_WARNING : SERD_LOG_LEVEL_ERROR,
                 caret.document ? &caret : NULL,
                 "invalid literal (%s)",
                 exess_strerror(r.status));

    if (!lax) {
      return r.status == EXESS_NO_SPACE ? SERD_BAD_ALLOC : SERD_BAD_LITERAL;
    }
  }

  const SerdStatus st = serd_sink_write_views(data->target,
                                              flags,
                                              statement.subject,
                                              statement.predicate,
                                              node.node,
                                              statement.graph);

  zix_free(allocator, node.string);
  return st;
}

static SerdStatus
serd_canon_on_event(void* const handle, const SerdEvent* const event)
{
  SerdCanonData* const data = (SerdCanonData*)handle;

  return (event->type == SERD_STATEMENT)
           ? serd_canon_on_statement(data,
                                     event->statement.flags,
                                     event->statement.statement,
                                     event->statement.caret)
           : serd_sink_write_event(data->target, event);
}

static void
serd_canon_data_free(void* const ptr)
{
  SerdCanonData* const data = (SerdCanonData*)ptr;
  zix_free(serd_world_allocator(data->world), data);
}

SerdSink*
serd_canon_new(const SerdWorld* const world,
               const SerdSink* const  target,
               const SerdCanonFlags   flags)
{
  assert(world);
  assert(target);

  ZixAllocator* const  allocator = serd_world_allocator(world);
  SerdCanonData* const data =
    (SerdCanonData*)zix_calloc(allocator, 1, sizeof(SerdCanonData));

  if (!data) {
    return NULL;
  }

  data->world  = world;
  data->target = target;
  data->flags  = flags;

  SerdSink* const sink =
    serd_sink_new(allocator, data, serd_canon_on_event, serd_canon_data_free);

  if (!sink) {
    zix_free(allocator, data);
  }

  return sink;
}

#undef MAX_LANG_LEN
