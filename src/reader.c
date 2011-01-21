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

#define TRY_THROW(exp) if (!(exp)) goto except;
#define TRY_RET(exp)   if (!(exp)) return 0;

#define STACK_CHUNK_SIZE  4096
#define STACK_INITIAL_TOP 8
#ifndef NDEBUG
#define STACK_DEBUG       1
#endif

typedef struct {
	const uint8_t* filename;
	unsigned       line;
	unsigned       col;
} Cursor;

typedef struct {
	uint8_t* buf;       ///< Stack memory
	size_t   buf_size;  ///< Allocated size of buf (>= size)
	size_t   size;      ///< Conceptual size of stack in buf
} Stack;

typedef uint32_t uchar;

typedef size_t Ref;

static const int32_t READ_BUF_LEN  = 4096;
static const int32_t MAX_READAHEAD = 8;

typedef struct {
	SerdNodeType type;
	Ref          value;
	Ref          datatype;
	Ref          lang;
} Node;

static const Node SERD_NODE_NULL = {0,0,0,0};

struct SerdReaderImpl {
	void*                handle;
	SerdBaseHandler      base_handler;
	SerdPrefixHandler    prefix_handler;
	SerdStatementHandler statement_handler;
	Node                 rdf_type;
	Node                 rdf_first;
	Node                 rdf_rest;
	Node                 rdf_nil;
	FILE*                fd;
	Stack                stack;
	Cursor               cur;
	uint8_t*             buf;
	unsigned             next_id;
	int                  err;
	uint8_t*             read_buf;
	int32_t              read_head;  ///< Offset into read_buf
	bool                 eof;
#ifdef STACK_DEBUG
	Ref*                 alloc_stack;  ///< Stack of push offsets
	size_t               n_allocs;     ///< Number of stack pushes
#endif
};

typedef enum {
	SERD_SUCCESS = 0,  ///< Completed successfully
	SERD_FAILURE = 1,  ///< Non-fatal failure
	SERD_ERROR   = 2,  ///< Fatal error
} SerdStatus;

static inline int
error(SerdReader parser, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "error: %s:%u:%u: ",
	        parser->cur.filename, parser->cur.line, parser->cur.col);
	vfprintf(stderr, fmt, args);
	parser->err = 1;
	return 0;
}

static Node
make_node(SerdNodeType type, Ref value, Ref datatype, Ref lang)
{
	const Node ret = { type, value, datatype, lang };
	return ret;
}

static inline bool
page(SerdReader parser)
{
	parser->read_head = 0;
	const int32_t n_read = fread(parser->read_buf, 1, READ_BUF_LEN, parser->fd);
	if (n_read == 0) {
		parser->read_buf[0] = '\0';
		parser->eof = true;
		return false;
	} else if (n_read < READ_BUF_LEN) {
		parser->read_buf[n_read] = '\0';
	}
	return true;
}

static inline bool
readahead(SerdReader parser, uint8_t* pre, int n)
{
	uint8_t* ptr = parser->read_buf + parser->read_head;
	for (int i = 0; i < n; ++i) {
		if (parser->read_head + i >= READ_BUF_LEN) {
			if (!page(parser)) {
				return false;
			}
			ptr = parser->read_buf;
			parser->read_head = -i;
			memcpy(parser->read_buf + parser->read_head, pre, i);
			assert(parser->read_buf[parser->read_head] == pre[0]);
		}
		if ((pre[i] = *ptr++) == '\0') {
			return false;
		}
	}
	return true;
}

static inline uint8_t
peek_byte(SerdReader parser)
{
	return parser->read_buf[parser->read_head];
}

static inline uint8_t
eat_byte(SerdReader parser, const uint8_t byte)
{
	const uint8_t c = peek_byte(parser);
	++parser->read_head;
	switch (c) {
	case '\0': return error(parser, "unexpected end of file\n");
	case '\n': ++parser->cur.line; parser->cur.col = 0; break;
	default:   ++parser->cur.col;
	}

	if (c != byte) {
		return error(parser, "expected `%c', not `%c'\n", byte, c);
	}
	if (parser->read_head == READ_BUF_LEN) {
		TRY_RET(page(parser));
	}
	assert(parser->read_head < READ_BUF_LEN);
	if (parser->read_buf[parser->read_head] == '\0') {
		parser->eof = true;
	}
	return c;
}

static inline void
eat_string(SerdReader parser, const char* str, unsigned n)
{
	for (unsigned i = 0; i < n; ++i) {
		eat_byte(parser, ((const uint8_t*)str)[i]);
	}
}

static inline bool
in_range(const uchar c, const uchar min, const uchar max)
{
	return (c >= min && c <= max);
}

