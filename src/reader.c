/*
  Copyright 2011-2017 David Robillard <http://drobilla.net>

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

#include "reader.h"

#include "byte_source.h"
#include "node.h"
#include "serd/serd.h"
#include "serd_internal.h"
#include "sink.h"
#include "stack.h"
#include "statement.h"
#include "system.h"
#include "world.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SerdStatus serd_reader_prepare(SerdReader* reader);

SerdStatus
r_err(SerdReader* reader, SerdStatus st, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const SerdError e = { st, &reader->source.cur, fmt, &args };
	serd_world_error(reader->world, &e);
	va_end(args);
	return st;
}

void
set_blank_id(SerdReader* reader, SerdNode* node, size_t buf_size)
{
	char*       buf    = (char*)(node + 1);
	const char* prefix = reader->bprefix ? (const char*)reader->bprefix : "";

	node->n_bytes =
		(size_t)snprintf(buf, buf_size, "%sb%u", prefix, reader->next_id++);
}

size_t
genid_size(SerdReader* reader)
{
	return reader->bprefix_len + 1 + 10 + 1;  // + "b" + UINT32_MAX + \0
}

SerdNode*
blank_id(SerdReader* reader)
{
	SerdNode* ref = push_node_padded(
		reader, genid_size(reader), SERD_BLANK, "", 0);
	if (ref) {
		set_blank_id(reader, ref, genid_size(reader));
	}
	return ref;
}

SerdNode*
push_node_padded(SerdReader* reader, size_t maxlen,
                 SerdNodeType type, const char* str, size_t n_bytes)
{
	// Push a null byte to ensure the previous node was null terminated
	char* terminator = (char*)serd_stack_push(&reader->stack, 1);
	if (!terminator) {
		return NULL;
	}
	*terminator = 0;

	void* mem = serd_stack_push_aligned(
		&reader->stack, sizeof(SerdNode) + maxlen + 1, sizeof(SerdNode));

	if (!mem) {
		reader->status = SERD_ERR_OVERFLOW;
		return NULL;
	}

	SerdNode* const node = (SerdNode*)mem;
	node->n_bytes = n_bytes;
	node->flags   = 0;
	node->type    = type;

	char* buf = (char*)(node + 1);
	memcpy(buf, str, n_bytes + 1);

#ifdef SERD_STACK_CHECK
	reader->allocs = realloc(
		reader->allocs, sizeof(reader->allocs) * (++reader->n_allocs));
	reader->allocs[reader->n_allocs - 1] = node;
#endif
	return node;
}

SerdNode*
push_node(SerdReader*  reader,
          SerdNodeType type,
          const char*  str,
          size_t       n_bytes)
{
	return push_node_padded(reader, n_bytes, type, str, n_bytes);
}

SerdStatus
emit_statement(SerdReader* reader, ReadContext ctx, SerdNode* o)
{
	SerdNode* graph = ctx.graph;
	if (!reader->sink->statement) {
		return SERD_SUCCESS;
	}

	/* Zero the pad of the object node on the top of the stack.  Lower nodes
	   (subject and predicate) were already zeroed by subsequent pushes. */
	serd_node_zero_pad(o);

	const SerdStatement statement = {
		{ ctx.subject, ctx.predicate, o, graph },
		&reader->source.cur
	};

	const SerdStatus st = reader->sink->statement(
		reader->sink->handle, *ctx.flags, &statement);
	*ctx.flags = 0;
	return st;
}

static SerdStatus
read_statement(SerdReader* reader)
{
	switch (reader->syntax) {
	default: return read_n3_statement(reader);
	}
}

