/*
  Copyright 2011 David Robillard <http://drobilla.net>

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

#include "serd_internal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

typedef struct {
	SerdNode graph;
	SerdNode subject;
	SerdNode predicate;
} WriteContext;

static const WriteContext WRITE_CONTEXT_NULL = {
	{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}
};

struct SerdWriterImpl {
	SerdSyntax   syntax;
	SerdStyle    style;
	SerdEnv*     env;
	SerdURI      base_uri;
	SerdStack    anon_stack;
	SerdBulkSink bulk_sink;
	SerdSink     sink;
	void*        stream;
	WriteContext context;
	uint8_t*     bprefix;
	size_t       bprefix_len;
	unsigned     indent;
	bool         empty;
};

typedef enum {
	WRITE_URI,
	WRITE_STRING,
	WRITE_LONG_STRING
} TextContext;

static inline WriteContext*
anon_stack_top(SerdWriter* writer)
{
	assert(!serd_stack_is_empty(&writer->anon_stack));
	return (WriteContext*)(writer->anon_stack.buf
	                       + writer->anon_stack.size - sizeof(WriteContext));
}

static void
copy_node(SerdNode* dst, const SerdNode* src)
{
	if (!src) {
		dst->type = SERD_NOTHING;
		return;
	}
	if (!dst->buf || dst->n_bytes < src->n_bytes) {
		dst->buf = realloc((char*)dst->buf, src->n_bytes + 1);
	}
	dst->n_bytes = src->n_bytes;
	dst->n_chars = src->n_chars;
	dst->flags   = src->flags;
	dst->type    = src->type;
	memcpy((char*)dst->buf, src->buf, src->n_bytes + 1);
}

static inline size_t
sink(const void* buf, size_t len, SerdWriter* writer)
{
	if (writer->style & SERD_STYLE_BULK) {
		return serd_bulk_sink_write(buf, len, &writer->bulk_sink);
	} else {
		return writer->sink(buf, len, writer->stream);
	}
}

static bool
write_text(SerdWriter* writer, TextContext ctx,
           const uint8_t* utf8, size_t n_bytes, uint8_t terminator)
{
	char escape[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (size_t i = 0; i < n_bytes;) {
		// Fast bulk write for long strings of printable ASCII
		size_t j = i;
		for (; j < n_bytes; ++j) {
			if (utf8[j] == terminator || utf8[j] == '\\' || utf8[j] == '"'
			    || (((writer->style & SERD_STYLE_ASCII) || ctx == WRITE_URI)
			        && !in_range(utf8[j], 0x20, 0x7E))) {
				break;
			}
		}

		if (j > i) {
			sink(&utf8[i], j - i, writer);
			i = j;
			continue;
		}

		uint8_t in = utf8[i++];
		if (ctx == WRITE_LONG_STRING) {
			if (in == '\\') {
				sink("\\\\", 2, writer); continue;
			} else if (in == '\"' && i == n_bytes) {
				sink("\\\"", 2, writer); continue;  // '"' at end of string
			}
		} else {
			switch (in) {
			case '\\': sink("\\\\", 2, writer); continue;
			case '\n': sink("\\n", 2, writer);  continue;
			case '\r': sink("\\r", 2, writer);  continue;
			case '\t': sink("\\t", 2, writer);  continue;
			case '"':
				if (terminator == '"') {
					sink("\\\"", 2, writer);
					continue;
				}  // else fall-through
			default: break;
			}

			if (in == terminator) {
				snprintf(escape, 7, "\\u%04X", terminator);
				sink(escape, 6, writer);
				continue;
			}
		}

		uint32_t c    = 0;
		size_t   size = 0;
		if ((in & 0x80) == 0) {  // Starts with `0'
			size = 1;
			c = in & 0x7F;
			if (in_range(in, 0x20, 0x7E)) {  // Printable ASCII
				sink(&in, 1, writer);
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
			fprintf(stderr, "Invalid UTF-8 at offset %zu: %X\n", i, in);
			return false;
		}

		if ((ctx == WRITE_STRING || ctx == WRITE_LONG_STRING)
		    && !(writer->style & SERD_STYLE_ASCII)) {
			// Write UTF-8 character directly to UTF-8 output
			// TODO: Scan to next escape and write entire range at once
			sink(utf8 + i - 1, size, writer);
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
			sink(escape, 6, writer);
		} else {
			snprintf(escape, 11, "\\U%08X", c);
			sink(escape, 10, writer);
		}
	}
	return true;
}

static void
serd_writer_write_delim(SerdWriter* writer, const uint8_t delim)
{
	switch (delim) {
	case '\n':
		break;
	default:
		sink(" ", 1, writer);
	case '[':
		sink(&delim, 1, writer);
	}
	sink("\n", 1, writer);
	for (unsigned i = 0; i < writer->indent; ++i) {
		sink("\t", 1, writer);
	}
}

static void
reset_context(SerdWriter* writer, bool delete)
{
	if (delete) {
		serd_node_free(&writer->context.graph);
		serd_node_free(&writer->context.subject);
		serd_node_free(&writer->context.predicate);
		writer->context = WRITE_CONTEXT_NULL;
	} else {
		writer->context.graph.type     = SERD_NOTHING;
		writer->context.subject.type   = SERD_NOTHING;
		writer->context.predicate.type = SERD_NOTHING;
	}
	writer->empty = false;
}

typedef enum {
	FIELD_NONE,
	FIELD_SUBJECT,
	FIELD_PREDICATE,
	FIELD_OBJECT
} Field;

static bool
write_node(SerdWriter*        writer,
           const SerdNode*    node,
           const SerdNode*    datatype,
           const SerdNode*    lang,
           Field              field,
           SerdStatementFlags flags)
{
	SerdChunk uri_prefix;
	SerdChunk uri_suffix;
	switch (node->type) {
	case SERD_NOTHING:
		return false;
	case SERD_BLANK:
		if (writer->syntax != SERD_NTRIPLES
		    && ((field == FIELD_SUBJECT && (flags & SERD_ANON_S_BEGIN))
		        || (field == FIELD_OBJECT && (flags & SERD_ANON_O_BEGIN)))) {
			++writer->indent;
			serd_writer_write_delim(writer, '[');
		} else if (writer->syntax != SERD_NTRIPLES
		    && ((field == FIELD_SUBJECT && (flags & SERD_EMPTY_S))
		        || (field == FIELD_OBJECT && (flags & SERD_EMPTY_O)))) {
			sink("[]", 2, writer);
		} else {
			sink("_:", 2, writer);
			if (writer->bprefix && !strncmp((const char*)node->buf,
			                                (const char*)writer->bprefix,
			                                writer->bprefix_len)) {
				sink(node->buf + writer->bprefix_len,
				     node->n_bytes - writer->bprefix_len,
				     writer);
			} else {
				sink(node->buf, node->n_bytes, writer);
			}
		}
		break;
	case SERD_CURIE:
		switch (writer->syntax) {
		case SERD_NTRIPLES:
			if (serd_env_expand(writer->env, node, &uri_prefix, &uri_suffix)) {
				fprintf(stderr, "Undefined namespace prefix `%s'\n", node->buf);
				return false;
			}
			sink("<", 1, writer);
			write_text(writer, WRITE_URI, uri_prefix.buf, uri_prefix.len, '>');
			write_text(writer, WRITE_URI, uri_suffix.buf, uri_suffix.len, '>');
			sink(">", 1, writer);
			break;
		case SERD_TURTLE:
			sink(node->buf, node->n_bytes, writer);
		}
		break;
	case SERD_LITERAL:
		if (writer->syntax == SERD_TURTLE && datatype && datatype->buf) {
			// TODO: compare against NS_XSD prefix once
			if (!strcmp((const char*)datatype->buf,    NS_XSD "boolean")
			    || !strcmp((const char*)datatype->buf, NS_XSD "decimal")
			    || !strcmp((const char*)datatype->buf, NS_XSD "integer")) {
				sink(node->buf, node->n_bytes, writer);
				break;
			}
		}
		if (writer->syntax != SERD_NTRIPLES
		    && ((node->flags & SERD_HAS_NEWLINE)
		        || (node->flags & SERD_HAS_QUOTE))) {
			sink("\"\"\"", 3, writer);
			write_text(writer, WRITE_LONG_STRING,
			           node->buf, node->n_bytes, '\0');
			sink("\"\"\"", 3, writer);
		} else {
			sink("\"", 1, writer);
			write_text(writer, WRITE_STRING, node->buf, node->n_bytes, '"');
			sink("\"", 1, writer);
		}
		if (lang && lang->buf) {
			sink("@", 1, writer);
			sink(lang->buf, lang->n_bytes, writer);
		} else if (datatype && datatype->buf) {
			sink("^^", 2, writer);
			write_node(writer, datatype, NULL, NULL, FIELD_NONE, flags);
		}
		break;
	case SERD_URI:
		if ((writer->syntax == SERD_TURTLE)
		    && !strcmp((const char*)node->buf, NS_RDF "type")) {
			sink("a", 1, writer);
			return true;
		} else if ((writer->style & SERD_STYLE_CURIED)
		           && serd_uri_string_has_scheme(node->buf)) {
			SerdNode  prefix;
			SerdChunk suffix;
			if (serd_env_qualify(writer->env, node, &prefix, &suffix)) {
				write_text(writer, WRITE_URI, prefix.buf, prefix.n_bytes, '>');
				sink(":", 1, writer);
				write_text(writer, WRITE_URI, suffix.buf, suffix.len, '>');
				return true;
			}
		} else if ((writer->style & SERD_STYLE_RESOLVED)
		           && !serd_uri_string_has_scheme(node->buf)) {
			SerdURI uri;
			if (!serd_uri_parse(node->buf, &uri)) {
				SerdURI abs_uri;
				serd_uri_resolve(&uri, &writer->base_uri, &abs_uri);
				sink("<", 1, writer);
				serd_uri_serialise(&abs_uri, (SerdSink)sink, writer);
				sink(">", 1, writer);
				return true;
			}
		}
		sink("<", 1, writer);
		write_text(writer, WRITE_URI, node->buf, node->n_bytes, '>');
		sink(">", 1, writer);
		return true;
	}
	return true;
}

SERD_API
SerdStatus
serd_writer_write_statement(SerdWriter*        writer,
                            SerdStatementFlags flags,
                            const SerdNode*    graph,
                            const SerdNode*    subject,
                            const SerdNode*    predicate,
                            const SerdNode*    object,
                            const SerdNode*    object_datatype,
                            const SerdNode*    object_lang)
{
	assert(subject && predicate && object);
	switch (writer->syntax) {
	case SERD_NTRIPLES:
		write_node(writer, subject, NULL, NULL, FIELD_SUBJECT, flags);
		sink(" ", 1, writer);
		write_node(writer, predicate, NULL, NULL, FIELD_PREDICATE, flags);
		sink(" ", 1, writer);
		if (!write_node(writer, object, object_datatype, object_lang,
		                FIELD_OBJECT, flags)) {
			return SERD_ERR_UNKNOWN;
		}
		sink(" .\n", 3, writer);
		return SERD_SUCCESS;
	case SERD_TURTLE:
		break;
	}
	if (serd_node_equals(subject, &writer->context.subject)) {
		if (serd_node_equals(predicate, &writer->context.predicate)) {
			// Abbreviate S P
			if ((flags & SERD_ANON_O_BEGIN)) {
				sink(" , ", 3, writer);  // ] , [
			} else {
				++writer->indent;
				serd_writer_write_delim(writer, ',');
			}
			write_node(writer, object, object_datatype, object_lang,
			           FIELD_OBJECT, flags);
			if (!(flags & SERD_ANON_O_BEGIN)) {
				--writer->indent;
			}
		} else {
			// Abbreviate S
			if (writer->context.predicate.type) {
				serd_writer_write_delim(writer, ';');
			} else {
				serd_writer_write_delim(writer, '\n');
			}
			write_node(writer, predicate, NULL, NULL, FIELD_PREDICATE, flags);
			copy_node(&writer->context.predicate, predicate);
			sink(" ", 1, writer);
			write_node(writer, object, object_datatype, object_lang,
			           FIELD_OBJECT, flags);
		}
	} else {
		if (writer->context.subject.type) {
			if (writer->indent > 0) {
				--writer->indent;
			}
			if (serd_stack_is_empty(&writer->anon_stack)) {
				serd_writer_write_delim(writer, '.');
				serd_writer_write_delim(writer, '\n');
			}
		} else if (!writer->empty) {
			serd_writer_write_delim(writer, '\n');
		}

		if (!(flags & SERD_ANON_CONT)) {
			write_node(writer, subject, NULL, NULL, FIELD_SUBJECT, flags);
			++writer->indent;
			if (!(flags & SERD_ANON_S_BEGIN)) {
				serd_writer_write_delim(writer, '\n');
			}
		} else {
			++writer->indent;
		}

		reset_context(writer, false);
		copy_node(&writer->context.subject, subject);

		write_node(writer, predicate, NULL, NULL, FIELD_PREDICATE, flags);
		copy_node(&writer->context.predicate, predicate);
		sink(" ", 1, writer);

		write_node(writer, object, object_datatype, object_lang,
		           FIELD_OBJECT, flags);
	}

	if (writer->syntax != SERD_NTRIPLES
	    && ((flags & SERD_ANON_S_BEGIN) || (flags & SERD_ANON_O_BEGIN))) {
		WriteContext* ctx = (WriteContext*)serd_stack_push(
			&writer->anon_stack, sizeof(WriteContext));
		*ctx = writer->context;
		const WriteContext new_context = { serd_node_copy(graph),
		                                   serd_node_copy(subject),
		                                   serd_node_copy(predicate) };
		writer->context = new_context;
	} else {
		copy_node(&writer->context.graph, graph);
		copy_node(&writer->context.subject, subject);
		copy_node(&writer->context.predicate, predicate);
	}

	return SERD_SUCCESS;
}

SERD_API
SerdStatus
serd_writer_end_anon(SerdWriter*     writer,
                     const SerdNode* node)
{
	if (writer->syntax == SERD_NTRIPLES) {
		return SERD_SUCCESS;
	}
	if (serd_stack_is_empty(&writer->anon_stack)) {
		fprintf(stderr, "Unexpected end of anonymous node\n");
		return SERD_ERR_UNKNOWN;
	}
	assert(writer->indent > 0);
	--writer->indent;
	serd_writer_write_delim(writer, '\n');
	sink("]", 1, writer);
	reset_context(writer, true);
	writer->context = *anon_stack_top(writer);
	serd_stack_pop(&writer->anon_stack, sizeof(WriteContext));
	const bool is_subject = serd_node_equals(node, &writer->context.subject);
	if (is_subject) {
		copy_node(&writer->context.subject, node);
		writer->context.predicate.type = SERD_NOTHING;
	}
	return SERD_SUCCESS;
}

SERD_API
SerdStatus
serd_writer_finish(SerdWriter* writer)
{
	if (writer->context.subject.type) {
		sink(" .\n", 3, writer);
	}
	if (writer->style & SERD_STYLE_BULK) {
		serd_bulk_sink_flush(&writer->bulk_sink);
	}
	reset_context(writer, true);
	return SERD_SUCCESS;
}

SERD_API
SerdWriter*
serd_writer_new(SerdSyntax     syntax,
                SerdStyle      style,
                SerdEnv*       env,
                const SerdURI* base_uri,
                SerdSink       sink,
                void*          stream)
{
	const WriteContext context = WRITE_CONTEXT_NULL;
	SerdWriter*        writer  = malloc(sizeof(struct SerdWriterImpl));
	writer->syntax      = syntax;
	writer->style       = style;
	writer->env         = env;
	writer->base_uri    = base_uri ? *base_uri : SERD_URI_NULL;
	writer->anon_stack  = serd_stack_new(sizeof(WriteContext));
	writer->sink        = sink;
	writer->stream      = stream;
	writer->context     = context;
	writer->bprefix     = NULL;
	writer->bprefix_len = 0;
	writer->indent      = 0;
	writer->empty       = true;
	if (style & SERD_STYLE_BULK) {
		writer->bulk_sink = serd_bulk_sink_new(sink, stream, SERD_PAGE_SIZE);
	}
	return writer;
}

SERD_API
void
serd_writer_chop_blank_prefix(SerdWriter*    writer,
                              const uint8_t* prefix)
{
	if (writer->bprefix) {
		free(writer->bprefix);
		writer->bprefix_len = 0;
		writer->bprefix     = NULL;
	}
	if (prefix) {
		writer->bprefix_len = strlen((const char*)prefix);
		writer->bprefix     = malloc(writer->bprefix_len + 1);
		memcpy(writer->bprefix, prefix, writer->bprefix_len + 1);
	}
}

SERD_API
SerdStatus
serd_writer_set_base_uri(SerdWriter*     writer,
                         const SerdNode* uri)
{
	if (!serd_env_set_base_uri(writer->env, uri)) {
		serd_env_get_base_uri(writer->env, &writer->base_uri);

		if (writer->syntax != SERD_NTRIPLES) {
			if (writer->context.graph.type || writer->context.subject.type) {
				sink(" .\n\n", 4, writer);
				reset_context(writer, false);
			}
			sink("@base <", 7, writer);
			sink(uri->buf, uri->n_bytes, writer);
			sink("> .\n", 4, writer);
		}
		reset_context(writer, false);
		return SERD_SUCCESS;
	}
	return SERD_ERR_UNKNOWN;
}

SERD_API
SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri)
{
	if (!serd_env_set_prefix(writer->env, name, uri)) {
		if (writer->syntax != SERD_NTRIPLES) {
			if (writer->context.graph.type || writer->context.subject.type) {
				sink(" .\n\n", 4, writer);
				reset_context(writer, false);
			}
			sink("@prefix ", 8, writer);
			sink(name->buf, name->n_bytes, writer);
			sink(": <", 3, writer);
			write_text(writer, WRITE_URI, uri->buf, uri->n_bytes, '>');
			sink("> .\n", 4, writer);
		}
		reset_context(writer, false);
		return SERD_SUCCESS;
	}
	return SERD_ERR_UNKNOWN;
}

SERD_API
void
serd_writer_free(SerdWriter* writer)
{
	serd_writer_finish(writer);
	serd_stack_free(&writer->anon_stack);
	free(writer->bprefix);
	if (writer->style & SERD_STYLE_BULK) {
		serd_bulk_sink_free(&writer->bulk_sink);
	}
	free(writer);
}

SERD_API
size_t
serd_file_sink(const void* buf, size_t len, void* stream)
{
	FILE* file = (FILE*)stream;
	return fwrite(buf, 1, len, file);
}

