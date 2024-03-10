// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "reader.h"

#include "byte_source.h"
#include "namespaces.h"
#include "node.h"
#include "node_impl.h"
#include "stack.h"
#include "system.h"
#include "world.h"

#include "serd/stream.h"
#include "serd/uri.h"
#include "zix/string_view.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SerdStatus
serd_reader_prepare(SerdReader* reader);

SerdStatus
r_err(SerdReader* const reader, const SerdStatus st, const char* const fmt, ...)
{
  va_list args; // NOLINT(cppcoreguidelines-init-variables)
  va_start(args, fmt);
  const SerdError e = {st, &reader->source.caret, fmt, &args};
  serd_world_error(reader->world, &e);
  va_end(args);
  return st;
}

void
set_blank_id(SerdReader* const reader,
             SerdNode* const   node,
             const size_t      buf_size)
{
  char* const       buf = (char*)(node + 1);
  const char* const prefix =
    reader->bprefix ? (const char*)reader->bprefix : "";

  node->length =
    (size_t)snprintf(buf, buf_size, "%sb%u", prefix, reader->next_id++);
}

size_t
genid_length(const SerdReader* const reader)
{
  return reader->bprefix_len + 10U; // + "b" + UINT32_MAX
}

bool
tolerate_status(const SerdReader* const reader, const SerdStatus status)
{
  if (status == SERD_SUCCESS || status == SERD_FAILURE) {
    return true;
  }

  if (status == SERD_BAD_STREAM || status == SERD_BAD_STACK ||
      status == SERD_BAD_WRITE || status == SERD_NO_DATA) {
    return false;
  }

  return !reader->strict;
}

SerdNode*
blank_id(SerdReader* const reader)
{
  SerdNode* const ref =
    push_node_padded(reader, genid_length(reader), SERD_BLANK, "", 0);

  if (ref) {
    set_blank_id(reader, ref, genid_length(reader) + 1U);
  }

  return ref;
}

SerdNode*
push_node_padded(SerdReader* const  reader,
                 const size_t       max_length,
                 const SerdNodeType type,
                 const char* const  str,
                 const size_t       length)
{
  // Push a null byte to ensure the previous node was null terminated
  char* terminator = (char*)serd_stack_push(&reader->stack, 1);
  if (!terminator) {
    return NULL;
  }
  *terminator = 0;

  void* mem = serd_stack_push_aligned(
    &reader->stack, sizeof(SerdNode) + max_length + 1U, sizeof(void*));

  if (!mem) {
    return NULL;
  }

  SerdNode* const node = (SerdNode*)mem;

  node->meta   = NULL;
  node->length = length;
  node->flags  = 0;
  node->type   = type;

  char* buf = (char*)(node + 1);
  memcpy(buf, str, length + 1);

  return node;
}

SerdNode*
push_node(SerdReader* const  reader,
          const SerdNodeType type,
          const char* const  str,
          const size_t       length)
{
  return push_node_padded(reader, length, type, str, length);
}

SerdStatus
push_node_termination(SerdReader* const reader)
{
  const size_t top = reader->stack.size;
  const size_t pad = serd_node_pad_length(top) - top;

  return serd_stack_push(&reader->stack, pad) ? SERD_SUCCESS : SERD_BAD_STACK;
}

SerdStatus
emit_statement(SerdReader* const reader,
               const ReadContext ctx,
               SerdNode* const   o)
{
  // Push termination for the top node (object, language, or datatype)
  // (Earlier nodes have been terminated by subsequent node pushed)
  push_node_termination(reader);

  const SerdStatus st = serd_sink_write(
    reader->sink, *ctx.flags, ctx.subject, ctx.predicate, o, ctx.graph);

  *ctx.flags = 0;
  return st;
}

SerdStatus
serd_reader_read_document(SerdReader* const reader)
{
  assert(reader);

  if (reader->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_SUCCESS;
  }

  if (!reader->source.prepared) {
    SerdStatus st = serd_reader_prepare(reader);
    if (st) {
      return st;
    }
  }

  return ((reader->syntax == SERD_NQUADS) ? read_nquadsDoc(reader)
                                          : read_turtleTrigDoc(reader));
}

SerdReader*
serd_reader_new(SerdWorld* const      world,
                const SerdSyntax      syntax,
                const SerdReaderFlags flags,
                const SerdSink* const sink)
{
  assert(world);
  assert(sink);

  const size_t stack_size = world->limits.reader_stack_size;
  if (stack_size < sizeof(void*) + (3U * (sizeof(SerdNode))) + 168) {
    return NULL;
  }

  SerdReader* me = (SerdReader*)calloc(1, sizeof(SerdReader));

  me->world   = world;
  me->sink    = sink;
  me->stack   = serd_stack_new(stack_size);
  me->syntax  = syntax;
  me->next_id = 1;
  me->strict  = !(flags & SERD_READ_LAX);

  // Reserve a bit of space at the end of the stack to zero pad nodes
  me->stack.buf_size -= sizeof(void*);

  me->rdf_first = push_node(me, SERD_URI, NS_RDF "first", 48);
  me->rdf_rest  = push_node(me, SERD_URI, NS_RDF "rest", 47);
  me->rdf_nil   = push_node(me, SERD_URI, NS_RDF "nil", 46);

  // The initial stack size check should cover this
  assert(me->rdf_first);
  assert(me->rdf_rest);
  assert(me->rdf_nil);

  return me;
}