SerdStatus
serd_reader_read_document(SerdReader* reader)
{
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
serd_reader_new(SerdWorld*      world,
                SerdSyntax      syntax,
                const SerdSink* sink,
                size_t          stack_size)
{
	SerdReader* me = (SerdReader*)calloc(1, sizeof(SerdReader));

	me->world         = world;
	me->sink          = sink;
	me->stack         = serd_stack_new(stack_size);
	me->syntax        = syntax;
	me->next_id       = 1;
	me->strict        = true;

	me->rdf_first = push_node(me, SERD_URI, NS_RDF "first", 48);
	me->rdf_rest  = push_node(me, SERD_URI, NS_RDF "rest", 47);
	me->rdf_nil   = push_node(me, SERD_URI, NS_RDF "nil", 46);

	return me;
}

void
serd_reader_set_strict(SerdReader* reader, bool strict)
{
	reader->strict = strict;
}

void
serd_reader_free(SerdReader* reader)
{
	serd_reader_finish(reader);

#ifdef SERD_STACK_CHECK
	free(reader->allocs);
#endif
	free(reader->stack.buf);
	free(reader->bprefix);
	free(reader);
}

void
serd_reader_add_blank_prefix(SerdReader* reader,
                             const char* prefix)
{
	free(reader->bprefix);
	reader->bprefix_len = 0;
	reader->bprefix     = NULL;
	if (prefix) {
		reader->bprefix_len = strlen(prefix);
		reader->bprefix     = (char*)malloc(reader->bprefix_len + 1);
		memcpy(reader->bprefix, prefix, reader->bprefix_len + 1);
	}
}

static SerdStatus
skip_bom(SerdReader* me)
{
	if (serd_byte_source_peek(&me->source) == 0xEF) {
		serd_byte_source_advance(&me->source);
		if (serd_byte_source_peek(&me->source) != 0xBB ||
		    serd_byte_source_advance(&me->source) ||
		    serd_byte_source_peek(&me->source) != 0xBF ||
		    serd_byte_source_advance(&me->source)) {
			r_err(me, SERD_ERR_BAD_SYNTAX, "corrupt byte order mark\n");
			return SERD_ERR_BAD_SYNTAX;
		}
	}

	return SERD_SUCCESS;
}

SerdStatus
serd_reader_start_stream(SerdReader*         reader,
                         SerdReadFunc        read_func,
                         SerdStreamErrorFunc error_func,
                         void*               stream,
                         const SerdNode*     name,
                         size_t              page_size)
{
	return serd_byte_source_open_source(
		&reader->source, read_func, error_func, NULL, stream, name, page_size);
}

SerdStatus
serd_reader_start_file(SerdReader* reader, const char* uri, bool bulk)
{
	char* const path = serd_file_uri_parse(uri, NULL);
	if (!path) {
		return SERD_ERR_BAD_ARG;
	}

	FILE* fd = serd_world_fopen(reader->world, path, "rb");
	free(path);
	if (!fd) {
		return SERD_ERR_UNKNOWN;
	}

	SerdNode* const name = serd_new_uri(uri);
	const SerdStatus st = serd_byte_source_open_source(
		&reader->source,
		bulk ? (SerdReadFunc)fread : serd_file_read_byte,
		(SerdStreamErrorFunc)ferror,
		(SerdStreamCloseFunc)fclose,
		fd,
		name,
		bulk ? SERD_PAGE_SIZE : 1);
	serd_node_free(name);
	return st;
}

SerdStatus
serd_reader_start_string(SerdReader*     reader,
                         const char*     utf8,
                         const SerdNode* name)
{
	return serd_byte_source_open_string(&reader->source, utf8, name);
}

static SerdStatus
serd_reader_prepare(SerdReader* reader)
{
	reader->status = serd_byte_source_prepare(&reader->source);
	if (reader->status == SERD_SUCCESS) {
		reader->status = skip_bom(reader);
	} else if (reader->status == SERD_FAILURE) {
		reader->source.eof = true;
	} else {
		r_err(reader, reader->status, "read error: %s\n", strerror(errno));
	}
	return reader->status;
}

SerdStatus
serd_reader_read_chunk(SerdReader* reader)
{
	SerdStatus st = SERD_SUCCESS;
	if (!reader->source.prepared) {
		st = serd_reader_prepare(reader);
	} else if (reader->source.eof) {
		st = serd_byte_source_advance(&reader->source);
	}

	if (peek_byte(reader) == 0) {
		// Skip leading null byte, for reading from a null-delimited socket
		eat_byte_safe(reader, 0);
	}

	return st ? st : read_statement(reader);
}

SerdStatus
serd_reader_finish(SerdReader* reader)
{
	return serd_byte_source_close(&reader->source);
}
