/*
  Copyright 2011-2020 David Robillard <d@drobilla.net>

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

#include "writer.h"

#include "serd/serd.h"

#include <assert.h>
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
  assert(str);

  static const char* const prelude =
    "_:s <http://www.w3.org/2000/01/rdf-schema#object>";

  const size_t str_len = strlen(str);
  const size_t doc_len = strlen(prelude) + str_len + 4;
  char* const  doc     = (char*)calloc(doc_len + 1, 1);

  snprintf(doc, doc_len + 1, "%s %s .", prelude, str);

  SerdNode*        object = NULL;
  SerdWorld* const world  = serd_world_new();
  SerdSink* const  sink   = serd_sink_new(&object, on_node_string_event, NULL);

  SerdByteSource* const source = serd_byte_source_new_string(doc, NULL);
  SerdReader* const     reader = serd_reader_new(
    world, syntax, SERD_READ_EXACT_BLANKS, env, sink, 1024 + doc_len);

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
  assert(str);

  if (env) {
    return serd_node_from_syntax_in(str, syntax, env);
  }

  SerdEnv* const  temp_env = serd_env_new(SERD_EMPTY_STRING());
  SerdNode* const node     = serd_node_from_syntax_in(str, syntax, temp_env);

  serd_env_free(temp_env);
  return node;
}

static char*
serd_node_to_syntax_in(const SerdNode* const node,
                       const SerdSyntax      syntax,
                       const SerdEnv* const  env)
{
  SerdWorld* const    world  = serd_world_new();
  SerdBuffer          buffer = {NULL, 0};
  SerdByteSink* const out    = serd_byte_sink_new_buffer(&buffer);
  SerdWriter* const   writer = serd_writer_new(world, syntax, 0, env, out);

  char* result = NULL;
  if (!serd_writer_write_node(writer, node) && !serd_writer_finish(writer)) {
    result = serd_buffer_sink_finish(&buffer);
  }

  serd_writer_free(writer);
  serd_byte_sink_free(out);
  serd_world_free(world);

  return result;
}

char*
serd_node_to_syntax(const SerdNode* const node,
                    const SerdSyntax      syntax,
                    const SerdEnv* const  env)
{
  assert(node);

  if (env) {
    return serd_node_to_syntax_in(node, syntax, env);
  }

  SerdEnv* const temp_env = serd_env_new(SERD_EMPTY_STRING());
  char* const    string   = serd_node_to_syntax_in(node, syntax, temp_env);

  serd_env_free(temp_env);
  return string;
}
