// Copyright 2018-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "serd/serd.h"
#include "serd/serd.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream> // IWYU pragma: keep
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

template<class T>
static int
test_move_only(T&& obj)
{
  static_assert(!std::is_copy_constructible<T>::value, "");
  static_assert(!std::is_copy_assignable<T>::value, "");

  const auto* const ptr = obj.cobj();

  // Move construct
  T moved{std::forward<T>(obj)};
  assert(moved.cobj() == ptr);
  assert(!obj.cobj()); // NOLINT

  // Move assign
  obj = std::move(moved);
  assert(obj.cobj() == ptr);
  assert(!moved.cobj()); // NOLINT

  return 0;
}

template<class T>
static int
test_copy_move(const T& obj)
{
  T copy{obj};
  assert(copy == obj);

  T moved{std::move(copy)};
  assert(moved == obj);
  assert(copy != obj); // NOLINT

  T copy_assigned{obj};
  copy_assigned = obj;
  assert(copy_assigned == obj);

  T move_assigned{obj};
  move_assigned = std::move(copy_assigned);
  assert(move_assigned == obj);
  assert(copy_assigned != obj); // NOLINT

  return 0;
}

static int
test_operators()
{
  int st = 0;

  serd::World world;

  serd::Model model(
    world, serd::StatementOrder::SPO, serd::ModelFlag::store_carets);

  model.insert(serd::Statement{serd::make_uri("http://example.org/s"),
                               serd::make_uri("http://example.org/p"),
                               serd::make_uri("http://example.org/o"),
                               serd::Caret{serd::make_uri("test.ttl"), 1, 1}});

  serd::Sink sink{world};
  serd::Env  env{world};

  std::ostringstream stream;

  // st |= test_move_only(serd::World{});
  st |= test_copy_move(serd::Statement{*model.begin()});
  st |=
    test_copy_move(serd::Caret{serd::make_uri("http://example.org/doc"), 1, 2});
  st |= test_copy_move(model.begin()->caret());
  st |= test_copy_move(serd::Env{world});
  st |=
    test_move_only(serd::Reader{world, serd::Syntax::Turtle, {}, env, sink});
  st |= test_copy_move(model.begin());
  // st |= test_copy_move(model.all());
  // Sink
  st |= test_copy_move(model);
  // st |= test_move_only(serd::Inserter{model, env});
  // st |= test_move_only(serd::Sink{});

  st |= test_copy_move(serd::Env{world});

  return st;
}

template<class Value>
static int
test_optional(const Value& value, const Value& other)
{
  test_copy_move(value);

  // Truthiness
  assert(!serd::Optional<Value>());
  // assert(!serd::Optional<Value>(nullptr));
  assert(serd::Optional<Value>(value));

  // Comparison and general sanity
  serd::Optional<Value> optional{value};
  assert(optional);
  assert(optional == value);
  assert(optional != other);
  assert(*optional == value);
  assert(optional.cobj() != value.cobj()); // non-const, must be a copy

  // Reset
  optional.reset();
  assert(!optional);
  assert(!optional.cobj());

  // Copying and moving
  Value       nonconst = value;
  const auto* c_ptr    = nonconst.cobj();

  optional = nonconst;
  serd::Optional<Value> copied{optional};
  assert(copied == nonconst);
  assert(copied.cobj() != c_ptr);

  optional = std::move(nonconst);
  serd::Optional<Value> moved{std::move(optional)};
  assert(moved.cobj() == c_ptr);
  assert(!optional); // NOLINT

  serd::Optional<Value> copy_assigned;
  copy_assigned = optional;
  assert(copy_assigned == optional);
  assert(copy_assigned.cobj() != c_ptr);

  serd::Optional<Value> move_assigned;
  move_assigned = std::move(moved);
  assert(move_assigned.cobj() == c_ptr);
  assert(!optional);

  serd::Optional<Value> nullopt_assigned;
  nullopt_assigned = {};
  assert(!nullopt_assigned.cobj());

  return 0;
}

