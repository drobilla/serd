// Copyright 2019-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "memory.h"
#include "namespaces.h"
#include "node.h"
#include "node_impl.h"
#include "string_utils.h"
#include "warnings.h"
#include "world.h"

#include "exess/exess.h"
#include "serd/canon.h"
#include "serd/caret_view.h"
#include "serd/error.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/sink.h"
#include "serd/statement_view.h"
#include "serd/status.h"
#include "serd/world.h"
#include "zix/allocator.h"
#include "zix/attributes.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

#define MAX_LANG_LEN 48 // RFC5646 requires 35, RFC4646 recommends 42

typedef struct {
  const SerdWorld* world;
  const SerdSink*  target;
  SerdCanonFlags   flags;
} SerdCanonData;

typedef struct {
  ExessResult result;
  SerdNode*   node;
} SerdCanonicalNode;

typedef struct {
  SerdNode node;
  char     buf[MAX_LANG_LEN];
} SerdLangNode;

static SerdStatus
c_err(const SerdWorld* const world,
      const SerdStatus       status,
      const SerdCaretView    caret,
      const char* const      fmt,
      ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);
  const SerdError e = {status, caret.document ? &caret : NULL, fmt, &args};
  serd_world_error(world, &e);
  va_end(args);
  return status;
}

static SerdCanonicalNode
build_typed(ZixAllocator* const ZIX_NONNULL   allocator,
            const SerdNode* const ZIX_NONNULL node,
            const SerdNode* const ZIX_NONNULL datatype)
{
  const char* const str          = serd_node_string(node);
  const char* const datatype_uri = serd_node_string(datatype);
  SerdCanonicalNode r            = {{EXESS_SUCCESS, 0U}, NULL};

  if (!strcmp(datatype_uri, NS_RDF "langString")) {
    r.node =
      serd_node_new(allocator, serd_a_string_view(serd_node_string_view(node)));
    return r;
  }

  const ExessDatatype value_type = exess_datatype_from_uri(datatype_uri);
  if (value_type == EXESS_NOTHING) {
    return r;
  }

  // Measure canonical form to know how much space to allocate for node
  if ((r.result = exess_write_canonical(str, value_type, 0, NULL)).status) {
    return r;
  }

  // Allocate node
  const size_t    len       = serd_node_pad_length(r.result.count);
  const size_t    node_size = sizeof(SerdNode) + len;
  SerdNode* const result    = serd_node_malloc(allocator, node_size);
  if (!result) {
    r.result.status = EXESS_NO_SPACE;
    return r;
  }

  result->meta   = datatype;
  result->length = r.result.count;
  result->flags  = SERD_HAS_DATATYPE;
  result->type   = SERD_LITERAL;

  // Write canonical form directly into node
  exess_write_canonical(
    str, value_type, r.result.count + 1, serd_node_buffer(result));

  r.node = result;
  return r;
}

static SerdCanonicalNode
build_tagged(ZixAllocator* const ZIX_NONNULL   allocator,
             const SerdNode* const ZIX_NONNULL node,
             const SerdNode* const ZIX_NONNULL language,
             SerdLangNode* const               new_lang)
{
  const size_t      node_len = serd_node_length(node);
  const char* const lang     = serd_node_string(language);
  const size_t      lang_len = serd_node_length(language);
  if (lang_len > MAX_LANG_LEN) {
    const SerdCanonicalNode r = {{EXESS_NO_SPACE, node_len}, NULL};
    return r;
  }

  // Convert language tag to lower-case
  new_lang->node.type   = SERD_LITERAL;
  new_lang->node.length = lang_len;
  for (size_t i = 0U; i < lang_len; ++i) {
    new_lang->buf[i] = serd_to_lower(lang[i]);
  }

  // Make a new literal that is otherwise identical
  SerdNode* const out = serd_node_new(
    allocator,
    serd_a_literal(
      serd_node_string_view(node), serd_node_flags(node), &new_lang->node));

  const SerdCanonicalNode r = {{EXESS_SUCCESS, node_len}, out};
  return r;
}

static SerdStatus
serd_canon_on_statement(SerdCanonData* const          data,
                        const SerdStatementEventFlags flags,
                        const SerdStatementView       statement,
                        SerdCaretView                 caret)
{
  ZixAllocator* const   allocator = serd_world_allocator(data->world);
  const SerdNode* const object    = statement.object;
  const SerdNode* const datatype  = serd_node_datatype(object);
  const SerdNode* const language  = serd_node_language(object);

  if (!datatype && !language) {
    return serd_sink_write_statement(data->target, flags, statement);
  }

  SerdLangNode canonical_lang = {{NULL, 0U, 0U, (SerdNodeType)0}, "\0"};

  const SerdCanonicalNode node =
    datatype ? build_typed(allocator, object, datatype)
             : build_tagged(allocator, object, language, &canonical_lang);

  const ExessResult r           = node.result;
  const size_t      node_length = node.result.count;
  if (r.status) {
    if (caret.document) {
      // Adjust column to point at the error within the literal
      caret.column = caret.column + 1U + (unsigned)node_length;
    }

    c_err(data->world,
          SERD_BAD_SYNTAX,
          caret,
          "invalid literal (%s)",
          exess_strerror(r.status));

    if (!(data->flags & SERD_CANON_LAX)) {
      return r.status == EXESS_NO_SPACE ? SERD_BAD_ALLOC : SERD_BAD_LITERAL;
    }
  }

  if (!node.node) {
    return serd_sink_write_statement(data->target, flags, statement);
  }

  SERD_DISABLE_NULL_WARNINGS
  const SerdStatus st = serd_sink_write(data->target,
                                        flags,
                                        statement.subject,
                                        statement.predicate,
                                        node.node,
                                        statement.graph);
  SERD_RESTORE_WARNINGS

  serd_node_free(allocator, node.node);
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

  SerdCanonData* const data =
    (SerdCanonData*)serd_wcalloc(world, 1, sizeof(SerdCanonData));

  if (!data) {
    return NULL;
  }

  data->world  = world;
  data->target = target;
  data->flags  = flags;

  SerdSink* const sink = serd_sink_new(serd_world_allocator(world),
                                       data,
                                       serd_canon_on_event,
                                       serd_canon_data_free);

  if (!sink) {
    serd_wfree(world, data);
  }

  return sink;
}

#undef MAX_LANG_LEN
