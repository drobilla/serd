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

#include "serd_internal.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

#define TRY_THROW(exp) if (!(exp)) goto except;
#define TRY_RET(exp)   if (!(exp)) return 0;

#ifdef SERD_STACK_CHECK
#    define SERD_STACK_ASSERT_TOP(reader, ref) \
            assert(ref == reader->allocs[reader->n_allocs - 1]);
#else
#    define SERD_STACK_ASSERT_TOP(reader, ref)
#endif

typedef struct {
	const uint8_t* filename;
	unsigned       line;
	unsigned       col;
} Cursor;

typedef uint32_t uchar;

/* Reference to a node in the stack (we can not use pointers since the
   stack may be reallocated, invalidating any pointers to elements).
*/
typedef size_t Ref;

typedef struct {
	Ref                 graph;
	Ref                 subject;
	Ref                 predicate;
	Ref                 object;
	Ref                 datatype;
	Ref                 lang;
	SerdStatementFlags* flags;
} ReadContext;

struct SerdReaderImpl {
	void*             handle;
	void              (*free_handle)(void* ptr);
	SerdBaseSink      base_sink;
	SerdPrefixSink    prefix_sink;
	SerdStatementSink statement_sink;
	SerdEndSink       end_sink;
	SerdErrorSink     error_sink;
	void*             error_handle;
	Ref               rdf_first;
	Ref               rdf_rest;
	Ref               rdf_nil;
	SerdNode          default_graph;
	SerdByteSource    source;
	SerdStack         stack;
	SerdSyntax        syntax;
	unsigned          next_id;
	Cursor            cur;
	SerdStatus        status;
	uint8_t*          buf;
	uint8_t*          bprefix;
	size_t            bprefix_len;
	bool              strict;     ///< True iff strict parsing
	bool              eof;
	bool              seen_genid;
#ifdef SERD_STACK_CHECK
	Ref*              allocs;     ///< Stack of push offsets
	size_t            n_allocs;   ///< Number of stack pushes
#endif
};

static inline bool
supports_fancy_literals(const SerdReader* reader)
{
	return reader->syntax == SERD_TURTLE || reader->syntax == SERD_TRIG;
}

static inline bool
supports_relative_iris(const SerdReader* reader)
{
	return reader->syntax == SERD_TURTLE || reader->syntax == SERD_TRIG;
}

static int
r_err(SerdReader* reader, SerdStatus st, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const SerdError e = {
		st, reader->cur.filename, reader->cur.line, reader->cur.col, fmt, &args
	};
	serd_error(reader->error_sink, reader->error_handle, &e);
	va_end(args);
	return 0;
}

/** fread-like wrapper for getc (which is faster). */
static size_t
serd_file_read_byte(void* buf, size_t size, size_t nmemb, void* stream)
{
	const int c = getc((FILE*)stream);
	if (c == EOF) {
		*((uint8_t*)buf) = 0;
		return 0;
	}
	*((uint8_t*)buf) = (uint8_t)c;
	return 1;
}

static inline uint8_t
peek_byte(SerdReader* reader)
{
	return serd_byte_source_peek(&reader->source);
}

static inline uint8_t
eat_byte_safe(SerdReader* reader, const uint8_t byte)
{
	assert(peek_byte(reader) == byte);
	switch (byte) {
	case '\0': reader->eof = (byte != '\0'); break;
	case '\n': ++reader->cur.line; reader->cur.col = 0; break;
	default:   ++reader->cur.col;
	}

	reader->status = serd_byte_source_advance(&reader->source);
	return byte;
}

static inline uint8_t
eat_byte_check(SerdReader* reader, const uint8_t byte)
{
	const uint8_t c = peek_byte(reader);
	if (c != byte) {
		return r_err(reader, SERD_ERR_BAD_SYNTAX,
		             "expected `%c', not `%c'\n", byte, c);
	}
	return eat_byte_safe(reader, byte);
}

static inline bool
eat_string(SerdReader* reader, const char* str, unsigned n)
{
	bool bad = false;
	for (unsigned i = 0; i < n; ++i) {
		bad |= eat_byte_check(reader, ((const uint8_t*)str)[i]);
	}
	return bad;
}

static Ref
push_node_padded(SerdReader* reader, size_t maxlen,
                 SerdType type, const char* str, size_t n_bytes)
{
	void* mem = serd_stack_push_aligned(
		&reader->stack, sizeof(SerdNode) + maxlen + 1, sizeof(SerdNode));

	SerdNode* const node = (SerdNode*)mem;
	node->n_bytes = node->n_chars = n_bytes;
	node->flags   = 0;
	node->type    = type;
	node->buf     = NULL;

	uint8_t* buf = (uint8_t*)(node + 1);
	memcpy(buf, str, n_bytes + 1);

#ifdef SERD_STACK_CHECK
	reader->allocs = realloc(
		reader->allocs, sizeof(reader->allocs) * (++reader->n_allocs));
	reader->allocs[reader->n_allocs - 1] = ((uint8_t*)mem - reader->stack.buf);
#endif
	return (uint8_t*)node - reader->stack.buf;
}

static Ref
push_node(SerdReader* reader, SerdType type, const char* str, size_t n_bytes)
{
	return push_node_padded(reader, n_bytes, type, str, n_bytes);
}

static inline SerdNode*
deref(SerdReader* reader, const Ref ref)
{
	if (ref) {
		SerdNode* node = (SerdNode*)(reader->stack.buf + ref);
		node->buf = (uint8_t*)node + sizeof(SerdNode);
		return node;
	}
	return NULL;
}

static inline void
push_byte(SerdReader* reader, Ref ref, const uint8_t c)
{
	SERD_STACK_ASSERT_TOP(reader, ref);
	uint8_t* const  s    = serd_stack_push(&reader->stack, 1);
	SerdNode* const node = (SerdNode*)(reader->stack.buf + ref);
	++node->n_bytes;
	if (!(c & 0x80)) {  // Starts with 0 bit, start of new character
		++node->n_chars;
	}
	*(s - 1) = c;
	*s       = '\0';
}

static inline void
push_bytes(SerdReader* reader, Ref ref, const uint8_t* bytes, unsigned len)
{
	for (unsigned i = 0; i < len; ++i) {
		push_byte(reader, ref, bytes[i]);
	}
}

static Ref
pop_node(SerdReader* reader, Ref ref)
{
	if (ref && ref != reader->rdf_first && ref != reader->rdf_rest
	    && ref != reader->rdf_nil) {
#ifdef SERD_STACK_CHECK
		SERD_STACK_ASSERT_TOP(reader, ref);
		--reader->n_allocs;
#endif
		SerdNode* const node = deref(reader, ref);
		uint8_t* const  top  = reader->stack.buf + reader->stack.size;
		serd_stack_pop_aligned(&reader->stack, top - (uint8_t*)node);
	}
	return 0;
}

static inline bool
emit_statement(SerdReader* reader, ReadContext ctx, Ref o, Ref d, Ref l)
{
	SerdNode* graph = deref(reader, ctx.graph);
	if (!graph && reader->default_graph.buf) {
		graph = &reader->default_graph;
	}
	bool ret = !reader->statement_sink ||
		!reader->statement_sink(
			reader->handle, *ctx.flags, graph,
			deref(reader, ctx.subject), deref(reader, ctx.predicate),
			deref(reader, o), deref(reader, d), deref(reader, l));
	*ctx.flags &= SERD_ANON_CONT|SERD_LIST_CONT;  // Preserve only cont flags
	return ret;
}