static int
test_optional()
{
  test_optional(serd::make_string("value"), serd::make_string("other"));

  {
    serd::World world;

    serd::Model value{world, serd::StatementOrder::SPO, {}};
    value.insert(serd::make_uri("http://example.org/s1"),
                 serd::make_uri("http://example.org/p1"),
                 serd::make_uri("http://example.org/o1"));

    serd::Model other(world, serd::StatementOrder::SPO, {});
    value.insert(serd::make_uri("http://example.org/s2"),
                 serd::make_uri("http://example.org/p2"),
                 serd::make_uri("http://example.org/o2"));

    test_optional(value, other);
  }

  return 0;
}

template<class T>
static int
test_node(const T& node)
{
  test_copy_move(node);

  if (node.datatype()) {
    return test_node(*node.datatype());
  }

  if (node.language()) {
    return test_node(*node.language());
  }

  return 0;
}

static int
test_string()
{
  assert(!strcmp(serd::strerror(serd::Status::unknown_error), "Unknown error"));

  return 0;
}

static int
test_stringview()
{
  const serd::StringView hello{"hello"};

  assert(hello.front() == 'h');
  assert(hello.back() == 'o');

  assert(*hello.begin() == 'h');
  assert(*hello.end() == '\0');
  assert(*(hello.end() - 1) == 'o');
  assert(*hello.cbegin() == 'h');
  assert(*hello.cend() == '\0');
  assert(*(hello.cend() - 1) == 'o');

  assert(hello[0] == 'h');
  assert(hello[1] == 'e');
  assert(hello.at(0) == 'h');
  assert(hello.at(1) == 'e');
  assert(hello.substr(2) == "llo");

  assert(hello.str() == "hello");
  assert(std::string(hello) == "hello");
  assert(!strcmp(static_cast<const char*>(hello), "hello"));

  std::stringstream ss;
  ss << hello;
  assert(ss.str() == "hello");

  bool threw = false;
  try {
    hello.at(6);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);

  try {
    hello.substr(6);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);

  assert(serd::StringView{} == serd::StringView{}); // NOLINT
  assert(hello == "hello");
  assert(hello == std::string{"hello"});
  assert(hello == serd::StringView{"hello"});

  assert(hello != "world");
  assert(hello != std::string{"world"});
  assert(hello != serd::StringView{"world"});

  assert(serd::StringView{"a"}.compare(serd::StringView{"ab"}) < 0);
  assert(serd::StringView{"ab"}.compare(serd::StringView{"a"}) > 0);
  assert(serd::StringView{"ab"}.compare(serd::StringView{"ab"}) == 0);

  assert(hello < serd::StringView{"world"});
  assert(hello < std::string{"world"});
  assert(hello < "world");

  assert(!(hello < serd::StringView{"apple"}));
  assert(!(hello < std::string{"apple"}));
  assert(!(hello < "apple"));

  return 0;
}

static int
test_syntax()
{
  assert(serd::syntax_by_name("Turtle") == serd::Syntax::Turtle);
  assert(serd::guess_syntax("foo.trig") == serd::Syntax::TriG);
  assert(!serd::syntax_has_graphs(serd::Syntax::NTriples));
  return 0;
}

