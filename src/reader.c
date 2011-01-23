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
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"
#include "serd_stack.h"

#define TRY_THROW(exp) if (!(exp)) goto except;
#define TRY_RET(exp)   if (!(exp)) return 0;

#define STACK_PAGE_SIZE 4096
#define READ_BUF_LEN    4096
#ifndef NDEBUG
#define STACK_DEBUG       1
#endif

typedef struct {
	const uint8_t* filename;
	unsigned       line;
	unsigned       col;
} Cursor;

typedef uint32_t uchar;

typedef size_t Ref;

typedef struct {
	SerdNodeType type;
	Ref          value;
	Ref          datatype;
	Ref          lang;
} Node;

typedef struct {
	const Node* graph;
	const Node* subject;
	const Node* predicate;
} ReadContext;

static const Node SERD_NODE_NULL = {0,0,0,0};

struct SerdReaderImpl {
	void*             handle;
	SerdBaseSink      base_sink;
	SerdPrefixSink    prefix_sink;
	SerdStatementSink statement_sink;
	SerdEndSink       end_sink;
	Node              rdf_type;
	Node              rdf_first;
	Node              rdf_rest;
	Node              rdf_nil;
	FILE*             fd;
	SerdStack         stack;
	Cursor            cur;
	uint8_t*          buf;
	unsigned          next_id;
	int               err;
	uint8_t*          read_buf;
	int32_t           read_head; ///< Offset into read_buf
	bool              eof;
#ifdef STACK_DEBUG
	Ref*              alloc_stack; ///< Stack of push offsets
	size_t            n_allocs; ///< Number of stack pushes
#endif
};

typedef enum {
	SERD_SUCCESS = 0,  ///< Completed successfully
	SERD_FAILURE = 1,  ///< Non-fatal failure
	SERD_ERROR   = 2,  ///< Fatal error
} SerdStatus;

static inline int
error(SerdReader reader, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "error: %s:%u:%u: ",
	        reader->cur.filename, reader->cur.line, reader->cur.col);
	vfprintf(stderr, fmt, args);
	reader->err = 1;
	return 0;
}

static Node
make_node(SerdNodeType type, Ref value, Ref datatype, Ref lang)
{
	const Node ret = { type, value, datatype, lang };
	return ret;
}

static inline bool
page(SerdReader reader)
{
	reader->read_head = 0;
	const int32_t n_read = fread(reader->read_buf, 1, READ_BUF_LEN, reader->fd);
	if (n_read == 0) {
		reader->read_buf[0] = '\0';
		reader->eof = true;
		return false;
	} else if (n_read < READ_BUF_LEN) {
		reader->read_buf[n_read] = '\0';
	}
	return true;
}

static inline bool
readahead(SerdReader reader, uint8_t* pre, int n)
{
	uint8_t* ptr = reader->read_buf + reader->read_head;
	for (int i = 0; i < n; ++i) {
		if (reader->read_head + i >= READ_BUF_LEN) {
			if (!page(reader)) {
				return false;
			}
			ptr = reader->read_buf;
			reader->read_head = -i;
			memcpy(reader->read_buf + reader->read_head, pre, i);
			assert(reader->read_buf[reader->read_head] == pre[0]);
		}
		if ((pre[i] = *ptr++) == '\0') {
			return false;
		}
	}
	return true;
}

static inline uint8_t
peek_byte(SerdReader reader)
{
	return reader->read_buf[reader->read_head];
}

static inline uint8_t
eat_byte(SerdReader reader, const uint8_t byte)
{
	const uint8_t c = peek_byte(reader);
	++reader->read_head;
	switch (c) {
	case '\n': ++reader->cur.line; reader->cur.col = 0; break;
	default:   ++reader->cur.col;
	}

	if (c != byte) {
		return error(reader, "expected `%c', not `%c'\n", byte, c);
	}
	if (reader->read_head == READ_BUF_LEN) {
		TRY_RET(page(reader));
	}
	assert(reader->read_head < READ_BUF_LEN);
	if (reader->read_buf[reader->read_head] == '\0') {
		reader->eof = true;
	}
	return c;
}

static inline void
eat_string(SerdReader reader, const char* str, unsigned n)
{
	for (unsigned i = 0; i < n; ++i) {
		eat_byte(reader, ((const uint8_t*)str)[i]);
	}
}

static inline bool
in_range(const uchar c, const uchar min, const uchar max)
{
	return (c >= min && c <= max);
}

#ifdef STACK_DEBUG
static inline bool
stack_is_top_string(SerdReader reader, Ref ref)
{
	return ref == reader->alloc_stack[reader->n_allocs - 1];
}
#endif

static inline intptr_t
pad_size(intptr_t size)
{
	return (size + 7) & (~7);
}

// Make a new string from a non-UTF-8 C string (internal use only)
static Ref
push_string(SerdReader reader, const char* c_str, size_t n_bytes)
{
	// Align strings to 64-bits (assuming malloc/realloc are aligned to 64-bits)
	const size_t      stack_size = pad_size((intptr_t)reader->stack.size);
	const size_t      pad        = stack_size - reader->stack.size;
	SerdString* const str = (SerdString*)(
		serd_stack_push(&reader->stack, pad + sizeof(SerdString) + n_bytes) + pad);
	str->n_bytes = n_bytes;
	str->n_chars = n_bytes - 1;
	memcpy(str->buf, c_str, n_bytes);
#ifdef STACK_DEBUG
	reader->alloc_stack = realloc(reader->alloc_stack, sizeof(uint8_t*) * (++reader->n_allocs));
	reader->alloc_stack[reader->n_allocs - 1] = ((uint8_t*)str - reader->stack.buf);
#endif
	return (uint8_t*)str - reader->stack.buf;
}

