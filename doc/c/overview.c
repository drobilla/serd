/*
  Copyright 2021 David Robillard <d@drobilla.net>

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

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static void
string_views(void)
{
  static const char* const string_pointer = "some string";

  // begin make-empty-string
  SerdStringView empty = SERD_EMPTY_STRING();
  // end make-empty-string

  // begin make-static-string
  SerdStringView hello = SERD_STATIC_STRING("hello");
  // end make-static-string

  // begin measure-string
  SerdStringView view = SERD_MEASURE_STRING(string_pointer);
  // end measure-string

  // begin make-string-view
  SerdStringView slice = SERD_STRING_VIEW(string_pointer, 4);
  // end make-string-view
}

static void
statements(void)
{
  // begin statement-new
  SerdNode* subject   = serd_new_curie(SERD_STATIC_STRING("eg:drobilla"));
  SerdNode* predicate = serd_new_curie(SERD_STATIC_STRING("eg:firstName"));
  SerdNode* object    = serd_new_string(SERD_STATIC_STRING("David"));

  SerdStatement* statement =
    serd_statement_new(subject, predicate, object, NULL, NULL);
  // end statement-new
}

static void
statements_accessing_fields(void)
{
  SerdStatement* statement = NULL;

  // begin get-subject
  const SerdNode* s = serd_statement_node(statement, SERD_SUBJECT);
  // end get-subject

  // begin get-pog
  const SerdNode* p = serd_statement_predicate(statement);
  const SerdNode* o = serd_statement_object(statement);
  const SerdNode* g = serd_statement_graph(statement);
  // end get-pog

  // begin get-cursor
  const SerdCursor* c = serd_statement_cursor(statement);
  // end get-cursor
}

static void
statements_comparison(void)
{
  SerdStatement* statement1 = NULL;
  SerdStatement* statement2 = NULL;

  // begin statement-equals
  if (serd_statement_equals(statement1, statement2)) {
    printf("Match\n");
  }
  // end statement-equals

  SerdStatement* statement = NULL;

  // begin statement-matches
  SerdNode* eg_name =
    serd_new_uri(SERD_STATIC_STRING("http://example.org/name"));

  if (serd_statement_matches(statement, NULL, eg_name, NULL, NULL)) {
    printf("%s has name %s\n",
           serd_node_string(serd_statement_subject(statement)),
           serd_node_string(serd_statement_object(statement)));
  }
  // end statement-matches
}

static void
statements_lifetime(void)
{
  SerdStatement* statement = NULL;

  // begin statement-copy
  SerdStatement* copy = serd_statement_copy(statement);
  // end statement-copy

  // begin statement-free
  serd_statement_free(copy);
  // end statement-free
}

static void
world(void)
{
  // begin world-new
  SerdWorld* world = serd_world_new();
  // end world-new

  // begin get-blank
  const SerdNode* blank = serd_node_copy(serd_world_get_blank(world));
  // end get-blank
}

static void
model(void)
{
  SerdWorld* world = serd_world_new();

  // begin model-new
  SerdModel* model = serd_model_new(world, SERD_INDEX_SPO);
  // end model-new

  // begin fancy-model-new
  SerdModel* fancy_model = serd_model_new(
    world, (SERD_INDEX_SPO | SERD_INDEX_PSO | SERD_STORE_CURSORS));
  // end fancy-model-new

  // begin model-copy
  SerdModel* copy = serd_model_copy(model);

  assert(serd_model_equals(copy, model));
  // end model-copy

  // begin model-size
  if (serd_model_empty(model)) {
    printf("Model is empty\n");
  } else if (serd_model_size(model) > 1000) {
    printf("Model has over 1000 statements\n");
  }
  // end model-size

  // begin model-free
  serd_model_free(copy);
  // end model-free

  // begin model-add
  SerdNode* s = serd_new_uri(SERD_STATIC_STRING("http://example.org/thing"));
  SerdNode* p = serd_new_uri(SERD_STATIC_STRING("http://example.org/name"));
  SerdNode* o = serd_new_string(SERD_STATIC_STRING("Thing"));

  serd_model_add(model, s, p, o, NULL);
  // end model-add

  SerdModel* other_model = NULL;

  // begin model-insert
  serd_model_insert(model, serd_iter_get(serd_model_begin(other_model)));
  // end model-insert

  // begin model-add-range
  SerdRange* other_range = serd_model_all(other_model);

  serd_model_add_range(model, other_range);

  serd_range_free(other_range);
  // end model-add-range

  // begin model-begin-end
  SerdIter* i = serd_model_begin(model);
  if (serd_iter_equals(i, serd_model_end(model))) {
    printf("Model is empty\n");
  } else {
    const SerdStatement* s = serd_iter_get(i);

    printf("First statement subject: %s\n",
           serd_node_string(serd_statement_subject(s)));
  }
  // end model-begin-end

  // begin iter-next
  if (!serd_iter_next(i)) {
    const SerdStatement* s = serd_iter_get(i);

    printf("Second statement subject: %s\n",
           serd_node_string(serd_statement_subject(s)));
  }
  // end iter-next

  // begin iter-free
  serd_iter_free(i);
  // end iter-free

  // begin model-all
  SerdRange* all = serd_model_all(model);
  // end model-all

  // begin range-next
  if (serd_range_empty(all)) {
    printf("Model is empty\n");
  } else {
    const SerdStatement* s = serd_range_front(all);

    printf("First statement subject: %s\n",
           serd_node_string(serd_statement_subject(s)));
  }

  if (!serd_range_next(all)) {
    const SerdStatement* s = serd_range_front(all);

    printf("Second statement subject: %s\n",
           serd_node_string(serd_statement_subject(s)));
  }
  // end range-next

  // begin model-ask
  SerdNode* rdf_type = serd_new_uri(
    SERD_STATIC_STRING("http://www.w3.org/1999/02/22-rdf-syntax-ns#type"));

  if (serd_model_ask(model, NULL, rdf_type, NULL, NULL)) {
    printf("Model contains a type statement\n");
  }
  // end model-ask

  // begin model-find
  SerdIter* it = serd_model_find(model, NULL, rdf_type, NULL, NULL);

  const SerdNode* instance = serd_statement_subject(serd_iter_get(it));
  // end model-find

  // begin model-count
  size_t n = serd_model_count(model, instance, rdf_type, NULL, NULL);
  printf("Instance has %zu types\n", n);
  // end model-count

  // begin model-range
  SerdRange* range = serd_model_range(model, instance, rdf_type, NULL, NULL);

  for (; !serd_range_empty(range); serd_range_next(range)) {
    const SerdStatement* s = serd_range_front(range);

    printf("Instance has type %s\n",
           serd_node_string(serd_statement_object(s)));
  }

  serd_range_free(range);
  // end model-range

  // begin model-get
  const SerdNode* t = serd_model_get(model, instance, rdf_type, NULL, NULL);
  if (t) {
    printf("Instance has type %s\n", serd_node_string(t));
  }
  // end model-get

  // begin model-get-statement
  const SerdStatement* ts =
    serd_model_get_statement(model, instance, rdf_type, NULL, NULL);

  if (ts) {
    printf("Instance %s has type %s\n",
           serd_node_string(serd_statement_subject(ts)),
           serd_node_string(serd_statement_object(ts)));
  }
  // end model-get-statement

  // begin model-erase
  SerdIter* some_type = serd_model_find(model, NULL, rdf_type, NULL, NULL);
  serd_model_erase(model, some_type);
  serd_iter_free(some_type);
  // end model-erase

  // begin model-erase-range
  SerdRange* all_types = serd_model_range(model, NULL, rdf_type, NULL, NULL);
  serd_model_erase_range(model, all_types);
  serd_range_free(all_types);
  // end model-erase-range
}

static void
reading_writing(void)
{
  SerdWorld* world = serd_world_new();

  // begin env-new
  SerdStringView host     = SERD_EMPTY_STRING();
  SerdStringView out_path = SERD_STATIC_STRING("/some/file.ttl");
  SerdNode*      base     = serd_new_file_uri(out_path, host);
  SerdEnv*       env      = serd_env_new(serd_node_string_view(base));
  // end env-new

  // begin env-set-prefix
  serd_env_set_prefix(
    env,
    SERD_STATIC_STRING("rdf"),
    SERD_STATIC_STRING("http://www.w3.org/1999/02/22-rdf-syntax-ns#"));
  // end env-set-prefix

  // begin byte-sink-new
  SerdByteSink* out = serd_byte_sink_new_filename("/tmp/eg.ttl", 4096);
  // end byte-sink-new

  // begin writer-new
  SerdWriter* writer = serd_writer_new(world, SERD_TURTLE, 0, env, out);
  // end writer-new

  // begin reader-new
  SerdReader* reader = serd_reader_new(world,
                                       SERD_TURTLE,
                                       (SerdReaderFlags)0,
                                       env,
                                       serd_writer_sink(writer),
                                       4096);
  // end reader-new

  // begin read-document
  SerdStatus st = serd_reader_read_document(reader);
  if (st) {
    printf("Error reading document: %s\n", serd_strerror(st));
  }
  // end read-document

  // begin reader-writer-free
  serd_reader_free(reader);
  serd_writer_free(writer);
  // end reader-writer-free

  // begin byte-sink-free
  serd_byte_sink_free(out);
  // end byte-sink-free

  // begin inserter-new
  SerdModel* model    = serd_model_new(world, SERD_INDEX_SPO);
  SerdSink*  inserter = serd_inserter_new(model, NULL);
  // end inserter-new

  // begin model-reader-new
  SerdReader* const model_reader =
    serd_reader_new(world, SERD_TURTLE, 0, env, inserter, 4096);

  st = serd_reader_read_document(model_reader);
  if (st) {
    printf("Error loading model: %s\n", serd_strerror(st));
  }
  // end model-reader-new

  // begin write-range
  serd_write_range(serd_model_all(model), serd_writer_sink(writer), 0);
  // end write-range

  // begin canon-new
  SerdSink* canon = serd_canon_new(world, inserter, 0);
  // end canon-new

  SerdNode* rdf_type = NULL;

  // begin filter-new
  SerdSink* filter = serd_filter_new(inserter, // Target
                                     NULL,     // Subject
                                     rdf_type, // Predicate
                                     NULL,     // Object
                                     NULL,     // Graph
                                     true);    // Inclusive
  // end filter-new
}

int
main(void)
{
  string_views();
  statements();
  statements_accessing_fields();
  statements_comparison();
  statements_lifetime();
  world();
  model();
  reading_writing();

  return 0;
}

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