static int
test_nodes()
{
  const auto type = serd::make_uri("http://example.org/Type");
  const auto base = serd::make_uri("http://example.org/");
  const auto root = serd::make_uri("http://example.org/");

  assert(base.type() == serd::NodeType::URI);
  assert(base.str() == "http://example.org/");
  assert(base.size() == strlen("http://example.org/"));
  assert(base == root);
  assert(base < type);
  assert(!base.empty());
  assert(std::count(base.begin(), base.end(), '/') == 3);

  const auto relative = serd::make_uri("rel/uri");
  // const auto resolved = relative.resolve(base);
  // assert(static_cast<std::string>(resolved) == "http://example.org/rel/uri");
  // assert(static_cast<serd::StringView>(resolved) ==
  //        "http://example.org/rel/uri");

  const auto string = serd::make_string("hello\n\"world\"");

  const auto number = serd::make_integer(42);
  assert(number.datatype() ==
         serd::make_uri("http://www.w3.org/2001/XMLSchema#integer"));

  const auto tagged = serd::make_plain_literal("hallo", "de");
  assert(tagged.language() == serd::make_string("de"));

  assert(!test_node(serd::make_string("hello")));
  assert(!test_node(serd::make_plain_literal("hello", "en")));
  assert(!test_node(serd::make_typed_literal("hello", serd::StringView(type))));
  assert(!test_node(serd::make_blank("blank")));
  assert(!test_node(serd::make_uri("http://example.org/thing")));
  assert(!test_node(serd::make_file_uri("/foo/bar", "host")));
  assert(!test_node(serd::make_file_uri("/foo/bar")));
  assert(!test_node(serd::make_file_uri("/foo/bar", "host")));
  assert(!test_node(serd::make_file_uri("/foo/bar")));
  assert(!test_node(serd::make_decimal(1.2)));
  assert(!test_node(serd::make_decimal(3.4)));
  assert(!test_node(serd::make_integer(56)));
  assert(!test_node(serd::make_base64("blob", 4)));

  assert(serd::get<bool>(serd::make(true)) == true);
  assert(serd::get<bool>(serd::make(false)) == false);
  assert(serd::get<double>(serd::make(1.5)) == 1.5);
  assert(serd::get<double>(serd::make(-2.5)) == -2.5);
  assert(serd::get<float>(serd::make(1.2f)) == 1.2f);
  assert(serd::get<float>(serd::make(-2.5f)) == -2.5f);
  assert(serd::get<int64_t>(serd::make(12)) == 12);
  assert(serd::get<int64_t>(serd::make(-34)) == -34);

  return 0;
}

static int
test_uri()
{
  const auto uri          = serd::make_uri("file:/path");
  const auto no_authority = serd::URI{uri.string_view()};
  assert(no_authority.scheme() == "file");
  assert(!no_authority.authority().data());
  assert(no_authority.path() == "/path");

  const auto empty_authority = serd::URI("file:///path");
  assert(empty_authority.scheme() == "file");
  assert(empty_authority.authority().data());
  assert(empty_authority.authority().empty());
  assert(empty_authority.path() == "/path");

  const auto base = serd::URI("http://example.org/base/");
  assert(base.scheme() == "http");
  assert(base.authority() == "example.org");
  assert(!base.path_prefix().data());
  assert(base.path() == "/base/");
  assert(!base.query().data());
  assert(!base.fragment().data());

  const auto rel = serd::URI("relative/path?query#fragment");
  assert(!rel.scheme().data());
  assert(!rel.authority().data());
  assert(!rel.path_prefix().data());
  assert(rel.path() == "relative/path");
  assert(rel.query() == "query");
  assert(rel.fragment() == "#fragment");

  const auto resolved = rel.resolve(base);
  assert(resolved.scheme() == "http");
  assert(resolved.authority() == "example.org");
  assert(resolved.path_prefix() == "/base/");
  assert(resolved.path() == "relative/path");
  assert(resolved.query() == "query");
  assert(resolved.fragment() == "#fragment");

  assert(resolved.string() ==
         "http://example.org/base/relative/path?query#fragment");
  std::cerr << resolved.relative_string(base) << std::endl;
  assert(resolved.relative_string(base) == "relative/path?query#fragment");

  const auto domain = serd::URI("http://example.org/");
  assert(domain.relative_string(resolved) == "../../");
  assert(domain.relative_string(resolved, base) == domain.string());

  auto local_file_uri = serd::parse_file_uri("file:///foo/%20bar");
  assert(local_file_uri == "/foo/ bar");

  auto hostname      = std::string();
  auto host_file_uri = serd::parse_file_uri("file://host/foo", &hostname);
  assert(hostname == "host");
  assert(host_file_uri == "/foo");

  assert(serd::uri_string_has_scheme("http://example.org/"));
  assert(!serd::uri_string_has_scheme("foo/bar"));

  std::ostringstream ss;
  ss << resolved;
  assert(ss.str() == "http://example.org/base/relative/path?query#fragment");

  return 0;
}

