// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "writer.h"

#include "serd/buffer.h"
#include "serd/byte_source.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/node_syntax.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/string_view.h"
#include "serd/syntax.h"
#include "serd/world.h"
#include "serd/writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SerdStatus
on_node_string_event(void* const handle, const SerdEvent* const event)
{
  if (event->type == SERD_STATEMENT) {
    *(SerdNode**)handle =
      serd_node_copy(serd_statement_object(event->statement.statement));
  }

  return SERD_SUCCESS;
}

static SerdNode*
serd_node_from_syntax_in(const char* const str,
                         const SerdSyntax  syntax,
                         SerdEnv* const    env)
{
  static const char* const prelude =
    "_:s <http://www.w3.org/2000/01/rdf-schema#object>";

  const size_t str_len = strlen(str);
  const size_t doc_len = strlen(prelude) + str_len + 5;
  char* const  doc     = (char*)calloc(doc_len + 2, 1);

  snprintf(doc, doc_len + 1, "%s %s .", prelude, str);

  SerdNode*        object = NULL;
  SerdWorld* const world  = serd_world_new();
  SerdSink* const  sink   = serd_sink_new(&object, on_node_string_event, NULL);

  SerdByteSource* const source = serd_byte_source_new_string(doc, NULL);
  SerdReader* const     reader = serd_reader_new(
    world, syntax, SERD_READ_VERBATIM, env, sink, 1024 + doc_len);

  serd_reader_start(reader, source);
  serd_reader_read_document(reader);
  serd_reader_finish(reader);
  serd_reader_free(reader);
  serd_byte_source_free(source);
  serd_sink_free(sink);
  serd_world_free(world);
  free(doc);

  return object;
}

SerdNode*
serd_node_from_syntax(const char* const str,
                      const SerdSyntax  syntax,
                      SerdEnv* const    env)
{
  if (env) {
    return serd_node_from_syntax_in(str, syntax, env);
  }

  SerdEnv* const  temp_env = serd_env_new(serd_empty_string());
  SerdNode* const node     = serd_node_from_syntax_in(str, syntax, temp_env);

  serd_env_free(temp_env);
  return node;
}

static char*
serd_node_to_syntax_in(const SerdNode* const node,
                       const SerdSyntax      syntax,
                       const SerdEnv* const  env)
{
  SerdWorld* const  world  = serd_world_new();
  SerdBuffer        buffer = {NULL, 0};
  SerdWriter* const writer =
    serd_writer_new(world, syntax, 0, env, serd_buffer_write, &buffer);

  char* result = NULL;
  if (!serd_writer_write_node(writer, node) && !serd_writer_finish(writer) &&
      !serd_buffer_close(&buffer)) {
    result = (char*)buffer.buf;
  }

  serd_writer_free(writer);
  serd_world_free(world);

  return result;
}

char*
serd_node_to_syntax(const SerdNode* const node,
                    const SerdSyntax      syntax,
                    const SerdEnv* const  env)
{
  if (env) {
    return serd_node_to_syntax_in(node, syntax, env);
  }

  SerdEnv* const temp_env = serd_env_new(serd_empty_string());
  char* const    string   = serd_node_to_syntax_in(node, syntax, temp_env);

  serd_env_free(temp_env);
  return string;
}
