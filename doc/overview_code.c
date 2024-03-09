// Copyright 2021-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

/*
  Example code that is included in the documentation.  Code in the
  documentation is included from here rather than written inline so that it can
  be tested and avoid rotting.  The code here doesn't make much sense, but is
  written such that it at least compiles and will run without crashing.
*/

#include "serd/serd.h"
#include "zix/string_view.h"

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
  const char* const string_pointer = "some string";

  // begin make-empty-string
  ZixStringView empty = zix_empty_string();
  // end make-empty-string

  // begin make-static-string
  static const ZixStringView hello = ZIX_STATIC_STRING("hello");
  // end make-static-string

  // begin measure-string
  ZixStringView view = zix_string(string_pointer);
  // end measure-string

  // begin make-string-view
  ZixStringView slice = zix_substring(string_pointer, 4);
  // end make-string-view
}

#if 0

static void
statements(void)
{
  SerdNodes* nodes = serd_nodes_new(NULL);

  // begin statement-new
  SerdStatement* statement = serd_statement_new(
    NULL,
    serd_nodes_get(nodes, serd_a_uri_string("http://example.org/drobilla")),
    serd_nodes_get(nodes, serd_a_uri_string("http://example.org/firstName")),
    serd_nodes_get(nodes, serd_a_string("David")),
    NULL,
    NULL);
  // end statement-new

  serd_statement_free(NULL, statement);
  serd_nodes_free(nodes);
}

static void
statements_accessing_fields(void)
{
  SerdNode* ss =
    serd_node_new(NULL, serd_a_uri_string(("http://example.org/s")));

  SerdNode* sp =
    serd_node_new(NULL, serd_a_uri_string(("http://example.org/p")));

  SerdNode* so =
    serd_node_new(NULL, serd_a_uri_string(("http://example.org/o")));

  SerdStatement* statement = serd_statement_new(NULL, ss, sp, so, NULL, NULL);

  // begin get-subject
  const SerdNode* s = serd_statement_node(statement, SERD_SUBJECT);
  // end get-subject

  // begin get-pog
  const SerdNode* p = serd_statement_predicate(statement);
  const SerdNode* o = serd_statement_object(statement);
  const SerdNode* g = serd_statement_graph(statement);
  // end get-pog

  // begin get-caret
  const SerdCaret* c = serd_statement_caret(statement);
  // end get-caret
}

static void
statements_comparison(void)
{
  SerdNode* ss = serd_node_new(NULL, serd_a_uri_string("http://example.org/s"));

  SerdNode* sp = serd_node_new(NULL, serd_a_uri_string("http://example.org/p"));

  SerdNode* so = serd_node_new(NULL, serd_a_uri_string("http://example.org/o"));

  SerdStatement* statement1 = serd_statement_new(NULL, ss, sp, so, NULL, NULL);
  SerdStatement* statement2 = serd_statement_new(NULL, ss, sp, so, NULL, NULL);

  // begin statement-equals
  if (serd_statement_equals(statement1, statement2)) {
    printf("Match\n");
  }
  // end statement-equals

  SerdStatement* statement = statement1;

  // begin statement-matches
  SerdNode* eg_name =
    serd_node_new(NULL, serd_a_uri_string("http://example.org/name"));

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
  SerdStatement* copy = serd_statement_copy(NULL, statement);
  // end statement-copy

  // begin statement-free
  serd_statement_free(NULL, copy);
  // end statement-free
}

#endif

static void
world(void)
{
  // begin world-new
  SerdWorld* world = serd_world_new(NULL);
  // end world-new

  // begin get-blank
  const SerdNode* world_blank = serd_world_get_blank(world);
  SerdNode*       my_blank    = serd_node_copy(NULL, world_blank);
  // end get-blank
}