static int
test_reader()
{
  serd::World                world;
  serd::Optional<serd::Node> base_uri;
  serd::Optional<serd::Node> ns_name;
  serd::Optional<serd::Node> ns_uri;
  serd::Optional<serd::Node> ended_node;
  size_t                     n_statements{};
  std::stringstream          stream{};
  serd::Sink                 sink{world};

  sink.set_base_func([&](const serd::NodeView uri) {
    base_uri = serd::Node{uri};
    return serd::Status::success;
  });

  sink.set_prefix_func(
    [&](const serd::NodeView name, const serd::NodeView uri) {
      ns_name = serd::Node{name};
      ns_uri  = serd::Node{uri};
      return serd::Status::success;
    });

  sink.set_statement_func(
    [&](const serd::StatementFlags, const serd::StatementView& statement) {
      ++n_statements;
      stream << statement.subject() << " " << statement.predicate() << " "
             << statement.object() << std::endl;
      return serd::Status::success;
    });

  sink.set_end_func([&](const serd::NodeView node) {
    ended_node = serd::Node{node};
    return serd::Status::success;
  });

  // FIXME
#if 0
  serd::World  world;
  serd::Env    env;
  serd::Reader reader(
    world, serd::Syntax::Turtle, serd::ReaderFlag::global, env, sink, 4096);

  const std::string input("@base <http://example.org/base> ."
                          "@prefix eg: <http://example.org/> ."
                          "eg:s eg:p [ eg:p2 eg:o2 ] .");

  // Read from string
  serd::InputStream string_source = serd::open_string(input);
  reader.start(string_source);
  reader.read_document();

  assert(n_statements == 2);
  assert(stream.str() == "http://example.org/s http://example.org/p b1\n"
                         "b1 http://example.org/p2 http://example.org/o2\n");

  assert(base_uri == serd::make_uri("http://example.org/base"));
  assert(ns_name == serd::make_string("eg"));
  assert(ns_uri == serd::make_uri("http://example.org/"));

  // Read from C++ stream
  std::stringstream ss("eg:s eg:p eg:o3 , _:blank .");
  serd::ByteSource  byte_source(ss);
  stream.str("");
  reader.start(byte_source);
  assert(reader.read_chunk() == serd::Status::success);
  assert(reader.read_chunk() != serd::Status::success);

  assert(n_statements == 4);
  assert(stream.str() ==
         "http://example.org/s http://example.org/p http://example.org/o3\n"
         "http://example.org/s http://example.org/p blank\n");

  assert(reader.finish() == serd::Status::success);
#endif

  return 0;
}

static serd::Status
write_test_doc(serd::Writer& writer)
{
  const auto& sink = writer.sink();

  const auto blank = serd::make_blank("b1");
  sink.base(serd::make_uri("http://drobilla.net/base/"));
  sink.prefix(serd::make_string("eg"), serd::make_uri("http://example.org/"));
  sink.write(serd::StatementFlag::anon_O,
             serd::make_uri("http://drobilla.net/base/s"),
             serd::make_uri("http://example.org/p"),
             blank);
  sink.statement({},
                 serd::Statement(blank,
                                 serd::make_uri("http://example.org/p2"),
                                 serd::make_uri("http://drobilla.net/o")));
  sink.end(blank);

  return writer.finish();
}

static const char* const writer_test_doc =
  "@base <http://drobilla.net/base/> .\n"
  "@prefix eg: <http://example.org/> .\n"
  "\n"
  "<http://drobilla.net/base/s>\n"
  "\t<http://example.org/p> [\n"
  "\t\t<http://example.org/p2> <http://drobilla.net/o>\n"
  "\t] .\n";

