// Copyright 2021-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

/*
  Example code that is included in the documentation.  Code in the
  documentation is included from here rather than written inline so that it can
  be tested to avoid rotting.  The code here doesn't make much sense, but is
  written such that it at least compiles and will run without crashing.
*/

#include <serd/serd.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static void
string_views(void)
{
  const char* const string_pointer = "some string";

  // begin make-static-string
  static const ZixStringView hello = ZIX_STATIC_STRING("hello");
  // end make-static-string

  // begin make-empty-string
  ZixStringView empty = zix_empty_string();
  // end make-empty-string

  // begin measure-string
  ZixStringView view = zix_string(string_pointer);
  // end measure-string

  // begin make-string-view
  ZixStringView slice = zix_substring(string_pointer, 4);
  // end make-string-view
}

static void
world(void)
{
  // begin world-new
  SerdWorld* world = serd_world_new(NULL);
  // end world-new

  serd_world_free(world);
}

static void
nodes(void)
{
  // begin nodes-new
  SerdNodes* nodes = serd_nodes_new(NULL);
  // end nodes-new

  // begin nodes-id
  SerdNodeID t = serd_nodes_id(nodes, serd_a_string(zix_string("a")));
  SerdNodeID o = serd_nodes_id(nodes, serd_a_decimal(3.1));
  // end nodes-id

  // begin nodes-find
  SerdNodeID i = serd_nodes_find(nodes, serd_a_integer(42));
  if (i) {
    printf("Node found\n");
  }
  // end nodes-find

  // begin strings-new
  SerdStrings* strings = serd_strings_new(NULL, nodes);
  // end strings-new

  // begin nodes-get-token
  SerdTokenView tok = serd_strings_token(strings, t);
  printf("First letter: %s\n", tok.string.data);
  // end nodes-get-token

  // begin nodes-get-object
  SerdObjectView obj = serd_strings_object(strings, o);
  printf("Number: %s\n", obj.string.data);
  printf("Datatype: %s\n", obj.meta.string.data);
  // end nodes-get-object

  // begin nodes-free
  serd_strings_free(strings);
  serd_nodes_free(nodes);
  // end nodes-free
}

