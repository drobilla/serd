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

/*
  Example code that is included in the documentation.  Code in the
  documentation is included from here rather than written inline so that it can
  be tested and avoid rotting.  The code here doesn't make much sense, but is
  written such that it at least compiles and will run without crashing.
*/

#include "serd/serd.hpp"

#include <cassert>
#include <iostream>

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

using namespace serd;

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
  Node subject{make_curie("eg:drobilla")};
  Node predicate{make_curie("eg:firstName")};
  Node object{make_curie("David")};

  Statement statement{subject, predicate, object};
  // end statement-new
}

static void
statements_accessing_fields(void)
{
  Node ss{make_uri("http://example.org/s")};
  Node sp{make_uri("http://example.org/p")};
  Node so{make_uri("http://example.org/o")};

  Statement statement{ss, sp, so};

  // begin get-subject
  NodeView s = statement.node(Field::subject);
  // end get-subject

  // begin get-pog
  NodeView           p = statement.predicate();
  NodeView           o = statement.object();
  Optional<NodeView> g = statement.graph();
  // end get-pog

  // begin get-cursor
  Optional<CursorView> c = statement.cursor();
  // end get-cursor
}

static void
statements_comparison(void)
{
  Node ss{make_uri("http://example.org/s")};
  Node sp{make_uri("http://example.org/p")};
  Node so{make_uri("http://example.org/o")};

  Statement statement1{ss, sp, so};
  Statement statement2{ss, sp, so};

  // begin statement-equals
  if (statement1 == statement2) {
    std::cout << "Match" << std::endl;
  }
  // end statement-equals

  Statement statement = statement1;

  // begin statement-matches
  if (statement.matches({}, make_uri("http://example.org/name"), {})) {
    std::cout << statement.subject() << " has name " << statement.object()
              << std::endl;
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
  World world;
  // end world-new

  // begin get-blank
  Node blank = world.get_blank();
  // end get-blank
}

static void
model(void)
{
  World world;

  // begin model-new
  Model model{world, ModelFlag::index_SPO};
  // end model-new

  // begin fancy-model-new
  Model fancy_model{
    world,
    (ModelFlag::index_SPO | ModelFlag::index_PSO | ModelFlag::store_cursors)};
  // end fancy-model-new

  // begin model-copy
  Model copy{model};
  assert(copy == model);

  copy = fancy_model;
  assert(copy == fancy_model);
  // end model-copy

  // begin model-size
  if (model.empty()) {
    std::cout << "Model is empty" << std::endl;
  } else if (model.size() > 1000) {
    std::cout << "Model has over 1000 statements" << std::endl;
  }
  // end model-size

  // begin model-add
  Node s{make_uri("http://example.org/thing")};
  Node p{make_uri("http://example.org/name")};
  Node o{make_string("Thing")};

  model.insert(s, p, o);
  // end model-add

  Model other_model{model};

  // begin model-insert
  model.insert(*other_model.begin());
  // end model-insert

  // begin model-add-range
  // FIXME: document consuming/move semantics
  model.insert(other_model.all());
  // end model-add-range

  // begin model-begin-end
  Model::iterator i = model.begin();
  if (i == model.end()) {
    std::cout << "Model is empty" << std::endl;
  } else {
    std::cout << "First statement subject: " << i->subject() << std::endl;
  }
  // end model-begin-end

  // begin iter-next
  if (++i != model.end()) {
    std::cout << "Second statement subject: " << i->subject() << std::endl;
  }
  // end iter-next

  // begin model-all
  Range all = model.all();
  // end model-all

  // begin range-next
  if (all.empty()) {
    std::cout << "Model is empty" << std::endl;
  } else {
    // FIXME
    // const SerdStatement* s = serd_range_front(all);

    // printf("First statement subject: %s\n",
    //        serd_node_string(serd_statement_subject(s)));
  }

  // FIXME
  // if (!serd_range_next(all)) {
  //   const SerdStatement* s = serd_range_front(all);

  //   printf("Second statement subject: %s\n",
  //          serd_node_string(serd_statement_subject(s)));
  // }
  // end range-next

  // begin model-ask
  Node rdf_type{make_uri("http://www.w3.org/1999/02/22-rdf-syntax-ns#type")};

  if (model.ask({}, rdf_type, {}, {})) {
    std::cout << "Model contains a type statement" << std::endl;
  }
  // end model-ask

  // Add a statement so that the searching examples below work
  Node inst{make_uri("http://example.org/i")};
  Node type{make_uri("http://example.org/T")};
  model.insert(inst, rdf_type, type);

  // begin model-find
  Model::iterator it = model.find({}, rdf_type, {});

  NodeView instance = it->subject();
  // end model-find

  // begin model-count
  size_t n = model.count(instance, rdf_type, {});
  std::cout << "Instance has " << n << " types" << std::endl;
  // end model-count

  // begin model-range
  Range range = model.range(instance, rdf_type, {});

  // FIXME
  // for (; !range.empty(); serd_range_empty(range); serd_range_next(range)) {
  //   const SerdStatement* s = serd_range_front(range);

  //   printf("Instance has type %s\n",
  //          serd_node_string(serd_statement_object(s)));
  // }

  // serd_range_free(range);
  // end model-range

  // begin model-get
  Optional<NodeView> t = model.get(instance, rdf_type, {});
  if (t) {
    std::cout << "Instance has type " << *t << std::endl;
  }
  // end model-get

  // begin model-get-statement
  Optional<StatementView> ts = model.get_statement(instance, rdf_type, {});
  if (ts) {
    std::cout << "Instance " << ts->subject() << " has type " << ts->object()
              << std::endl;
  }
  // end model-get-statement

  // begin model-erase
  Model::iterator some_type = model.find({}, rdf_type, {});
  // FIXME
  // model.erase(some_type);
  // end model-erase

  // begin model-erase-range
  Range all_types = model.range({}, rdf_type, {});
  // FIXME
  // serd_model_erase_range(model, all_types);
  // serd_range_free(all_types);
  // end model-erase-range
}

static void
reading_writing(void)
{
  World world;

  // begin env-new
  StringView host     = {};
  StringView out_path = "/some/file.ttl";
  Node       base     = make_file_uri(out_path, host);

  Env env{base};
  // end env-new

  // begin env-set-prefix
  env.set_prefix("rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
  // end env-set-prefix

  // begin byte-sink-new
  // FIXME
  // SerdByteSink* out = serd_byte_sink_new_filename("/tmp/eg.ttl", 4096);
  // end byte-sink-new

#if 0
  // begin writer-new
  Writer writer{world, serd::Syntax::Turtle, 0, env, out};
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
    std::cout << "Error reading document: " << strerror(st) << std::endl;
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
    std::cout << "Error loading model: " << strerror(st) << std::endl;
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
#endif
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
