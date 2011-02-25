/*
  Copyright 2011 David Robillard <http://drobilla.net>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serd_internal.h"

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

typedef struct {
	SerdNode graph;
	SerdNode subject;
	SerdNode predicate;
} WriteContext;

static const WriteContext WRITE_CONTEXT_NULL = {
	{ 0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}
};

struct SerdWriterImpl {
	SerdSyntax   syntax;
	SerdStyle    style;
	SerdEnv      env;
	SerdURI      base_uri;
	SerdStack    anon_stack;
	SerdSink     sink;
	void*        stream;
	WriteContext context;
	unsigned     indent;
};

typedef enum {
	WRITE_NORMAL,
	WRITE_URI,
	WRITE_STRING
} TextContext;

static inline WriteContext*
anon_stack_top(SerdWriter writer)
{
	assert(!serd_stack_is_empty(&writer->anon_stack));
	return (WriteContext*)(writer->anon_stack.buf
	                       + writer->anon_stack.size - sizeof(WriteContext));
}

static bool
write_text(SerdWriter writer, TextContext ctx,
           const uint8_t* utf8, size_t n_bytes, uint8_t terminator)
{
	char escape[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (size_t i = 0; i < n_bytes;) {
		uint8_t in = utf8[i++];
		switch (in) {
		case '\\': writer->sink("\\\\", 2, writer->stream); continue;
		case '\n': writer->sink("\\n", 2, writer->stream);  continue;
		case '\r': writer->sink("\\r", 2, writer->stream);  continue;
		case '\t': writer->sink("\\t", 2, writer->stream);  continue;
		case '"':
			if (terminator == '"') {
				writer->sink("\\\"", 2, writer->stream);
				continue;
			}  // else fall-through
		default: break;
		}

		if (in == terminator) {
			snprintf(escape, 7, "\\u%04X", terminator);
			writer->sink(escape, 6, writer->stream);
			continue;
		}

		uint32_t c    = 0;
		size_t   size = 0;
		if ((in & 0x80) == 0) {  // Starts with `0'
			size = 1;
			c = in & 0x7F;
			if (in_range(in, 0x20, 0x7E)) {  // Printable ASCII
				writer->sink(&in, 1, writer->stream);
				continue;
			}
		} else if ((in & 0xE0) == 0xC0) {  // Starts with `110'
			size = 2;
			c = in & 0x1F;
		} else if ((in & 0xF0) == 0xE0) {  // Starts with `1110'
			size = 3;
			c = in & 0x0F;
		} else if ((in & 0xF8) == 0xF0) {  // Starts with `11110'
			size = 4;
			c = in & 0x07;
		} else {
			fprintf(stderr, "invalid UTF-8 at offset %zu: %X\n", i, in);
			return false;
		}

		if (ctx == WRITE_STRING && !(writer->style & SERD_STYLE_ASCII)) {
			// Write UTF-8 character directly to UTF-8 output
			// TODO: Scan to next escape and write entire range at once
			writer->sink(utf8 + i - 1, size, writer->stream);
			i += size - 1;
			continue;
		}

#define READ_BYTE() do { \
			assert(i < n_bytes); \
			in = utf8[i++] & 0x3f; \
			c <<= 6; \
			c |= in; \
		} while (0)

		switch (size) {
		case 4: READ_BYTE();
		case 3: READ_BYTE();
		case 2: READ_BYTE();
		}

		if (c < 0xFFFF) {
			snprintf(escape, 7, "\\u%04X", c);
			writer->sink(escape, 6, writer->stream);
		} else {
			snprintf(escape, 11, "\\U%08X", c);
			writer->sink(escape, 10, writer->stream);
		}
	}
	return true;
}

static void
serd_writer_write_delim(SerdWriter writer, const uint8_t delim)
{
	switch (delim) {
	case '\n':
		break;
	default:
		writer->sink(" ", 1, writer->stream);
	case '[':
		writer->sink(&delim, 1, writer->stream);
	}
	writer->sink("\n", 1, writer->stream);
	for (unsigned i = 0; i < writer->indent; ++i) {
		writer->sink("\t", 1, writer->stream);
	}
}

static bool
write_node(SerdWriter      writer,
           const SerdNode* node,
           const SerdNode* datatype,
           const SerdNode* lang)
{
	SerdChunk uri_prefix;
	SerdChunk uri_suffix;
	switch (node->type) {
	case SERD_NOTHING:
		return false;
	case SERD_ANON_BEGIN:
		if (writer->syntax != SERD_NTRIPLES) {
			++writer->indent;
			serd_writer_write_delim(writer, '[');
			WriteContext* ctx = (WriteContext*)serd_stack_push(
				&writer->anon_stack, sizeof(WriteContext));
			*ctx = writer->context;
			writer->context.subject   = *node;
			writer->context.predicate = SERD_NODE_NULL;
			break;
		}
	case SERD_ANON:
		if (writer->syntax != SERD_NTRIPLES) {
			break;
		}  // else fall through
	case SERD_BLANK_ID:
		writer->sink("_:", 2, writer->stream);
		writer->sink(node->buf, node->n_bytes - 1, writer->stream);
		break;
	case SERD_CURIE:
		switch (writer->syntax) {
		case SERD_NTRIPLES:
			if (!serd_env_expand(writer->env, node, &uri_prefix, &uri_suffix)) {
				fprintf(stderr, "error: undefined namespace prefix `%s'\n", node->buf);
				return false;
			}
			writer->sink("<", 1, writer->stream);
			write_text(writer, WRITE_URI, uri_prefix.buf, uri_prefix.len, '>');
			write_text(writer, WRITE_URI, uri_suffix.buf, uri_suffix.len, '>');
			writer->sink(">", 1, writer->stream);
			break;
		case SERD_TURTLE:
			writer->sink(node->buf, node->n_bytes - 1, writer->stream);
		}
		break;
	case SERD_LITERAL:
		if (writer->syntax == SERD_TURTLE && datatype && datatype->buf) {
			// TODO: compare against NS_XSD prefix once
			if (!strcmp((const char*)datatype->buf,    NS_XSD "boolean")
			    || !strcmp((const char*)datatype->buf, NS_XSD "decimal")
			    || !strcmp((const char*)datatype->buf, NS_XSD "integer")) {
				writer->sink(node->buf, node->n_bytes - 1, writer->stream);
				break;
			}
		}
		writer->sink("\"", 1, writer->stream);
		write_text(writer, WRITE_STRING, node->buf, node->n_bytes - 1, '"');
		writer->sink("\"", 1, writer->stream);
		if (lang && lang->buf) {
			writer->sink("@", 1, writer->stream);
			writer->sink(lang->buf, lang->n_bytes - 1, writer->stream);
		} else if (datatype && datatype->buf) {
			writer->sink("^^", 2, writer->stream);
			write_node(writer, datatype, NULL, NULL);
		}
		break;
	case SERD_URI:
		if ((writer->syntax == SERD_TURTLE)
		    && !strcmp((const char*)node->buf, NS_RDF "type")) {
			writer->sink("a", 1, writer->stream);
			return true;
		} else if ((writer->style & SERD_STYLE_CURIED)
		           && serd_uri_string_has_scheme(node->buf)) {
			SerdNode  prefix;
			SerdChunk suffix;
			if (serd_env_qualify(writer->env, node, &prefix, &suffix)) {
				write_text(writer, WRITE_URI, prefix.buf, prefix.n_bytes - 1, '>');
				writer->sink(":", 1, writer->stream);
				write_text(writer, WRITE_URI, suffix.buf, suffix.len, '>');
				return true;
			}
		} else if ((writer->style & SERD_STYLE_RESOLVED)
		           && !serd_uri_string_has_scheme(node->buf)) {
			SerdURI uri;
			if (serd_uri_parse(node->buf, &uri)) {
				SerdURI abs_uri;
				serd_uri_resolve(&uri, &writer->base_uri, &abs_uri);
				writer->sink("<", 1, writer->stream);
				serd_uri_serialise(&abs_uri, writer->sink, writer->stream);
				writer->sink(">", 1, writer->stream);
				return true;
			}
		}
		writer->sink("<", 1, writer->stream);
		write_text(writer, WRITE_URI, node->buf, node->n_bytes - 1, '>');
		writer->sink(">", 1, writer->stream);
		return true;
	}
	return true;
}

SERD_API
bool
serd_writer_write_statement(SerdWriter      writer,
                            const SerdNode* graph,
                            const SerdNode* subject,
                            const SerdNode* predicate,
                            const SerdNode* object,
                            const SerdNode* object_datatype,
                            const SerdNode* object_lang)
{
	assert(subject && predicate && object);
	switch (writer->syntax) {
	case SERD_NTRIPLES:
		write_node(writer, subject, NULL, NULL);
		writer->sink(" ", 1, writer->stream);
		write_node(writer, predicate, NULL, NULL);
		writer->sink(" ", 1, writer->stream);
		if (!write_node(writer, object, object_datatype, object_lang)) {
			return false;
		}
		writer->sink(" .\n", 3, writer->stream);
		return true;
	case SERD_TURTLE:
		break;
	}
	if (subject->buf == writer->context.subject.buf) {
		if (predicate->buf == writer->context.predicate.buf) {  // Abbreviate S P
			++writer->indent;
			serd_writer_write_delim(writer, ',');
			write_node(writer, object, object_datatype, object_lang);
			--writer->indent;
		} else {  // Abbreviate S
			if (writer->context.predicate.buf) {
				serd_writer_write_delim(writer, ';');
			} else {
				++writer->indent;
				serd_writer_write_delim(writer, '\n');
			}
			write_node(writer, predicate, NULL, NULL);
			writer->context.predicate = *predicate;
			writer->sink(" ", 1, writer->stream);
			write_node(writer, object, object_datatype, object_lang);
		}
	} else {
		if (writer->context.subject.buf) {
			if (writer->indent > 0) {
				--writer->indent;
			}
			if (serd_stack_is_empty(&writer->anon_stack)) {
				serd_writer_write_delim(writer, '.');
				serd_writer_write_delim(writer, '\n');
			}
		}

		if (subject->type == SERD_ANON_BEGIN) {
			writer->sink("[ ", 2, writer->stream);
			++writer->indent;
			WriteContext* ctx = (WriteContext*)serd_stack_push(
				&writer->anon_stack, sizeof(WriteContext));
			*ctx = writer->context;
		} else {
			write_node(writer, subject, NULL, NULL);
			++writer->indent;
			if (subject->type != SERD_ANON_BEGIN && subject->type != SERD_ANON) {
				serd_writer_write_delim(writer, '\n');
			}
		}

		writer->context.subject   = *subject;
		writer->context.predicate = SERD_NODE_NULL;

		write_node(writer, predicate, NULL, NULL);
		writer->context.predicate = *predicate;
		writer->sink(" ", 1, writer->stream);

		write_node(writer, object, object_datatype, object_lang);
	}

	const WriteContext new_context = { graph ? *graph : SERD_NODE_NULL,
	                                   *subject,
	                                   *predicate };
	writer->context = new_context;
	return true;
}

SERD_API
bool
serd_writer_end_anon(SerdWriter      writer,
                     const SerdNode* node)
{
	if (writer->syntax == SERD_NTRIPLES) {
		return true;
	}
	if (serd_stack_is_empty(&writer->anon_stack)) {
		fprintf(stderr, "unexpected end of anonymous node\n");
		return false;
	}
	assert(writer->indent > 0);
	--writer->indent;
	serd_writer_write_delim(writer, '\n');
	writer->sink("]", 1, writer->stream);
	writer->context = *anon_stack_top(writer);
	serd_stack_pop(&writer->anon_stack, sizeof(WriteContext));
	if (!writer->context.subject.buf) {  // End of anonymous subject
		writer->context.subject = *node;
	}
	return true;
}

SERD_API
void
serd_writer_finish(SerdWriter writer)
{
	if (writer->context.subject.buf) {
		writer->sink(" .\n", 3, writer->stream);
		writer->context.subject.buf = NULL;
	}
}

SERD_API
SerdWriter
serd_writer_new(SerdSyntax     syntax,
                SerdStyle      style,
                SerdEnv        env,
                const SerdURI* base_uri,
                SerdSink       sink,
                void*          stream)
{
	const WriteContext context = WRITE_CONTEXT_NULL;
	SerdWriter         writer  = malloc(sizeof(struct SerdWriterImpl));
	writer->syntax     = syntax;
	writer->style      = style;
	writer->env        = env;
	writer->base_uri   = base_uri ? *base_uri : SERD_URI_NULL;
	writer->anon_stack = serd_stack_new(sizeof(WriteContext));
	writer->sink       = sink;
	writer->stream     = stream;
	writer->context    = context;
	writer->indent     = 0;
	return writer;
}

SERD_API
void
serd_writer_set_base_uri(SerdWriter     writer,
                         const SerdURI* uri)
{
	writer->base_uri = *uri;
	if (writer->syntax != SERD_NTRIPLES) {
		if (writer->context.graph.buf || writer->context.subject.buf) {
			writer->sink(" .\n\n", 4, writer->stream);
			writer->context = WRITE_CONTEXT_NULL;
		}
		writer->sink("@base <", 7, writer->stream);
		serd_uri_serialise(uri, writer->sink, writer->stream);
		writer->sink("> .\n", 4, writer->stream);
	}
	writer->context = WRITE_CONTEXT_NULL;
}

SERD_API
bool
serd_writer_set_prefix(SerdWriter      writer,
                       const SerdNode* name,
                       const SerdNode* uri)
{
	if (writer->syntax != SERD_NTRIPLES) {
		if (writer->context.graph.buf || writer->context.subject.buf) {
			writer->sink(" .\n\n", 4, writer->stream);
			writer->context = WRITE_CONTEXT_NULL;
		}
		writer->sink("@prefix ", 8, writer->stream);
		writer->sink(name->buf, name->n_bytes - 1, writer->stream);
		writer->sink(": <", 3, writer->stream);
		write_text(writer, WRITE_URI, uri->buf, uri->n_bytes - 1, '>');
		writer->sink("> .\n", 4, writer->stream);
	}
	writer->context = WRITE_CONTEXT_NULL;
	return true;
}

SERD_API
void
serd_writer_free(SerdWriter writer)
{
	SerdWriter const me = (SerdWriter)writer;
	serd_writer_finish(me);
	serd_stack_free(&writer->anon_stack);
	free(me);
}