static bool
read_collection(SerdReader* reader, ReadContext ctx, Ref* dest);

static bool
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot);

static inline uint8_t
read_HEX(SerdReader* reader)
{
	const uint8_t c = peek_byte(reader);
	if (is_digit(c) || in_range(c, 'A', 'F') || in_range(c, 'a', 'f')) {
		return eat_byte_safe(reader, c);
	}
	return r_err(reader, SERD_ERR_BAD_SYNTAX,
	             "invalid hexadecimal digit `%c'\n", c);
}

// Read UCHAR escape, initial \ is already eaten by caller
static inline bool
read_UCHAR(SerdReader* reader, Ref dest, uint32_t* char_code)
{
	const uint8_t b      = peek_byte(reader);
	unsigned      length = 0;
	switch (b) {
	case 'U':
		length = 8;
		break;
	case 'u':
		length = 4;
		break;
	default:
		return false;
	}
	eat_byte_safe(reader, b);

	uint8_t buf[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (unsigned i = 0; i < length; ++i) {
		if (!(buf[i] = read_HEX(reader))) {
			return false;
		}
	}

	uint32_t code;
	sscanf((const char*)buf, "%X", &code);

	unsigned size = 0;
	if (code < 0x00000080) {
		size = 1;
	} else if (code < 0x00000800) {
		size = 2;
	} else if (code < 0x00010000) {
		size = 3;
	} else if (code < 0x00110000) {
		size = 4;
	} else {
		r_err(reader, SERD_ERR_BAD_SYNTAX,
		      "unicode character 0x%X out of range\n", code);
		push_bytes(reader, dest, replacement_char, 3);
		*char_code = 0xFFFD;
		return true;
	}

	// Build output in buf
	// (Note # of bytes = # of leading 1 bits in first byte)
	uint32_t c = code;
	switch (size) {
	case 4:
		buf[3] = 0x80 | (uint8_t)(c & 0x3F);
		c >>= 6;
		c |= (16 << 12);  // set bit 4
	case 3:
		buf[2] = 0x80 | (uint8_t)(c & 0x3F);
		c >>= 6;
		c |= (32 << 6);  // set bit 5
	case 2:
		buf[1] = 0x80 | (uint8_t)(c & 0x3F);
		c >>= 6;
		c |= 0xC0;  // set bits 6 and 7
	case 1:
		buf[0] = (uint8_t)c;
	}

	push_bytes(reader, dest, buf, size);
	*char_code = code;
	return true;
}

// Read ECHAR escape, initial \ is already eaten by caller
static inline bool
read_ECHAR(SerdReader* reader, Ref dest, SerdNodeFlags* flags)
{
	const uint8_t c = peek_byte(reader);
	switch (c) {
	case 't':
		eat_byte_safe(reader, 't');
		push_byte(reader, dest, '\t');
		return true;
	case 'b':
		eat_byte_safe(reader, 'b');
		push_byte(reader, dest, '\b');
		return true;
	case 'n':
		*flags |= SERD_HAS_NEWLINE;
		eat_byte_safe(reader, 'n');
		push_byte(reader, dest, '\n');
		return true;
	case 'r':
		*flags |= SERD_HAS_NEWLINE;
		eat_byte_safe(reader, 'r');
		push_byte(reader, dest, '\r');
		return true;
	case 'f':
		eat_byte_safe(reader, 'f');
		push_byte(reader, dest, '\f');
		return true;
	case '\\': case '"': case '\'':
		push_byte(reader, dest, eat_byte_safe(reader, c));
		return true;
	default:
		return false;
	}
}

static inline SerdStatus
bad_char(SerdReader* reader, Ref dest, const char* fmt, uint8_t c)
{
	r_err(reader, SERD_ERR_BAD_SYNTAX, fmt, c);
	push_bytes(reader, dest, replacement_char, 3);

	// Skip bytes until the next start byte
	for (uint8_t b = peek_byte(reader); (b & 0x80);) {
		eat_byte_safe(reader, b);
		b = peek_byte(reader);
	}

	return SERD_FAILURE;
}

static SerdStatus
read_utf8_character(SerdReader* reader, Ref dest, uint8_t c)
{
	const uint32_t size = utf8_num_bytes(c);
	if (size <= 1 || size > 4) {
		return bad_char(reader, dest, "invalid UTF-8 start 0x%X\n", c);
	}

	uint8_t bytes[4];
	bytes[0] = c;

	// Check character validity
	for (unsigned i = 1; i < size; ++i) {
		if (((bytes[i] = peek_byte(reader)) & 0x80) == 0) {
			return bad_char(reader, dest, "invalid UTF-8 continuation 0x%X\n",
			                bytes[i]);
		}
		eat_byte_safe(reader, bytes[i]);
	}

	// Emit character
	push_bytes(reader, dest, bytes, size);
	return SERD_SUCCESS;
}

// Read one character (possibly multi-byte)
// The first byte, c, has already been eaten by caller
static inline SerdStatus
read_character(SerdReader* reader, Ref dest, SerdNodeFlags* flags, uint8_t c)
{
	if (!(c & 0x80)) {
		switch (c) {
		case 0xA: case 0xD:
			*flags |= SERD_HAS_NEWLINE;
			break;
		case '"': case '\'':
			*flags |= SERD_HAS_QUOTE;
			break;
		}
		push_byte(reader, dest, c);
		return SERD_SUCCESS;
	}
	return read_utf8_character(reader, dest, c);
}

// [10] comment ::= '#' ( [^#xA #xD] )*
static void
read_comment(SerdReader* reader)
{
	eat_byte_safe(reader, '#');
	uint8_t c;
	while (((c = peek_byte(reader)) != 0xA) && (c != 0xD) && c) {
		eat_byte_safe(reader, c);
	}
}

// [24] ws ::= #x9 | #xA | #xD | #x20 | comment
static inline bool
read_ws(SerdReader* reader)
{
	const uint8_t c = peek_byte(reader);
	switch (c) {
	case 0x9: case 0xA: case 0xD: case 0x20:
		eat_byte_safe(reader, c);
		return true;
	case '#':
		read_comment(reader);
		return true;
	default:
		return false;
	}
}

static inline bool
read_ws_star(SerdReader* reader)
{
	while (read_ws(reader)) {}
	return true;
}

static inline bool
peek_delim(SerdReader* reader, const char delim)
{
	read_ws_star(reader);
	return peek_byte(reader) == delim;
}

static inline bool
eat_delim(SerdReader* reader, const char delim)
{
	if (peek_delim(reader, delim)) {
		eat_byte_safe(reader, delim);
		return read_ws_star(reader);
	}
	return false;
}

// STRING_LITERAL_LONG_QUOTE and STRING_LITERAL_LONG_SINGLE_QUOTE
// Initial triple quotes are already eaten by caller
static Ref
read_STRING_LITERAL_LONG(SerdReader* reader, SerdNodeFlags* flags, uint8_t q)
{
	Ref ref = push_node(reader, SERD_LITERAL, "", 0);
	while (!reader->status) {
		const uint8_t c = peek_byte(reader);
		uint32_t      code;
		switch (c) {
		case '\\':
			eat_byte_safe(reader, c);
			if (!read_ECHAR(reader, ref, flags) &&
			    !read_UCHAR(reader, ref, &code)) {
				r_err(reader, SERD_ERR_BAD_SYNTAX,
				      "invalid escape `\\%c'\n", peek_byte(reader));
				return pop_node(reader, ref);
			}
			break;
		default:
			if (c == q) {
				eat_byte_safe(reader, q);
				const uint8_t q2 = eat_byte_safe(reader, peek_byte(reader));
				const uint8_t q3 = peek_byte(reader);
				if (q2 == q && q3 == q) {  // End of string
					eat_byte_safe(reader, q3);
					return ref;
				}
				*flags |= SERD_HAS_QUOTE;
				push_byte(reader, ref, c);
				read_character(reader, ref, flags, q2);
			} else {
				read_character(reader, ref, flags, eat_byte_safe(reader, c));
			}
		}
	}
	return ref;
}

// STRING_LITERAL_QUOTE and STRING_LITERAL_SINGLE_QUOTE
// Initial quote is already eaten by caller
static Ref
read_STRING_LITERAL(SerdReader* reader, SerdNodeFlags* flags, uint8_t q)
{
	Ref ref = push_node(reader, SERD_LITERAL, "", 0);
	while (!reader->status) {
		const uint8_t c    = peek_byte(reader);
		uint32_t      code = 0;
		switch (c) {
		case '\n': case '\r':
			r_err(reader, SERD_ERR_BAD_SYNTAX, "line end in short string\n");
			return pop_node(reader, ref);
		case '\\':
			eat_byte_safe(reader, c);
			if (!read_ECHAR(reader, ref, flags) &&
			    !read_UCHAR(reader, ref, &code)) {
				r_err(reader, SERD_ERR_BAD_SYNTAX,
				      "invalid escape `\\%c'\n", peek_byte(reader));
				return pop_node(reader, ref);
			}
			break;
		default:
			if (c == q) {
				eat_byte_check(reader, q);
				return ref;
			} else {
				read_character(reader, ref, flags, eat_byte_safe(reader, c));
			}
		}
	}
	eat_byte_check(reader, q);
	return ref;
}

static Ref
read_String(SerdReader* reader, SerdNodeFlags* flags)
{
	const uint8_t q1 = peek_byte(reader);
	eat_byte_safe(reader, q1);

	const uint8_t q2 = peek_byte(reader);
	if (q2 != q1) {  // Short string (not triple quoted)
		return read_STRING_LITERAL(reader, flags, q1);
	}

	eat_byte_safe(reader, q2);
	const uint8_t q3 = peek_byte(reader);
	if (q3 != q1) {  // Empty short string ("" or '')
		return push_node(reader, SERD_LITERAL, "", 0);
	}

	if (!supports_fancy_literals(reader)) {
		return r_err(reader, SERD_ERR_BAD_SYNTAX,
		             "syntax does not support long literals\n");
	}

	eat_byte_safe(reader, q3);
	return read_STRING_LITERAL_LONG(reader, flags, q1);
}

static SerdStatus
read_PN_CHARS_BASE(SerdReader* reader, Ref dest)
{
	const uint8_t c = peek_byte(reader);
	if ((c & 0x80)) {  // Multi-byte character
		return read_utf8_character(reader, dest, eat_byte_safe(reader, c));
	} else if (is_alpha(c)) {
		push_byte(reader, dest, eat_byte_safe(reader, c));
		return SERD_SUCCESS;
	}
	return SERD_FAILURE;
}

static SerdStatus
read_PN_CHARS(SerdReader* reader, Ref dest)
{
	const uint8_t c = peek_byte(reader);
	if ((c & 0x80)) {  // Multi-byte character
		return read_utf8_character(reader, dest, eat_byte_safe(reader, c));
	} else if (is_alpha(c) || is_digit(c) || c == '_' || c == '-') {
		push_byte(reader, dest, eat_byte_safe(reader, c));
		return SERD_SUCCESS;
	}
	return SERD_FAILURE;
}

static bool
read_PERCENT(SerdReader* reader, Ref dest)
{
	push_byte(reader, dest, eat_byte_safe(reader, '%'));
	const uint8_t h1 = read_HEX(reader);
	const uint8_t h2 = read_HEX(reader);
	if (h1 && h2) {
		push_byte(reader, dest, h1);
		push_byte(reader, dest, h2);
		return true;
	}
	return false;
}

static SerdStatus
read_PLX(SerdReader* reader, Ref dest)
{
	uint8_t c = peek_byte(reader);
	switch (c) {
	case '%':
		if (!read_PERCENT(reader, dest)) {
			return SERD_ERR_BAD_SYNTAX;
		}
		return SERD_SUCCESS;
	case '\\':
		eat_byte_safe(reader, c);
		if (is_alpha(c = peek_byte(reader))) {
			// Escapes like \u \n etc. are not supported
			return SERD_ERR_BAD_SYNTAX;
		}
		// Allow escaping of pretty much any other character
		push_byte(reader, dest, eat_byte_safe(reader, c));
		return SERD_SUCCESS;
	default:
		return SERD_FAILURE;
	}
}

static SerdStatus
read_PN_LOCAL(SerdReader* reader, Ref dest, bool* ate_dot)
{
	uint8_t    c = peek_byte(reader);
	SerdStatus st;
	switch (c) {
	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9': case ':': case '_':
		push_byte(reader, dest, eat_byte_safe(reader, c));
		break;
	default:
		if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
			return st;
		} else if (st != SERD_SUCCESS && read_PN_CHARS_BASE(reader, dest)) {
			return SERD_FAILURE;
		}
	}

	while ((c = peek_byte(reader))) {  // Middle: (PN_CHARS | '.' | ';')*
		if (c == '.' || c == ':') {
			push_byte(reader, dest, eat_byte_safe(reader, c));
		} else if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
			return st;
		} else if (st != SERD_SUCCESS && read_PN_CHARS(reader, dest)) {
			break;
		}
	}

	SerdNode* const n = deref(reader, dest);
	if (n->buf[n->n_bytes - 1] == '.') {
		// Ate trailing dot, pop it from stack/node and inform caller
		--n->n_bytes;
		serd_stack_pop(&reader->stack, 1);
		*ate_dot = true;
	}

	return SERD_SUCCESS;
}

