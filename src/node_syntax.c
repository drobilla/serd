// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "memory.h"
#include "node_internal.h"
#include "writer.h"

#include "serd/buffer.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/input_stream.h"
#include "serd/node.h"
#include "serd/node_syntax.h"
#include "serd/output_stream.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"
#include "zix/allocator.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  ZixAllocator* allocator;
  SerdNode*     object;
} NodeSyntaxContext;

static SerdStatus
on_syntax_event(void* const handle, const SerdEvent* const event)
{
  NodeSyntaxContext* const ctx = (NodeSyntaxContext*)handle;

  if (event->type == SERD_STATEMENT) {
    ctx->object =
      serd_node_deep_copy(ctx->allocator, event->statement.statement.object);
  }

  return SERD_SUCCESS;
}

static SerdNode*
serd_node_from_syntax_in(SerdWorld* const  world,
                         const char* const str,
                         const SerdSyntax  syntax,
                         SerdEnv* const    env)
{
  assert(str);

  static const char* const prelude =
    "_:s <http://www.w3.org/2000/01/rdf-schema#object>";

  ZixAllocator* const alloc = serd_world_allocator(world);

  const size_t      str_len = strlen(str);
  const size_t      doc_len = strlen(prelude) + str_len + 5;
  NodeSyntaxContext ctx     = {serd_world_allocator(world), NULL};
  char* const       doc     = (char*)serd_wcalloc(world, doc_len + 2, 1);
  SerdSink* const   sink    = serd_sink_new(alloc, &ctx, on_syntax_event, NULL);

  if (doc && sink) {
    snprintf(doc, doc_len + 1, "%s %s .", prelude, str);

    const SerdLimits old_limits = serd_world_limits(world);
    const SerdLimits limits     = {1024 + doc_len, 8U};
    serd_world_set_limits(world, limits);

    SerdReader* const reader = serd_reader_new(
      world,
      syntax,
      SERD_READ_RELATIVE | SERD_READ_GLOBAL | SERD_READ_GENERATED,
      env,
      sink);

    if (reader) {
      const char*     position = doc;
      SerdInputStream in       = serd_open_input_string(&position);
      serd_reader_start(reader, &in, NULL, 1);
      serd_reader_read_document(reader);
      serd_reader_finish(reader);
      serd_close_input(&in);
    }

    serd_reader_free(reader);
    serd_world_set_limits(world, old_limits);
  }

  serd_sink_free(sink);
  serd_wfree(world, doc);

  return ctx.object;
}

SerdNode*
serd_node_from_syntax(ZixAllocator* const allocator,
                      const char* const   str,
                      const SerdSyntax    syntax,
                      SerdEnv* const      env)
{
  assert(str);

  SerdWorld* const temp_world = serd_world_new(allocator);
  if (!temp_world) {
    return NULL;
  }

  SerdNode* node = NULL;
  if (env) {
    node = serd_node_from_syntax_in(temp_world, str, syntax, env);
  } else {
    SerdEnv* const temp_env = serd_env_new(allocator, zix_empty_string());
    if (temp_env) {
      node = serd_node_from_syntax_in(temp_world, str, syntax, temp_env);
    }
    serd_env_free(temp_env);
  }

  serd_world_free(temp_world);
  return node;
}

static char*
serd_node_to_syntax_in(SerdWorld* const      world,
                       const SerdNode* const node,
                       const SerdSyntax      syntax,
                       const SerdEnv* const  env)
{
  SerdBuffer       buffer = {serd_world_allocator(world), NULL, 0};
  SerdOutputStream out    = serd_open_output_buffer(&buffer);

  const SerdLimits old_limits = serd_world_limits(world);
  const SerdLimits limits     = {0U, 4U};
  serd_world_set_limits(world, limits);

  SerdWriter* const writer = serd_writer_new(world, syntax, 0, env, &out, 1);
  serd_world_set_limits(world, old_limits);
  if (!writer) {
    return NULL;
  }

  char* result = NULL;
  if (!serd_writer_write_node(writer, node) && !serd_writer_finish(writer)) {
    if (!serd_close_output(&out)) {
      result = (char*)buffer.buf;
    }
  } else {
    serd_close_output(&out);
  }

  serd_writer_free(writer);

  if (!result) {
    zix_free(buffer.allocator, buffer.buf);
  }

  return result;
}

char*
serd_node_to_syntax(ZixAllocator* const   allocator,
                    const SerdNode* const node,
                    const SerdSyntax      syntax,
                    const SerdEnv* const  env)
{
  assert(node);

  SerdWorld* const temp_world = serd_world_new(allocator);
  if (!temp_world) {
    return NULL;
  }

  char* string = NULL;
  if (env) {
    string = serd_node_to_syntax_in(temp_world, node, syntax, env);
  } else {
    SerdEnv* const temp_env = serd_env_new(allocator, zix_empty_string());
    if (temp_env) {
      string = serd_node_to_syntax_in(temp_world, node, syntax, temp_env);
    }
    serd_env_free(temp_env);
  }

  serd_world_free(temp_world);
  return string;
}