static int
test_writer_ostream()
{
  serd::World world;
  serd::Env   env{world};

  {
    std::ostringstream stream;
    serd::OutputStream out{serd::open_output_stream(stream)};
    serd::Writer       writer(world, serd::Syntax::Turtle, {}, env, out);

    write_test_doc(writer);
    assert(stream.str() == writer_test_doc);
  }

  {
    std::ofstream bad_file("/does/not/exist");
    bad_file.clear();
    bad_file.exceptions(std::ofstream::badbit);

    serd::OutputStream bad_file_out{serd::open_output_stream(bad_file)};
    serd::Writer writer(world, serd::Syntax::Turtle, {}, env, bad_file_out);

    const serd::Status st =
      writer.sink().base(serd::make_uri("http://drobilla.net/base/"));

    assert(st == serd::Status::bad_write);
  }

  return 0;
}

static int
test_writer_string_sink()
{
// FIXME
#if 0
  serd::World world;
  serd::Env   env;
  std::string output;

  serd::ByteSink byte_sink{[&output](const char* str, size_t len) {
    output += str;
    return len;
  }};

  serd::Writer writer(world, serd::Syntax::Turtle, {}, env, byte_sink);

  write_test_doc(writer);
  assert(output == writer_test_doc);
#endif

  return 0;
}

static int
test_env()
{
  serd::World world;
  serd::Env   env{world, serd::make_uri("http://example.org/")};
  assert(env.base_uri() == serd::make_uri("http://example.org/"));

  env = serd::Env{world};

  const auto base = serd::make_uri("http://drobilla.net/");
  env.set_base_uri(base.string_view());
  assert(env.base_uri() == base);

  env.set_prefix("eg", "http://drobilla.net/");
  env.set_prefix("eg", "http://example.org/");

  assert(env.expand(serd::make_uri("foo")) ==
         serd::make_uri("http://drobilla.net/foo"));

  serd::Env copied{env};
  assert(copied.cobj() != env.cobj());
  assert(copied.expand(serd::make_uri("foo")) ==
         serd::make_uri("http://drobilla.net/foo"));

  serd::Env assigned{world};
  assigned = env;
  assert(assigned.cobj() != env.cobj());
  assert(assigned.expand(serd::make_uri("foo")) ==
         serd::make_uri("http://drobilla.net/foo"));

  serd::Sink                 sink{world};
  serd::Optional<serd::Node> ns_name;
  serd::Optional<serd::Node> ns_uri;

  sink.set_prefix_func([&](serd::NodeView name, serd::NodeView uri) {
    ns_name = serd::Node{name};
    ns_uri  = serd::Node{uri};
    return serd::Status::success;
  });

  env.describe(sink);
  assert(ns_name == serd::make_string("eg"));
  assert(ns_uri == serd::make_uri("http://example.org/"));

  return 0;
}

static int
test_statement()
{
  const auto s   = serd::make_uri("http://example.org/s");
  const auto p   = serd::make_uri("http://example.org/p");
  const auto o   = serd::make_uri("http://example.org/o");
  const auto g   = serd::make_uri("http://example.org/g");
  const auto cur = serd::Caret{serd::make_string("test"), 42, 53};

  const auto t_statement = serd::Statement{s, p, o};

  assert(t_statement.subject() == s);
  assert(t_statement.predicate() == p);
  assert(t_statement.object() == o);
  assert(!t_statement.graph());
  assert(!t_statement.caret());

  const auto q_statement = serd::Statement{s, p, o, g, cur};
  assert(q_statement.subject() == s);
  assert(q_statement.predicate() == p);
  assert(q_statement.object() == o);
  assert(q_statement.graph() == g);
  assert(q_statement.caret() == cur);

  assert(q_statement.node(serd::Field::subject) == s);
  assert(q_statement.node(serd::Field::predicate) == p);
  assert(q_statement.node(serd::Field::object) == o);
  assert(q_statement.node(serd::Field::graph) == g);

  return 0;
}