// Read the remainder of a PN_PREFIX after some initial characters
static SerdStatus
read_PN_PREFIX_tail(SerdReader* reader, Ref dest)
{
	uint8_t c;
	while ((c = peek_byte(reader))) {  // Middle: (PN_CHARS | '.')*
		if (c == '.') {
			push_byte(reader, dest, eat_byte_safe(reader, c));
		} else if (read_PN_CHARS(reader, dest)) {
			break;
		}
	}

	const SerdNode* const n = deref(reader, dest);
	if (n->buf[n->n_bytes - 1] == '.' && read_PN_CHARS(reader, dest)) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "prefix ends with `.'\n");
		return SERD_ERR_BAD_SYNTAX;
	}

	return SERD_SUCCESS;
}

static SerdStatus
read_PN_PREFIX(SerdReader* reader, Ref dest)
{
	if (!read_PN_CHARS_BASE(reader, dest)) {
		return read_PN_PREFIX_tail(reader, dest);
	}
	return SERD_FAILURE;
}

static Ref
read_LANGTAG(SerdReader* reader)
{
	uint8_t c = peek_byte(reader);
	if (!is_alpha(c)) {
		return r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected `%c'\n", c);
	}
	Ref ref = push_node(reader, SERD_LITERAL, "", 0);
	push_byte(reader, ref, eat_byte_safe(reader, c));
	while ((c = peek_byte(reader)) && is_alpha(c)) {
		push_byte(reader, ref, eat_byte_safe(reader, c));
	}
	while (peek_byte(reader) == '-') {
		push_byte(reader, ref, eat_byte_safe(reader, '-'));
		while ((c = peek_byte(reader)) && (is_alpha(c) || is_digit(c))) {
			push_byte(reader, ref, eat_byte_safe(reader, c));
		}
	}
	return ref;
}

static bool
read_IRIREF_scheme(SerdReader* reader, Ref dest)
{
	uint8_t c = peek_byte(reader);
	if (!isalpha(c)) {
		return r_err(reader, SERD_ERR_BAD_SYNTAX,
		             "bad IRI scheme start `%c'\n", c);
	}

	while ((c = peek_byte(reader))) {
		if (c == '>') {
			return r_err(reader, SERD_ERR_BAD_SYNTAX, "missing IRI scheme\n");
		} else if (!is_uri_scheme_char(c)) {
			return r_err(reader, SERD_ERR_BAD_SYNTAX,
			             "bad IRI scheme char `%X'\n", c);
		}

		push_byte(reader, dest, eat_byte_safe(reader, c));
		if (c == ':') {
			return true;  // End of scheme
		}
	}

	return false;
}

