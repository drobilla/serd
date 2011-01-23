/* Serd, an RDF serialisation library.
 * Copyright 2011 David Robillard <d@drobilla.net>
 *
 * Serd is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Serd is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "serd/serd.h"
#include "serd_stack.h"

typedef struct {
	const SerdString* graph;
	const SerdString* subject;
	const SerdString* predicate;
} WriteContext;

static const WriteContext WRITE_CONTEXT_NULL = { 0, 0, 0 };

typedef bool (*StatementWriter)(SerdWriter        writer,
                                const SerdString* graph,
                                const SerdString* subject,
                                SerdNodeType      subject_type,
                                const SerdString* predicate,
                                SerdNodeType      predicate_type,
                                const SerdString* object,
                                SerdNodeType      object_type,
                                const SerdString* object_datatype,
                                const SerdString* object_lang);

typedef bool (*NodeWriter)(SerdWriter        writer,
                           SerdNodeType      type,
                           const SerdString* str,
                           const SerdString* datatype,
                           const SerdString* lang);

struct SerdWriterImpl {
	SerdSyntax        syntax;
	SerdStyle         style;
	SerdEnv           env;
	SerdURI           base_uri;
	SerdStack         anon_stack;
	SerdSink          sink;
	void*             stream;
	StatementWriter   write_statement;
	NodeWriter        write_node;
	WriteContext      context;
	unsigned          indent;
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
		case '\n': writer->sink("\\n", 2, writer->stream); continue;
		case '\r': writer->sink("\\r", 2, writer->stream); continue;
		case '\t': writer->sink("\\t", 2, writer->stream); continue;
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
			if ((in >= 0x20) && (in <= 0x7E)) {  // Printable ASCII
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
		} else if ((in & 0xFC) == 0xF8) {  // Starts with `111110'
			size = 5;
			c = in & 0x03;
		} else if ((in & 0xFE) == 0xFC) {  // Starts with `1111110'
			size = 6;
			c = in & 0x01;
		} else {
			fprintf(stderr, "invalid UTF-8 at offset %zu: %X\n", i, in);
			return false;
		}

		if (ctx == WRITE_STRING && !(writer->style & SERD_STYLE_ASCII)) {
			// Write UTF-8 character directly to UTF-8 output
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
		case 6: READ_BYTE();
		case 5: READ_BYTE();
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
	case 0:
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
write_node(SerdWriter        writer,
           SerdNodeType      type,
           const SerdString* str,
           const SerdString* datatype,
           const SerdString* lang)
{
	SerdChunk uri_prefix;
	SerdChunk uri_suffix;
	switch (type) {
	case SERD_ANON_BEGIN:
		if (writer->syntax != SERD_NTRIPLES) {
			++writer->indent;
			serd_writer_write_delim(writer, '[');
			WriteContext* ctx = (WriteContext*)serd_stack_push(
				&writer->anon_stack, sizeof(WriteContext));
			*ctx = writer->context;
			writer->context.subject   = str;
			writer->context.predicate = 0;
			break;
		}
	case SERD_ANON:
		if (writer->syntax != SERD_NTRIPLES) {
			break;
		} // else fall through
	case SERD_BLANK_ID:
		writer->sink("_:", 2, writer->stream);
		writer->sink(str->buf, str->n_bytes - 1, writer->stream);
		break;
	case SERD_CURIE:
		switch (writer->syntax) {
		case SERD_NTRIPLES:
			if (!serd_env_expand(writer->env, str, &uri_prefix, &uri_suffix)) {
				fprintf(stderr, "error: undefined namespace prefix `%s'\n", str->buf);
				return false;
			}
			writer->sink("<", 1, writer->stream);
			write_text(writer, WRITE_URI, uri_prefix.buf, uri_prefix.len, '>');
			write_text(writer, WRITE_URI, uri_suffix.buf, uri_suffix.len, '>');
			writer->sink(">", 1, writer->stream);
			break;
		case SERD_TURTLE:
			writer->sink(str->buf, str->n_bytes - 1, writer->stream);
		}
		break;
	case SERD_LITERAL:
		writer->sink("\"", 1, writer->stream);
		write_text(writer, WRITE_STRING, str->buf, str->n_bytes - 1, '"');
		writer->sink("\"", 1, writer->stream);
		if (lang) {
			writer->sink("@", 1, writer->stream);
			writer->sink(lang->buf, lang->n_bytes - 1, writer->stream);
		} else if (datatype) {
			writer->sink("^^", 2, writer->stream);
			write_node(writer, SERD_URI, datatype, NULL, NULL);
		}
		break;
	case SERD_URI:
		if (!serd_uri_string_has_scheme(str->buf)) {
			SerdURI uri;
			if (serd_uri_parse(str->buf, &uri)) {
				SerdURI abs_uri;
				if (serd_uri_resolve(&uri, &writer->base_uri, &abs_uri)) {
					writer->sink("<", 1, writer->stream);
					serd_uri_serialise(&abs_uri, writer->sink, writer->stream);
					writer->sink(">", 1, writer->stream);
					return true;
				}
			}
		} else {
			writer->sink("<", 1, writer->stream);
			write_text(writer, WRITE_URI, str->buf, str->n_bytes - 1, '>');
			writer->sink(">", 1, writer->stream);
			return true;
		}
		return false;
	}
	return true;
}

SERD_API
bool
serd_writer_write_statement(SerdWriter        writer,
                            const SerdString* graph,
                            const SerdString* subject,
                            SerdNodeType      subject_type,
                            const SerdString* predicate,
                            SerdNodeType      predicate_type,
                            const SerdString* object,
                            SerdNodeType      object_type,
                            const SerdString* object_datatype,
                            const SerdString* object_lang)
{
	return writer->write_statement(writer,
		graph,
		subject, subject_type,
		predicate, predicate_type,
		object, object_type, object_datatype, object_lang);
}

static bool
serd_writer_write_statement_abbrev(SerdWriter        writer,
                                   const SerdString* graph,
                                   const SerdString* subject,
                                   SerdNodeType      subject_type,
                                   const SerdString* predicate,
                                   SerdNodeType      predicate_type,
                                   const SerdString* object,
                                   SerdNodeType      object_type,
                                   const SerdString* object_datatype,
                                   const SerdString* object_lang)
{
	assert(subject && predicate && object);
	if (subject == writer->context.subject) {
		if (predicate == writer->context.predicate) {
			++writer->indent;
			serd_writer_write_delim(writer, ',');
			write_node(writer, object_type, object, object_datatype, object_lang);
			--writer->indent;
		} else {
			if (writer->context.predicate) {
				serd_writer_write_delim(writer, ';');
			} else {
				++writer->indent;
				serd_writer_write_delim(writer, '\n');
			}
			write_node(writer, predicate_type, predicate, NULL, NULL);
			writer->context.predicate = predicate;
			writer->sink(" ", 1, writer->stream);
			write_node(writer, object_type, object, object_datatype, object_lang);
		}
	} else {
		if (writer->context.subject) {
			if (writer->indent > 0) {
				--writer->indent;
			}
			if (serd_stack_is_empty(&writer->anon_stack)) {
				serd_writer_write_delim(writer, '.');
				serd_writer_write_delim(writer, '\n');
			}
		}

		if (subject_type == SERD_ANON_BEGIN) {
			writer->sink("[ ", 2, writer->stream);
			++writer->indent;
			WriteContext* ctx = (WriteContext*)serd_stack_push(
				&writer->anon_stack, sizeof(WriteContext));
			*ctx = writer->context;
			writer->context.subject   = subject;
			writer->context.predicate = 0;
		} else {
			write_node(writer, subject_type, subject, NULL, NULL);
			++writer->indent;
			if (subject_type != SERD_ANON_BEGIN && subject_type != SERD_ANON) {
				serd_writer_write_delim(writer, '\n');
			}
		}

		writer->context.subject   = subject;
		writer->context.predicate = 0;

		write_node(writer, predicate_type, predicate, NULL, NULL);
		writer->context.predicate = predicate;
		writer->sink(" ", 1, writer->stream);

		write_node(writer, object_type, object, object_datatype, object_lang);
	}

	const WriteContext new_context = { graph, subject, predicate };
	writer->context = new_context;
	return true;
}

SERD_API
bool
serd_writer_write_statement_flat(SerdWriter        writer,
                                 const SerdString* graph,
                                 const SerdString* subject,
                                 SerdNodeType      subject_type,
                                 const SerdString* predicate,
                                 SerdNodeType      predicate_type,
                                 const SerdString* object,
                                 SerdNodeType      object_type,
                                 const SerdString* object_datatype,
                                 const SerdString* object_lang)
{
	assert(subject && predicate && object);
	write_node(writer, subject_type, subject, NULL, NULL);
	writer->sink(" ", 1, writer->stream);
	write_node(writer, predicate_type, predicate, NULL, NULL);
	writer->sink(" ", 1, writer->stream);
	write_node(writer, object_type, object, object_datatype, object_lang);
	writer->sink(" .\n", 3, writer->stream);
	return true;
}

SERD_API
bool
serd_writer_end_anon(SerdWriter        writer,
                     const SerdString* subject)
{
	if (writer->syntax == SERD_NTRIPLES) {
		return true;
	}
	if (serd_stack_is_empty(&writer->anon_stack)) {
		fprintf(stderr, "unexpected SERD_END received\n");
		return false;
	}
	assert(writer->indent > 0);
	--writer->indent;
	serd_writer_write_delim(writer, '\n');
	writer->sink("]", 1, writer->stream);
	writer->context = *anon_stack_top(writer);
	serd_stack_pop(&writer->anon_stack, sizeof(WriteContext));
	if (serd_stack_is_empty(&writer->anon_stack)) {
		// End of anonymous subject, reset context
		writer->context.subject   = subject;
		writer->context.predicate = 0;
	}
	return true;
}

SERD_API
void
serd_writer_finish(SerdWriter writer)
{
	if (writer->context.subject) {
		writer->sink(" .\n", 3, writer->stream);
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
	writer->base_uri   = *base_uri;
	writer->anon_stack = serd_stack_new(sizeof(WriteContext));
	writer->sink       = sink;
	writer->stream     = stream;
	writer->context    = context; 
	writer->indent     = 0;
	writer->write_node = write_node;
	if ((style & SERD_STYLE_ABBREVIATED)) {
		writer->write_statement = serd_writer_write_statement_abbrev;
	} else {
		writer->write_statement = serd_writer_write_statement_flat;
	}
	return writer;
}

SERD_API
void
serd_writer_set_base_uri(SerdWriter     writer,
                         const SerdURI* uri)
{
	writer->base_uri = *uri;
	if (writer->syntax != SERD_NTRIPLES) {
		if (writer->context.graph || writer->context.subject) {
			writer->sink(" .\n\n", 4, writer->stream);
			writer->context = WRITE_CONTEXT_NULL;
		}
		writer->sink("@base ", 6, writer->stream);
		writer->sink(" <", 2, writer->stream);
		serd_uri_serialise(uri, writer->sink, writer->stream);
		writer->sink("> .\n", 4, writer->stream);
	}
}

SERD_API
void
serd_writer_set_prefix(SerdWriter        writer,
                       const SerdString* name,
                       const SerdString* uri)
{
	if (writer->syntax != SERD_NTRIPLES) {
		if (writer->context.graph || writer->context.subject) {
			writer->sink(" .\n\n", 4, writer->stream);
			writer->context = WRITE_CONTEXT_NULL;
		}
		writer->sink("@prefix ", 8, writer->stream);
		writer->sink(name->buf, name->n_bytes - 1, writer->stream);
		writer->sink(": <", 3, writer->stream);
		write_text(writer, WRITE_URI, uri->buf, uri->n_bytes - 1, '>');
		writer->sink("> .\n", 4, writer->stream);
	}
}

SERD_API
void
serd_writer_free(SerdWriter writer)
{
	SerdWriter const me = (SerdWriter)writer;
	serd_stack_free(&writer->anon_stack);
	free(me);
}