#ifdef STACK_DEBUG
static inline bool
stack_is_top_string(SerdReader parser, Ref ref)
{
	return ref == parser->alloc_stack[parser->n_allocs - 1];
}
#endif

static inline uint8_t*
stack_push(SerdReader parser, size_t n_bytes)
{
	const size_t new_size = parser->stack.size + n_bytes;
	if (parser->stack.buf_size < new_size) {
		parser->stack.buf_size = ((new_size / STACK_CHUNK_SIZE) + 1) * STACK_CHUNK_SIZE;
		parser->stack.buf      = realloc(parser->stack.buf, parser->stack.buf_size);
	}
	uint8_t* const ret = (parser->stack.buf + parser->stack.size);
	parser->stack.size = new_size;
	return ret;
}

static inline intptr_t
pad_size(intptr_t size)
{
	return (size + 7) & (~7);
}

// Make a new string from a non-UTF-8 C string (internal use only)
static Ref
push_string(SerdReader parser, const char* c_str, size_t n_bytes)
{
	// Align strings to 64-bits (assuming malloc/realloc are aligned to 64-bits)
	const size_t      stack_size = pad_size((intptr_t)parser->stack.size);
	const size_t      pad        = stack_size - parser->stack.size;
	SerdString* const str = (SerdString*)(
		stack_push(parser, pad + sizeof(SerdString) + n_bytes) + pad);
	str->n_bytes = n_bytes;
	str->n_chars = n_bytes - 1;
	memcpy(str->buf, c_str, n_bytes);
#ifdef STACK_DEBUG
	parser->alloc_stack = realloc(parser->alloc_stack, sizeof(uint8_t*) * (++parser->n_allocs));
	parser->alloc_stack[parser->n_allocs - 1] = ((uint8_t*)str - parser->stack.buf);
#endif
	return (uint8_t*)str - parser->stack.buf;
}

static inline SerdString*
deref(SerdReader parser, const Ref ref)
{
	if (ref) {
		return (SerdString*)(parser->stack.buf + ref);
	}
	return NULL;
}