static void
model(void)
{
  SerdWorld* world = serd_world_new(NULL);

  // begin model-new
  SerdModel* model = serd_model_new(world, SERD_ORDER_SPO, 0U);
  // end model-new

  // begin fancy-model-new
  SerdModel* fancy_model =
    serd_model_new(world, SERD_ORDER_SPO, SERD_STORE_CARETS);

  serd_model_add_index(fancy_model, SERD_ORDER_PSO);
  // end fancy-model-new

  // begin model-copy
  SerdModel* copy = serd_model_copy(NULL, model);

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
  SerdNodes* nodes = serd_nodes_new(NULL);

  serd_model_add(
    model,
    serd_nodes_get(nodes, serd_a_uri_string("http://example.org/thing")), // S
    serd_nodes_get(nodes, serd_a_uri_string("http://example.org/name")),  // P
    serd_nodes_get(nodes, serd_a_string("Thing")),                        // O
    NULL);                                                                // G
  // end model-add

  SerdModel* other_model = model;

  // begin model-insert
  const SerdCursor* cursor = serd_model_begin(NULL, other_model);

  serd_model_insert(model, serd_cursor_get(cursor));
  // end model-insert

  // begin model-add-range
  SerdCursor* other_range = serd_model_begin(NULL, other_model);

  serd_model_insert_statements(model, other_range);

  serd_cursor_free(NULL, other_range);
  // end model-add-range

  // begin model-begin-end
  SerdCursor* i = serd_model_begin(NULL, model);
  if (serd_cursor_equals(i, serd_model_end(model))) {
    printf("Model is empty\n");
  } else {
    const SerdStatementView s = serd_cursor_get(i);

    printf("First statement subject: %s\n", serd_node_string(s.subject));
  }
  // end model-begin-end

  // begin iter-next
  if (!serd_cursor_advance(i)) {
    const SerdStatementView s = serd_cursor_get(i);

    printf("Second statement subject: %s\n", serd_node_string(s.subject));
  }
  // end iter-next

  // begin iter-free
  serd_cursor_free(NULL, i);
  // end iter-free

  // begin model-all
  SerdCursor* all = serd_model_begin(NULL, model);
  // end model-all

  // begin range-next
  if (serd_cursor_is_end(all)) {
    printf("Model is empty\n");
  } else {
    const SerdStatementView s = serd_cursor_get(all);

    printf("First statement subject: %s\n", serd_node_string(s.subject));
  }

  if (!serd_cursor_advance(all)) {
    const SerdStatementView s = serd_cursor_get(all);

    printf("Second statement subject: %s\n", serd_node_string(s.subject));
  }
  // end range-next

  // begin model-ask
  const SerdNode* rdf_type = serd_nodes_get(
    nodes,
    serd_a_uri_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#type"));

  if (serd_model_ask(model, NULL, rdf_type, NULL, NULL)) {
    printf("Model contains a type statement\n");
  }
  // end model-ask

  // Add a statement so that the searching examples below work
  SerdNode* inst =
    serd_node_new(NULL, serd_a_uri_string("http://example.org/i"));
  SerdNode* type =
    serd_node_new(NULL, serd_a_uri_string("http://example.org/T"));
  serd_model_add(model, inst, rdf_type, type, NULL);

  // begin model-find
  SerdCursor* it = serd_model_find(NULL, model, NULL, rdf_type, NULL, NULL);

  const SerdStatementView statement = serd_cursor_get(it);
  const SerdNode*         instance  = statement.subject;
  // end model-find

  // begin model-count
  size_t n = serd_model_count(model, instance, rdf_type, NULL, NULL);
  printf("Instance has %zu types\n", n);
  // end model-count

  // begin model-range
  SerdCursor* range = serd_model_find(NULL,
                                      model,
                                      instance, // Subject = instance
                                      rdf_type, // Predicate = rdf:type
                                      NULL,     // Object = anything
                                      NULL);    // Graph = anything

  for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
    const SerdStatementView s = serd_cursor_get(range);

    printf("Instance has type %s\n", serd_node_string(s.object));
  }

  serd_cursor_free(NULL, range);
  // end model-range

  // begin model-get
  const SerdNode* t = serd_model_get(model,
                                     instance, // Subject
                                     rdf_type, // Predicate
                                     NULL,     // Object
                                     NULL);    // Graph
  if (t) {
    printf("Instance has type %s\n", serd_node_string(t));
  }
  // end model-get

  // begin model-get-statement
  const SerdStatementView ts =
    serd_model_get_statement(model, instance, rdf_type, NULL, NULL);

  if (ts.subject) {
    printf("Instance %s has type %s\n",
           serd_node_string(ts.subject),
           serd_node_string(ts.object));
  }
  // end model-get-statement

  // begin model-erase
  SerdCursor* some_type =
    serd_model_find(NULL, model, NULL, rdf_type, NULL, NULL);
  serd_model_erase(model, some_type);
  serd_cursor_free(NULL, some_type);
  // end model-erase

  // begin model-erase-range
  SerdCursor* all_types =
    serd_model_find(NULL, model, NULL, rdf_type, NULL, NULL);
  serd_model_erase_statements(model, all_types);
  serd_cursor_free(NULL, all_types);
  // end model-erase-range
}

static void
reading_writing(void)
{
  SerdWorld* world = serd_world_new(NULL);

  // begin env-new
  ZixStringView host = zix_empty_string();
  ZixStringView path = zix_string("/some/file.ttl");
  SerdNode*     base = serd_node_new(NULL, serd_a_file_uri(path, host));
  SerdEnv*      env  = serd_env_new(NULL, serd_node_string_view(base));
  // end env-new

  // begin env-set-prefix
  serd_env_set_prefix(
    env,
    zix_string("rdf"),
    zix_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#"));
  // end env-set-prefix

  // begin byte-sink-new
  SerdOutputStream out = serd_open_output_file("/tmp/eg.ttl");
  // end byte-sink-new

  // clang-format off
  // begin writer-new
  SerdWriter* writer = serd_writer_new(
    world,       // World
    SERD_TURTLE, // Syntax
    0,           // Writer flags
    env,         // Environment
    &out,        // Output stream
    4096);       // Block size
  // end writer-new

  // begin reader-new
  SerdReader* reader = serd_reader_new(
    world,                     // World
    SERD_TURTLE,               // Syntax
    0,                         // Reader flags
    env,                       // Environment
    serd_writer_sink(writer)); // Target sink
  // end reader-new

  // clang-format on

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
  serd_close_output(&out);
  // end byte-sink-free

  // begin inserter-new
  SerdModel* model    = serd_model_new(world, SERD_ORDER_SPO, 0U);
  SerdSink*  inserter = serd_inserter_new(model, NULL);
  // end inserter-new

  // begin model-reader-new
  SerdReader* const model_reader =
    serd_reader_new(world, SERD_TURTLE, 0, env, inserter);

  st = serd_reader_read_document(model_reader);
  if (st) {
    printf("Error loading model: %s\n", serd_strerror(st));
  }
  // end model-reader-new

  // begin write-range
  serd_describe_range(
    NULL, serd_model_begin(NULL, model), serd_writer_sink(writer), 0);
  // end write-range

  // begin canon-new
  SerdSink* canon = serd_canon_new(world, inserter, 0);
  // end canon-new

  SerdNode* rdf_type = NULL;

  // begin filter-new
  SerdSink* filter = serd_filter_new(world,    // World
                                     inserter, // Target
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
  /* statements(); */
  /* statements_accessing_fields(); */
  /* statements_comparison(); */
  /* statements_lifetime(); */
  world();
  model();
  reading_writing();

  return 0;
}

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