static Ref
read_IRIREF(SerdReader* reader)
{
	TRY_RET(eat_byte_check(reader, '<'));
	Ref ref = push_node(reader, SERD_URI, "", 0);
	if (!supports_relative_iris(reader) && !read_IRIREF_scheme(reader, ref)) {
		return pop_node(reader, ref);
	}

	uint32_t code = 0;
	while (!reader->status) {
		const uint8_t c = eat_byte_safe(reader, peek_byte(reader));
		switch (c) {
		case '"': case '<': case '^': case '`': case '{': case '|': case '}':
			r_err(reader, SERD_ERR_BAD_SYNTAX,
			      "invalid IRI character `%c'\n", c);
			return pop_node(reader, ref);
		case '>':
			return ref;
		case '\\':
			if (!read_UCHAR(reader, ref, &code)) {
				r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid IRI escape\n");
				return pop_node(reader, ref);
			}
			switch (code) {
			case 0: case ' ': case '<': case '>':
				r_err(reader, SERD_ERR_BAD_SYNTAX,
				      "invalid escaped IRI character %X %c\n", code, code);
				return pop_node(reader, ref);
			}
			break;
		default:
			if (c <= 0x20) {
				if (isprint(c)) {
					r_err(reader, SERD_ERR_BAD_SYNTAX,
					      "invalid IRI character `%c' (escape %%%02X)\n", c, c);
				} else {
					r_err(reader, SERD_ERR_BAD_SYNTAX,
					      "invalid IRI character (escape %%%02X)\n", c, c);
				}
				if (reader->strict) {
					return pop_node(reader, ref);
				}
				reader->status = SERD_FAILURE;
				push_byte(reader, ref, c);
			} else if (!(c & 0x80)) {
				push_byte(reader, ref, c);
			} else if (read_utf8_character(reader, ref, c)) {
				if (reader->strict) {
					return pop_node(reader, ref);
				}
				reader->status = SERD_FAILURE;
			}
		}
	}
	return pop_node(reader, ref);
}

static bool
read_PrefixedName(SerdReader* reader, Ref dest, bool read_prefix, bool* ate_dot)
{
	if (read_prefix && read_PN_PREFIX(reader, dest) > SERD_FAILURE) {
		return false;
	} else if (peek_byte(reader) != ':') {
		return false;
	}

	push_byte(reader, dest, eat_byte_safe(reader, ':'));
	return read_PN_LOCAL(reader, dest, ate_dot) <= SERD_FAILURE;
}

static bool
read_0_9(SerdReader* reader, Ref str, bool at_least_one)
{
	unsigned count = 0;
	for (uint8_t c; is_digit((c = peek_byte(reader))); ++count) {
		push_byte(reader, str, eat_byte_safe(reader, c));
	}
	if (at_least_one && count == 0) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "expected digit\n");
	}
	return count;
}

static bool
read_number(SerdReader* reader, Ref* dest, Ref* datatype, bool* ate_dot)
{
	#define XSD_DECIMAL NS_XSD "decimal"
	#define XSD_DOUBLE  NS_XSD "double"
	#define XSD_INTEGER NS_XSD "integer"
	Ref     ref         = push_node(reader, SERD_LITERAL, "", 0);
	uint8_t c           = peek_byte(reader);
	bool    has_decimal = false;
	if (c == '-' || c == '+') {
		push_byte(reader, ref, eat_byte_safe(reader, c));
	}
	if ((c = peek_byte(reader)) == '.') {
		has_decimal = true;
		// decimal case 2 (e.g. '.0' or `-.0' or `+.0')
		push_byte(reader, ref, eat_byte_safe(reader, c));
		TRY_THROW(read_0_9(reader, ref, true));
	} else {
		// all other cases ::= ( '-' | '+' ) [0-9]+ ( . )? ( [0-9]+ )? ...
		TRY_THROW(is_digit(c));
		read_0_9(reader, ref, true);
		if ((c = peek_byte(reader)) == '.') {
			has_decimal = true;

			// Annoyingly, dot can be end of statement, so tentatively eat
			eat_byte_safe(reader, c);
			c = peek_byte(reader);
			if (!is_digit(c) && c != 'e' && c != 'E') {
				*dest    = ref;
				*ate_dot = true;  // Force caller to deal with stupid grammar
				return true;  // Next byte is not a number character, done
			}

			push_byte(reader, ref, '.');
			read_0_9(reader, ref, false);
		}
	}
	c = peek_byte(reader);
	if (c == 'e' || c == 'E') {
		// double
		push_byte(reader, ref, eat_byte_safe(reader, c));
		switch ((c = peek_byte(reader))) {
		case '+': case '-':
			push_byte(reader, ref, eat_byte_safe(reader, c));
		default: break;
		}
		TRY_THROW(read_0_9(reader, ref, true));
		*datatype = push_node(reader, SERD_URI,
		                      XSD_DOUBLE, sizeof(XSD_DOUBLE) - 1);
	} else if (has_decimal) {
		*datatype = push_node(reader, SERD_URI,
		                      XSD_DECIMAL, sizeof(XSD_DECIMAL) - 1);
	} else {
		*datatype = push_node(reader, SERD_URI,
		                      XSD_INTEGER, sizeof(XSD_INTEGER) - 1);
	}
	*dest = ref;
	return true;
except:
	pop_node(reader, *datatype);
	pop_node(reader, ref);
	return false;
}

static bool
read_iri(SerdReader* reader, Ref* dest, bool* ate_dot)
{
	switch (peek_byte(reader)) {
	case '<':
		*dest = read_IRIREF(reader);
		return true;
	default:
		*dest = push_node(reader, SERD_CURIE, "", 0);
		return read_PrefixedName(reader, *dest, true, ate_dot);
	}
}

static bool
read_literal(SerdReader* reader, Ref* dest,
             Ref* datatype, Ref* lang, SerdNodeFlags* flags, bool* ate_dot)
{
	Ref str = read_String(reader, flags);
	if (!str) {
		return false;
	}

	switch (peek_byte(reader)) {
	case '@':
		eat_byte_safe(reader, '@');
		TRY_THROW(*lang = read_LANGTAG(reader));
		break;
	case '^':
		eat_byte_safe(reader, '^');
		eat_byte_check(reader, '^');
		TRY_THROW(read_iri(reader, datatype, ate_dot));
		break;
	}
	*dest = str;
	return true;
except:
	*datatype = pop_node(reader, *datatype);
	*lang     = pop_node(reader, *lang);
	pop_node(reader, str);
	return false;
}

