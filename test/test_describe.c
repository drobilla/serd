// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "failing_allocator.h"

#include <serd/buffer.h>
#include <serd/caret_view.h>
#include <serd/cursor.h>
#include <serd/describe.h>
#include <serd/env.h>
#include <serd/event.h>
#include <serd/model.h>
#include <serd/model_caret.h>
#include <serd/node_args.h>
#include <serd/node_id.h>
#include <serd/nodes.h>
#include <serd/output_stream.h>
#include <serd/sink.h>
#include <serd/status.h>
#include <serd/stream_result.h>
#include <serd/syntax.h>
#include <serd/tuple.h>
#include <serd/world.h>
#include <serd/writer.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

typedef struct {
  SerdNodeID rdf_first;
  SerdNodeID rdf_nil;
  SerdNodeID rdf_rest;
  SerdNodeID s;
  SerdNodeID p;
  SerdNodeID o;
  SerdNodeID l1;
  SerdNodeID l2;
  SerdNodeID one;
  SerdNodeID two;
  SerdNodeID a;
  SerdNodeID b;
} TestIDs;

static TestIDs
make_ids(SerdNodes* const nodes)
{
  const TestIDs ctx = {
    serd_nodes_id(nodes, serd_a_uri(zix_string(NS_RDF "first"))),
    serd_nodes_id(nodes, serd_a_uri(zix_string(NS_RDF "nil"))),
    serd_nodes_id(nodes, serd_a_uri(zix_string(NS_RDF "rest"))),
    serd_nodes_id(nodes, serd_a_uri(zix_string("urn:s"))),
    serd_nodes_id(nodes, serd_a_uri(zix_string("urn:p"))),
    serd_nodes_id(nodes, serd_a_uri(zix_string("urn:o"))),
    serd_nodes_id(nodes, serd_a_blank(zix_string("l1"))),
    serd_nodes_id(nodes, serd_a_blank(zix_string("l2"))),
    serd_nodes_id(nodes, serd_a_integer(1)),
    serd_nodes_id(nodes, serd_a_integer(2)),
    serd_nodes_id(nodes, serd_a_string(zix_string("a"))),
    serd_nodes_id(nodes, serd_a_string(zix_string("b"))),
  };

  return ctx;
}

static SerdStatus
check_describe(SerdWorld* const       world,
               const SerdModel* const model,
               SerdOutputStream       out)
{
  ZixAllocator* const allocator = serd_world_allocator(world);

  SerdEnv* const env = serd_env_new(allocator, zix_empty_string());
  if (!env) {
    return SERD_BAD_ALLOC;
  }

  SerdWriter* const writer = serd_writer_new(world, SERD_TURTLE, 0, env);
  if (!writer) {
    serd_env_free(env);
    return SERD_BAD_ALLOC;
  }

  assert(!serd_writer_start(writer, &out, 1U));

  SerdStatus st = serd_env_set_prefix(
    env,
    zix_string("rdf"),
    zix_string("http://www.w3.org/1999/02/22-rdf-syntax-ns#"));

  if (!st) {
    const SerdSink* const sink = serd_writer_sink(writer);
    SerdCursor* const     all  = serd_model_begin(NULL, model);

    st = serd_describe_range(allocator, all, sink, 0);

    serd_cursor_free(NULL, all);
  }

  serd_writer_free(writer);
  serd_env_free(env);
  return st;
}

static void
check_describe_output(SerdWorld* const       world,
                      const SerdModel* const model,
                      const char* const      expected)
{
  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream out    = serd_open_output_buffer(&buffer);
  assert(!check_describe(world, model, out));
  serd_close_output(&out);

  const char* const str = (const char*)buffer.buf;
  assert(str);
  assert(!strcmp(str, expected));

  zix_free(buffer.allocator, buffer.buf);
}