static void
model(void)
{
  SerdWorld* world = serd_world_new(NULL);

  // begin model-new
  SerdNodes* nodes = serd_nodes_new(NULL);
  SerdModel* model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  // end model-new

  // begin model-add-index
  serd_model_add_index(model, SERD_ORDER_PSO);
  // end model-add-index

  // begin model-copy
  SerdModel* copy = serd_model_copy(NULL, nodes, model);

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
  serd_model_insert(
    model,
    serd_nodes_id(nodes,
                  serd_a_uri(zix_string("http://example.org/thing"))), // S
    serd_nodes_id(nodes,
                  serd_a_uri(zix_string(
                    "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"))), // P
    serd_nodes_id(nodes, serd_a_string(zix_string("Thing"))),
    0 // G
  );
  // end model-add

  SerdModel* other_model = model;

  // begin model-insert
  SerdCursor* cursor = serd_model_begin(NULL, other_model);

  serd_model_insert_tuple(
    model, serd_cursor_tuple(cursor), serd_cursor_caret(cursor));

  serd_cursor_free(NULL, cursor);
  // end model-insert

  // begin model-insert-range
  SerdCursor* other_range = serd_model_begin(NULL, other_model);

  serd_model_insert_range(model, other_range);

  serd_cursor_free(NULL, other_range);
  // end model-insert-range

  // begin model-strings
  SerdStrings* strings = serd_strings_new(NULL, nodes);
  // end model-strings

  // begin model-begin-end
  SerdCursor* i = serd_model_begin(NULL, model);
  if (serd_cursor_equals(i, serd_model_end(model))) {
    printf("Model is empty\n");
  } else {
    const SerdNodeID s = serd_cursor_field(i, SERD_SUBJECT);
    printf("First statement subject: %s\n",
           serd_strings_token(strings, s).string.data);
  }
  // end model-begin-end

  // begin iter-next
  if (!serd_cursor_advance(i)) {
    const SerdStatementView s =
      serd_strings_statement(strings, serd_cursor_tuple(i));

    printf("Second statement subject: %s\n", s.subject.string.data);
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
    const SerdStatementView s =
      serd_strings_statement(strings, serd_cursor_tuple(i));

    printf("First statement subject: %s\n", s.subject.string.data);
  }

  if (!serd_cursor_advance(all)) {
    const SerdStatementView s =
      serd_strings_statement(strings, serd_cursor_tuple(i));

    printf("Second statement subject: %s\n", s.subject.string.data);
  }
  // end range-next

  serd_cursor_free(NULL, all);

  // begin model-ask
  const SerdNodeID rdf_type = serd_nodes_id(
    nodes,
    serd_a_uri(zix_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#type")));

  if (serd_model_ask(model, 0, rdf_type, 0, 0)) {
    printf("Model contains a type statement\n");
  }
  // end model-ask

  // Add a statement so that the searching examples below work
  serd_model_insert(
    model,
    serd_nodes_id(nodes, serd_a_uri(zix_string("http://example.org/i"))),
    rdf_type,
    serd_nodes_id(nodes, serd_a_uri(zix_string("http://example.org/T"))),
    0);

  // begin model-find
  SerdCursor* it = serd_model_find(NULL, model, 0, rdf_type, 0, 0);

  const SerdNodeID instance = serd_cursor_tuple(it).nodes[SERD_SUBJECT];
  // end model-find

  serd_cursor_free(NULL, it);

  // begin model-count
  size_t n = serd_model_count(model, instance, rdf_type, 0, 0);
  printf("Instance has %zu types\n", n);
  // end model-count

  // begin model-range
  SerdCursor* range = serd_model_find(NULL,
                                      model,
                                      instance, // Subject = instance
                                      rdf_type, // Predicate = rdf:type
                                      0,        // Object = anything
                                      0);       // Graph = anything

  for (; !serd_cursor_is_end(range); serd_cursor_advance(range)) {
    const SerdTokenView o =
      serd_strings_token(strings, serd_cursor_field(range, SERD_OBJECT));

    printf("Instance has type %s\n", o.string.data);
  }

  serd_cursor_free(NULL, range);
  // end model-range

  // begin model-find-node
  const SerdNodeID t = serd_model_find_node(model,
                                            instance, // Subject
                                            rdf_type, // Predicate
                                            0,        // Object
                                            0);       // Graph
  if (t) {
    printf("Instance has type %s\n",
           serd_strings_token(strings, t).string.data);
  }
  // end model-find-node

  // begin model-get-statement
  SerdCursor*       ts = serd_model_find(NULL, model, instance, rdf_type, 0, 0);
  SerdStatementView statement =
    serd_strings_statement(strings, serd_cursor_tuple(ts));

  if (statement.subject.type) {
    printf("Instance %s has type %s\n",
           statement.subject.string.data,
           statement.object.string.data);
  }

  serd_cursor_free(NULL, ts);
  // end model-get-statement

  // begin model-erase
  SerdCursor* some_type = serd_model_find(NULL, model, 0, rdf_type, 0, 0);
  serd_model_erase(model, some_type);
  serd_cursor_free(NULL, some_type);
  // end model-erase

  // begin model-erase-range
  SerdCursor* all_types = serd_model_find(NULL, model, 0, rdf_type, 0, 0);
  serd_model_erase_range(model, all_types);
  serd_cursor_free(NULL, all_types);
  // end model-erase-range

  serd_model_free(model);
  serd_strings_free(strings);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

static void
reading_writing(void)
{
  SerdWorld*   world   = serd_world_new(NULL);
  SerdNodes*   nodes   = serd_nodes_new(NULL);
  SerdStrings* strings = serd_strings_new(NULL, nodes);

  // begin env-new
  ZixStringView host    = zix_empty_string();
  ZixStringView path    = zix_string("/some/file.ttl");
  SerdNodeID    base_id = serd_nodes_id(nodes, serd_a_path(path, host));
  SerdTokenView base    = serd_strings_token(strings, base_id);
  SerdEnv*      env     = serd_env_new(NULL, base.string);
  // end env-new

  // begin env-set-prefix
  serd_env_set_prefix(
    env,
    zix_string("rdf"),
    zix_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#"));
  // end env-set-prefix

  // begin byte-sink-new
  SerdOutputStream out = serd_open_output_file("/tmp/eg.ttl");
  SerdInputStream  in  = serd_open_input_file(path.data);
  // end byte-sink-new

  // clang-format off

  // begin writer-new
  SerdWriter* writer = serd_writer_new(
    world,       // World
    SERD_TURTLE, // Syntax
    0,           // Writer flags
    env);        // Environment
  // end writer-new

  // begin reader-new
  SerdReader* reader = serd_reader_new(
    world,                     // World
    SERD_TURTLE,               // Syntax
    0,                         // Reader flags
    serd_writer_sink(writer)); // Target sink
  // end reader-new

  // clang-format on

  // begin reader-writer-start
  serd_writer_start(writer, &out, 4096);
  serd_reader_start(reader, &in, zix_empty_string(), 4096);
  // end reader-writer-start

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
  serd_close_input(&in);
  // end byte-sink-free

  // begin inserter-new
  SerdModel*      model    = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  SerdHandler*    inserter = serd_inserter_new(model, env, serd_no_token());
  const SerdSink* sink     = serd_handler_sink(inserter);
  // end inserter-new

  // begin model-reader-new
  SerdReader* const model_reader = serd_reader_new(world, SERD_TURTLE, 0, sink);

  st = serd_reader_read_document(model_reader);
  if (st) {
    printf("Error loading model: %s\n", serd_strerror(st));
  }
  // end model-reader-new

  serd_reader_free(model_reader);

  // begin write-range
  SerdCursor* i = serd_model_begin(NULL, model);
  serd_describe_range(NULL, i, serd_writer_sink(writer), 0);
  // end write-range

  serd_cursor_free(NULL, i);

  // begin canon-new
  SerdHandler* canon = serd_canon_new(world, env, sink, 0);
  // end canon-new

  const SerdTokenView rdf_type = {
    SERD_URI, zix_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#type")};

  // begin filter-new
  SerdHandler* filter = serd_filter_new(world,            // World
                                        env,              // Environment
                                        sink,             // Target
                                        serd_no_token(),  // Subject
                                        rdf_type,         // Predicate
                                        serd_no_object(), // Object
                                        serd_no_token(),  // Graph
                                        true);            // Inclusive
  // end filter-new

  serd_handler_free(filter);
  serd_handler_free(canon);
  serd_handler_free(inserter);
  serd_model_free(model);
  serd_env_free(env);
  serd_strings_free(strings);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

int
main(void)
{
  string_views();
  world();
  nodes();
  model();
  reading_writing();

  return 0;
}

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