void
serd_reader_free(SerdReader* const reader)
{
  if (!reader) {
    return;
  }

  serd_reader_finish(reader);

  free(reader->stack.buf);
  free(reader->bprefix);
  free(reader);
}

void
serd_reader_add_blank_prefix(SerdReader* const reader, const char* const prefix)
{
  assert(reader);

  free(reader->bprefix);
  reader->bprefix_len = 0;
  reader->bprefix     = NULL;

  const size_t prefix_len = prefix ? strlen(prefix) : 0;
  if (prefix_len) {
    reader->bprefix_len = prefix_len;
    reader->bprefix     = (char*)malloc(reader->bprefix_len + 1);
    memcpy(reader->bprefix, prefix, reader->bprefix_len + 1);
  }
}

static SerdStatus
skip_bom(SerdReader* const me)
{
  if (serd_byte_source_peek(&me->source) == 0xEF) {
    if (serd_byte_source_advance(&me->source) ||
        serd_byte_source_peek(&me->source) != 0xBB ||
        serd_byte_source_advance(&me->source) ||
        serd_byte_source_peek(&me->source) != 0xBF ||
        serd_byte_source_advance(&me->source)) {
      r_err(me, SERD_BAD_SYNTAX, "corrupt byte order mark");
      return SERD_BAD_SYNTAX;
    }
  }

  return SERD_SUCCESS;
}

SerdStatus
serd_reader_start_stream(SerdReader* const     reader,
                         const SerdReadFunc    read_func,
                         const SerdErrorFunc   error_func,
                         void* const           stream,
                         const SerdNode* const name,
                         const size_t          page_size)
{
  assert(reader);
  assert(read_func);
  assert(error_func);

  return serd_byte_source_open_source(
    &reader->source, read_func, error_func, NULL, stream, name, page_size);
}

SerdStatus
serd_reader_start_file(SerdReader* reader, const char* uri, bool bulk)
{
  assert(reader);
  assert(uri);

  char* const path = serd_parse_file_uri(uri, NULL);
  if (!path) {
    return SERD_BAD_ARG;
  }

  FILE* fd = serd_world_fopen(reader->world, path, "rb");
  free(path);
  if (!fd) {
    return SERD_BAD_STREAM;
  }

  SerdNode* const  name = serd_new_uri(zix_string(uri));
  const SerdStatus st   = serd_byte_source_open_source(
    &reader->source,
    bulk ? (SerdReadFunc)fread : serd_file_read_byte,
    (SerdErrorFunc)ferror,
    (SerdCloseFunc)fclose,
    fd,
    name,
    bulk ? SERD_PAGE_SIZE : 1U);

  serd_node_free(name);
  return st;
}

SerdStatus
serd_reader_start_string(SerdReader* const     reader,
                         const char* const     utf8,
                         const SerdNode* const name)
{
  assert(reader);
  assert(utf8);
  return serd_byte_source_open_string(&reader->source, utf8, name);
}

static SerdStatus
serd_reader_prepare(SerdReader* const reader)
{
  SerdStatus st = serd_byte_source_prepare(&reader->source);
  if (st == SERD_SUCCESS) {
    st = skip_bom(reader);
  } else if (st == SERD_FAILURE) {
    reader->source.eof = true;
  } else {
    r_err(reader, st, "error preparing read: %s", strerror(errno));
  }
  return st;
}

SerdStatus
serd_reader_read_chunk(SerdReader* const reader)
{
  assert(reader);

  if (reader->syntax == SERD_SYNTAX_EMPTY) {
    return SERD_FAILURE;
  }

  SerdStatus st = SERD_SUCCESS;
  if (!reader->source.prepared) {
    st = serd_reader_prepare(reader);
  } else if (reader->source.eof) {
    st = serd_byte_source_advance(&reader->source);
  }

  if (peek_byte(reader) == 0) {
    // Skip leading null byte, for reading from a null-delimited socket
    st = skip_byte(reader, 0);
  }

  return st                                ? st
         : (reader->syntax == SERD_NQUADS) ? read_nquads_statement(reader)
                                           : read_n3_statement(reader);
}

SerdStatus
serd_reader_finish(SerdReader* const reader)
{
  assert(reader);

  return serd_byte_source_close(&reader->source);
}
