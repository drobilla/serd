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

#include "env.h"
#include "node.h"
#include "sink.h"
#include "stack.h"
#include "string_utils.h"
#include "system.h"
#include "uri_utils.h"
#include "world.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	CTX_NAMED, ///< Normal non-anonymous context
	CTX_BLANK, ///< Anonymous blank node
	CTX_LIST   ///< Anonymous list
} ContextType;

typedef struct
{
	ContextType type;
	SerdNode*   graph;
	SerdNode*   subject;
	SerdNode*   predicate;
} WriteContext;

static const WriteContext WRITE_CONTEXT_NULL = { CTX_NAMED, NULL, NULL, NULL };

typedef enum {
	SEP_NONE,
	SEP_NODE,        ///< Placeholder for nodes
	SEP_END_S,       ///< End of a subject ('.')
	SEP_END_P,       ///< End of a predicate (';')
	SEP_END_O,       ///< End of an object (',')
	SEP_S_P,         ///< Between a subject and predicate (whitespace)
	SEP_P_O,         ///< Between a predicate and object (whitespace)
	SEP_ANON_BEGIN,  ///< Start of anonymous node ('[')
	SEP_ANON_S_P,    ///< Between start of anonymous node and predicate
	SEP_ANON_END,    ///< End of anonymous node (']')
	SEP_LIST_BEGIN,  ///< Start of list ('(')
	SEP_LIST_SEP,    ///< List separator (whitespace)
	SEP_LIST_END,    ///< End of list (')')
	SEP_GRAPH_BEGIN, ///< Start of graph ('{')
	SEP_GRAPH_END,   ///< End of graph ('}')
} Sep;

typedef uint32_t SepMask; ///< Bitfield of separator flags

#define SEP_ALL ((SepMask)-1)
#define M(s)  (1U << (s))

typedef struct {
	const char* str;             ///< Sep string
	size_t      len;             ///< Length of sep string
	int         indent;          ///< Indent delta
	SepMask     pre_space_after; ///< Leading space if after given seps
	SepMask     pre_line_after;  ///< Leading newline if after given seps
	SepMask     post_line_after; ///< Trailing newline if after given seps
} SepRule;

static const SepRule rules[] = {
	{"",    0, +0, SEP_NONE,     SEP_NONE, SEP_NONE},
	{"",    0, +0, SEP_NONE,     SEP_NONE, SEP_NONE},
	{".\n", 2, -1, SEP_ALL,      SEP_NONE, SEP_NONE},
	{";",   1, +0, SEP_ALL,      SEP_NONE, SEP_ALL},
	{",",   1, +0, SEP_ALL,      SEP_NONE, ~(M(SEP_ANON_END) | M(SEP_LIST_END))},
	{"",    0, +1, SEP_NONE,     SEP_NONE, SEP_ALL},
	{" ",   1, +0, SEP_NONE,     SEP_NONE, SEP_NONE},
	{"[",   1, +1, M(SEP_END_O), SEP_NONE, SEP_NONE},
	{"",    0, +0, SEP_NONE,     SEP_ALL, SEP_NONE},
	{"]",   1, -1, SEP_NONE,     ~M(SEP_ANON_BEGIN), SEP_NONE},
	{"(",   1, +1, M(SEP_END_O), SEP_NONE, SEP_ALL},
	{"",    0, +0, SEP_NONE,     SEP_ALL, SEP_NONE},
	{")",   1, -1, SEP_NONE,     SEP_ALL, SEP_NONE},
	{"{",   1, +1, SEP_ALL,      SEP_NONE, SEP_NONE},
	{"}",   1, -1, SEP_NONE,     SEP_NONE, SEP_ALL},
	{"<",   1, +0, SEP_NONE,     SEP_NONE, SEP_NONE},
	{">",   1, +0, SEP_NONE,     SEP_NONE, SEP_NONE},
};

struct SerdWriterImpl {
	SerdWorld*    world;
	SerdSink      iface;
	SerdSyntax    syntax;
	SerdStyle     style;
	SerdEnv*      env;
	SerdNode*     root_node;
	SerdURI       root_uri;
	SerdStack     anon_stack;
	SerdWriteFunc write_func;
	void*         stream;
	SerdErrorSink error_sink;
	void*         error_handle;
	WriteContext  context;
	unsigned      indent;
	char*         bprefix;
	size_t        bprefix_len;
	Sep           last_sep;
	bool          empty;
};