static inline void
push_byte(SerdReader parser, Ref ref, const uint8_t c)
{
	#ifdef STACK_DEBUG
	assert(stack_is_top_string(parser, ref));
	#endif
	stack_push(parser, 1);
	SerdString* const str = deref(parser, ref);
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
pop_string(SerdReader parser, Ref ref)
{
	if (ref) {
		if (ref == parser->rdf_nil.value
		    || ref == parser->rdf_first.value
		    || ref == parser->rdf_rest.value) {
			return;
		}
		#ifdef STACK_DEBUG
		if (!stack_is_top_string(parser, ref)) {
			fprintf(stderr, "attempt to pop non-top string %s\n", deref(parser, ref)->buf);
			fprintf(stderr, "top: %s\n",
			        deref(parser, parser->alloc_stack[parser->n_allocs - 1])->buf);
		}
		assert(stack_is_top_string(parser, ref));
		--parser->n_allocs;
		#endif
		parser->stack.size -= deref(parser, ref)->n_bytes;
	}
}

static inline void
emit_statement(SerdReader parser,
               const Node* g, const Node* s, const Node* p, const Node* o)
{
	parser->statement_handler(parser->handle,
	                          g ? deref(parser, g->value) : NULL,
	                          deref(parser, s->value), s->type,
	                          deref(parser, p->value), p->type,
	                          deref(parser, o->value), o->type,
	                          deref(parser, o->datatype), deref(parser, o->lang));
}

static bool read_collection(SerdReader parser, Node* dest);
static bool read_predicateObjectList(SerdReader parser, const Node* subject);

// [40]	hex	::=	[#x30-#x39] | [#x41-#x46]
static inline uint8_t
read_hex(SerdReader parser)
{
	const uint8_t c = peek_byte(parser);
	if (in_range(c, 0x30, 0x39) || in_range(c, 0x41, 0x46)) {
		return eat_byte(parser, c);
	} else {
		return error(parser, "illegal hexadecimal digit `%c'\n", c);
	}
}

static inline bool
read_hex_escape(SerdReader parser, unsigned length, Ref dest)
{
	uint8_t buf[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (unsigned i = 0; i < length; ++i) {
		buf[i] = read_hex(parser);
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
		push_byte(parser, dest, buf[i]);
	}
	return true;
}

static inline bool
read_character_escape(SerdReader parser, Ref dest)
{
	switch (peek_byte(parser)) {
	case '\\':
		push_byte(parser, dest, eat_byte(parser, '\\'));
		return true;
	case 'u':
		eat_byte(parser, 'u');
		return read_hex_escape(parser, 4, dest);
	case 'U':
		eat_byte(parser, 'U');
		return read_hex_escape(parser, 8, dest);
	default:
		return false;
	}
}

static inline bool
read_echaracter_escape(SerdReader parser, Ref dest)
{
	switch (peek_byte(parser)) {
	case 't':
		eat_byte(parser, 't');
		push_byte(parser, dest, '\t');
		return true;
	case 'n':
		eat_byte(parser, 'n');
		push_byte(parser, dest, '\n');
		return true;
	case 'r':
		eat_byte(parser, 'r');
		push_byte(parser, dest, '\r');
		return true;
	default:
		return read_character_escape(parser, dest);
	}
}

static inline bool
read_scharacter_escape(SerdReader parser, Ref dest)
{
	switch (peek_byte(parser)) {
	case '"':
		push_byte(parser, dest, eat_byte(parser, '"'));
		return true;
	default:
		return read_echaracter_escape(parser, dest);
	}
}

static inline bool
read_ucharacter_escape(SerdReader parser, Ref dest)
{
	switch (peek_byte(parser)) {
	case '>':
		push_byte(parser, dest, eat_byte(parser, '>'));
		return true;
	default:
		return read_echaracter_escape(parser, dest);
	}
}

// [38] character ::= '\u' hex hex hex hex
//                  | '\U' hex hex hex hex hex hex hex hex
//                  | '\\'
//                  | [#x20-#x5B] | [#x5D-#x10FFFF]
static inline SerdStatus
read_character(SerdReader parser, Ref dest)
{
	const uint8_t c = peek_byte(parser);
	switch (c) {
	case '\0':
		error(parser, "unexpected end of file\n", peek_byte(parser));
		return SERD_ERROR;
	case '\\':
		eat_byte(parser, '\\');
		if (read_character_escape(parser, dest)) {
			return SERD_SUCCESS;
		} else {
			error(parser, "invalid escape `\\%c'\n", peek_byte(parser));
			return SERD_ERROR;
		}
	default:
		if (in_range(c, 0x20, 0x5B) || in_range(c, 0x5D, 0x10FFF)) {
			push_byte(parser, dest, eat_byte(parser, c));
			return SERD_SUCCESS;
		} else {
			return SERD_FAILURE;
		}
	}
}

// [39] echaracter ::= character | '\t' | '\n' | '\r'
static inline SerdStatus
read_echaracter(SerdReader parser, Ref dest)
{
	uint8_t c = peek_byte(parser);
	switch (c) {
	case '\\':
		eat_byte(parser, '\\');
		if (read_echaracter_escape(parser, peek_byte(parser))) {
			return SERD_SUCCESS;
		} else {
			error(parser, "illegal escape `\\%c'\n", peek_byte(parser));
			return SERD_ERROR;
		}
	default:
		return read_character(parser, dest);
	}
}

// [43] lcharacter ::= echaracter | '\"' | #x9 | #xA | #xD
static inline SerdStatus
read_lcharacter(SerdReader parser, Ref dest)
{
	const uint8_t c = peek_byte(parser);
	uint8_t       pre[3];
	switch (c) {
	case '"':
		readahead(parser, pre, 3);
		if (pre[1] == '\"' && pre[2] == '\"') {
			eat_byte(parser, '\"');
			eat_byte(parser, '\"');
			eat_byte(parser, '\"');
			return SERD_FAILURE;
		} else {
			push_byte(parser, dest, eat_byte(parser, '"'));
			return SERD_SUCCESS;
		}
	case '\\':
		eat_byte(parser, '\\');
		if (read_scharacter_escape(parser, dest)) {
			return SERD_SUCCESS;
		} else {
			error(parser, "illegal escape `\\%c'\n", peek_byte(parser));
			return SERD_ERROR;
		}
	case 0x9: case 0xA: case 0xD:
		push_byte(parser, dest, eat_byte(parser, c));
		return SERD_SUCCESS;
	default:
		return read_echaracter(parser, dest);
	}
}

// [42] scharacter ::= ( echaracter - #x22 ) | '\"'
static inline SerdStatus
read_scharacter(SerdReader parser, Ref dest)
{
	uint8_t c = peek_byte(parser);
	switch (c) {
	case '\\':
		eat_byte(parser, '\\');
		if (read_scharacter_escape(parser, dest)) {
			return SERD_SUCCESS;
		} else {
			error(parser, "illegal escape `\\%c'\n", peek_byte(parser));
			return SERD_ERROR;
		}
	case '\"':
		return SERD_FAILURE;
	default:
		return read_character(parser, dest);
	}
}

// Spec:   [41] ucharacter ::= ( character - #x3E ) | '\>'
// Actual: [41] ucharacter ::= ( echaracter - #x3E ) | '\>'
static inline SerdStatus
read_ucharacter(SerdReader parser, Ref dest)
{
	const uint8_t c = peek_byte(parser);
	switch (c) {
	case '\\':
		eat_byte(parser, '\\');
		if (read_ucharacter_escape(parser, dest)) {
			return SERD_SUCCESS;
		} else {
			return error(parser, "illegal escape `\\%c'\n", peek_byte(parser));
		}
	case '>':
		return SERD_FAILURE;
	default:
		return read_character(parser, dest);
	}
}

// [10] comment ::= '#' ( [^#xA #xD] )*
static void
read_comment(SerdReader parser)
{
	eat_byte(parser, '#');
	uint8_t c;
	while (((c = peek_byte(parser)) != 0xA) && (c != 0xD)) {
		eat_byte(parser, c);
	}
}

// [24] ws ::= #x9 | #xA | #xD | #x20 | comment
static inline bool
read_ws(SerdReader parser)
{
	const uint8_t c = peek_byte(parser);
	switch (c) {
	case 0x9: case 0xA: case 0xD: case 0x20:
		eat_byte(parser, c);
		return true;
	case '#':
		read_comment(parser);
		return true;
	default:
		return false;
	}
}

static inline void
read_ws_star(SerdReader parser)
{
	while (read_ws(parser)) {}
}

static inline bool
read_ws_plus(SerdReader parser)
{
	TRY_RET(read_ws(parser));
	read_ws_star(parser);
	return true;
}

// [37] longSerdString ::= #x22 #x22 #x22 lcharacter* #x22 #x22 #x22
static Ref
read_longString(SerdReader parser)
{
	eat_string(parser, "\"\"\"", 3);
	Ref        str = push_string(parser, "", 1);
	SerdStatus st;
	while (!(st = read_lcharacter(parser, str))) {}
	if (st != SERD_ERROR) {
		return str;
	}
	pop_string(parser, str);
	return 0;
}

// [36] string ::= #x22 scharacter* #x22
static Ref
read_string(SerdReader parser)
{
	eat_byte(parser, '\"');
	Ref        str = push_string(parser, "", 1);
	SerdStatus st;
	while (!(st = read_scharacter(parser, str))) {}
	if (st != SERD_ERROR) {
		eat_byte(parser, '\"');
		return str;
	}
	pop_string(parser, str);
	return 0;
}

// [35] quotedString ::= string | longSerdString
static Ref
read_quotedString(SerdReader parser)
{
	uint8_t pre[3];
	readahead(parser, pre, 3);
	assert(pre[0] == '\"');
	switch (pre[1]) {
	case '\"':
		if (pre[2] == '\"')
			return read_longString(parser);
		else
			return read_string(parser);
	default:
		return read_string(parser);
	}
}

// [34] relativeURI ::= ucharacter*
static inline Ref
read_relativeURI(SerdReader parser)
{
	Ref str = push_string(parser, "", 1);
	SerdStatus st;
	while (!(st = read_ucharacter(parser, str))) {}
	if (st != SERD_ERROR) {
		return str;
	}
	return st;
}

// [30] nameStartChar ::= [A-Z] | "_" | [a-z]
//      | [#x00C0-#x00D6] | [#x00D8-#x00F6] | [#x00F8-#x02FF] | [#x0370-#x037D]
//      | [#x037F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF]
//      | [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] | [#x10000-#xEFFFF]
static inline uchar
read_nameStartChar(SerdReader parser, bool required)
{
	const uint8_t c = peek_byte(parser);
	if (in_range(c, 'A', 'Z') || (c == '_') || in_range(c, 'a', 'z')) {
		return eat_byte(parser, c);
	} else {
		if (required) {
			error(parser, "illegal character `%c'\n", c);
		}
		return 0;
	}
}

// [31] nameChar ::= nameStartChar | '-' | [0-9] | #x00B7 | [#x0300-#x036F] | [#x203F-#x2040]
static inline uchar
read_nameChar(SerdReader parser)
{
	uchar c = read_nameStartChar(parser, false);
	if (c)
		return c;

	switch ((c = peek_byte(parser))) {
	case '-': case 0xB7: case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return eat_byte(parser, c);
	default:
		if (in_range(c, 0x300, 0x036F) || in_range(c, 0x203F, 0x2040)) {
			return eat_byte(parser, c);
		}
	}
	return 0;
}

// [33] prefixName ::= ( nameStartChar - '_' ) nameChar*
static Ref
read_prefixName(SerdReader parser)
{
	uint8_t c = peek_byte(parser);
	if (c == '_') {
		error(parser, "unexpected `_'\n");
		return 0;
	}
	TRY_RET(c = read_nameStartChar(parser, false));
	Ref str = push_string(parser, "", 1);
	push_byte(parser, str, c);
	while ((c = read_nameChar(parser)) != 0) {
		push_byte(parser, str, c);
	}
	return str;
}

// [32] name ::= nameStartChar nameChar*
static Ref
read_name(SerdReader parser, Ref dest, bool required)
{
	uchar c = read_nameStartChar(parser, required);
	if (!c) {
		if (required) {
			error(parser, "illegal character at start of name\n");
		}
		return 0;
	}
	do {
		push_byte(parser, dest, c);
	} while ((c = read_nameChar(parser)) != 0);
	return dest;
}

// [29] language ::= [a-z]+ ('-' [a-z0-9]+ )*
static Ref
read_language(SerdReader parser)
{
	const uint8_t start = peek_byte(parser);
	if (!in_range(start, 'a', 'z')) {
		error(parser, "unexpected `%c'\n", start);
		return 0;
	}
	Ref str = push_string(parser, "", 1);
	push_byte(parser, str, eat_byte(parser, start));
	uint8_t c;
	while ((c = peek_byte(parser)) && in_range(c, 'a', 'z')) {
		push_byte(parser, str, eat_byte(parser, c));
	}
	if (peek_byte(parser) == '-') {
		push_byte(parser, str, eat_byte(parser, '-'));
		while ((c = peek_byte(parser)) && (in_range(c, 'a', 'z') || in_range(c, '0', '9'))) {
			push_byte(parser, str, eat_byte(parser, c));
		}
	}
	return str;
}

// [28] uriref ::= '<' relativeURI '>'
static Ref
read_uriref(SerdReader parser)
{
	TRY_RET(eat_byte(parser, '<'));
	Ref const str = read_relativeURI(parser);
	if (str) {
		TRY_THROW(eat_byte(parser, '>'));
		return str;
	}
except:
	pop_string(parser, str);
	return 0;
}

// [27] qname ::= prefixName? ':' name?
static Ref
read_qname(SerdReader parser)
{
	Ref prefix = read_prefixName(parser);
	if (!prefix) {
		prefix = push_string(parser, "", 1);
	}
	TRY_THROW(eat_byte(parser, ':'));
	push_byte(parser, prefix, ':');
	Ref str = read_name(parser, prefix, false);
	if (parser->err) {
		pop_string(parser, prefix);
		return 0;
	}
	return str ? str : prefix;
except:
	pop_string(parser, prefix);
	return 0;
}


static Ref
read_0_9(SerdReader parser, Ref str, bool at_least_one)
{
	uint8_t c;
	if (at_least_one) {
		TRY_RET(in_range(c = peek_byte(parser), '0', '9'));
		push_byte(parser, str, eat_byte(parser, c));
	}
	while (in_range((c = peek_byte(parser)), '0', '9')) {
		push_byte(parser, str, eat_byte(parser, c));
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
read_number(SerdReader parser, Node* dest)
{
	#define XSD_DECIMAL "http://www.w3.org/2001/XMLSchema#decimal"
	#define XSD_DOUBLE  "http://www.w3.org/2001/XMLSchema#double"
	#define XSD_INTEGER "http://www.w3.org/2001/XMLSchema#integer"
	Ref     str         = push_string(parser, "", 1);
	uint8_t c           = peek_byte(parser);
	bool    has_decimal = false;
	Ref     datatype    = 0;
	if (c == '-' || c == '+') {
		push_byte(parser, str, eat_byte(parser, c));
	}
	if ((c = peek_byte(parser)) == '.') {
		has_decimal = true;
		// decimal case 2 (e.g. '.0' or `-.0' or `+.0')
		push_byte(parser, str, eat_byte(parser, c));
		TRY_THROW(str = read_0_9(parser, str, true));
	} else {
		// all other cases ::= ( '-' | '+' ) [0-9]+ ( . )? ( [0-9]+ )? ...
		TRY_THROW(str = read_0_9(parser, str, true));
		if ((c = peek_byte(parser)) == '.') {
			has_decimal = true;
			push_byte(parser, str, eat_byte(parser, c));
			TRY_THROW(str = read_0_9(parser, str, false));
		}
	}
	c = peek_byte(parser);
	if (c == 'e' || c == 'E') {
		// double
		push_byte(parser, str, eat_byte(parser, c));
		str = read_0_9(parser, str, true);
		datatype = push_string(parser, XSD_DOUBLE, strlen(XSD_DOUBLE) + 1);
	} else if (has_decimal) {
		datatype = push_string(parser, XSD_DECIMAL, strlen(XSD_DECIMAL) + 1);
	} else {
		datatype = push_string(parser, XSD_INTEGER, strlen(XSD_INTEGER) + 1);
	}
	*dest = make_node(LITERAL, str, datatype, 0);
	assert(dest->value);
	return true;
except:
	pop_string(parser, datatype);
	pop_string(parser, str);
	return false;
}

// [25] resource ::= uriref | qname
static bool
read_resource(SerdReader parser, Node* dest)
{
	switch (peek_byte(parser)) {
	case '<':
		*dest = make_node(URI, read_uriref(parser), 0, 0);
		break;
	default:
		*dest = make_node(QNAME, read_qname(parser), 0, 0);
	}
	return (dest->value != 0);
}

// [14] literal ::= quotedString ( '@' language )? | datatypeSerdString
//                | integer | double | decimal | boolean
static bool
read_literal(SerdReader parser, Node* dest)
{
	Ref           str      = 0;
	Node          datatype = SERD_NODE_NULL;
	const uint8_t c        = peek_byte(parser);
	if (in_range(c, '0', '9') || c == '-' || c == '+') {
		return read_number(parser, dest);
	} else if (c == '\"') {
		str = read_quotedString(parser);
		if (!str) {
			return false;
		}

		Ref lang = 0;
		switch (peek_byte(parser)) {
		case '^':
			eat_byte(parser, '^');
			eat_byte(parser, '^');
			TRY_THROW(read_resource(parser, &datatype));
			break;
		case '@':
			eat_byte(parser, '@');
			TRY_THROW(lang = read_language(parser));
		}
		*dest = make_node(LITERAL, str, datatype.value, lang);
	} else {
		*dest = make_node(QNAME, read_qname(parser), 0, 0);
	}
	return true;
except:
	pop_string(parser, str);
	return false;
}

// [12] predicate ::= resource
static bool
read_predicate(SerdReader parser, Node* dest)
{
	return read_resource(parser, dest);
}

// [9] verb ::= predicate | 'a'
static bool
read_verb(SerdReader parser, Node* dest)
{
	#define RDF_TYPE "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
	uint8_t pre[2];
	readahead(parser, pre, 2);
	switch (pre[0]) {
	case 'a':
		switch (pre[1]) {
		case 0x9: case 0xA: case 0xD: case 0x20:
			eat_byte(parser, 'a');
			*dest = make_node(URI, push_string(parser, RDF_TYPE, 48), 0, 0);
			return true;
		default: break;  // fall through
		}
	default:
		return read_predicate(parser, dest);
	}
}

// [26] nodeID ::= '_:' name
static Ref
read_nodeID(SerdReader parser)
{
	eat_byte(parser, '_');
	eat_byte(parser, ':');
	Ref str = push_string(parser, "", 1);
	return read_name(parser, str, true);
}

static Ref
blank_id(SerdReader parser)
{
	char str[32];
	const int len = snprintf(str, sizeof(str), "genid%u", parser->next_id++);
	return push_string(parser, str, len + 1);
}

// Spec:   [21] blank ::= nodeID | '[]' | '[' predicateObjectList ']' | collection
// Actual: [21] blank ::= nodeID | '[ ws* ]' | '[' ws* predicateObjectList ws* ']' | collection
static bool
read_blank(SerdReader parser, Node* dest)
{
	switch (peek_byte(parser)) {
	case '_':
		*dest = make_node(BLANK, read_nodeID(parser), 0, 0);
		return true;
	case '[':
		eat_byte(parser, '[');
		read_ws_star(parser);
		if (peek_byte(parser) == ']') {
			eat_byte(parser, ']');
			*dest = make_node(BLANK, blank_id(parser), 0, 0);
			return true;
		} else {
			*dest = make_node(BLANK, blank_id(parser), 0, 0);
			read_predicateObjectList(parser, dest);
			read_ws_star(parser);
			eat_byte(parser, ']');
			return true;
		}
	case '(':
		return read_collection(parser, dest);
	default:
		error(parser, "illegal blank node\n");
	}
	// TODO: collections
	return false;
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
// Recurses, calling statement_handler for every statement encountered.
static bool
read_object(SerdReader parser, const Node* subject, const Node* predicate)
{
	static const char* const XSD_BOOLEAN     = "http://www.w3.org/2001/XMLSchema#boolean";
	static const size_t      XSD_BOOLEAN_LEN = 40;

	bool          ret = false;
	Node          o   = SERD_NODE_NULL;
	const uint8_t c   = peek_byte(parser);
	switch (c) {
	case ')':
		return false;
	case '[': case '(': case '_':
		TRY_THROW(ret = read_blank(parser, &o));
		break;
	case '<': case ':':
		TRY_THROW(ret = read_resource(parser, &o));
		break;
	case '\"': case '+': case '-':
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		TRY_THROW(ret = read_literal(parser, &o));
		break;
	case '.':
		TRY_THROW(ret = read_literal(parser, &o));
	default: break;
	}

	if (!ret) {
		assert(o.value == 0);
		/* Either a boolean literal, or a qname.
		   Unfortunately there is no way to distinbuish these without a lot of readahead,
		   since `true' or `false' could be the start of a qname.
		*/
		uint8_t pre[6];
		readahead(parser, pre, 6);
		if (!strncmp((char*)pre, "true", 4) && is_object_end(pre[4])) {
			eat_string(parser, "true", 4);
			const Ref value     = push_string(parser, "true", 5);
			const Ref datatype  = push_string(parser, XSD_BOOLEAN, XSD_BOOLEAN_LEN + 1);
			o = make_node(LITERAL, value, datatype, 0);
		} else if (!strncmp((char*)pre, "false", 5) && is_object_end(pre[5])) {
			eat_string(parser, "false", 5);
			const Ref value     = push_string(parser, "false", 6);
			const Ref datatype  = push_string(parser, XSD_BOOLEAN, XSD_BOOLEAN_LEN + 1);
			o = make_node(LITERAL, value, datatype, 0);
		} else if (!is_object_end(c)) {
			o = make_node(QNAME, read_qname(parser), 0, 0);
		}
	}

	if (o.value != 0) {
		emit_statement(parser, NULL, subject, predicate, &o);
		pop_string(parser, o.lang);
		pop_string(parser, o.datatype);
		pop_string(parser, o.value);
		return true;
	}
except:
	pop_string(parser, o.lang);
	pop_string(parser, o.datatype);
	pop_string(parser, o.value);
	return false;
}

// Spec:   [8] objectList ::= object ( ',' object )*
// Actual: [8] objectList ::= object ( ws* ',' ws* object )*
static bool
read_objectList(SerdReader parser, const Node* subject, const Node* predicate)
{
	TRY_RET(read_object(parser, subject, predicate));
	read_ws_star(parser);
	while (peek_byte(parser) == ',') {
		eat_byte(parser, ',');
		read_ws_star(parser);
		TRY_RET(read_object(parser, subject, predicate));
		read_ws_star(parser);
	}
	return true;
}

// Spec:   [7] predicateObjectList ::= verb objectList ( ';' verb objectList )* ( ';' )?
// Actual: [7] predicateObjectList ::= verb ws+ objectList ( ws* ';' ws* verb ws+ objectList )* ( ';' )?
static bool
read_predicateObjectList(SerdReader parser, const Node* subject)
{
	if (parser->eof) {
		return false;
	}
	Node predicate = SERD_NODE_NULL;
	TRY_RET(read_verb(parser, &predicate));
	TRY_THROW(read_ws_plus(parser));
	TRY_THROW(read_objectList(parser, subject, &predicate));
	pop_string(parser, predicate.value);
	predicate.value = 0;
	read_ws_star(parser);
	while (peek_byte(parser) == ';') {
		eat_byte(parser, ';');
		read_ws_star(parser);
		switch (peek_byte(parser)) {
		case '.': case ']':
			return true;
		default:
			TRY_THROW(read_verb(parser, &predicate));
			TRY_THROW(read_ws_plus(parser));
			TRY_THROW(read_objectList(parser, subject, &predicate));
			pop_string(parser, predicate.value);
			predicate.value = 0;
			read_ws_star(parser);
		}
	}
	return true;
except:
	pop_string(parser, predicate.value);
	return false;
}

/** Recursive helper for read_collection. */
static bool
read_collection_rec(SerdReader parser, const Node* head)
{
	read_ws_star(parser);
	if (peek_byte(parser) == ')') {
		eat_byte(parser, ')');
		emit_statement(parser, NULL, head, &parser->rdf_rest, &parser->rdf_nil);
		return false;
	} else {
		const Node rest = make_node(BLANK, blank_id(parser), 0, 0);
		emit_statement(parser, NULL, head, &parser->rdf_rest, &rest);
		if (read_object(parser, &rest, &parser->rdf_first)) {
			read_collection_rec(parser, &rest);
			pop_string(parser, rest.value);
			return true;
		} else {
			pop_string(parser, rest.value);
			return false;
		}
	}
}

// [22] itemList   ::= object+
// [23] collection ::= '(' itemList? ')'
static bool
read_collection(SerdReader parser, Node* dest)
{
	TRY_RET(eat_byte(parser, '('));
	read_ws_star(parser);
	if (peek_byte(parser) == ')') {  // Empty collection
		eat_byte(parser, ')');
		*dest = parser->rdf_nil;
		return true;
	}

	*dest = make_node(BLANK, blank_id(parser), 0, 0);
	if (!read_object(parser, dest, &parser->rdf_first)) {
		pop_string(parser, dest->value);
		return error(parser, "unexpected end of collection\n");
	}

	return read_collection_rec(parser, dest);
}

// [11] subject ::= resource | blank
static Node
read_subject(SerdReader parser)
{
	Node subject = SERD_NODE_NULL;
	switch (peek_byte(parser)) {
	case '[': case '(': case '_':
		read_blank(parser, &subject);
		break;
	default:
		read_resource(parser, &subject);
	}
	return subject;
}

// Spec:   [6] triples ::= subject predicateObjectList
// Actual: [6] triples ::= subject ws+ predicateObjectList
static bool
read_triples(SerdReader parser)
{
	const Node subject = read_subject(parser);
	if (subject.value != 0) {
		TRY_RET(read_ws_plus(parser));
		const bool ret = read_predicateObjectList(parser, &subject);
		pop_string(parser, subject.value);
		return ret;
	}
	return false;
}

// [5] base ::= '@base' ws+ uriref
static bool
read_base(SerdReader parser)
{
	// `@' is already eaten in read_directive
	eat_string(parser, "base", 4);
	TRY_RET(read_ws_plus(parser));
	Ref uri;
	TRY_RET(uri = read_uriref(parser));
	parser->base_handler(parser->handle, deref(parser, uri));
	pop_string(parser, uri);
	return true;
}

// Spec:   [4] prefixID ::= '@prefix' ws+ prefixName? ':' uriref
// Actual: [4] prefixID ::= '@prefix' ws+ prefixName? ':' ws* uriref
static bool
read_prefixID(SerdReader parser)
{
	// `@' is already eaten in read_directive
	eat_string(parser, "prefix", 6);
	TRY_RET(read_ws_plus(parser));
	bool ret = false;
	Ref name = read_prefixName(parser);
	if (!name) {
		name = push_string(parser, "", 1);
	}
	TRY_THROW(eat_byte(parser, ':') == ':');
	read_ws_star(parser);
	Ref uri = 0;
	TRY_THROW(uri = read_uriref(parser));
	ret = parser->prefix_handler(parser->handle,
	                             deref(parser, name),
	                             deref(parser, uri));
	pop_string(parser, uri);
except:
	pop_string(parser, name);
	return ret;
}

// [3] directive ::= prefixID | base
static bool
read_directive(SerdReader parser)
{
	eat_byte(parser, '@');
	switch (peek_byte(parser)) {
	case 'b':
		return read_base(parser);
	case 'p':
		return read_prefixID(parser);
	default:
		return error(parser, "illegal directive\n");
	}
}

// Spec:   [1] statement ::= directive '.' | triples '.' | ws+
// Actual: [1] statement ::= directive ws* '.' | triples ws* '.' | ws+
static bool
read_statement(SerdReader parser)
{
	read_ws_star(parser);
	if (parser->eof) {
		return true;
	}
	switch (peek_byte(parser)) {
	case '@':
		TRY_RET(read_directive(parser));
		break;
	default:
		TRY_RET(read_triples(parser));
		break;
	}
	read_ws_star(parser);
	return eat_byte(parser, '.');
}

// [1] turtleDoc ::= statement
static bool
read_turtleDoc(SerdReader parser)
{
	while (!parser->err && !parser->eof) {
		TRY_RET(read_statement(parser));
	}
	return !parser->err;
}

SERD_API
SerdReader
serd_reader_new(SerdSyntax           syntax,
                void*                handle,
                SerdBaseHandler      base_handler,
                SerdPrefixHandler    prefix_handler,
                SerdStatementHandler statement_handler)
{
	const Cursor cur    = { NULL, 0, 0 };
	SerdReader   reader = malloc(sizeof(struct SerdReaderImpl));
	reader->handle            = handle;
	reader->base_handler      = base_handler;
	reader->prefix_handler    = prefix_handler;
	reader->statement_handler = statement_handler;
	reader->fd                = 0;
	reader->stack.buf         = malloc(STACK_CHUNK_SIZE);
	reader->stack.buf_size    = STACK_CHUNK_SIZE;
	reader->stack.size        = 8;
	reader->cur               = cur;
	reader->next_id           = 1;
	reader->err               = 0;
	reader->read_buf          = (uint8_t*)malloc(READ_BUF_LEN * 2);
	reader->read_head         = 0;
	reader->eof               = false;
#ifdef STACK_DEBUG
	reader->alloc_stack       = 0;
	reader->n_allocs          = 0;
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
	SerdReader const me = (SerdReader)reader;
	const Cursor cur = { name, 1, 1 };
	me->fd  = file;
	me->cur = cur;
	me->rdf_first = make_node(URI, push_string(me, RDF_FIRST, 49), 0, 0);
	me->rdf_rest  = make_node(URI, push_string(me, RDF_REST, 48), 0, 0);
	me->rdf_nil   = make_node(URI, push_string(me, RDF_NIL, 47), 0, 0);
	fread(me->read_buf, 1, READ_BUF_LEN, file);
	const bool ret = read_turtleDoc(me);
	pop_string(me, me->rdf_nil.value);
	pop_string(me, me->rdf_rest.value);
	pop_string(me, me->rdf_first.value);
	me->fd  = 0;
	me->cur = cur;
	return ret;
}