static inline SerdString*
deref(SerdReader reader, const Ref ref)
{
	if (ref) {
		return (SerdString*)(reader->stack.buf + ref);
	}
	return NULL;
}

static inline void
push_byte(SerdReader reader, Ref ref, const uint8_t c)
{
	#ifdef STACK_DEBUG
	assert(stack_is_top_string(reader, ref));
	#endif
	serd_stack_push(&reader->stack, 1);
	SerdString* const str = deref(reader, ref);
	++str->n_bytes;
	if ((c & 0xC0) != 0x80) {
		// Does not start with `10', start of a new character
		++str->n_chars;
	}
	assert(str->n_bytes > str->n_chars);
	str->buf[str->n_bytes - 2] = c;
	str->buf[str->n_bytes - 1] = '\0';
}

static void
pop_string(SerdReader reader, Ref ref)
{
	if (ref) {
		if (ref == reader->rdf_nil.value
		    || ref == reader->rdf_first.value
		    || ref == reader->rdf_rest.value) {
			return;
		}
		#ifdef STACK_DEBUG
		if (!stack_is_top_string(reader, ref)) {
			fprintf(stderr, "attempt to pop non-top string %s\n", deref(reader, ref)->buf);
			fprintf(stderr, "top: %s\n",
			        deref(reader, reader->alloc_stack[reader->n_allocs - 1])->buf);
		}
		assert(stack_is_top_string(reader, ref));
		--reader->n_allocs;
		#endif
		serd_stack_pop(&reader->stack, deref(reader, ref)->n_bytes);
	}
}

static inline void
emit_statement(SerdReader reader,
               const Node* g, const Node* s, const Node* p, const Node* o)
{
	assert(s->value && p->value && o->value);
	reader->statement_sink(reader->handle,
	                       g ? deref(reader, g->value) : NULL,
	                       deref(reader, s->value), s->type,
	                       deref(reader, p->value), p->type,
	                       deref(reader, o->value), o->type,
	                       deref(reader, o->datatype), deref(reader, o->lang));
}

static bool read_collection(SerdReader reader, ReadContext ctx, Node* dest);
static bool read_predicateObjectList(SerdReader reader, ReadContext ctx);

// [40]	hex	::=	[#x30-#x39] | [#x41-#x46]
static inline uint8_t
read_hex(SerdReader reader)
{
	const uint8_t c = peek_byte(reader);
	if (in_range(c, 0x30, 0x39) || in_range(c, 0x41, 0x46)) {
		return eat_byte(reader, c);
	} else {
		return error(reader, "illegal hexadecimal digit `%c'\n", c);
	}
}