static int
test_model()
{
  serd::World world;
  serd::Model model(world, serd::StatementOrder::SPO, {});

  model.add_index(serd::StatementOrder::OPS);

  assert(model.empty());

  const auto s  = serd::make_uri("http://example.org/s");
  const auto p  = serd::make_uri("http://example.org/p");
  const auto o1 = serd::make_uri("http://example.org/o1");
  const auto o2 = serd::make_uri("http://example.org/o2");

  serd::NodeView b = world.get_blank();
  // auto           r = b.resolve(s);

  model.insert(s, p, o1);
  model.insert(serd::Statement{s, p, o2});

  assert(!model.empty());
  assert(model.size() == 2);
  assert(model.ask(s, p, o1));
  assert(model.count(s, p, o1) == 1);
  assert(!model.ask(s, p, s));

  size_t total_count = 0;
  for (const auto& statement : model) {
    assert(statement.subject() == s);
    assert(statement.predicate() == p);
    assert(statement.object() == o1 || statement.object() == o2);
    ++total_count;
  }
  assert(total_count == 2);

  size_t o1_count = 0;
  for (const auto& statement : model.find({}, {}, o1)) {
    assert(statement.cobj());
    assert(statement.subject() == s);
    assert(statement.predicate() == p);
    assert(statement.object() == o1);
    ++o1_count;
  }
  assert(o1_count == 1);

  size_t o2_count = 0;
  for (const auto& statement : model.find({}, {}, o2)) {
    assert(statement.subject() == s);
    assert(statement.predicate() == p);
    assert(statement.object() == o2);
    ++o2_count;
  }
  assert(o2_count == 1);

  assert(model.get({}, p, o1) == s);

  const auto statement = model.get_statement(s, p, {});
  assert(statement);
  assert(statement->subject() == s);
  assert(statement->predicate() == p);
  assert(statement->object() == o1);

  const auto range = model.find(s, p, {});
  assert(range.begin()->subject() == s);
  assert(range.begin()->predicate() == p);
  assert(range.begin()->object() == o1);

  serd::Model copy(model);
  assert(copy == model);

  copy.insert(s, p, s);
  assert(copy != model);

  return 0;
}

static int
test_log()
{
  serd::World world;
  bool        called = false;
  world.set_message_func([&called](const serd::LogLevel   level,
                                   const serd::LogFields& fields,
                                   const std::string&     msg) {
    assert(fields.at("TEST_EXTRA") == "extra field");
    assert(level == serd::LogLevel::error);
    assert(msg == "bad argument to something: 42\n");
    called = true;
    return serd::Status::success;
  });

  const auto success = world.log(serd::LogLevel::error,
                                 {{"TEST_EXTRA", "extra field"}},
                                 "bad argument to %s: %d\n",
                                 "something",
                                 42);

  assert(called);
  assert(success == serd::Status::success);

  world.set_message_func([](const serd::LogLevel,
                            const serd::LogFields&,
                            const std::string&) -> serd::Status {
    throw std::runtime_error("error");
  });

  const auto failure = world.log(serd::LogLevel::error, {}, "failure");
  assert(failure == serd::Status::unknown_error);

  return 0;
}

int
main()
{
  using TestFunc = int (*)();

  constexpr std::array<TestFunc, 14> tests{{test_operators,
                                            test_optional,
                                            test_nodes,
                                            test_string,
                                            test_stringview,
                                            test_syntax,
                                            test_uri,
                                            test_env,
                                            test_reader,
                                            test_writer_ostream,
                                            test_writer_string_sink,
                                            test_statement,
                                            test_model,
                                            test_log}};

  int failed = 0;
  for (const auto& test : tests) {
    failed += test();
  }

  std::cerr << "Failed " << failed << " tests" << std::endl;

  return failed;
}