inline static bool
is_token_end(uint8_t c)
{
	switch (c) {
	case 0x9: case 0xA: case 0xD: case 0x20: case '\0':
	case '#': case '.': case ';': case '<':
		return true;
	default:
		return false;
	}
}

static bool
read_verb(SerdReader* reader, Ref* dest)
{
	if (peek_byte(reader) == '<') {
		return (*dest = read_IRIREF(reader));
	}

	/* Either a qname, or "a".  Read the prefix first, and if it is in fact
	   "a", produce that instead.
	*/
	*dest = push_node(reader, SERD_CURIE, "", 0);
	SerdNode*        node    = deref(reader, *dest);
	const SerdStatus st      = read_PN_PREFIX(reader, *dest);
	bool             ate_dot = false;
	if (!st && node->n_bytes == 1 && node->buf[0] == 'a' &&
	    is_token_end(peek_byte(reader))) {
		pop_node(reader, *dest);
		return (*dest = push_node(reader, SERD_URI, NS_RDF "type", 47));
	} else if (st > SERD_FAILURE ||
	           !read_PrefixedName(reader, *dest, false, &ate_dot) ||
	           ate_dot) {
		return (*dest = pop_node(reader, *dest));
	}

	return true;
}

static Ref
read_BLANK_NODE_LABEL(SerdReader* reader, bool* ate_dot)
{
	eat_byte_safe(reader, '_');
	eat_byte_check(reader, ':');
	Ref ref = push_node(reader, SERD_BLANK,
	                    reader->bprefix ? (char*)reader->bprefix : "",
	                    reader->bprefix_len);

	uint8_t c = peek_byte(reader);  // First: (PN_CHARS | '_' | [0-9])
	if (is_digit(c) || c == '_') {
		push_byte(reader, ref, eat_byte_safe(reader, c));
	} else if (read_PN_CHARS(reader, ref)) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid name start character\n");
		return pop_node(reader, ref);
	}

	while ((c = peek_byte(reader))) {  // Middle: (PN_CHARS | '.')*
		if (c == '.') {
			push_byte(reader, ref, eat_byte_safe(reader, c));
		} else if (read_PN_CHARS(reader, ref)) {
			break;
		}
	}

	SerdNode* n = deref(reader, ref);
	if (n->buf[n->n_bytes - 1] == '.' && read_PN_CHARS(reader, ref)) {
		// Ate trailing dot, pop it from stack/node and inform caller
		--n->n_bytes;
		serd_stack_pop(&reader->stack, 1);
		*ate_dot = true;
	}

	if (reader->syntax == SERD_TURTLE) {
		if (is_digit(n->buf[reader->bprefix_len + 1])) {
			if ((n->buf[reader->bprefix_len]) == 'b') {
				((char*)n->buf)[reader->bprefix_len] = 'B';  // Prevent clash
				reader->seen_genid = true;
			} else if (reader->seen_genid &&
			           n->buf[reader->bprefix_len] == 'B') {
				r_err(reader, SERD_ERR_ID_CLASH,
				      "found both `b' and `B' blank IDs, prefix required\n");
				return pop_node(reader, ref);
			}
		}
	}
	return ref;
}

static void
set_blank_id(SerdReader* reader, Ref ref, size_t buf_size)
{
	SerdNode*   node   = deref(reader, ref);
	const char* prefix = reader->bprefix ? (const char*)reader->bprefix : "";
	node->n_bytes = node->n_chars = snprintf(
		(char*)node->buf, buf_size, "%sb%u", prefix, reader->next_id++);
}

static size_t
genid_size(SerdReader* reader)
{
	return reader->bprefix_len + 1 + 10 + 1;  // + "b" + UINT32_MAX + \0
}

static Ref
blank_id(SerdReader* reader)
{
	Ref ref = push_node_padded(reader, genid_size(reader), SERD_BLANK, "", 0);
	set_blank_id(reader, ref, genid_size(reader));
	return ref;
}

static Ref
read_blankName(SerdReader* reader)
{
	eat_byte_safe(reader, '=');
	if (eat_byte_check(reader, '=') != '=') {
		return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected `='\n");
	}

	Ref  subject = 0;
	bool ate_dot = false;
	read_ws_star(reader);
	read_iri(reader, &subject, &ate_dot);
	return subject;
}

static bool
read_anon(SerdReader* reader, ReadContext ctx, bool subject, Ref* dest)
{
	const SerdStatementFlags old_flags = *ctx.flags;
	bool empty;
	eat_byte_safe(reader, '[');
	if ((empty = peek_delim(reader, ']'))) {
		*ctx.flags |= (subject) ? SERD_EMPTY_S : SERD_EMPTY_O;
	} else {
		*ctx.flags |= (subject) ? SERD_ANON_S_BEGIN : SERD_ANON_O_BEGIN;
		if (peek_delim(reader, '=')) {
			if (!(*dest = read_blankName(reader)) ||
			    !eat_delim(reader, ';')) {
				return false;
			}
		}
	}

	if (!*dest) {
		*dest = blank_id(reader);
	}
	if (ctx.subject) {
		TRY_RET(emit_statement(reader, ctx, *dest, 0, 0));
	}

	ctx.subject = *dest;
	if (!empty) {
		*ctx.flags &= ~(SERD_LIST_CONT);
		if (!subject) {
			*ctx.flags |= SERD_ANON_CONT;
		}
		bool ate_dot_in_list = false;
		read_predicateObjectList(reader, ctx, &ate_dot_in_list);
		if (ate_dot_in_list) {
			return r_err(reader, SERD_ERR_BAD_SYNTAX, "`.' inside blank\n");
		}
		read_ws_star(reader);
		if (reader->end_sink) {
			reader->end_sink(reader->handle, deref(reader, *dest));
		}
		*ctx.flags = old_flags;
	}
	return (eat_byte_check(reader, ']') == ']');
}

/* If emit is true: recurses, calling statement_sink for every statement
   encountered, and leaves stack in original calling state (i.e. pops
   everything it pushes). */