typedef enum {
	WRITE_STRING,
	WRITE_LONG_STRING
} TextContext;

static SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri);

static bool
write_node(SerdWriter*        writer,
           const SerdNode*    node,
           SerdField          field,
           SerdStatementFlags flags);

static bool
supports_abbrev(const SerdWriter* writer)
{
	return writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG;
}

static inline WriteContext*
anon_stack_top(SerdWriter* writer)
{
	assert(!serd_stack_is_empty(&writer->anon_stack));
	return (WriteContext*)(writer->anon_stack.buf
	                       + writer->anon_stack.size - sizeof(WriteContext));
}

static inline SerdNode*
ctx(SerdWriter* writer, const SerdField field)
{
	SerdNode* node = NULL;
	if (field == SERD_SUBJECT) {
		node = writer->context.subject;
	} else if (field == SERD_PREDICATE) {
		node = writer->context.predicate;
	} else if (field == SERD_GRAPH) {
		node = writer->context.graph;
	}

	return node && node->type ? node : NULL;
}

static inline size_t
sink(const void* buf, size_t len, SerdWriter* writer)
{
	return writer->write_func(buf, 1, len, writer->stream);
}

// Write a single character, as an escape for single byte characters
// (Caller prints any single byte characters that don't need escaping)
static size_t
write_character(SerdWriter* writer, const uint8_t* utf8, size_t* size)
{
	char           escape[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	const uint32_t c          = parse_utf8_char(utf8, size);
	switch (*size) {
	case 0:
		serd_world_errorf(
			writer->world, SERD_ERR_BAD_ARG, "invalid UTF-8: %X\n", utf8[0]);
		return sink(replacement_char, sizeof(replacement_char), writer);
	case 1:
		snprintf(escape, sizeof(escape), "\\u%04X", utf8[0]);
		return sink(escape, 6, writer);
	default:
		break;
	}

	if (!(writer->style & SERD_STYLE_ASCII)) {
		// Write UTF-8 character directly to UTF-8 output
		return sink(utf8, *size, writer);
	}

	if (c <= 0xFFFF) {
		snprintf(escape, sizeof(escape), "\\u%04X", c);
		return sink(escape, 6, writer);
	} else {
		snprintf(escape, sizeof(escape), "\\U%08X", c);
		return sink(escape, 10, writer);
	}
}

static inline bool
uri_must_escape(const char c)
{
	switch (c) {
	case ' ': case '"': case '<': case '>': case '\\':
	case '^': case '`': case '{': case '|': case '}':
		return true;
	default:
		return !in_range(c, 0x20, 0x7E);
	}
}

static size_t
write_uri(SerdWriter* writer, const char* utf8, size_t n_bytes)
{
	size_t len = 0;
	for (size_t i = 0; i < n_bytes;) {
		size_t j = i;  // Index of next character that must be escaped
		for (; j < n_bytes; ++j) {
			if (uri_must_escape(utf8[j])) {
				break;
			}
		}

		// Bulk write all characters up to this special one
		len += sink(&utf8[i], j - i, writer);
		if ((i = j) == n_bytes) {
			break;  // Reached end
		}

		// Write UTF-8 character
		size_t size = 0;
		len += write_character(writer, (const uint8_t*)utf8 + i, &size);
		i   += size;
		if (size == 0) {
			// Corrupt input, scan to start of next character
			for (++i; i < n_bytes && (utf8[i] & 0x80); ++i) {}
		}
	}
	return len;
}

static size_t
write_uri_from_node(SerdWriter* writer, const SerdNode* node)
{
	return write_uri(writer, serd_node_get_string(node), node->n_bytes);
}

static bool
lname_must_escape(const char c)
{
	/* This arbitrary list of characters, most of which have nothing to do with
	   Turtle, must be handled as special cases here because the RDF and SPARQL
	   WGs are apparently intent on making the once elegant Turtle a baroque
	   and inconsistent mess, throwing elegance and extensibility completely
	   out the window for no good reason.

	   Note '-', '.', and '_' are also in PN_LOCAL_ESC, but are valid unescaped
	   in local names, so they are not escaped here. */

	switch (c) {
	case '\'': case '!': case '#': case '$': case '%': case '&':
	case '(': case ')': case '*': case '+': case ',': case '/':
	case ';': case '=': case '?': case '@': case '~':
		return true;
	}
	return false;
}

static size_t
write_lname(SerdWriter* writer, const char* utf8, size_t n_bytes)
{
	size_t len = 0;
	for (size_t i = 0; i < n_bytes; ++i) {
		size_t j = i;  // Index of next character that must be escaped
		for (; j < n_bytes; ++j) {
			if (lname_must_escape(utf8[j])) {
				break;
			}
		}

		// Bulk write all characters up to this special one
		len += sink(&utf8[i], j - i, writer);
		if ((i = j) == n_bytes) {
			break;  // Reached end
		}

		// Write escape
		len += sink("\\", 1, writer);
		len += sink(&utf8[i], 1, writer);
	}
	return len;
}

static size_t
write_text(SerdWriter* writer, TextContext ctx,
           const char* utf8, size_t n_bytes)
{
	size_t len = 0;
	for (size_t i = 0; i < n_bytes;) {
		// Fast bulk write for long strings of printable ASCII
		size_t j = i;
		for (; j < n_bytes; ++j) {
			if (utf8[j] == '\\' || utf8[j] == '"'
			    || (!in_range(utf8[j], 0x20, 0x7E))) {
				break;
			}
		}

		len += sink(&utf8[i], j - i, writer);
		if ((i = j) == n_bytes) {
			break;  // Reached end
		}

		const uint8_t in = utf8[i++];
		if (ctx == WRITE_LONG_STRING) {
			switch (in) {
			case '\\': len += sink("\\\\", 2, writer); continue;
			case '\b': len += sink("\\b", 2, writer);  continue;
			case '\n': case '\r': case '\t': case '\f':
				len += sink(&in, 1, writer);  // Write character as-is
				continue;
			case '\"':
				if (i == n_bytes) {  // '"' at string end
					len += sink("\\\"", 2, writer);
				} else {
					len += sink(&in, 1, writer);
				}
				continue;
			default: break;
			}
		} else if (ctx == WRITE_STRING) {
			switch (in) {
			case '\\': len += sink("\\\\", 2, writer); continue;
			case '\n': len += sink("\\n", 2, writer);  continue;
			case '\r': len += sink("\\r", 2, writer);  continue;
			case '\t': len += sink("\\t", 2, writer);  continue;
			case '"':  len += sink("\\\"", 2, writer); continue;
			default: break;
			}
			if (writer->syntax == SERD_TURTLE) {
				switch (in) {
				case '\b': len += sink("\\b", 2, writer); continue;
				case '\f': len += sink("\\f", 2, writer); continue;
				}
			}
		}

		// Write UTF-8 character
		size_t size = 0;
		len += write_character(writer, (const uint8_t*)utf8 + i - 1, &size);

		if (size == 0) {
			// Corrupt input, scan to start of next character
			for (; i < n_bytes && (utf8[i] & 0x80); ++i) {}
		} else {
			i += size - 1;
		}
	}
	return len;
}

static size_t
uri_sink(const void* buf, size_t size, size_t nmemb, void* stream)
{
	(void)size;
	assert(size == 1);
	return write_uri((SerdWriter*)stream, (const char*)buf, nmemb);
}

static void
write_newline(SerdWriter* writer)
{
	sink("\n", 1, writer);
	for (unsigned i = 0; i < writer->indent; ++i) {
		sink("\t", 1, writer);
	}
}

static bool
write_sep(SerdWriter* writer, const Sep sep)
{
	const SepRule* rule = &rules[sep];

	// Adjust indent, but tolerate if it would become negative
	writer->indent =
		((rule->indent >= 0 || writer->indent >= (unsigned)-rule->indent)
		 ? writer->indent + rule->indent
		 : 0);

	// Write newline or space before separator if necessary
	if (rule->pre_line_after & (1u << writer->last_sep)) {
		write_newline(writer);
	} else if (rule->pre_space_after & (1u << writer->last_sep)) {
		sink(" ", 1, writer);
	}

	// Write actual separator string
	sink(rule->str, rule->len, writer);

	// Write newline after separator if necessary
	if (rule->post_line_after & (1u << writer->last_sep)) {
		write_newline(writer);
	}

	writer->last_sep = sep;
	return true;
}

static SerdStatus
reset_context(SerdWriter* writer, bool graph)
{
	if (graph && writer->context.graph) {
		memset(writer->context.graph, 0, sizeof(SerdNode));
	}
	if (writer->context.subject) {
		memset(writer->context.subject, 0, sizeof(SerdNode));
	}
	if (writer->context.predicate) {
		memset(writer->context.predicate, 0, sizeof(SerdNode));
	}
	writer->empty = false;
	return SERD_SUCCESS;
}

static SerdStatus
free_context(SerdWriter* writer)
{
	serd_node_free(writer->context.graph);
	serd_node_free(writer->context.subject);
	serd_node_free(writer->context.predicate);
	return SERD_SUCCESS;
}

static bool
is_inline_start(const SerdWriter*  writer,
                SerdField          field,
                SerdStatementFlags flags)
{
	return (supports_abbrev(writer) &&
	        ((field == SERD_SUBJECT && (flags & SERD_ANON_S_BEGIN)) ||
	         (field == SERD_OBJECT &&  (flags & SERD_ANON_O_BEGIN))));
}

static bool
write_literal(SerdWriter*        writer,
              const SerdNode*    node,
              SerdStatementFlags flags)
{
	writer->last_sep = SEP_NONE;

	const SerdNode* datatype = serd_node_get_datatype(node);
	const SerdNode* lang     = serd_node_get_language(node);
	const char*     node_str = serd_node_get_string(node);
	const char*     type_uri = serd_node_get_string(datatype);
	if (supports_abbrev(writer) && type_uri) {
		if (serd_node_equals(datatype, writer->world->xsd_boolean) ||
		    serd_node_equals(datatype, writer->world->xsd_integer)) {
			sink(node_str, node->n_bytes, writer);
			return true;
		} else if (serd_node_equals(datatype, writer->world->xsd_decimal) &&
		           strchr(node_str, '.') &&
		           node_str[node->n_bytes - 1] != '.') {
			/* xsd:decimal literals without trailing digits, e.g. "5.", can
			   not be written bare in Turtle.  We could add a 0 which is
			   prettier, but changes the text and breaks round tripping.
			*/
			sink(node_str, node->n_bytes, writer);
			return true;
		}
	}

	if (supports_abbrev(writer)
	    && (node->flags & (SERD_HAS_NEWLINE|SERD_HAS_QUOTE))) {
		sink("\"\"\"", 3, writer);
		write_text(writer, WRITE_LONG_STRING, node_str, node->n_bytes);
		sink("\"\"\"", 3, writer);
	} else {
		sink("\"", 1, writer);
		write_text(writer, WRITE_STRING, node_str, node->n_bytes);
		sink("\"", 1, writer);
	}
	if (lang && serd_node_get_string(lang)) {
		sink("@", 1, writer);
		sink(serd_node_get_string(lang), lang->n_bytes, writer);
	} else if (type_uri) {
		sink("^^", 2, writer);
		return write_node(writer, datatype, (SerdField)-1, flags);
	}
	return true;
}

// Return true iff `buf` is a valid prefixed name prefix or suffix
static inline bool
is_name(const char* buf, const size_t len)
{
	// TODO: This is more strict than it should be.
	for (size_t i = 0; i < len; ++i) {
		if (!(is_alpha(buf[i]) || is_digit(buf[i]))) {
			return false;
		}
	}
	return true;
}

static bool
write_uri_node(SerdWriter* const        writer,
               const SerdNode*          node,
               const SerdField          field,
               const SerdStatementFlags flags)
{
	writer->last_sep = SEP_NONE;
	if (is_inline_start(writer, field, flags)) {
		write_sep(writer, SEP_ANON_BEGIN);
		sink(" == ", 4, writer);
	}

	const SerdNode* prefix;
	SerdStringView  suffix;
	const char*     node_str   = serd_node_get_string(node);
	const bool      has_scheme = serd_uri_string_has_scheme(node_str);
	if (field == SERD_PREDICATE && supports_abbrev(writer) &&
	    serd_node_equals(node, writer->world->rdf_type)) {
		return sink("a", 1, writer) == 1;
	} else if (supports_abbrev(writer) &&
	           serd_node_equals(node, writer->world->rdf_nil)) {
		return sink("()", 2, writer) == 2;
	} else if (has_scheme && supports_abbrev(writer) &&
	           serd_env_qualify_in_place(writer->env, node, &prefix, &suffix) &&
	           is_name(serd_node_get_string(prefix), serd_node_get_length(prefix)) &&
	           is_name(suffix.buf, suffix.len)) {
		write_uri_from_node(writer, prefix);
		sink(":", 1, writer);
		write_uri(writer, suffix.buf, suffix.len);
		return true;
	}

	sink("<", 1, writer);
	if (serd_env_get_base_uri(writer->env)) {
		const SerdURI* base_uri = serd_env_get_parsed_base_uri(writer->env);
		SerdURI uri, abs_uri;
		serd_uri_parse(node_str, &uri);
		serd_uri_resolve(&uri, base_uri, &abs_uri);
		bool rooted = uri_is_under(base_uri, &writer->root_uri);
		const SerdURI* root = rooted ? &writer->root_uri : base_uri;
		if (!uri_is_under(&abs_uri, root) ||
		    writer->syntax == SERD_NTRIPLES ||
		    writer->syntax == SERD_NQUADS) {
			serd_uri_serialise(&abs_uri, uri_sink, writer);
		} else {
			serd_uri_serialise_relative(&uri, base_uri, root, uri_sink, writer);
		}
	} else {
		write_uri_from_node(writer, node);
	}
	sink(">", 1, writer);
	writer->last_sep = SEP_NONE;
	if (is_inline_start(writer, field, flags)) {
		sink(" ;", 2, writer);
	}
	return true;
}

static bool
write_curie(SerdWriter* const        writer,
            const SerdNode*          node,
            const SerdField          field,
            const SerdStatementFlags flags)
{
	writer->last_sep = SEP_NONE;

	SerdStringView prefix;
	SerdStringView suffix;
	SerdStatus     st;
	switch (writer->syntax) {
	case SERD_NTRIPLES:
	case SERD_NQUADS:
		if ((st = serd_env_expand_in_place(
			     writer->env, node, &prefix, &suffix))) {
			serd_world_errorf(writer->world,
			                  st,
			                  "undefined namespace prefix `%s'\n",
			                  serd_node_get_string(node));
			return false;
		}
		sink("<", 1, writer);
		write_uri(writer, prefix.buf, prefix.len);
		write_uri(writer, suffix.buf, suffix.len);
		sink(">", 1, writer);
		break;
	case SERD_TURTLE:
	case SERD_TRIG:
		if (is_inline_start(writer, field, flags)) {
			write_sep(writer, SEP_ANON_BEGIN);
			sink(" == ", 4, writer);
		}
		write_lname(writer, serd_node_get_string(node), node->n_bytes);
		if (is_inline_start(writer, field, flags)) {
			sink(" ;", 2, writer);
		}
	}
	return true;
}

static bool
write_blank(SerdWriter* const        writer,
            const SerdNode*          node,
            const SerdField          field,
            const SerdStatementFlags flags)
{
	const char* node_str = serd_node_get_string(node);
	if (supports_abbrev(writer)) {
		if (is_inline_start(writer, field, flags)) {
			return write_sep(writer, SEP_ANON_BEGIN);
		} else if ((field == SERD_SUBJECT && (flags & SERD_LIST_S_BEGIN)) ||
		           (field == SERD_OBJECT && (flags & SERD_LIST_O_BEGIN))) {
			return write_sep(writer, SEP_LIST_BEGIN);
		} else if (field == SERD_SUBJECT && (flags & SERD_EMPTY_S)) {
			/* Last character is technically a separator, but reset because we
			   want to treat "[]" like a node. */
			writer->last_sep = SEP_NONE;
			return sink("[]", 2, writer) == 2;
		}
	}

	sink("_:", 2, writer);
	if (writer->bprefix &&
	    !strncmp(node_str, writer->bprefix, writer->bprefix_len)) {
		sink(node_str + writer->bprefix_len,
		     node->n_bytes - writer->bprefix_len,
		     writer);
	} else {
		sink(node_str, node->n_bytes, writer);
	}

	writer->last_sep = SEP_NONE;
	return true;
}

static bool
write_node(SerdWriter*        writer,
           const SerdNode*    node,
           const SerdField    field,
           SerdStatementFlags flags)
{
	bool ret = false;
	switch (node->type) {
	case SERD_LITERAL:
		ret = write_literal(writer, node, flags);
		break;
	case SERD_URI:
		ret = write_uri_node(writer, node, field, flags);
		break;
	case SERD_CURIE:
		ret = write_curie(writer, node, field, flags);
		break;
	case SERD_BLANK:
		ret = write_blank(writer, node, field, flags);
	default: break;
	}
	return ret;
}

static inline bool
is_resource(const SerdNode* node)
{
	return node->type > SERD_LITERAL;
}

static void
write_pred(SerdWriter* writer, SerdStatementFlags flags, const SerdNode* pred)
{
	write_node(writer, pred, SERD_PREDICATE, flags);
	write_sep(writer, SEP_P_O);
	serd_node_set(&writer->context.predicate, pred);
}

static bool
write_list_obj(SerdWriter*        writer,
               SerdStatementFlags flags,
               const SerdNode*    predicate,
               const SerdNode*    object)
{
	if (serd_node_equals(object, writer->world->rdf_nil)) {
		write_sep(writer, SEP_LIST_END);
		return true;
	}

	if (serd_node_equals(predicate, writer->world->rdf_first)) {
		write_node(writer, object, SERD_OBJECT, flags);
	} else {
		write_sep(writer, SEP_LIST_SEP);
	}

	return false;
}

static SerdStatus
serd_writer_write_statement(SerdWriter*          writer,
                            SerdStatementFlags   flags,
                            const SerdStatement* statement)
{
	const SerdNode* const subject   = serd_statement_get_subject(statement);
	const SerdNode* const predicate = serd_statement_get_predicate(statement);
	const SerdNode* const object    = serd_statement_get_object(statement);
	const SerdNode* const graph     = serd_statement_get_graph(statement);

	if (!subject || !predicate || !object ||
	    !is_resource(subject) || !is_resource(predicate)) {
		return SERD_ERR_BAD_ARG;
	}

#define TRY(write_result) \
	if (!(write_result)) { \
		return SERD_ERR_UNKNOWN; \
	}

	switch (writer->syntax) {
	case SERD_NTRIPLES:
	case SERD_NQUADS:
		TRY(write_node(writer, subject, SERD_SUBJECT, flags));
		sink(" ", 1, writer);
		TRY(write_node(writer, predicate, SERD_PREDICATE, flags));
		sink(" ", 1, writer);
		TRY(write_node(writer, object, SERD_OBJECT, flags));
		if (writer->syntax == SERD_NQUADS && graph) {
			sink(" ", 1, writer);
			TRY(write_node(writer, graph, SERD_GRAPH, flags));
		}
		sink(" .\n", 3, writer);
		return SERD_SUCCESS;
	default:
		break;
	}

	if ((graph && !serd_node_equals(graph, writer->context.graph)) ||
	    (!graph && ctx(writer, SERD_GRAPH))) {
		if (ctx(writer, SERD_SUBJECT)) {
			write_sep(writer, SEP_END_S);
		}
		if (ctx(writer, SERD_GRAPH)) {
			write_sep(writer, SEP_GRAPH_END);
		}
		if (!writer->empty) {
			write_newline(writer); // Blank line between top level items
		}

		reset_context(writer, true);
		if (graph) {
			TRY(write_node(writer, graph, SERD_GRAPH, flags));
			write_sep(writer, SEP_GRAPH_BEGIN);
			serd_node_set(&writer->context.graph, graph);
		}
	}

	if (writer->context.type == CTX_LIST) {
		if (write_list_obj(writer, flags, predicate, object)) {
			// Reached end of list
			free_context(writer);
			writer->context = *anon_stack_top(writer);
			serd_stack_pop(&writer->anon_stack, sizeof(WriteContext));
			return SERD_SUCCESS;
		}
	} else if (serd_node_equals(subject, writer->context.subject)) {
		if (serd_node_equals(predicate, writer->context.predicate)) {
			// Abbreviate S P
			++writer->indent;
			write_sep(writer, SEP_END_O);
			--writer->indent;
			write_node(writer, object, SERD_OBJECT, flags);
		} else {
			// Abbreviate S
			Sep sep = ctx(writer, SERD_PREDICATE) ? SEP_END_P : SEP_S_P;
			write_sep(writer, sep);
			write_pred(writer, flags, predicate);
			write_node(writer, object, SERD_OBJECT, flags);
		}
	} else {
		// No abbreviation
		if (serd_stack_is_empty(&writer->anon_stack)) {
			if (ctx(writer, SERD_SUBJECT)) {
				write_sep(writer, SEP_END_S); // Terminate last subject
			}
			if (!writer->empty) {
				write_newline(writer); // Blank line between top level items
			}
		}

		if (serd_stack_is_empty(&writer->anon_stack)) {
			write_node(writer, subject, SERD_SUBJECT, flags);
			if (!(flags & (SERD_ANON_S_BEGIN | SERD_LIST_S_BEGIN))) {
				write_sep(writer, SEP_S_P);
			} else if (flags & SERD_ANON_S_BEGIN) {
				write_sep(writer, SEP_ANON_S_P);
			}
		} else {
			write_sep(writer, SEP_ANON_S_P);
		}

		reset_context(writer, false);
		serd_node_set(&writer->context.subject, subject);

		if (!(flags & SERD_LIST_S_BEGIN)) {
			write_pred(writer, flags, predicate);
		}

		write_node(writer, object, SERD_OBJECT, flags);
	}

	if (flags & (SERD_LIST_S_BEGIN)) {
		WriteContext* ctx = (WriteContext*)serd_stack_push(
			&writer->anon_stack, sizeof(WriteContext));
		*ctx = writer->context;
		WriteContext new_context = {
			CTX_LIST,
			serd_node_copy(graph), serd_node_copy(subject), NULL };
		writer->context = new_context;
	}

	if (flags & (SERD_LIST_O_BEGIN)) {
		WriteContext* ctx = (WriteContext*)serd_stack_push(
			&writer->anon_stack, sizeof(WriteContext));
		*ctx = writer->context;
		WriteContext new_context = {
			CTX_LIST,
			serd_node_copy(graph), serd_node_copy(object), NULL };
		writer->context = new_context;
	}

	if (flags & (SERD_ANON_S_BEGIN|SERD_ANON_O_BEGIN)) {
		WriteContext* ctx = (WriteContext*)serd_stack_push(
		        &writer->anon_stack, sizeof(WriteContext));
		*ctx = writer->context;
		WriteContext new_context = {
			(flags & (SERD_LIST_S_BEGIN|SERD_LIST_O_BEGIN))
			? CTX_LIST : CTX_BLANK,
			serd_node_copy(graph), serd_node_copy(subject), NULL };
		if ((flags & SERD_ANON_S_BEGIN)) {
			new_context.predicate = serd_node_copy(predicate);
		}
		writer->context = new_context;
	} else {
		serd_node_set(&writer->context.graph, graph);
		serd_node_set(&writer->context.subject, subject);
		serd_node_set(&writer->context.predicate, predicate);
	}

	return SERD_SUCCESS;
}

static SerdStatus
serd_writer_end_anon(SerdWriter*     writer,
                     const SerdNode* node)
{
	if (writer->syntax == SERD_NTRIPLES || writer->syntax == SERD_NQUADS) {
		return SERD_SUCCESS;
	} else if (serd_stack_is_empty(&writer->anon_stack)) {
		return serd_world_errorf(writer->world, SERD_ERR_UNKNOWN,
		                         "unexpected end of anonymous node\n");
	}

	write_sep(writer, SEP_ANON_END);
	free_context(writer);
	writer->context = *anon_stack_top(writer);
	serd_stack_pop(&writer->anon_stack, sizeof(WriteContext));

	if (serd_node_equals(node, writer->context.subject)) {
		// Now-finished anonymous node is the new subject with no other context
		memset(writer->context.predicate, 0, sizeof(SerdNode));
	}

	return SERD_SUCCESS;
}

SerdStatus
serd_writer_finish(SerdWriter* writer)
{
	if (ctx(writer, SERD_SUBJECT)) {
		write_sep(writer, SEP_END_S);
	}
	if (ctx(writer, SERD_GRAPH)) {
		write_sep(writer, SEP_GRAPH_END);
	}
	free_context(writer);
	writer->indent  = 0;
	writer->context = WRITE_CONTEXT_NULL;
	return SERD_SUCCESS;
}

SerdWriter*
serd_writer_new(SerdWorld*     world,
                SerdSyntax     syntax,
                SerdStyleFlags style,
                SerdEnv*       env,
                SerdWriteFunc  write_func,
                void*          stream)
{
	const WriteContext context = WRITE_CONTEXT_NULL;
	SerdWriter*        writer  = (SerdWriter*)calloc(1, sizeof(SerdWriter));
	writer->world        = world;
	writer->syntax       = syntax;
	writer->style        = style;
	writer->env          = env;
	writer->root_node    = NULL;
	writer->root_uri     = SERD_URI_NULL;
	writer->anon_stack   = serd_stack_new(SERD_PAGE_SIZE);
	writer->write_func   = write_func;
	writer->stream       = stream;
	writer->context      = context;
	writer->empty        = true;

	writer->iface.handle    = writer;
	writer->iface.base      = (SerdBaseSink)serd_writer_set_base_uri;
	writer->iface.prefix    = (SerdPrefixSink)serd_writer_set_prefix;
	writer->iface.statement = (SerdStatementSink)serd_writer_write_statement;
	writer->iface.end       = (SerdEndSink)serd_writer_end_anon;

	return writer;
}

void
serd_writer_chop_blank_prefix(SerdWriter* writer,
                              const char* prefix)
{
	free(writer->bprefix);
	writer->bprefix_len = 0;
	writer->bprefix     = NULL;
	if (prefix) {
		writer->bprefix_len = strlen(prefix);
		writer->bprefix     = (char*)malloc(writer->bprefix_len + 1);
		memcpy(writer->bprefix, prefix, writer->bprefix_len + 1);
	}
}

SerdStatus
serd_writer_set_base_uri(SerdWriter*     writer,
                         const SerdNode* uri)
{
	if (!serd_env_set_base_uri(writer->env, uri)) {
		if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
			if (ctx(writer, SERD_GRAPH) || ctx(writer, SERD_SUBJECT)) {
				sink(" .\n\n", 4, writer);
				reset_context(writer, true);
			}
			sink("@base <", 7, writer);
			sink(serd_node_get_string(uri), uri->n_bytes, writer);
			sink("> .\n", 4, writer);
		}
		writer->indent = 0;
		return reset_context(writer, true);
	}
	return SERD_ERR_UNKNOWN;
}

