// Copyright 2018-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

static size_t n_base      = 0;
static size_t n_prefix    = 0;
static size_t n_statement = 0;
static size_t n_end       = 0;

static SerdStatus
on_base(void* handle, const SerdNode* uri)
{
  (void)handle;
  (void)uri;

  ++n_base;
  return SERD_SUCCESS;
}

static SerdStatus
on_prefix(void* handle, const SerdNode* name, const SerdNode* uri)
{
  (void)handle;
  (void)name;
  (void)uri;

  ++n_prefix;
  return SERD_SUCCESS;
}

static SerdStatus
on_statement(void*              handle,
             SerdStatementFlags flags,
             const SerdNode*    graph,
             const SerdNode*    subject,
             const SerdNode*    predicate,
             const SerdNode*    object,
             const SerdNode*    object_datatype,
             const SerdNode*    object_lang)
{
  (void)handle;
  (void)flags;
  (void)graph;
  (void)subject;
  (void)predicate;
  (void)object;
  (void)object_datatype;
  (void)object_lang;

  ++n_statement;
  return SERD_SUCCESS;
}

static SerdStatus
on_end(void* handle, const SerdNode* node)
{
  (void)handle;
  (void)node;

  ++n_end;
  return SERD_SUCCESS;
}

int
main(void)
{
  FILE* file = tmpfile();

  fprintf(file,
          "@prefix eg: <http://example.org/> .\n"
          "@base <http://example.org/base> .\n"
          "eg:s1 eg:p1 eg:o1 ;\n"
          "      eg:p2 eg:o2 ,\n"
          "            eg:o3 .\n"
          "eg:s2 eg:p1 eg:o1 ;\n"
          "      eg:p2 eg:o2 .\n"
          "eg:s3 eg:p1 eg:o1 .\n"
          "eg:s4 eg:p1 [ eg:p3 eg:o1 ] .\n");

  fseek(file, 0, SEEK_SET);

  SerdReader* reader = serd_reader_new(
    SERD_TURTLE, NULL, NULL, on_base, on_prefix, on_statement, on_end);

  assert(reader);
  assert(!serd_reader_start_stream(reader, file, NULL, true));

  assert(!serd_reader_read_chunk(reader) && n_prefix == 1);
  assert(!serd_reader_read_chunk(reader) && n_base == 1);
  assert(!serd_reader_read_chunk(reader) && n_statement == 3);
  assert(!serd_reader_read_chunk(reader) && n_statement == 5);
  assert(!serd_reader_read_chunk(reader) && n_statement == 6);
  assert(!serd_reader_read_chunk(reader) && n_statement == 8 && n_end == 1);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
  assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

  assert(!serd_reader_end_stream(reader));
  serd_reader_free(reader);
  fclose(file);

  return 0;
}