static void
test_list_subject_failed_alloc(void)
{
  SerdFailingAllocator allocator = serd_failing_allocator();

  SerdWorld* const world = serd_world_new(&allocator.base);
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_GSPO, SERD_MODEL_GRAPHS);
  // FIXME: ORDER_SPO vs ORDER_GSPO

  const TestIDs ids = make_ids(nodes);

  assert(!serd_model_add_index(model, SERD_ORDER_OPS));

  assert(!serd_model_insert(model, ids.l1, ids.rdf_first, ids.a, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.rdf_rest, ids.rdf_nil, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.p, ids.o, 0U));
  serd_failing_allocator_reset(&allocator, SIZE_MAX);

  static const char* const expected = "(\n"
                                      "	\"a\"\n"
                                      ")\n"
                                      "	<urn:p> <urn:o> .\n";

  // Successfully describe model to count allocations
  check_describe_output(world, model, expected);
  const size_t n_allocs = serd_failing_allocator_reset(&allocator, SIZE_MAX);

  SerdBuffer       buffer = {NULL, NULL, 0};
  SerdOutputStream out    = serd_open_output_buffer(&buffer);
  serd_failing_allocator_reset(&allocator, 0U);
  for (size_t i = 0; i < n_allocs; ++i) {
    serd_failing_allocator_reset(&allocator, i);
    assert(check_describe(world, model, out) == SERD_BAD_ALLOC);
  }
  zix_free(buffer.allocator, buffer.buf);

  serd_model_free(model);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