SerdStatus
serd_writer_set_root_uri(SerdWriter*     writer,
                         const SerdNode* uri)
{
	serd_node_free(writer->root_node);
	if (uri) {
		writer->root_node = serd_node_copy(uri);
		serd_uri_parse(serd_node_get_string(writer->root_node),
		               &writer->root_uri);
	} else {
		writer->root_node = NULL;
		writer->root_uri  = SERD_URI_NULL;
	}
	return SERD_SUCCESS;
}

SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri)
{
	if (!serd_env_set_prefix(writer->env, name, uri)) {
		if (writer->syntax == SERD_TURTLE || writer->syntax == SERD_TRIG) {
			if (ctx(writer, SERD_GRAPH) || ctx(writer, SERD_SUBJECT)) {
				sink(" .\n\n", 4, writer);
				reset_context(writer, true);
			}
			sink("@prefix ", 8, writer);
			sink(serd_node_get_string(name), name->n_bytes, writer);
			sink(": <", 3, writer);
			write_uri_from_node(writer, uri);
			sink("> .\n", 4, writer);
		}
		writer->indent = 0;
		return reset_context(writer, true);
	}
	return SERD_ERR_UNKNOWN;
}

void
serd_writer_free(SerdWriter* writer)
{
	serd_writer_finish(writer);
	serd_stack_free(&writer->anon_stack);
	free(writer->bprefix);
	serd_node_free(writer->root_node);
	free(writer);
}

const SerdSink*
serd_writer_get_sink(SerdWriter* writer)
{
	return &writer->iface;
}

SerdEnv*
serd_writer_get_env(SerdWriter* writer)
{
	return writer->env;
}

size_t
serd_buffer_sink(const void* buf, size_t size, size_t nmemb, void* stream)
{
	assert(size == 1);
	(void)size;

	SerdBuffer* buffer = (SerdBuffer*)stream;
	buffer->buf = (char*)realloc(buffer->buf, buffer->len + nmemb);
	memcpy((uint8_t*)buffer->buf + buffer->len, buf, nmemb);
	buffer->len += nmemb;
	return nmemb;
}

char*
serd_buffer_sink_finish(SerdBuffer* stream)
{
	serd_buffer_sink("", 1, 1, stream);
	return (char*)stream->buf;
}