static bool
read_object(SerdReader* reader, ReadContext* ctx, bool emit, bool* ate_dot)
{
	static const char* const XSD_BOOLEAN     = NS_XSD "boolean";
	static const size_t      XSD_BOOLEAN_LEN = 40;

#ifndef NDEBUG
	const size_t orig_stack_size = reader->stack.size;
#endif

	bool          ret      = false;
	bool          simple   = (ctx->subject != 0);
	SerdNode*     node     = NULL;
	Ref           o        = 0;
	Ref           datatype = 0;
	Ref           lang     = 0;
	uint32_t      flags    = 0;
	const uint8_t c        = peek_byte(reader);
	if (!supports_fancy_literals(reader)) {
		switch (c) {
		case '"': case ':': case '<': case '_': break;
		default: return r_err(reader, SERD_ERR_BAD_SYNTAX,
		                      "expected: ':', '<', or '_'\n");
		}
	}
	switch (c) {
	case '\0':
	case ')':
		return false;
	case '[':
		simple = false;
		TRY_THROW(ret = read_anon(reader, *ctx, false, &o));
		break;
	case '(':
		simple = false;
		TRY_THROW(ret = read_collection(reader, *ctx, &o));
		break;
	case '_':
		TRY_THROW(ret = (o = read_BLANK_NODE_LABEL(reader, ate_dot)));
		break;
	case '<': case ':':
		TRY_THROW(ret = read_iri(reader, &o, ate_dot));
		break;
	case '+': case '-': case '.': case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7': case '8': case '9':
		TRY_THROW(ret = read_number(reader, &o, &datatype, ate_dot));
		break;
	case '\"':
	case '\'':
		TRY_THROW(ret = read_literal(reader, &o, &datatype, &lang, &flags, ate_dot));
		break;
	default:
		/* Either a boolean literal, or a qname.  Read the prefix first, and if
		   it is in fact a "true" or "false" literal, produce that instead.
		*/
		node = deref(reader, o = push_node(reader, SERD_CURIE, "", 0));
		while (!read_PN_CHARS_BASE(reader, o)) {}
		if ((node->n_bytes == 4 && !memcmp(node->buf, "true", 4)) ||
		    (node->n_bytes == 5 && !memcmp(node->buf, "false", 5))) {
			node->type = SERD_LITERAL;
			datatype   = push_node(
				reader, SERD_URI, XSD_BOOLEAN, XSD_BOOLEAN_LEN);
			ret = true;
		} else if (read_PN_PREFIX_tail(reader, o) > SERD_FAILURE) {
			ret = false;
		} else {
			ret = read_PrefixedName(reader, o, false, ate_dot);
		}
	}

	if (simple && o) {
		deref(reader, o)->flags = flags;
	}

	if (ret && emit && simple) {
		ret = emit_statement(reader, *ctx, o, datatype, lang);
	} else if (ret && !emit) {
		ctx->object   = o;
		ctx->datatype = datatype;
		ctx->lang     = lang;
		return true;
	}

except:
	pop_node(reader, lang);
	pop_node(reader, datatype);
	pop_node(reader, o);
#ifndef NDEBUG
	assert(reader->stack.size == orig_stack_size);
#endif
	return ret;
}

static bool
read_objectList(SerdReader* reader, ReadContext ctx, bool* ate_dot)
{
	TRY_RET(read_object(reader, &ctx, true, ate_dot));
	while (!*ate_dot && eat_delim(reader, ',')) {
		TRY_RET(read_object(reader, &ctx, true, ate_dot));
	}
	return true;
}

static bool
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot)
{
	while (read_verb(reader, &ctx.predicate) &&
	       read_ws_star(reader) &&
	       read_objectList(reader, ctx, ate_dot)) {
		ctx.predicate = pop_node(reader, ctx.predicate);
		if (*ate_dot) {
			return true;
		}

		bool    ate_semi = false;
		uint8_t c;
		do {
			read_ws_star(reader);
			switch (c = peek_byte(reader)) {
			case 0:
				return false;
			case '.': case ']': case '}':
				return true;
			case ';':
				eat_byte_safe(reader, c);
				ate_semi = true;
			}
		} while (c == ';');

		if (!ate_semi) {
			return r_err(reader, SERD_ERR_BAD_SYNTAX, "missing ';' or '.'\n");
		}
	}

	return pop_node(reader, ctx.predicate);
}

static bool
end_collection(SerdReader* reader, ReadContext ctx, Ref n1, Ref n2, bool ret)
{
	pop_node(reader, n2);
	pop_node(reader, n1);
	*ctx.flags &= ~SERD_LIST_CONT;
	return ret && (eat_byte_safe(reader, ')') == ')');
}

static bool
read_collection(SerdReader* reader, ReadContext ctx, Ref* dest)
{
	eat_byte_safe(reader, '(');
	bool end = peek_delim(reader, ')');
	*dest = end ? reader->rdf_nil : blank_id(reader);
	if (ctx.subject) {
		// subject predicate _:head
		*ctx.flags |= (end ? 0 : SERD_LIST_O_BEGIN);
		TRY_RET(emit_statement(reader, ctx, *dest, 0, 0));
		*ctx.flags |= SERD_LIST_CONT;
	} else {
		*ctx.flags |= (end ? 0 : SERD_LIST_S_BEGIN);
	}

	if (end) {
		return end_collection(reader, ctx, 0, 0, true);
	}

	/* The order of node allocation here is necessarily not in stack order,
	   so we create two nodes and recycle them throughout. */
	Ref n1   = push_node_padded(reader, genid_size(reader), SERD_BLANK, "", 0);
	Ref n2   = 0;
	Ref node = n1;
	Ref rest = 0;

	ctx.subject = *dest;
	while (!(end = peek_delim(reader, ')'))) {
		// _:node rdf:first object
		ctx.predicate = reader->rdf_first;
		bool ate_dot = false;
		if (!read_object(reader, &ctx, true, &ate_dot) || ate_dot) {
			return end_collection(reader, ctx, n1, n2, false);
		}

		if (!(end = peek_delim(reader, ')'))) {
			/* Give rest a new ID.  Done as late as possible to ensure it is
			   used and > IDs generated by read_object above. */
			if (!rest) {
				rest = n2 = blank_id(reader);  // First pass, push
			} else {
				set_blank_id(reader, rest, genid_size(reader));
			}
		}

		// _:node rdf:rest _:rest
		*ctx.flags |= SERD_LIST_CONT;
		ctx.predicate = reader->rdf_rest;
		TRY_RET(emit_statement(reader, ctx,
		                       (end ? reader->rdf_nil : rest), 0, 0));

		ctx.subject = rest;         // _:node = _:rest
		rest        = node;         // _:rest = (old)_:node
		node        = ctx.subject;  // invariant
	}

	return end_collection(reader, ctx, n1, n2, true);
}

static Ref
read_subject(SerdReader* reader, ReadContext ctx, Ref* dest, char* s_type)
{
	bool ate_dot = false;
	switch ((*s_type = peek_byte(reader))) {
	case '[':
		read_anon(reader, ctx, true, dest);
		break;
	case '(':
		read_collection(reader, ctx, dest);
		break;
	case '_':
		*dest = read_BLANK_NODE_LABEL(reader, &ate_dot);
		break;
	default:
		TRY_RET(read_iri(reader, dest, &ate_dot));
	}
	return ate_dot ? pop_node(reader, *dest) : *dest;
}

static Ref
read_labelOrSubject(SerdReader* reader, ReadContext ctx)
{
	Ref  subject = 0;
	bool ate_dot = false;
	switch (peek_byte(reader)) {
	case '[':
		eat_byte_safe(reader, '[');
		read_ws_star(reader);
		TRY_RET(eat_byte_check(reader, ']'));
		return blank_id(reader);
	case '_':
		return read_BLANK_NODE_LABEL(reader, &ate_dot);
	default:
		read_iri(reader, &subject, &ate_dot);
	}
	return subject;
}

static bool
read_triples(SerdReader* reader, ReadContext ctx, bool* ate_dot)
{
	bool ret = false;
	if (ctx.subject) {
		read_ws_star(reader);
		switch (peek_byte(reader)) {
		case '.':
			*ate_dot = eat_byte_safe(reader, '.');
			return false;
		case '}':
			return false;
		}
		ret = read_predicateObjectList(reader, ctx, ate_dot);
	}
	ctx.subject = ctx.predicate = 0;
	return ret;
}

static bool
read_base(SerdReader* reader, bool sparql, bool token)
{
	if (token) {
		TRY_RET(eat_string(reader, "base", 4));
	}

	Ref uri;
	read_ws_star(reader);
	TRY_RET(uri = read_IRIREF(reader));
	if (reader->base_sink) {
		reader->base_sink(reader->handle, deref(reader, uri));
	}
	pop_node(reader, uri);

	read_ws_star(reader);
	if (!sparql) {
		return eat_byte_check(reader, '.');
	} else if (peek_byte(reader) == '.') {
		return r_err(reader, SERD_ERR_BAD_SYNTAX,
		             "full stop after SPARQL BASE\n");
	}
	return true;
}

