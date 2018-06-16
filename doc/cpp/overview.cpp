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
#include <cstddef>
#include <iostream>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-variable"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

using namespace serd;

static void
statements()
{
  // begin statement-new
  Statement triple{make_uri("http://example.org/drobilla"),  // Subject
                   make_uri("http://example.org/firstName"), // Predicate
                   make_string("David")};                    // Object
  // end statement-new

  // begin statement-new-graph
  Statement quad{make_uri("http://example.org/drobilla"),  // Subject
                 make_uri("http://example.org/firstName"), // Predicate
                 make_string("David"),                     // Object
                 make_uri("http://example.org/userData")}; // Graph
  // end statement-new-graph

  // begin statement-new-cursor
  Node      file{make_uri("file:///tmp/userdata.ttl")};
  Statement triple2{make_uri("http://example.org/drobilla"),  // Subject
                    make_uri("http://example.org/firstName"), // Predicate
                    make_string("David"),                     // Object
                    Cursor{file, 4, 27}};                     // Cursor
  // end statement-new-cursor

  // begin statement-new-graph-cursor
  Statement quad2{make_uri("http://example.org/drobilla"),  // Subject
                  make_uri("http://example.org/firstName"), // Predicate
                  make_string("David"),                     // Object
                  make_uri("http://example.org/userData"),  // Graph
                  Cursor{file, 4, 27}};                     // Cursor
  // end statement-new-graph-cursor
}

static void
statements_accessing_fields()
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
statements_comparison()
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

  const Statement& statement = statement1;

  // begin statement-matches
  if (statement.matches({}, make_uri("http://example.org/name"), {})) {
    std::cout << statement.subject() << " has name " << statement.object()
              << std::endl;
  }
  // end statement-matches
}

static void
world()
{
  // begin world-new
  World world;
  // end world-new

  // begin get-blank
  Node blank = world.get_blank();
  // end get-blank
}

static void
model()
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
  } else if (model.size() > 9000) {
    std::cout << "Model has over 9000 statements" << std::endl;
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
  model.insert(other_model.all());
  // end model-add-range

  // begin model-begin-end
  Iter i = model.begin();
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

  // begin model-iteration
  for (const StatementView& statement : model) {
    std::cout << "Model statement subject: " << statement.subject()
              << std::endl;
  }
  // end model-iteration

  // begin model-all
  Range all = model.all();
  // end model-all

  // begin model-ordered
  Range ordered = model.ordered(StatementOrder::OPS);
  // end model-ordered

  // begin range-iteration
  for (const StatementView& statement : all) {
    std::cout << "Range statement subject: " << statement.subject()
              << std::endl;
  }
  // end range-iteration

  // begin model-ask
  Node rdf_type = make_uri("http://www.w3.org/1999/02/22-rdf-syntax-ns#type");

  if (model.ask({}, rdf_type, {}, {})) {
    std::cout << "Model contains a type statement" << std::endl;
  }
  // end model-ask

  // Add a statement so that the searching examples below work
  Node inst{make_uri("http://example.org/i")};
  Node type{make_uri("http://example.org/T")};
  model.insert(inst, rdf_type, type);

  // begin model-find
  Iter it = model.find({}, rdf_type, {});

  NodeView instance = it->subject();
  // end model-find

  // begin model-count
  size_t n = model.count(instance, rdf_type, {});
  std::cout << "Instance has " << n << " types" << std::endl;
  // end model-count

  // begin model-range
  for (const StatementView& statement : model.range(instance, rdf_type, {})) {
    std::cout << "Instance has type " << statement.object() << std::endl;
  }
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
  Iter itype = model.find({}, rdf_type, {});
  model.erase(itype);
  // end model-erase

  // begin model-erase-range
  Range all_types = model.range({}, rdf_type, {});
  model.erase(all_types);
  // end model-erase-range
}

static void
reading_writing()
{
  World world;

  // begin env-new
  Node base = make_file_uri("/some/file.ttl");

  Env env{base};
  // end env-new

  // begin env-set-prefix
  env.set_prefix("rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
  // end env-set-prefix

  // begin byte-sink-new
  ByteSink out{"/tmp/eg.ttl", 4096};
  // end byte-sink-new

  // begin writer-new
  Writer writer{world, serd::Syntax::Turtle, {}, env, out};
  // end writer-new

  // begin reader-new
  Reader reader{world, Syntax::Turtle, {}, env, writer.sink(), 4096};
  // end reader-new

  // begin read-document
  Status st = reader.read_document();
  if (st != Status::success) {
    std::cout << "Error reading document: " << strerror(st) << std::endl;
  }
  // end read-document

  // begin byte-sink-close
  out.close();
  // end byte-sink-close

  // begin inserter-new
  Model                 model{world, ModelFlag::index_SPO};
  SinkWrapper<SerdSink> inserter = make_inserter(model);
  // end inserter-new

  // begin model-reader-new
  Reader model_reader{world, Syntax::Turtle, {}, env, inserter, 4096};

  st = model_reader.read_document();
  if (st != Status::success) {
    std::cout << "Error loading model: " << strerror(st) << std::endl;
  }
  // end model-reader-new

  // begin write-range
  model.all().write(writer.sink(), {});
  // end write-range

  // begin canon-new
  SinkWrapper<SerdSink> canon = make_canon(world, inserter, {});
  // end canon-new

  Node rdf_type = make_uri("http://www.w3.org/1999/02/22-rdf-syntax-ns#type");

  // begin filter-new
  SinkWrapper<SerdSink> filter = make_filter(inserter, // Target
                                             {},       // Subject
                                             rdf_type, // Predicate
                                             {},       // Object
                                             {},       // Graph
                                             true);    // Inclusive
  // end filter-new
}

int
main()
{
  statements();
  statements_accessing_fields();
  statements_comparison();
  world();
  model();
  reading_writing();

  return 0;
}

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