static void
test_bad_list(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

  const TestIDs ids = make_ids(nodes);

  assert(!serd_model_add_index(model, SERD_ORDER_OPS));

  const SerdNodeID nofirst =
    serd_nodes_id(nodes, serd_a_blank(zix_string("nof")));

  const SerdNodeID norest =
    serd_nodes_id(nodes, serd_a_blank(zix_string("nor")));

  // List where second node has no rdf:first
  assert(!serd_model_insert(model, ids.s, ids.p, ids.l1, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.rdf_first, ids.a, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.rdf_rest, nofirst, 0U));

  // List where second node has no rdf:rest
  assert(!serd_model_insert(model, ids.s, ids.p, ids.l2, 0U));
  assert(!serd_model_insert(model, ids.l2, ids.rdf_first, ids.a, 0U));
  assert(!serd_model_insert(model, ids.l2, ids.rdf_rest, norest, 0U));
  assert(!serd_model_insert(model, norest, ids.rdf_first, ids.b, 0U));

  static const char* const expected = "<urn:s>\n"
                                      "	<urn:p> (\n"
                                      "		\"a\"\n"
                                      "	) , (\n"
                                      "		\"a\"\n"
                                      "		\"b\"\n"
                                      "	) .\n";

  check_describe_output(world, model, expected);

  serd_model_free(model);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

static void
test_infinite_list(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_GRAPHS);

  const TestIDs ids = make_ids(nodes);

  assert(!serd_model_add_index(model, SERD_ORDER_OPS));

  // List with a cycle: l1 -> l2 -> l1 -> l2 ...
  assert(!serd_model_insert(model, ids.s, ids.p, ids.l1, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.rdf_first, ids.a, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.rdf_rest, ids.l2, 0U));
  assert(!serd_model_insert(model, ids.l2, ids.rdf_first, ids.b, 0U));
  assert(!serd_model_insert(model, ids.l2, ids.rdf_rest, ids.l1, 0U));

  static const char* const expected = "<urn:s>\n"
                                      "	<urn:p> _:l1 .\n"
                                      "\n"
                                      "_:l1\n"
                                      "	rdf:first \"a\" ;\n"
                                      "	rdf:rest [\n"
                                      "		rdf:first \"b\" ;\n"
                                      "		rdf:rest _:l1\n"
                                      "	] .\n";

  check_describe_output(world, model, expected);
  serd_model_free(model);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

typedef struct {
  size_t n_written;
  size_t max_successes;
} FailingWriteFuncState;

/// Write function that fails after a certain number of writes
static SerdStreamResult
failing_write(void* const stream, const size_t len, const void* const buf)
{
  (void)buf;

  FailingWriteFuncState* const state = (FailingWriteFuncState*)stream;
  SerdStreamResult             r     = {SERD_SUCCESS, len};

  if (++state->n_written > state->max_successes) {
    r.status = SERD_BAD_WRITE;
    r.count  = 0U;
  }

  return r;
}

static void
test_error_in_list_subject(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  const TestIDs    ids   = make_ids(nodes);

  assert(!serd_model_add_index(model, SERD_ORDER_OPS));

  assert(!serd_model_insert(model, ids.l1, ids.rdf_first, ids.one, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.rdf_rest, ids.l2, 0U));
  assert(!serd_model_insert(model, ids.l2, ids.rdf_first, ids.two, 0U));
  assert(!serd_model_insert(model, ids.l2, ids.rdf_rest, ids.rdf_nil, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.p, ids.o, 0U));

  for (size_t max_successes = 0; max_successes < 18; ++max_successes) {
    FailingWriteFuncState state = {0, max_successes};
    SerdOutputStream out = serd_open_output_stream(failing_write, NULL, &state);

    assert(check_describe(world, model, out) == SERD_BAD_WRITE);
    serd_close_output(&out);
  }

  serd_model_free(model);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

static void
test_error_in_list_object(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdNodes* const nodes = serd_nodes_new(NULL);
  SerdModel* const model = serd_model_new(world, nodes, SERD_ORDER_SPO, 0U);
  const TestIDs    ids   = make_ids(nodes);

  assert(!serd_model_add_index(model, SERD_ORDER_OPS));

  assert(!serd_model_insert(model, ids.s, ids.p, ids.l1, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.rdf_first, ids.one, 0U));
  assert(!serd_model_insert(model, ids.l1, ids.rdf_rest, ids.l2, 0U));
  assert(!serd_model_insert(model, ids.l2, ids.rdf_first, ids.two, 0U));
  assert(!serd_model_insert(model, ids.l2, ids.rdf_rest, ids.rdf_nil, 0U));

  for (size_t max_successes = 0; max_successes < 21; ++max_successes) {
    FailingWriteFuncState state = {0, max_successes};
    SerdOutputStream out = serd_open_output_stream(failing_write, NULL, &state);

    assert(check_describe(world, model, out) == SERD_BAD_WRITE);
    serd_close_output(&out);
  }

  serd_model_free(model);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

static SerdStatus
caret_event_handler(void* const handle, const SerdEvent* const event)
{
  SerdCaretView* const caret = (SerdCaretView*)handle;

  *caret = event->caret;
  return SERD_SUCCESS;
}

static void
test_cursor(void)
{
  static const SerdCaretView caret = {ZIX_STATIC_STRING("doc"), 42U, 43U};

  SerdWorld* const world = serd_world_new(NULL);
  SerdNodes* const nodes = serd_nodes_new(NULL);
  assert(nodes);

  SerdModel* const model =
    serd_model_new(world, nodes, SERD_ORDER_SPO, SERD_MODEL_CARETS);
  assert(model);

  const SerdNodeID s =
    serd_nodes_id(nodes, serd_a_uri(zix_string("http://example.org/s")));
  const SerdNodeID p =
    serd_nodes_id(nodes, serd_a_uri(zix_string("http://example.org/p")));
  const SerdNodeID o =
    serd_nodes_id(nodes, serd_a_uri(zix_string("http://example.org/o")));
  const SerdNodeID doc = serd_nodes_id(nodes, serd_a_uri(zix_string("doc")));

  assert(!serd_model_add_index(model, SERD_ORDER_OPS));

  const SerdTuple      tuple       = {{s, p, o}};
  const SerdModelCaret model_caret = {doc, 42U, 43U};
  assert(!serd_model_insert_tuple(model, tuple, model_caret));

  SerdCursor* const cur       = serd_model_begin(NULL, model);
  SerdCaretView     out_caret = {zix_empty_string(), 0U, 0U};
  const SerdSink    sink      = {&out_caret, caret_event_handler};

  // Emitted events have no caret by default
  assert(!serd_describe_range(NULL, cur, &sink, 0U));
  assert(zix_string_view_equals(out_caret.document, zix_empty_string()));
  assert(out_caret.line == 0U);
  assert(out_caret.column == 0U);

  // Emitted events have a caret if SERD_DESCRIBE_CARET is given
  assert(!serd_describe_range(NULL, cur, &sink, SERD_DESCRIBE_CARET));
  assert(zix_string_view_equals(out_caret.document, caret.document));
  assert(out_caret.line == caret.line);
  assert(out_caret.column == caret.column);

  serd_cursor_free(NULL, cur);
  serd_model_free(model);
  serd_nodes_free(nodes);
  serd_world_free(world);
}

int
main(void)
{
  test_list_subject_failed_alloc();
  test_bad_list();
  test_infinite_list();
  test_error_in_list_subject();
  test_error_in_list_object();
  test_cursor();
  return 0;
}