static bool
read_prefixID(SerdReader* reader, bool sparql, bool token)
{
	if (token) {
		TRY_RET(eat_string(reader, "prefix", 6));
	}

	read_ws_star(reader);
	bool ret  = true;
	Ref  name = push_node(reader, SERD_LITERAL, "", 0);
	if (read_PN_PREFIX(reader, name) > SERD_FAILURE) {
		return pop_node(reader, name);
	}

	if (eat_byte_check(reader, ':') != ':') {
		return pop_node(reader, name);
	}

	read_ws_star(reader);
	const Ref uri = read_IRIREF(reader);
	if (!uri) {
		pop_node(reader, name);
		return false;
	}

	if (reader->prefix_sink) {
		ret = !reader->prefix_sink(reader->handle,
		                           deref(reader, name),
		                           deref(reader, uri));
	}
	pop_node(reader, uri);
	pop_node(reader, name);
	if (!sparql) {
		read_ws_star(reader);
		return eat_byte_check(reader, '.');
	}
	return ret;
}

static bool
read_directive(SerdReader* reader)
{
	const bool sparql = peek_byte(reader) != '@';
	if (!sparql) {
		eat_byte_safe(reader, '@');
		switch (peek_byte(reader)) {
		case 'B': case 'P':
			return r_err(reader, SERD_ERR_BAD_SYNTAX,
			             "uppercase directive\n");
		}
	}

	switch (peek_byte(reader)) {
	case 'B': case 'b': return read_base(reader, sparql, true);
	case 'P': case 'p': return read_prefixID(reader, sparql, true);
	default:
		return r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid directive\n");
	}

	return true;
}

static bool
read_wrappedGraph(SerdReader* reader, ReadContext* ctx)
{
	bool ate_dot = false;
	char s_type  = 0;
	TRY_RET(eat_byte_check(reader, '{'));
	read_ws_star(reader);
	while (peek_byte(reader) != '}') {
		ctx->subject = 0;
		Ref subj = read_subject(reader, *ctx, &ctx->subject, &s_type);
		if (!subj ||
		    (!read_triples(reader, *ctx, &ate_dot) && s_type != '[')) {
			return false;
		}
		pop_node(reader, subj);
		read_ws_star(reader);
		if (peek_byte(reader) == '.') {
			eat_byte_safe(reader, '.');
		}
		read_ws_star(reader);
	}
	return eat_byte_check(reader, '}');
}

static int
tokcmp(SerdReader* reader, Ref ref, const char* tok, size_t n)
{
	SerdNode* node = deref(reader, ref);
	if (!node || node->n_bytes != n) {
		return -1;
	}
	const char* s1 = (const char*)node->buf;
	const char* s2 = tok;
	for (; n > 0 && *s2; s1++, s2++, --n) {
		if (toupper(*s1) != toupper(*s2)) {
			return ((*(uint8_t*)s1 < *(uint8_t*)s2) ? -1 : +1);
		}
	}
	return 0;
}

static bool
read_statement(SerdReader* reader)
{
	SerdStatementFlags flags   = 0;
	ReadContext        ctx     = { 0, 0, 0, 0, 0, 0, &flags };
	Ref                subj    = 0;
	bool               ate_dot = false;
	char               s_type  = false;
	bool               ret     = true;
	read_ws_star(reader);
	switch (peek_byte(reader)) {
	case '\0':
		reader->eof = true;
		return reader->status <= SERD_FAILURE;
	case '@':
		TRY_RET(read_directive(reader));
		read_ws_star(reader);
		break;
	case '{':
		if (reader->syntax == SERD_TRIG) {
			TRY_RET(read_wrappedGraph(reader, &ctx));
			read_ws_star(reader);
		} else {
			return r_err(reader, SERD_ERR_BAD_SYNTAX, "graph in Turtle\n");
		}
		break;
	default:
		subj = read_subject(reader, ctx, &ctx.subject, &s_type);
		if (!tokcmp(reader, ctx.subject, "base", 4)) {
			ret = read_base(reader, true, false);
		} else if (!tokcmp(reader, ctx.subject, "prefix", 6)) {
			ret = read_prefixID(reader, true, false);
		} else if (!tokcmp(reader, ctx.subject, "graph", 5)) {
			read_ws_star(reader);
			TRY_RET((ctx.graph = read_labelOrSubject(reader, ctx)));
			read_ws_star(reader);
			TRY_RET(read_wrappedGraph(reader, &ctx));
			read_ws_star(reader);
		} else if (read_ws_star(reader) && peek_byte(reader) == '{') {
			if (s_type == '(' || (s_type == '[' && !*ctx.flags)) {
				return false;  // invalid graph with complex label
			}
			ctx.graph   = subj;
			ctx.subject = subj = 0;
			TRY_RET(read_wrappedGraph(reader, &ctx));
			read_ws_star(reader);
		} else if (!subj) {
			ret = r_err(reader, SERD_ERR_BAD_SYNTAX, "bad subject\n");
		} else if (!read_triples(reader, ctx, &ate_dot)) {
			ret = (s_type == '[');
		} else if (!ate_dot) {
			read_ws_star(reader);
			ret = (eat_byte_check(reader, '.') == '.');
		}
		pop_node(reader, subj);
		break;
	}
	return ret;
}

static bool
read_turtleDoc(SerdReader* reader)
{
	while (!reader->eof) {
		TRY_RET(read_statement(reader));
	}
	return reader->status <= SERD_FAILURE;
}

static bool
read_trigDoc(SerdReader* reader)
{
	while (!reader->eof) {
		TRY_RET(read_statement(reader));
	}
	return reader->status <= SERD_FAILURE;
}

static bool
read_nquadsDoc(SerdReader* reader)
{
	while (!reader->eof) {
		SerdStatementFlags flags   = 0;
		ReadContext        ctx     = { 0, 0, 0, 0, 0, 0, &flags };
		bool               ate_dot = false;
		char               s_type  = false;
		read_ws_star(reader);
		if (peek_byte(reader) == '\0') {
			reader->eof = true;
			break;
		}

		// subject predicate object
		if (!(ctx.subject = read_subject(reader, ctx, &ctx.subject, &s_type)) ||
		    !read_ws_star(reader) ||
		    !(ctx.predicate = read_IRIREF(reader)) ||
		    !read_ws_star(reader) ||
		    !read_object(reader, &ctx, false, &ate_dot)) {
			return false;
		}

		if (!ate_dot) {  // graphLabel?
			TRY_RET(read_ws_star(reader));
			switch (peek_byte(reader)) {
			case '.':
				break;
			case '_':
				ctx.graph = read_BLANK_NODE_LABEL(reader, &ate_dot);
				break;
			default:
				if (!(ctx.graph = read_IRIREF(reader))) {
					return false;
				}
			}

			// Terminating '.'
			TRY_RET(read_ws_star(reader));
			eat_byte_check(reader, '.');
		}

		TRY_RET(emit_statement(reader, ctx, ctx.object, ctx.datatype, ctx.lang));
		pop_node(reader, ctx.graph);
		pop_node(reader, ctx.lang);
		pop_node(reader, ctx.datatype);
		pop_node(reader, ctx.object);
	}
	return reader->status <= SERD_FAILURE;
}

static bool
read_doc(SerdReader* reader)
{
	switch (reader->syntax) {
	case SERD_NQUADS: return read_nquadsDoc(reader);
	case SERD_TRIG:   return read_trigDoc(reader);
	default:          return read_turtleDoc(reader);
	}
}