static inline bool
read_hex_escape(SerdReader reader, unsigned length, Ref dest)
{
	uint8_t buf[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (unsigned i = 0; i < length; ++i) {
		buf[i] = read_hex(reader);
	}

	uint32_t c;
	sscanf((const char*)buf, "%X", &c);

	unsigned size = 0;
	if (c < 0x00000080) {
		size = 1;
	} else if (c < 0x00000800) {
		size = 2;
	} else if (c < 0x00010000) {
		size = 3;
	} else if (c < 0x00200000) {
		size = 4;
	} else if (c < 0x04000000) {
		size = 5;
	} else if (c < 0x80000000) {
		size = 6;
	} else {
		return false;
	}

	// Build output in buf
	// (Note # of bytes = # of leading 1 bits in first byte)
	switch (size) {
	case 6:
		buf[5] = 0x80 | (uint8_t)(c & 0x3F);
		c >>= 6;
		c |= (4 << 24);  // set bit 2
	case 5:
		buf[4] = 0x80 | (uint8_t)(c & 0x3F);
		c >>= 6;
		c |= (8 << 18);  // set bit 3
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

	for (unsigned i = 0; i < size; ++i) {
		push_byte(reader, dest, buf[i]);
	}
	return true;
}

static inline bool
read_character_escape(SerdReader reader, Ref dest)
{
	switch (peek_byte(reader)) {
	case '\\':
		push_byte(reader, dest, eat_byte(reader, '\\'));
		return true;
	case 'u':
		eat_byte(reader, 'u');
		return read_hex_escape(reader, 4, dest);
	case 'U':
		eat_byte(reader, 'U');
		return read_hex_escape(reader, 8, dest);
	default:
		return false;
	}
}

static inline bool
read_echaracter_escape(SerdReader reader, Ref dest)
{
	switch (peek_byte(reader)) {
	case 't':
		eat_byte(reader, 't');
		push_byte(reader, dest, '\t');
		return true;
	case 'n':
		eat_byte(reader, 'n');
		push_byte(reader, dest, '\n');
		return true;
	case 'r':
		eat_byte(reader, 'r');
		push_byte(reader, dest, '\r');
		return true;
	default:
		return read_character_escape(reader, dest);
	}
}

static inline bool
read_scharacter_escape(SerdReader reader, Ref dest)
{
	switch (peek_byte(reader)) {
	case '"':
		push_byte(reader, dest, eat_byte(reader, '"'));
		return true;
	default:
		return read_echaracter_escape(reader, dest);
	}
}

static inline bool
read_ucharacter_escape(SerdReader reader, Ref dest)
{
	switch (peek_byte(reader)) {
	case '>':
		push_byte(reader, dest, eat_byte(reader, '>'));
		return true;
	default:
		return read_echaracter_escape(reader, dest);
	}
}

// [38] character ::= '\u' hex hex hex hex
//                  | '\U' hex hex hex hex hex hex hex hex
//                  | '\\'
//                  | [#x20-#x5B] | [#x5D-#x10FFFF]
static inline SerdStatus
read_character(SerdReader reader, Ref dest)
{
	const uint8_t c = peek_byte(reader);
	switch (c) {
	case '\0':
		error(reader, "unexpected end of file\n", peek_byte(reader));
		return SERD_ERROR;
	case '\\':  // 0x5C
		eat_byte(reader, '\\');
		if (read_character_escape(reader, dest)) {
			return SERD_SUCCESS;
		} else {
			error(reader, "invalid escape `\\%c'\n", peek_byte(reader));
			return SERD_ERROR;
		}
	default:
		if (c < 0x20) {  // ASCII control character
			error(reader, "unexpected control character\n");
			return SERD_ERROR;
		} else if (c <= 0x7E) {  // Printable ASCII
			push_byte(reader, dest, eat_byte(reader, c));
			return SERD_SUCCESS;
		} else {  // Wide UTF-8 character
			unsigned size = 1;
			if ((c & 0xE0) == 0xC0) {  // Starts with `110'
				size = 2;
			} else if ((c & 0xF0) == 0xE0) {  // Starts with `1110'
				size = 3;
			} else if ((c & 0xF8) == 0xF0) {  // Starts with `11110'
				size = 4;
			} else if ((c & 0xFC) == 0xF8) {  // Starts with `111110'
				size = 5;
			} else if ((c & 0xFE) == 0xFC) {  // Starts with `1111110'
				size = 6;
			} else {
				error(reader, "invalid character\n");
				return SERD_ERROR;
			}
			for (unsigned i = 0; i < size; ++i) {
				push_byte(reader, dest, eat_byte(reader, peek_byte(reader)));
			}
			return SERD_SUCCESS;
		}
	}
}

// [39] echaracter ::= character | '\t' | '\n' | '\r'
static inline SerdStatus
read_echaracter(SerdReader reader, Ref dest)
{
	uint8_t c = peek_byte(reader);
	switch (c) {
	case '\\':
		eat_byte(reader, '\\');
		if (read_echaracter_escape(reader, peek_byte(reader))) {
			return SERD_SUCCESS;
		} else {
			error(reader, "illegal escape `\\%c'\n", peek_byte(reader));
			return SERD_ERROR;
		}
	default:
		return read_character(reader, dest);
	}
}

// [43] lcharacter ::= echaracter | '\"' | #x9 | #xA | #xD
static inline SerdStatus
read_lcharacter(SerdReader reader, Ref dest)
{
	const uint8_t c = peek_byte(reader);
	uint8_t       pre[3];
	switch (c) {
	case '"':
		readahead(reader, pre, 3);
		if (pre[1] == '\"' && pre[2] == '\"') {
			eat_byte(reader, '\"');
			eat_byte(reader, '\"');
			eat_byte(reader, '\"');
			return SERD_FAILURE;
		} else {
			push_byte(reader, dest, eat_byte(reader, '"'));
			return SERD_SUCCESS;
		}
	case '\\':
		eat_byte(reader, '\\');
		if (read_scharacter_escape(reader, dest)) {
			return SERD_SUCCESS;
		} else {
			error(reader, "illegal escape `\\%c'\n", peek_byte(reader));
			return SERD_ERROR;
		}
	case 0x9: case 0xA: case 0xD:
		push_byte(reader, dest, eat_byte(reader, c));
		return SERD_SUCCESS;
	default:
		return read_echaracter(reader, dest);
	}
}

// [42] scharacter ::= ( echaracter - #x22 ) | '\"'
static inline SerdStatus
read_scharacter(SerdReader reader, Ref dest)
{
	uint8_t c = peek_byte(reader);
	switch (c) {
	case '\\':
		eat_byte(reader, '\\');
		if (read_scharacter_escape(reader, dest)) {
			return SERD_SUCCESS;
		} else {
			error(reader, "illegal escape `\\%c'\n", peek_byte(reader));
			return SERD_ERROR;
		}
	case '\"':
		return SERD_FAILURE;
	default:
		return read_character(reader, dest);
	}
}

// Spec:   [41] ucharacter ::= ( character - #x3E ) | '\>'
// Actual: [41] ucharacter ::= ( echaracter - #x3E ) | '\>'
static inline SerdStatus
read_ucharacter(SerdReader reader, Ref dest)
{
	const uint8_t c = peek_byte(reader);
	switch (c) {
	case '\\':
		eat_byte(reader, '\\');
		if (read_ucharacter_escape(reader, dest)) {
			return SERD_SUCCESS;
		} else {
			return error(reader, "illegal escape `\\%c'\n", peek_byte(reader));
		}
	case '>':
		return SERD_FAILURE;
	default:
		return read_character(reader, dest);
	}
}

// [10] comment ::= '#' ( [^#xA #xD] )*
static void
read_comment(SerdReader reader)
{
	eat_byte(reader, '#');
	uint8_t c;
	while (((c = peek_byte(reader)) != 0xA) && (c != 0xD)) {
		eat_byte(reader, c);
	}
}

// [24] ws ::= #x9 | #xA | #xD | #x20 | comment
static inline bool
read_ws(SerdReader reader)
{
	const uint8_t c = peek_byte(reader);
	switch (c) {
	case 0x9: case 0xA: case 0xD: case 0x20:
		eat_byte(reader, c);
		return true;
	case '#':
		read_comment(reader);
		return true;
	default:
		return false;
	}
}

static inline void
read_ws_star(SerdReader reader)
{
	while (read_ws(reader)) {}
}

static inline bool
read_ws_plus(SerdReader reader)
{
	TRY_RET(read_ws(reader));
	read_ws_star(reader);
	return true;
}

// [37] longSerdString ::= #x22 #x22 #x22 lcharacter* #x22 #x22 #x22
static Ref
read_longString(SerdReader reader)
{
	eat_string(reader, "\"\"\"", 3);
	Ref        str = push_string(reader, "", 1);
	SerdStatus st;
	while (!(st = read_lcharacter(reader, str))) {}
	if (st != SERD_ERROR) {
		return str;
	}
	pop_string(reader, str);
	return 0;
}

// [36] string ::= #x22 scharacter* #x22
static Ref
read_string(SerdReader reader)
{
	eat_byte(reader, '\"');
	Ref        str = push_string(reader, "", 1);
	SerdStatus st;
	while (!(st = read_scharacter(reader, str))) {}
	if (st != SERD_ERROR) {
		eat_byte(reader, '\"');
		return str;
	}
	pop_string(reader, str);
	return 0;
}

// [35] quotedString ::= string | longSerdString
static Ref
read_quotedString(SerdReader reader)
{
	uint8_t pre[3];
	readahead(reader, pre, 3);
	assert(pre[0] == '\"');
	switch (pre[1]) {
	case '\"':
		if (pre[2] == '\"')
			return read_longString(reader);
		else
			return read_string(reader);
	default:
		return read_string(reader);
	}
}

// [34] relativeURI ::= ucharacter*
static inline Ref
read_relativeURI(SerdReader reader)
{
	Ref str = push_string(reader, "", 1);
	SerdStatus st;
	while (!(st = read_ucharacter(reader, str))) {}
	if (st != SERD_ERROR) {
		return str;
	}
	pop_string(reader, str);
	return 0;
}

// [30] nameStartChar ::= [A-Z] | "_" | [a-z]
//      | [#x00C0-#x00D6] | [#x00D8-#x00F6] | [#x00F8-#x02FF] | [#x0370-#x037D]
//      | [#x037F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF]
//      | [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] | [#x10000-#xEFFFF]
static inline uchar
read_nameStartChar(SerdReader reader, bool required)
{
	const uint8_t c = peek_byte(reader);
	if (in_range(c, 'A', 'Z') || (c == '_') || in_range(c, 'a', 'z')) {
		return eat_byte(reader, c);
	} else {
		if (required) {
			error(reader, "illegal character `%c'\n", c);
		}
		return 0;
	}
}

// [31] nameChar ::= nameStartChar | '-' | [0-9] | #x00B7 | [#x0300-#x036F] | [#x203F-#x2040]
static inline uchar
read_nameChar(SerdReader reader)
{
	uchar c = read_nameStartChar(reader, false);
	if (c)
		return c;

	switch ((c = peek_byte(reader))) {
	case '-': case 0xB7: case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return eat_byte(reader, c);
	default:
		if (in_range(c, 0x300, 0x036F) || in_range(c, 0x203F, 0x2040)) {
			return eat_byte(reader, c);
		}
	}
	return 0;
}

// [33] prefixName ::= ( nameStartChar - '_' ) nameChar*
static Ref
read_prefixName(SerdReader reader)
{
	uint8_t c = peek_byte(reader);
	if (c == '_') {
		error(reader, "unexpected `_'\n");
		return 0;
	}
	TRY_RET(c = read_nameStartChar(reader, false));
	Ref str = push_string(reader, "", 1);
	push_byte(reader, str, c);
	while ((c = read_nameChar(reader)) != 0) {
		push_byte(reader, str, c);
	}
	return str;
}

// [32] name ::= nameStartChar nameChar*
static Ref
read_name(SerdReader reader, Ref dest, bool required)
{
	uchar c = read_nameStartChar(reader, required);
	if (!c) {
		if (required) {
			error(reader, "illegal character at start of name\n");
		}
		return 0;
	}
	do {
		push_byte(reader, dest, c);
	} while ((c = read_nameChar(reader)) != 0);
	return dest;
}

// [29] language ::= [a-z]+ ('-' [a-z0-9]+ )*
static Ref
read_language(SerdReader reader)
{
	const uint8_t start = peek_byte(reader);
	if (!in_range(start, 'a', 'z')) {
		error(reader, "unexpected `%c'\n", start);
		return 0;
	}
	Ref str = push_string(reader, "", 1);
	push_byte(reader, str, eat_byte(reader, start));
	uint8_t c;
	while ((c = peek_byte(reader)) && in_range(c, 'a', 'z')) {
		push_byte(reader, str, eat_byte(reader, c));
	}
	if (peek_byte(reader) == '-') {
		push_byte(reader, str, eat_byte(reader, '-'));
		while ((c = peek_byte(reader)) && (in_range(c, 'a', 'z') || in_range(c, '0', '9'))) {
			push_byte(reader, str, eat_byte(reader, c));
		}
	}
	return str;
}

// [28] uriref ::= '<' relativeURI '>'
static Ref
read_uriref(SerdReader reader)
{
	TRY_RET(eat_byte(reader, '<'));
	Ref const str = read_relativeURI(reader);
	if (str && eat_byte(reader, '>')) {
		return str;
	}
	pop_string(reader, str);
	return 0;
}

// [27] qname ::= prefixName? ':' name?
static Ref
read_qname(SerdReader reader)
{
	Ref prefix = read_prefixName(reader);
	if (!prefix) {
		prefix = push_string(reader, "", 1);
	}
	TRY_THROW(eat_byte(reader, ':'));
	push_byte(reader, prefix, ':');
	Ref str = read_name(reader, prefix, false);
	if (reader->err) {
		pop_string(reader, prefix);
		return 0;
	}
	return str ? str : prefix;
except:
	pop_string(reader, prefix);
	return 0;
}


static Ref
read_0_9(SerdReader reader, Ref str, bool at_least_one)
{
	uint8_t c;
	if (at_least_one) {
		TRY_RET(in_range(c = peek_byte(reader), '0', '9'));
		push_byte(reader, str, eat_byte(reader, c));
	}
	while (in_range((c = peek_byte(reader)), '0', '9')) {
		push_byte(reader, str, eat_byte(reader, c));
	}
	return str;
}

// [19] exponent ::= [eE] ('-' | '+')? [0-9]+
// [18] decimal ::= ( '-' | '+' )? ( [0-9]+ '.' [0-9]*
//                                  | '.' ([0-9])+
//                                  | ([0-9])+ )
// [17] double  ::= ( '-' | '+' )? ( [0-9]+ '.' [0-9]* exponent
//                                  | '.' ([0-9])+ exponent
//                                  | ([0-9])+ exponent )
// [16] integer ::= ( '-' | '+' ) ? [0-9]+
static bool
read_number(SerdReader reader, Node* dest)
{
	#define XSD_DECIMAL "http://www.w3.org/2001/XMLSchema#decimal"
	#define XSD_DOUBLE  "http://www.w3.org/2001/XMLSchema#double"
	#define XSD_INTEGER "http://www.w3.org/2001/XMLSchema#integer"
	Ref     str         = push_string(reader, "", 1);
	uint8_t c           = peek_byte(reader);
	bool    has_decimal = false;
	Ref     datatype    = 0;
	if (c == '-' || c == '+') {
		push_byte(reader, str, eat_byte(reader, c));
	}
	if ((c = peek_byte(reader)) == '.') {
		has_decimal = true;
		// decimal case 2 (e.g. '.0' or `-.0' or `+.0')
		push_byte(reader, str, eat_byte(reader, c));
		TRY_THROW(str = read_0_9(reader, str, true));
	} else {
		// all other cases ::= ( '-' | '+' ) [0-9]+ ( . )? ( [0-9]+ )? ...
		TRY_THROW(str = read_0_9(reader, str, true));
		if ((c = peek_byte(reader)) == '.') {
			has_decimal = true;
			push_byte(reader, str, eat_byte(reader, c));
			TRY_THROW(str = read_0_9(reader, str, false));
		}
	}
	c = peek_byte(reader);
	if (c == 'e' || c == 'E') {
		// double
		push_byte(reader, str, eat_byte(reader, c));
		str = read_0_9(reader, str, true);
		datatype = push_string(reader, XSD_DOUBLE, strlen(XSD_DOUBLE) + 1);
	} else if (has_decimal) {
		datatype = push_string(reader, XSD_DECIMAL, strlen(XSD_DECIMAL) + 1);
	} else {
		datatype = push_string(reader, XSD_INTEGER, strlen(XSD_INTEGER) + 1);
	}
	*dest = make_node(SERD_LITERAL, str, datatype, 0);
	assert(dest->value);
	return true;
except:
	pop_string(reader, datatype);
	pop_string(reader, str);
	return false;
}

// [25] resource ::= uriref | qname
static bool
read_resource(SerdReader reader, Node* dest)
{
	switch (peek_byte(reader)) {
	case '<':
		*dest = make_node(SERD_URI, read_uriref(reader), 0, 0);
		break;
	default:
		*dest = make_node(SERD_CURIE, read_qname(reader), 0, 0);
	}
	return (dest->value != 0);
}

// [14] literal ::= quotedString ( '@' language )? | datatypeSerdString
//                | integer | double | decimal | boolean
static bool
read_literal(SerdReader reader, Node* dest)
{
	Ref           str      = 0;
	Node          datatype = SERD_NODE_NULL;
	const uint8_t c        = peek_byte(reader);
	if (in_range(c, '0', '9') || c == '-' || c == '+') {
		return read_number(reader, dest);
	} else if (c == '\"') {
		str = read_quotedString(reader);
		if (!str) {
			return false;
		}

		Ref lang = 0;
		switch (peek_byte(reader)) {
		case '^':
			eat_byte(reader, '^');
			eat_byte(reader, '^');
			TRY_THROW(read_resource(reader, &datatype));
			break;
		case '@':
			eat_byte(reader, '@');
			TRY_THROW(lang = read_language(reader));
		}
		*dest = make_node(SERD_LITERAL, str, datatype.value, lang);
	} else {
		*dest = make_node(SERD_CURIE, read_qname(reader), 0, 0);
	}
	return true;
except:
	pop_string(reader, str);
	return false;
}

// [12] predicate ::= resource
static bool
read_predicate(SerdReader reader, Node* dest)
{
	return read_resource(reader, dest);
}

// [9] verb ::= predicate | 'a'
static bool
read_verb(SerdReader reader, Node* dest)
{
	#define RDF_TYPE "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
	uint8_t pre[2];
	readahead(reader, pre, 2);
	switch (pre[0]) {
	case 'a':
		switch (pre[1]) {
		case 0x9: case 0xA: case 0xD: case 0x20:
			eat_byte(reader, 'a');
			*dest = make_node(SERD_URI, push_string(reader, RDF_TYPE, 48), 0, 0);
			return true;
		default: break;  // fall through
		}
	default:
		return read_predicate(reader, dest);
	}
}

// [26] nodeID ::= '_:' name
static Ref
read_nodeID(SerdReader reader)
{
	eat_byte(reader, '_');
	eat_byte(reader, ':');
	Ref str = push_string(reader, "", 1);
	return read_name(reader, str, true);
}

static Ref
blank_id(SerdReader reader)
{
	char str[32];
	const int len = snprintf(str, sizeof(str), "genid%u", reader->next_id++);
	return push_string(reader, str, len + 1);
}

// Spec:   [21] blank ::= nodeID | '[]' | '[' predicateObjectList ']' | collection
// Actual: [21] blank ::= nodeID | '[ ws* ]' | '[' ws* predicateObjectList ws* ']' | collection
static bool
read_blank(SerdReader reader, ReadContext ctx, Node* dest)
{
	switch (peek_byte(reader)) {
	case '_':
		*dest = make_node(SERD_BLANK_ID, read_nodeID(reader), 0, 0);
		return true;
	case '[':
		eat_byte(reader, '[');
		read_ws_star(reader);
		if (peek_byte(reader) == ']') {
			eat_byte(reader, ']');
			*dest = make_node(SERD_BLANK_ID, blank_id(reader), 0, 0);
			if (ctx.subject) {
				emit_statement(reader, ctx.graph, ctx.subject, ctx.predicate, dest);
			}
			return true;
		}
		*dest = make_node(SERD_ANON_BEGIN, blank_id(reader), 0, 0);
		if (ctx.subject) {
			emit_statement(reader, ctx.graph, ctx.subject, ctx.predicate, dest);
			dest->type = SERD_ANON;
		}
		ctx.subject = dest;
		read_predicateObjectList(reader, ctx);
		read_ws_star(reader);
		eat_byte(reader, ']');
		if (reader->end_sink) {
			reader->end_sink(reader->handle, deref(reader, dest->value));
		}
		return true;
	case '(':
		if (read_collection(reader, ctx, dest)) {
			if (ctx.subject) {
				emit_statement(reader, ctx.graph, ctx.subject, ctx.predicate, dest);
			}
			return true;
		}
		return false;
	default:
		return error(reader, "illegal blank node\n");
	}
}

inline static bool
is_object_end(const uint8_t c)
{
	switch (c) {
	case 0x9: case 0xA: case 0xD: case 0x20:
	case '#': case '.': case ';':
		return true;
	default:
		return false;
	}
}

// [13] object ::= resource | blank | literal
// Recurses, calling statement_sink for every statement encountered.
// Leaves stack in original calling state (i.e. pops everything it pushes).
static bool
read_object(SerdReader reader, ReadContext ctx)
{
	static const char* const XSD_BOOLEAN     = "http://www.w3.org/2001/XMLSchema#boolean";
	static const size_t      XSD_BOOLEAN_LEN = 40;

	uint8_t       pre[6];
	bool          ret  = false;
	bool          emit = (ctx.subject != 0);
	Node          o    = SERD_NODE_NULL;
	const uint8_t c    = peek_byte(reader);
	switch (c) {
	case ')':
		return false;
	case '[': case '(':
		emit = false;
		// fall through
	case '_':
		TRY_THROW(ret = read_blank(reader, ctx, &o));
		break;
	case '<': case ':':
		TRY_THROW(ret = read_resource(reader, &o));
		break;
	case '\"': case '+': case '-':
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		TRY_THROW(ret = read_literal(reader, &o));
		break;
	case '.':
		TRY_THROW(ret = read_literal(reader, &o));
		break;
	default:
		/* Either a boolean literal, or a qname.
		   Unfortunately there is no way to distinguish these without
		   readahead, since `true' or `false' could be the start of a qname.
		*/
		readahead(reader, pre, 6);
		if (!memcmp(pre, "true", 4) && is_object_end(pre[4])) {
			eat_string(reader, "true", 4);
			const Ref value     = push_string(reader, "true", 5);
			const Ref datatype  = push_string(reader, XSD_BOOLEAN, XSD_BOOLEAN_LEN + 1);
			o = make_node(SERD_LITERAL, value, datatype, 0);
		} else if (!memcmp(pre, "false", 5) && is_object_end(pre[5])) {
			eat_string(reader, "false", 5);
			const Ref value     = push_string(reader, "false", 6);
			const Ref datatype  = push_string(reader, XSD_BOOLEAN, XSD_BOOLEAN_LEN + 1);
			o = make_node(SERD_LITERAL, value, datatype, 0);
		} else if (!is_object_end(c)) {
			o = make_node(SERD_CURIE, read_qname(reader), 0, 0);
		}
		ret = o.value;
	}

	if (ret && emit) {
		assert(o.value);
		emit_statement(reader, ctx.graph, ctx.subject, ctx.predicate, &o);
	}

except:
	pop_string(reader, o.lang);
	pop_string(reader, o.datatype);
	pop_string(reader, o.value);
	return ret;
}

// Spec:   [8] objectList ::= object ( ',' object )*
// Actual: [8] objectList ::= object ( ws* ',' ws* object )*
static bool
read_objectList(SerdReader reader, ReadContext ctx)
{
	TRY_RET(read_object(reader, ctx));
	read_ws_star(reader);
	while (peek_byte(reader) == ',') {
		eat_byte(reader, ',');
		read_ws_star(reader);
		TRY_RET(read_object(reader, ctx));
		read_ws_star(reader);
	}
	return true;
}

// Spec:   [7] predicateObjectList ::= verb objectList ( ';' verb objectList )* ( ';' )?
// Actual: [7] predicateObjectList ::= verb ws+ objectList ( ws* ';' ws* verb ws+ objectList )* ( ';' )?
static bool
read_predicateObjectList(SerdReader reader, ReadContext ctx)
{
	if (reader->eof) {
		return false;
	}
	Node predicate = SERD_NODE_NULL;
	TRY_RET(read_verb(reader, &predicate));
	TRY_THROW(read_ws_plus(reader));
	ctx.predicate = &predicate;
	TRY_THROW(read_objectList(reader, ctx));
	pop_string(reader, predicate.value);
	predicate.value = 0;
	read_ws_star(reader);
	while (peek_byte(reader) == ';') {
		eat_byte(reader, ';');
		read_ws_star(reader);
		switch (peek_byte(reader)) {
		case '.': case ']':
			return true;
		default:
			TRY_THROW(read_verb(reader, &predicate));
			ctx.predicate = &predicate;
			TRY_THROW(read_ws_plus(reader));
			TRY_THROW(read_objectList(reader, ctx));
			pop_string(reader, predicate.value);
			predicate.value = 0;
			read_ws_star(reader);
		}
	}
	return true;
except:
	pop_string(reader, predicate.value);
	return false;
}

/** Recursive helper for read_collection. */
static bool
read_collection_rec(SerdReader reader, ReadContext ctx)
{
	read_ws_star(reader);
	if (peek_byte(reader) == ')') {
		eat_byte(reader, ')');
		emit_statement(reader, NULL, ctx.subject, &reader->rdf_rest, &reader->rdf_nil);
		return false;
	} else {
		const Node rest = make_node(SERD_BLANK_ID, blank_id(reader), 0, 0);
		emit_statement(reader, ctx.graph, ctx.subject, &reader->rdf_rest, &rest);
		ctx.subject = &rest;
		ctx.predicate = &reader->rdf_first;
		if (read_object(reader, ctx)) {
			read_collection_rec(reader, ctx);
			pop_string(reader, rest.value);
			return true;
		} else {
			pop_string(reader, rest.value);
			return false;
		}
	}
}

// [22] itemList   ::= object+
// [23] collection ::= '(' itemList? ')'
static bool
read_collection(SerdReader reader, ReadContext ctx, Node* dest)
{
	TRY_RET(eat_byte(reader, '('));
	read_ws_star(reader);
	if (peek_byte(reader) == ')') {  // Empty collection
		eat_byte(reader, ')');
		*dest = reader->rdf_nil;
		return true;
	}

	*dest = make_node(SERD_BLANK_ID, blank_id(reader), 0, 0);
	ctx.subject   = dest;
	ctx.predicate = &reader->rdf_first;
	if (!read_object(reader, ctx)) {
		pop_string(reader, dest->value);
		return error(reader, "unexpected end of collection\n");
	}

	ctx.subject = dest;
	return read_collection_rec(reader, ctx);
}

// [11] subject ::= resource | blank
static Node
read_subject(SerdReader reader, ReadContext ctx)
{
	Node    subject = SERD_NODE_NULL;
	switch (peek_byte(reader)) {
	case '[': case '(': case '_':
		read_blank(reader, ctx, &subject);
		break;
	default:
		read_resource(reader, &subject);
	}
	return subject;
}

// Spec:   [6] triples ::= subject predicateObjectList
// Actual: [6] triples ::= subject ws+ predicateObjectList
static bool
read_triples(SerdReader reader, ReadContext ctx)
{
	const Node subject = read_subject(reader, ctx);
	bool       ret     = false;
	if (subject.value != 0) {
		ctx.subject = &subject;
		TRY_RET(read_ws_plus(reader));
		ret = read_predicateObjectList(reader, ctx);
		pop_string(reader, subject.value);
	}
	ctx.subject = ctx.predicate = 0;
	return ret;
}

// [5] base ::= '@base' ws+ uriref
static bool
read_base(SerdReader reader)
{
	// `@' is already eaten in read_directive
	eat_string(reader, "base", 4);
	TRY_RET(read_ws_plus(reader));
	Ref uri;
	TRY_RET(uri = read_uriref(reader));
	reader->base_sink(reader->handle, deref(reader, uri));
	pop_string(reader, uri);
	return true;
}

// Spec:   [4] prefixID ::= '@prefix' ws+ prefixName? ':' uriref
// Actual: [4] prefixID ::= '@prefix' ws+ prefixName? ':' ws* uriref
static bool
read_prefixID(SerdReader reader)
{
	// `@' is already eaten in read_directive
	eat_string(reader, "prefix", 6);
	TRY_RET(read_ws_plus(reader));
	bool ret = false;
	Ref name = read_prefixName(reader);
	if (!name) {
		name = push_string(reader, "", 1);
	}
	TRY_THROW(eat_byte(reader, ':') == ':');
	read_ws_star(reader);
	Ref uri = 0;
	TRY_THROW(uri = read_uriref(reader));
	ret = reader->prefix_sink(reader->handle,
	                          deref(reader, name),
	                          deref(reader, uri));
	pop_string(reader, uri);
except:
	pop_string(reader, name);
	return ret;
}

// [3] directive ::= prefixID | base
static bool
read_directive(SerdReader reader)
{
	eat_byte(reader, '@');
	switch (peek_byte(reader)) {
	case 'b':
		return read_base(reader);
	case 'p':
		return read_prefixID(reader);
	default:
		return error(reader, "illegal directive\n");
	}
}

// Spec:   [1] statement ::= directive '.' | triples '.' | ws+
// Actual: [1] statement ::= directive ws* '.' | triples ws* '.' | ws+
static bool
read_statement(SerdReader reader)
{
	ReadContext ctx = { 0, 0, 0 };
	read_ws_star(reader);
	if (reader->eof) {
		return true;
	}
	switch (peek_byte(reader)) {
	case '@':
		TRY_RET(read_directive(reader));
		break;
	default:
		TRY_RET(read_triples(reader, ctx));
		break;
	}
	read_ws_star(reader);
	return eat_byte(reader, '.');
}

// [1] turtleDoc ::= statement
static bool
read_turtleDoc(SerdReader reader)
{
	while (!reader->err && !reader->eof) {
		TRY_RET(read_statement(reader));
	}
	return !reader->err;
}

SERD_API
SerdReader
serd_reader_new(SerdSyntax        syntax,
                void*             handle,
                SerdBaseSink      base_sink,
                SerdPrefixSink    prefix_sink,
                SerdStatementSink statement_sink,
                SerdEndSink       end_sink)
{
	const Cursor cur    = { NULL, 0, 0 };
	SerdReader   reader = malloc(sizeof(struct SerdReaderImpl));
	reader->handle         = handle;
	reader->base_sink      = base_sink;
	reader->prefix_sink    = prefix_sink;
	reader->statement_sink = statement_sink;
	reader->end_sink       = end_sink;
	reader->fd             = 0;
	reader->stack          = serd_stack_new(STACK_PAGE_SIZE);
	reader->cur            = cur;
	reader->next_id        = 1;
	reader->err            = 0;
	reader->read_buf       = (uint8_t*)malloc(READ_BUF_LEN * 2);
	reader->read_head      = 0;
	reader->eof            = false;
#ifdef STACK_DEBUG
	reader->alloc_stack    = 0;
	reader->n_allocs       = 0;
#endif

	memset(reader->read_buf, '\0', READ_BUF_LEN * 2);

	/* Read into the second page of the buffer.  Occasionally readahead
	   will move the read_head to before this point when readahead causes
	   a page fault.
	*/
	reader->read_buf += READ_BUF_LEN;  // Read 1 page in
	return reader;
}

SERD_API
void
serd_reader_free(SerdReader reader)
{
	SerdReader const me = (SerdReader)reader;
#ifdef STACK_DEBUG
	free(me->alloc_stack);
#endif
	free(me->stack.buf);
	free(me->read_buf - READ_BUF_LEN);
	free(me);
}

SERD_API
bool
serd_reader_read_file(SerdReader reader, FILE* file, const uint8_t* name)
{
	#define RDF_FIRST "http://www.w3.org/1999/02/22-rdf-syntax-ns#first"
	#define RDF_REST  "http://www.w3.org/1999/02/22-rdf-syntax-ns#rest"
	#define RDF_NIL   "http://www.w3.org/1999/02/22-rdf-syntax-ns#nil"
	SerdReader const me  = (SerdReader)reader;
	const Cursor     cur = { name, 1, 1 };

	me->fd        = file;
	me->cur       = cur;
	me->rdf_first = make_node(SERD_URI, push_string(me, RDF_FIRST, 49), 0, 0);
	me->rdf_rest  = make_node(SERD_URI, push_string(me, RDF_REST, 48), 0, 0);
	me->rdf_nil   = make_node(SERD_URI, push_string(me, RDF_NIL, 47), 0, 0);

	fread(me->read_buf, 1, READ_BUF_LEN, file);
	const bool ret = read_turtleDoc(me);

	pop_string(me, me->rdf_nil.value);
	pop_string(me, me->rdf_rest.value);
	pop_string(me, me->rdf_first.value);
	me->cur = cur;
	me->fd  = 0;

	return ret;
}