SERD_API
SerdReader*
serd_reader_new(SerdSyntax        syntax,
                void*             handle,
                void              (*free_handle)(void*),
                SerdBaseSink      base_sink,
                SerdPrefixSink    prefix_sink,
                SerdStatementSink statement_sink,
                SerdEndSink       end_sink)
{
	const Cursor cur = { NULL, 0, 0 };
	SerdReader*  me  = (SerdReader*)calloc(1, sizeof(SerdReader));
	me->handle           = handle;
	me->free_handle      = free_handle;
	me->base_sink        = base_sink;
	me->prefix_sink      = prefix_sink;
	me->statement_sink   = statement_sink;
	me->end_sink         = end_sink;
	me->default_graph    = SERD_NODE_NULL;
	me->stack            = serd_stack_new(SERD_PAGE_SIZE);
	me->syntax           = syntax;
	me->cur              = cur;
	me->next_id          = 1;

	me->rdf_first = push_node(me, SERD_URI, NS_RDF "first", 48);
	me->rdf_rest  = push_node(me, SERD_URI, NS_RDF "rest", 47);
	me->rdf_nil   = push_node(me, SERD_URI, NS_RDF "nil", 46);

	return me;
}

SERD_API
void
serd_reader_set_strict(SerdReader* reader, bool strict)
{
	reader->strict = strict;
}

SERD_API
void
serd_reader_set_error_sink(SerdReader*   reader,
                           SerdErrorSink error_sink,
                           void*         error_handle)
{
	reader->error_sink   = error_sink;
	reader->error_handle = error_handle;
}

SERD_API
void
serd_reader_free(SerdReader* reader)
{
	pop_node(reader, reader->rdf_nil);
	pop_node(reader, reader->rdf_rest);
	pop_node(reader, reader->rdf_first);
	serd_node_free(&reader->default_graph);

#ifdef SERD_STACK_CHECK
	free(reader->allocs);
#endif
	free(reader->stack.buf);
	free(reader->bprefix);
	if (reader->free_handle) {
		reader->free_handle(reader->handle);
	}
	free(reader);
}

SERD_API
void*
serd_reader_get_handle(const SerdReader* reader)
{
	return reader->handle;
}

SERD_API
void
serd_reader_add_blank_prefix(SerdReader*    reader,
                             const uint8_t* prefix)
{
	free(reader->bprefix);
	reader->bprefix_len = 0;
	reader->bprefix     = NULL;
	if (prefix) {
		reader->bprefix_len = strlen((const char*)prefix);
		reader->bprefix     = (uint8_t*)malloc(reader->bprefix_len + 1);
		memcpy(reader->bprefix, prefix, reader->bprefix_len + 1);
	}
}

SERD_API
void
serd_reader_set_default_graph(SerdReader*     reader,
                              const SerdNode* graph)
{
	serd_node_free(&reader->default_graph);
	reader->default_graph = serd_node_copy(graph);
}

SERD_API
SerdStatus
serd_reader_read_file(SerdReader*    reader,
                      const uint8_t* uri)
{
	uint8_t* const path = serd_file_uri_parse(uri, NULL);
	if (!path) {
		return SERD_ERR_BAD_ARG;
	}

	FILE* fd = serd_fopen((const char*)path, "r");
	if (!fd) {
		free(path);
		return SERD_ERR_UNKNOWN;
	}

	SerdStatus ret = serd_reader_read_file_handle(reader, fd, path);
	fclose(fd);
	free(path);
	return ret;
}

static SerdStatus
skip_bom(SerdReader* me)
{
	if (peek_byte(me) == 0xEF) {
		eat_byte_safe(me, 0xEF);
		if (eat_byte_check(me, 0xBB) != 0xBB ||
		    eat_byte_check(me, 0xBF) != 0xBF) {
			r_err(me, SERD_ERR_BAD_SYNTAX, "corrupt byte order mark\n");
			return SERD_ERR_BAD_SYNTAX;
		}
	}

	return SERD_SUCCESS;
}

SERD_API
SerdStatus
serd_reader_start_stream(SerdReader*    reader,
                         FILE*          file,
                         const uint8_t* name,
                         bool           bulk)
{
	return serd_reader_start_source_stream(
		reader,
		bulk ? (SerdSource)fread : serd_file_read_byte,
		(SerdStreamErrorFunc)ferror,
		file,
		name,
		bulk ? SERD_PAGE_SIZE : 1);
}

SERD_API
SerdStatus
serd_reader_start_source_stream(SerdReader*         reader,
                                SerdSource          read_func,
                                SerdStreamErrorFunc error_func,
                                void*               stream,
                                const uint8_t*      name,
                                size_t              page_size)
{
	const Cursor cur = { name, 1, 1 };
	reader->cur = cur;

	return serd_byte_source_open_source(
		&reader->source, read_func, error_func, stream, page_size);
}

static SerdStatus
serd_reader_prepare(SerdReader* reader)
{
	reader->eof    = false;
	reader->status = serd_byte_source_prepare(&reader->source);
	if (reader->status == SERD_SUCCESS) {
		reader->status = skip_bom(reader);
	} else if (reader->status == SERD_FAILURE) {
		reader->eof = true;
	} else {
		r_err(reader, reader->status, "read error: %s\n", strerror(errno));
	}
	return reader->status;
}

SERD_API
SerdStatus
serd_reader_read_chunk(SerdReader* reader)
{
	SerdStatus st = SERD_SUCCESS;
	if (!reader->source.prepared) {
		if ((st = serd_reader_prepare(reader))) {
			return st;
		}
	} else if (reader->eof) {
		reader->eof = false;
		if ((st = serd_byte_source_advance(&reader->source))) {
			return st;
		}
	}
	return read_statement(reader) ? SERD_SUCCESS : SERD_FAILURE;
}

SERD_API
SerdStatus
serd_reader_end_stream(SerdReader* reader)
{
	return serd_byte_source_close(&reader->source);
}

SERD_API
SerdStatus
serd_reader_read_file_handle(SerdReader*    reader,
                             FILE*          file,
                             const uint8_t* name)
{
	return serd_reader_read_source(
		reader, (SerdSource)fread, (SerdStreamErrorFunc)ferror,
		file, name, SERD_PAGE_SIZE);
}

SERD_API
SerdStatus
serd_reader_read_source(SerdReader*         reader,
                        SerdSource          source,
                        SerdStreamErrorFunc error,
                        void*               stream,
                        const uint8_t*      name,
                        size_t              page_size)
{
	SerdStatus st = serd_reader_start_source_stream(
		reader, source, error, stream, name, page_size);

	if (st || (st = serd_reader_prepare(reader))) {
		serd_reader_end_stream(reader);
		return st;
	} else if (!read_doc(reader)) {
		serd_reader_end_stream(reader);
		return SERD_ERR_UNKNOWN;
	}

	return serd_reader_end_stream(reader);
}

SERD_API
SerdStatus
serd_reader_read_string(SerdReader* reader, const uint8_t* utf8)
{
	const Cursor cur = { (const uint8_t*)"(string)", 1, 1 };

	serd_byte_source_open_string(&reader->source, utf8);
	reader->cur = cur;
	reader->eof = false;

	SerdStatus st = serd_reader_prepare(reader);
	if (!st) {
		st = read_doc(reader) ? SERD_SUCCESS : SERD_ERR_UNKNOWN;
	}

	serd_byte_source_close(&reader->source);

	return st;
}
