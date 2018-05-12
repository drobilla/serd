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

#include "byte_source.h"
#include "node.h"
#include "reader.h"
#include "serd_internal.h"
#include "stack.h"
#include "string_utils.h"
#include "uri_utils.h"

#include "serd/serd.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY_THROW(exp) if (!(exp)) goto except;
#define TRY_RET(exp)   if (!(exp)) return 0;

static inline bool
fancy_syntax(const SerdReader* reader)
{
	return reader->syntax == SERD_TURTLE || reader->syntax == SERD_TRIG;
}

static bool
read_collection(SerdReader* reader, ReadContext ctx, SerdNode** dest);

static bool
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot);

static inline uint8_t
read_HEX(SerdReader* reader)
{
	const int c = peek_byte(reader);
	if (is_xdigit(c)) {
		return (uint8_t)eat_byte_safe(reader, c);
	}

	return (uint8_t)r_err(reader, SERD_ERR_BAD_SYNTAX,
	                      "invalid hexadecimal digit `%c'\n", c);
}

// Read UCHAR escape, initial \ is already eaten by caller
static inline SerdStatus
read_UCHAR(SerdReader* reader, SerdNode* dest, uint32_t* char_code)
{
	const int b      = peek_byte(reader);
	unsigned  length = 0;
	switch (b) {
	case 'U':
		length = 8;
		break;
	case 'u':
		length = 4;
		break;
	default:
		return SERD_ERR_BAD_SYNTAX;
	}
	eat_byte_safe(reader, b);

	uint8_t buf[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (unsigned i = 0; i < length; ++i) {
		if (!(buf[i] = read_HEX(reader))) {
			return SERD_ERR_BAD_SYNTAX;
		}
	}

	char*          endptr = NULL;
	const uint32_t code   = (uint32_t)strtoul((const char*)buf, &endptr, 16);
	assert(endptr == (char*)buf + length);

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
		*char_code = 0xFFFD;
		const SerdStatus st = push_bytes(reader, dest, replacement_char, 3);
		return st ? st : SERD_SUCCESS;
	}

	// Build output in buf
	// (Note # of bytes = # of leading 1 bits in first byte)
	uint32_t c = code;
	switch (size) {
	case 4:
		buf[3] = (uint8_t)(0x80u | (c & 0x3Fu));
		c >>= 6;
		c |= (16 << 12);  // set bit 4
        // fallthru
	case 3:
		buf[2] = (uint8_t)(0x80u | (c & 0x3Fu));
		c >>= 6;
		c |= (32 << 6);  // set bit 5
        // fallthru
	case 2:
		buf[1] = (uint8_t)(0x80u | (c & 0x3Fu));
		c >>= 6;
		c |= 0xC0;  // set bits 6 and 7
        // fallthru
	case 1:
		buf[0] = (uint8_t)c;
	}

	*char_code = code;
	return push_bytes(reader, dest, buf, size);
}

// Read ECHAR escape, initial \ is already eaten by caller
static inline SerdStatus
read_ECHAR(SerdReader* reader, SerdNode* dest, SerdNodeFlags* flags)
{
	const int c = peek_byte(reader);
	switch (c) {
	case 't':
		eat_byte_safe(reader, 't');
		return push_byte(reader, dest, '\t');
	case 'b':
		eat_byte_safe(reader, 'b');
		return push_byte(reader, dest, '\b');
	case 'n':
		*flags |= SERD_HAS_NEWLINE;
		eat_byte_safe(reader, 'n');
		return push_byte(reader, dest, '\n');
	case 'r':
		*flags |= SERD_HAS_NEWLINE;
		eat_byte_safe(reader, 'r');
		return push_byte(reader, dest, '\r');
	case 'f':
		eat_byte_safe(reader, 'f');
		return push_byte(reader, dest, '\f');
	case '\\': case '"': case '\'':
		return push_byte(reader, dest, eat_byte_safe(reader, c));
	default:
		return SERD_ERR_BAD_SYNTAX;
	}
}

static inline SerdStatus
bad_char(SerdReader* reader, const char* fmt, uint8_t c)
{
	// Skip bytes until the next start byte
	for (int b = peek_byte(reader); b != EOF && ((uint8_t)b & 0x80);) {
		eat_byte_safe(reader, b);
		b = peek_byte(reader);
	}

	r_err(reader, SERD_ERR_BAD_SYNTAX, fmt, c);
	return reader->strict ? SERD_ERR_BAD_SYNTAX : SERD_FAILURE;
}

static SerdStatus
read_utf8_bytes(SerdReader* reader, uint8_t bytes[4], uint32_t* size, uint8_t c)
{
	*size = utf8_num_bytes(c);
	if (*size <= 1 || *size > 4) {
		return bad_char(reader, "invalid UTF-8 start 0x%X\n", c);
	}

	bytes[0] = c;
	for (unsigned i = 1; i < *size; ++i) {
		const int b = peek_byte(reader);
		if (b == EOF || ((uint8_t)b & 0x80) == 0) {
			return bad_char(reader, "invalid UTF-8 continuation 0x%X\n",
			                (uint8_t)b);
		}

		eat_byte_safe(reader, b);
		bytes[i] = (uint8_t)b;
	}

	return SERD_SUCCESS;
}

static SerdStatus
read_utf8_character(SerdReader* reader, SerdNode* dest, uint8_t c)
{
	uint32_t   size;
	uint8_t    bytes[4];
	SerdStatus st = read_utf8_bytes(reader, bytes, &size, c);
	if (st) {
		push_bytes(reader, dest, replacement_char, 3);
		return st;
	}

	return push_bytes(reader, dest, bytes, size);
}

static SerdStatus
read_utf8_code(SerdReader* reader, SerdNode* dest, uint32_t* code, uint8_t c)
{
	uint32_t   size;
	uint8_t    bytes[4] = { 0, 0, 0, 0 };
	SerdStatus st = read_utf8_bytes(reader, bytes, &size, c);
	if (st) {
		push_bytes(reader, dest, replacement_char, 3);
		return st;
	}

	if (!(st = push_bytes(reader, dest, bytes, size))) {
		*code = parse_counted_utf8_char(bytes, size);
	}

	return st;
}

// Read one character (possibly multi-byte)
// The first byte, c, has already been eaten by caller
static inline SerdStatus
read_character(SerdReader* reader, SerdNode* dest, SerdNodeFlags* flags, uint8_t c)
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

		const SerdStatus st = push_byte(reader, dest, c);
		return st ? st : SERD_SUCCESS;
	}
	return read_utf8_character(reader, dest, c);
}

// [10] comment ::= '#' ( [^#xA #xD] )*
static void
read_comment(SerdReader* reader)
{
	eat_byte_safe(reader, '#');
	int c;
	while (((c = peek_byte(reader)) != 0xA) && c != 0xD && c != EOF && c) {
		eat_byte_safe(reader, c);
	}
}

// [24] ws ::= #x9 | #xA | #xD | #x20 | comment
static inline bool
read_ws(SerdReader* reader)
{
	const int c = peek_byte(reader);
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
static SerdNode*
read_STRING_LITERAL_LONG(SerdReader* reader, SerdNodeFlags* flags, uint8_t q)
{
	SerdNode*  ref = push_node(reader, SERD_LITERAL, "", 0);
	SerdStatus st  = SERD_SUCCESS;
	while (!reader->status && !(st && reader->strict)) {
		const int c = peek_byte(reader);
		if (c == '\\') {
			eat_byte_safe(reader, c);
			uint32_t code;
			if (read_ECHAR(reader, ref, flags) &&
			    read_UCHAR(reader, ref, &code)) {
				r_err(reader, SERD_ERR_BAD_SYNTAX,
				      "invalid escape `\\%c'\n", peek_byte(reader));
				return NULL;
			}
		} else if (c == q) {
			eat_byte_safe(reader, q);
			const int q2 = eat_byte_safe(reader, peek_byte(reader));
			const int q3 = peek_byte(reader);
			if (q2 == q && q3 == q) {  // End of string
				eat_byte_safe(reader, q3);
				break;
			}
			*flags |= SERD_HAS_QUOTE;
			push_byte(reader, ref, c);
			st = read_character(reader, ref, flags, (uint8_t)q2);
		} else if (c == EOF) {
			r_err(reader, SERD_ERR_BAD_SYNTAX, "end of file in long string\n");
			return NULL;
		} else {
			st = read_character(
				reader, ref, flags, (uint8_t)eat_byte_safe(reader, c));
		}
	}

	return ref;
}

// STRING_LITERAL_QUOTE and STRING_LITERAL_SINGLE_QUOTE
// Initial quote is already eaten by caller
static SerdNode*
read_STRING_LITERAL(SerdReader* reader, SerdNodeFlags* flags, uint8_t q)
{
	SerdNode*  ref = push_node(reader, SERD_LITERAL, "", 0);
	SerdStatus st  = SERD_SUCCESS;
	while (!reader->status && !(st && reader->strict)) {
		const int c    = peek_byte(reader);
		uint32_t  code = 0;
		switch (c) {
		case EOF:
			r_err(reader, SERD_ERR_BAD_SYNTAX, "end of file in short string\n");
			return NULL;
		case '\n': case '\r':
			r_err(reader, SERD_ERR_BAD_SYNTAX, "line end in short string\n");
			return NULL;
		case '\\':
			eat_byte_safe(reader, c);
			if (read_ECHAR(reader, ref, flags) &&
			    read_UCHAR(reader, ref, &code)) {
				r_err(reader, SERD_ERR_BAD_SYNTAX,
				      "invalid escape `\\%c'\n", peek_byte(reader));
				return NULL;
			}
			break;
		default:
			if (c == q) {
				eat_byte_check(reader, q);
				return ref;
			} else {
				st = read_character(
					reader, ref, flags, (uint8_t)eat_byte_safe(reader, c));
			}
		}
	}

	if (st) {
		reader->status = st;
		return NULL;
	}

	return eat_byte_check(reader, q) ? ref : NULL;
}

static SerdNode*
read_String(SerdReader* reader, SerdNodeFlags* flags)
{
	const int q1 = peek_byte(reader);
	eat_byte_safe(reader, q1);

	const int q2 = peek_byte(reader);
	if (q2 == EOF) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected end of file\n");
		return NULL;
	} else if (q2 != q1) {  // Short string (not triple quoted)
		return read_STRING_LITERAL(reader, flags, (uint8_t)q1);
	}

	eat_byte_safe(reader, q2);
	const int q3 = peek_byte(reader);
	if (q3 == EOF) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected end of file\n");
		return NULL;
	} else if (q3 != q1) {  // Empty short string ("" or '')
		return push_node(reader, SERD_LITERAL, "", 0);
	}

	if (!fancy_syntax(reader)) {
		r_err(reader, SERD_ERR_BAD_SYNTAX,
		      "syntax does not support long literals\n");
		return NULL;
	}

	eat_byte_safe(reader, q3);
	return read_STRING_LITERAL_LONG(reader, flags, (uint8_t)q1);
}

static inline bool
is_PN_CHARS_BASE(const uint32_t c)
{
	return ((c >= 0x00C0 && c <= 0x00D6) || (c >= 0x00D8 && c <= 0x00F6) ||
	        (c >= 0x00F8 && c <= 0x02FF) || (c >= 0x0370 && c <= 0x037D) ||
	        (c >= 0x037F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D) ||
	        (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) ||
	        (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) ||
	        (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF));
}

static SerdStatus
read_PN_CHARS_BASE(SerdReader* reader, SerdNode* dest)
{
	uint32_t   code;
	const int  c  = peek_byte(reader);
	SerdStatus st = SERD_SUCCESS;
	if (is_alpha(c)) {
		push_byte(reader, dest, eat_byte_safe(reader, c));
	} else if (c == EOF || !(c & 0x80)) {
		return SERD_FAILURE;
	} else if ((st = read_utf8_code(reader, dest, &code,
	                                (uint8_t)eat_byte_safe(reader, c)))) {
		return st;
	} else if (!is_PN_CHARS_BASE(code)) {
		r_err(reader, SERD_ERR_BAD_SYNTAX,
		      "invalid character U+%04X in name\n", code);
		if (reader->strict) {
			return SERD_ERR_BAD_SYNTAX;
		}
	}
	return st;
}

static inline bool
is_PN_CHARS(const uint32_t c)
{
	return (is_PN_CHARS_BASE(c) || c == 0xB7 ||
	        (c >= 0x0300 && c <= 0x036F) || (c >= 0x203F && c <= 0x2040));
}

static SerdStatus
read_PN_CHARS(SerdReader* reader, SerdNode* dest)
{
	uint32_t   code;
	const int  c  = peek_byte(reader);
	SerdStatus st = SERD_SUCCESS;
	if (is_alpha(c) || is_digit(c) || c == '_' || c == '-') {
		push_byte(reader, dest, eat_byte_safe(reader, c));
	} else if (c == EOF || !(c & 0x80)) {
		return SERD_FAILURE;
	} else if ((st = read_utf8_code(reader, dest, &code,
	                                (uint8_t)eat_byte_safe(reader, c)))) {
		return st;
	} else if (!is_PN_CHARS(code)) {
		r_err(reader, (st = SERD_ERR_BAD_SYNTAX),
		      "invalid character U+%04X in name\n", code);
	}
	return st;
}

static bool
read_PERCENT(SerdReader* reader, SerdNode* dest)
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
read_PLX(SerdReader* reader, SerdNode* dest)
{
	int c = peek_byte(reader);
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
read_PN_LOCAL(SerdReader* reader, SerdNode* dest, bool* ate_dot)
{
	int        c                      = peek_byte(reader);
	SerdStatus st                     = SERD_SUCCESS;
	bool       trailing_unescaped_dot = false;
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

	while ((c = peek_byte(reader))) {  // Middle: (PN_CHARS | '.' | ':')*
		if (c == '.' || c == ':') {
			push_byte(reader, dest, eat_byte_safe(reader, c));
		} else if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
			return st;
		} else if (st != SERD_SUCCESS && (st = read_PN_CHARS(reader, dest))) {
			break;
		}
		trailing_unescaped_dot = (c == '.');
	}

	if (trailing_unescaped_dot) {
		// Ate trailing dot, pop it from stack/node and inform caller
		--dest->n_bytes;
		serd_stack_pop(&reader->stack, 1);
		*ate_dot = true;
	}

	return (st > SERD_FAILURE) ? st : SERD_SUCCESS;
}

// Read the remainder of a PN_PREFIX after some initial characters
static SerdStatus
read_PN_PREFIX_tail(SerdReader* reader, SerdNode* dest)
{
	int c;
	while ((c = peek_byte(reader))) {  // Middle: (PN_CHARS | '.')*
		if (c == '.') {
			push_byte(reader, dest, eat_byte_safe(reader, c));
		} else if (read_PN_CHARS(reader, dest)) {
			break;
		}
	}

	if (serd_node_get_string(dest)[dest->n_bytes - 1] == '.' &&
	    read_PN_CHARS(reader, dest)) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "prefix ends with `.'\n");
		return SERD_ERR_BAD_SYNTAX;
	}

	return SERD_SUCCESS;
}

static SerdStatus
read_PN_PREFIX(SerdReader* reader, SerdNode* dest)
{
	if (!read_PN_CHARS_BASE(reader, dest)) {
		return read_PN_PREFIX_tail(reader, dest);
	}
	return SERD_FAILURE;
}

static SerdNode*
read_LANGTAG(SerdReader* reader)
{
	int c = peek_byte(reader);
	if (!is_alpha(c)) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected `%c'\n", c);
		return NULL;
	}

	SerdNode* ref = push_node(reader, SERD_LITERAL, "", 0);
	if (!ref) {
		return NULL;
	}

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
read_IRIREF_scheme(SerdReader* reader, SerdNode* dest)
{
	int c = peek_byte(reader);
	if (!is_alpha(c)) {
		return r_err(reader, SERD_ERR_BAD_SYNTAX,
		             "bad IRI scheme start `%c'\n", c);
	}

	while ((c = peek_byte(reader)) != EOF) {
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

	return r_err(reader, SERD_ERR_BAD_SYNTAX, "unexpected end of file\n");
}

static SerdNode*
read_IRIREF(SerdReader* reader)
{
	TRY_RET(eat_byte_check(reader, '<'));
	SerdNode* ref = push_node(reader, SERD_URI, "", 0);
	if (!ref || (!fancy_syntax(reader) && !read_IRIREF_scheme(reader, ref))) {
		return NULL;
	}

	SerdStatus st   = SERD_SUCCESS;
	uint32_t   code = 0;
	while (!reader->status && !(st && reader->strict)) {
		const int c = eat_byte_safe(reader, peek_byte(reader));
		switch (c) {
		case '"': case '<': case '^': case '`': case '{': case '|': case '}':
			r_err(reader, SERD_ERR_BAD_SYNTAX,
			      "invalid IRI character `%c'\n", c);
			return NULL;
		case '>':
			return ref;
		case '\\':
			if (read_UCHAR(reader, ref, &code)) {
				r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid IRI escape\n");
				return NULL;
			}
			switch (code) {
			case 0: case ' ': case '<': case '>':
				r_err(reader, SERD_ERR_BAD_SYNTAX,
				      "invalid escaped IRI character %X %c\n", code, code);
				return NULL;
			}
			break;
		default:
			if (c <= 0x20) {
				if (isprint(c)) {
					r_err(reader, SERD_ERR_BAD_SYNTAX,
					      "invalid IRI character `%c' (escape %%%02X)\n",
					      c, (unsigned)c);
				} else {
					r_err(reader, SERD_ERR_BAD_SYNTAX,
					      "invalid IRI character (escape %%%02X)\n",
					      (unsigned)c);
				}
				if (reader->strict) {
					return NULL;
				}
				reader->status = SERD_FAILURE;
				push_byte(reader, ref, c);
			} else if (!(c & 0x80)) {
				push_byte(reader, ref, c);
			} else if ((st = read_utf8_character(reader, ref, (uint8_t)c))) {
				if (reader->strict) {
					return NULL;
				}
			}
		}
	}
	return NULL;
}

static bool
read_PrefixedName(SerdReader* reader, SerdNode* dest, bool read_prefix, bool* ate_dot)
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
read_0_9(SerdReader* reader, SerdNode* str, bool at_least_one)
{
	unsigned count = 0;
	for (int c; is_digit((c = peek_byte(reader))); ++count) {
		push_byte(reader, str, eat_byte_safe(reader, c));
	}
	if (at_least_one && count == 0) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "expected digit\n");
	}
	return count;
}

static bool
read_number(SerdReader*    reader,
            SerdNode**     dest,
            SerdNodeFlags* flags,
            bool*          ate_dot)
{
	#define XSD_DECIMAL NS_XSD "decimal"
	#define XSD_DOUBLE  NS_XSD "double"
	#define XSD_INTEGER NS_XSD "integer"

	SerdNode* ref         = push_node(reader, SERD_LITERAL, "", 0);
	int       c           = peek_byte(reader);
	bool      has_decimal = false;
	if (!ref) {
		return false;
	} else if (c == '-' || c == '+') {
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
		push_node(reader, SERD_URI, XSD_DOUBLE, sizeof(XSD_DOUBLE) - 1);
	} else if (has_decimal) {
		push_node(reader, SERD_URI, XSD_DECIMAL, sizeof(XSD_DECIMAL) - 1);
	} else {
		push_node(reader, SERD_URI, XSD_INTEGER, sizeof(XSD_INTEGER) - 1);
	}
	*flags |= SERD_HAS_DATATYPE;
	*dest = ref;
	return true;
except:
	return r_err(reader, SERD_ERR_BAD_SYNTAX, "bad number syntax\n");
}

static bool
read_iri(SerdReader* reader, SerdNode** dest, bool* ate_dot)
{
	switch (peek_byte(reader)) {
	case '<':
		*dest = read_IRIREF(reader);
		return true;
	default:
		if (!(*dest = push_node(reader, SERD_CURIE, "", 0))) {
			return false;
		}
		return read_PrefixedName(reader, *dest, true, ate_dot);
	}
}

static bool
read_literal(SerdReader*    reader,
             SerdNode**     dest,
             SerdNodeFlags* flags,
             bool*          ate_dot)
{
	SerdNode* str = read_String(reader, flags);
	if (!str) {
		return false;
	}

	SerdNode* datatype = NULL;
	switch (peek_byte(reader)) {
	case '@':
		eat_byte_safe(reader, '@');
		*flags |= SERD_HAS_LANGUAGE;
		TRY_THROW(read_LANGTAG(reader));
		break;
	case '^':
		eat_byte_safe(reader, '^');
		eat_byte_check(reader, '^');
		*flags |= SERD_HAS_DATATYPE;
		TRY_THROW(read_iri(reader, &datatype, ate_dot));
		break;
	}
	*dest = str;
	return true;
except:
	return r_err(reader, SERD_ERR_BAD_SYNTAX, "bad literal syntax\n");
}

static bool
read_verb(SerdReader* reader, SerdNode** dest)
{
	const size_t orig_stack_size = reader->stack.size;
	if (peek_byte(reader) == '<') {
		return (*dest = read_IRIREF(reader));
	}

	/* Either a qname, or "a".  Read the prefix first, and if it is in fact
	   "a", produce that instead.
	*/
	if (!(*dest = push_node(reader, SERD_CURIE, "", 0))) {
		return false;
	}

	const SerdStatus st      = read_PN_PREFIX(reader, *dest);
	bool             ate_dot = false;
	SerdNode*        node    = *dest;
	const int        next    = peek_byte(reader);
	if (!st && node->n_bytes == 1 &&
	    serd_node_get_string(node)[0] == 'a' &&
	    next != ':' && !is_PN_CHARS_BASE((uint32_t)next)) {
		serd_stack_pop_to(&reader->stack, orig_stack_size);
		return (*dest = push_node(reader, SERD_URI, NS_RDF "type", 47));
	} else if (st > SERD_FAILURE ||
	           !read_PrefixedName(reader, *dest, false, &ate_dot) ||
	           ate_dot) {
		*dest = NULL;
		return r_err(reader, SERD_ERR_BAD_SYNTAX, "bad verb\n");
	}

	return true;
}

static SerdNode*
read_BLANK_NODE_LABEL(SerdReader* reader, bool* ate_dot)
{
	eat_byte_safe(reader, '_');
	eat_byte_check(reader, ':');
	SerdNode* n = push_node(reader, SERD_BLANK,
	                        reader->bprefix ? reader->bprefix : "",
	                        reader->bprefix_len);
	if (!n) {
		return NULL;
	}

	int c = peek_byte(reader);  // First: (PN_CHARS | '_' | [0-9])
	if (is_digit(c) || c == '_') {
		push_byte(reader, n, eat_byte_safe(reader, c));
	} else if (read_PN_CHARS(reader, n)) {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid name start character\n");
		return NULL;
	}

	while ((c = peek_byte(reader))) {  // Middle: (PN_CHARS | '.')*
		if (c == '.') {
			push_byte(reader, n, eat_byte_safe(reader, c));
		} else if (read_PN_CHARS(reader, n)) {
			break;
		}
	}

	char* buf = serd_node_buffer(n);
	if (buf[n->n_bytes - 1] == '.' && read_PN_CHARS(reader, n)) {
		// Ate trailing dot, pop it from stack/node and inform caller
		--n->n_bytes;
		serd_stack_pop(&reader->stack, 1);
		*ate_dot = true;
	}

	if (fancy_syntax(reader)) {
		if (is_digit(buf[reader->bprefix_len + 1])) {
			if ((buf[reader->bprefix_len]) == 'b') {
				buf[reader->bprefix_len] = 'B';  // Prevent clash
				reader->seen_genid = true;
			} else if (reader->seen_genid && buf[reader->bprefix_len] == 'B') {
				r_err(reader, SERD_ERR_ID_CLASH,
				      "found both `b' and `B' blank IDs, prefix required\n");
				return NULL;
			}
		}
	}
	return n;
}

static SerdNode*
read_blankName(SerdReader* reader)
{
	eat_byte_safe(reader, '=');
	if (eat_byte_check(reader, '=') != '=') {
		r_err(reader, SERD_ERR_BAD_SYNTAX, "expected `='\n");
		return NULL;
	}

	SerdNode* subject = 0;
	bool      ate_dot = false;
	read_ws_star(reader);
	read_iri(reader, &subject, &ate_dot);
	return subject;
}

static bool
read_anon(SerdReader* reader, ReadContext ctx, bool subject, SerdNode** dest)
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
		TRY_RET(emit_statement(reader, ctx, *dest));
	}

	ctx.subject = *dest;
	if (!empty) {
		*ctx.flags &= ~(unsigned)SERD_LIST_CONT;
		if (!subject) {
			*ctx.flags |= SERD_ANON_CONT;
		}
		bool ate_dot_in_list = false;
		read_predicateObjectList(reader, ctx, &ate_dot_in_list);
		if (ate_dot_in_list) {
			return r_err(reader, SERD_ERR_BAD_SYNTAX, "`.' inside blank\n");
		}
		read_ws_star(reader);
		if (reader->sink->end) {
			reader->sink->end(reader->sink->handle, *dest);
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

	const size_t orig_stack_size = reader->stack.size;

	bool      ret    = false;
	bool      simple = (ctx->subject != 0);
	SerdNode* o      = 0;
	uint32_t  flags  = 0;
	const int c      = peek_byte(reader);
	if (!fancy_syntax(reader)) {
		switch (c) {
		case '"': case ':': case '<': case '_': break;
		default: return r_err(reader, SERD_ERR_BAD_SYNTAX,
		                      "expected: ':', '<', or '_'\n");
		}
	}
	switch (c) {
	case EOF: case '\0': case ')':
		return r_err(reader, SERD_ERR_BAD_SYNTAX, "expected object\n");
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
		TRY_THROW(ret = read_number(reader, &o, &flags, ate_dot));
		break;
	case '\"':
	case '\'':
		TRY_THROW(ret = read_literal(reader, &o, &flags, ate_dot));
		break;
	default:
		/* Either a boolean literal, or a qname.  Read the prefix first, and if
		   it is in fact a "true" or "false" literal, produce that instead.
		*/
		TRY_THROW(o = push_node(reader, SERD_CURIE, "", 0));
		while (!read_PN_CHARS_BASE(reader, o)) {}
		if ((o->n_bytes == 4 &&
		     !memcmp(serd_node_get_string(o), "true", 4)) ||
		    (o->n_bytes == 5 &&
		     !memcmp(serd_node_get_string(o), "false", 5))) {
			flags    = flags | SERD_HAS_DATATYPE;
			o->type  = SERD_LITERAL;
			TRY_THROW(
				push_node(reader, SERD_URI, XSD_BOOLEAN, XSD_BOOLEAN_LEN));
			ret = true;
		} else if (read_PN_PREFIX_tail(reader, o) > SERD_FAILURE) {
			ret = false;
		} else {
			if (!(ret = read_PrefixedName(reader, o, false, ate_dot))) {
				r_err(reader, SERD_ERR_BAD_SYNTAX, "expected prefixed name\n");
			}
		}
	}

	if (simple && o) {
		o->flags = flags;
	}

	if (ret && emit && simple) {
		ret = emit_statement(reader, *ctx, o);
	} else if (ret && !emit) {
		ctx->object   = o;
		return true;
	}

except:
	serd_stack_pop_to(&reader->stack, orig_stack_size);
#ifndef NDEBUG
	assert(reader->stack.size == orig_stack_size);
#endif
	return ret;
}

static bool
read_objectList(SerdReader* reader, ReadContext ctx, bool* ate_dot)
{
	TRY_RET(read_object(reader, &ctx, true, ate_dot));
	if (!fancy_syntax(reader) && peek_delim(reader, ',')) {
		return r_err(reader, SERD_ERR_BAD_SYNTAX,
		             "syntax does not support abbreviation\n");
	}

	while (!*ate_dot && eat_delim(reader, ',')) {
		TRY_RET(read_object(reader, &ctx, true, ate_dot));
	}
	return true;
}

static bool
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot)
{
	const size_t orig_stack_size = reader->stack.size;

	while (read_verb(reader, &ctx.predicate) &&
	       read_ws_star(reader) &&
	       read_objectList(reader, ctx, ate_dot)) {
		if (*ate_dot) {
			serd_stack_pop_to(&reader->stack, orig_stack_size);
			return true;
		}

		bool ate_semi = false;
		int  c;
		do {
			read_ws_star(reader);
			switch (c = peek_byte(reader)) {
			case EOF: case '\0':
				serd_stack_pop_to(&reader->stack, orig_stack_size);
				return r_err(reader, SERD_ERR_BAD_SYNTAX,
				             "unexpected end of file\n");
			case '.': case ']': case '}':
				serd_stack_pop_to(&reader->stack, orig_stack_size);
				return true;
			case ';':
				eat_byte_safe(reader, c);
				ate_semi = true;
			}
		} while (c == ';');

		if (!ate_semi) {
			serd_stack_pop_to(&reader->stack, orig_stack_size);
			return r_err(reader, SERD_ERR_BAD_SYNTAX, "missing ';' or '.'\n");
		}
	}

	serd_stack_pop_to(&reader->stack, orig_stack_size);
	ctx.predicate = 0;
	return false;
}

static bool
end_collection(SerdReader* reader, ReadContext ctx, bool ret)
{
	*ctx.flags &= ~(unsigned)SERD_LIST_CONT;
	return ret && (eat_byte_safe(reader, ')') == ')');
}

static bool
read_collection(SerdReader* reader, ReadContext ctx, SerdNode** dest)
{
	eat_byte_safe(reader, '(');
	bool end = peek_delim(reader, ')');
	*dest = end ? reader->rdf_nil : blank_id(reader);
	if (ctx.subject) {
		// subject predicate _:head
		*ctx.flags |= (end ? 0 : SERD_LIST_O_BEGIN);
		TRY_RET(emit_statement(reader, ctx, *dest));
		*ctx.flags |= SERD_LIST_CONT;
	} else {
		*ctx.flags |= (end ? 0 : SERD_LIST_S_BEGIN);
	}

	if (end) {
		return end_collection(reader, ctx, true);
	}

	/* The order of node allocation here is necessarily not in stack order,
	   so we create two nodes and recycle them throughout. */
	SerdNode* n1   = push_node_padded(reader, genid_size(reader), SERD_BLANK, "", 0);
	SerdNode* n2   = 0;
	SerdNode* node = n1;
	SerdNode* rest = 0;

	if (!n1) {
		return false;
	}

	ctx.subject = *dest;
	while (!(end = peek_delim(reader, ')'))) {
		// _:node rdf:first object
		ctx.predicate = reader->rdf_first;
		bool ate_dot = false;
		if (!read_object(reader, &ctx, true, &ate_dot) || ate_dot) {
			return end_collection(reader, ctx, false);
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
		TRY_RET(emit_statement(reader, ctx, (end ? reader->rdf_nil : rest)));

		ctx.subject = rest;         // _:node = _:rest
		rest        = node;         // _:rest = (old)_:node
		node        = ctx.subject;  // invariant
	}

	return end_collection(reader, ctx, true);
}

static SerdNode*
read_subject(SerdReader* reader, ReadContext ctx, SerdNode** dest, int* s_type)
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
	return ate_dot ? NULL : *dest;
}

static SerdNode*
read_labelOrSubject(SerdReader* reader)
{
	SerdNode* subject = 0;
	bool      ate_dot = false;
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

	read_ws_star(reader);
	SerdNode* uri = read_IRIREF(reader);
	if (!uri) {
		return false;
	} else if (reader->sink->base) {
		reader->sink->base(reader->sink->handle, uri);
	}

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
	bool      ret  = true;
	SerdNode* name = push_node(reader, SERD_LITERAL, "", 0);
	if (!name) {
		return false;
	} else if (read_PN_PREFIX(reader, name) > SERD_FAILURE) {
		return false;
	}

	if (eat_byte_check(reader, ':') != ':') {
		return false;
	}

	read_ws_star(reader);
	const SerdNode* uri = read_IRIREF(reader);
	if (!uri) {
		return false;
	}

	if (reader->sink->prefix) {
		ret = !reader->sink->prefix(reader->sink->handle, name, uri);
	}
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
	TRY_RET(eat_byte_check(reader, '{'));
	read_ws_star(reader);
	while (peek_byte(reader) != '}') {
		const size_t orig_stack_size = reader->stack.size;
		bool         ate_dot         = false;
		int          s_type          = 0;

		ctx->subject = 0;
		SerdNode* subj = read_subject(reader, *ctx, &ctx->subject, &s_type);
		if (!subj && ctx->subject) {
			return r_err(reader, SERD_ERR_BAD_SYNTAX, "bad subject\n");
		} else if (!subj) {
			return false;
		} else if (!read_triples(reader, *ctx, &ate_dot) && s_type != '[') {
			return r_err(reader, SERD_ERR_BAD_SYNTAX,
			             "missing predicate object list\n");
		}
		serd_stack_pop_to(&reader->stack, orig_stack_size);
		read_ws_star(reader);
		if (peek_byte(reader) == '.') {
			eat_byte_safe(reader, '.');
		}
		read_ws_star(reader);
	}
	return eat_byte_check(reader, '}');
}

static int
tokcmp(SerdNode* node, const char* tok, size_t n)
{
	return ((!node || node->n_bytes != n)
	        ? -1
	        : serd_strncasecmp(serd_node_get_string(node), tok, n));
}

bool
read_n3_statement(SerdReader* reader)
{
	SerdStatementFlags flags   = 0;
	ReadContext        ctx     = { 0, 0, 0, 0, &flags };
	SerdNode*          subj    = 0;
	bool               ate_dot = false;
	int                s_type  = 0;
	bool               ret     = true;
	read_ws_star(reader);
	switch (peek_byte(reader)) {
	case EOF: case '\0':
		return reader->status <= SERD_FAILURE;
	case '@':
		if (!fancy_syntax(reader)) {
			return r_err(reader, SERD_ERR_BAD_SYNTAX,
			             "syntax does not support directives\n");
		}
		TRY_RET(read_directive(reader));
		read_ws_star(reader);
		break;
	case '{':
		if (reader->syntax == SERD_TRIG) {
			TRY_RET(read_wrappedGraph(reader, &ctx));
			read_ws_star(reader);
		} else {
			return r_err(reader, SERD_ERR_BAD_SYNTAX,
			             "syntax does not support graphs\n");
		}
		break;
	default:
		subj = read_subject(reader, ctx, &ctx.subject, &s_type);
		if (!tokcmp(ctx.subject, "base", 4)) {
			ret = read_base(reader, true, false);
		} else if (!tokcmp(ctx.subject, "prefix", 6)) {
			ret = read_prefixID(reader, true, false);
		} else if (!tokcmp(ctx.subject, "graph", 5)) {
			read_ws_star(reader);
			TRY_RET((ctx.graph = read_labelOrSubject(reader)));
			read_ws_star(reader);
			TRY_RET(read_wrappedGraph(reader, &ctx));
			ctx.graph = 0;
			read_ws_star(reader);
		} else if (read_ws_star(reader) && peek_byte(reader) == '{') {
			if (s_type == '(' || (s_type == '[' && !*ctx.flags)) {
				return r_err(reader, SERD_ERR_BAD_SYNTAX,
				             "invalid graph name\n");
			}
			ctx.graph   = subj;
			ctx.subject = subj = 0;
			TRY_RET(read_wrappedGraph(reader, &ctx));
			read_ws_star(reader);
		} else if (!subj) {
			ret = r_err(reader, SERD_ERR_BAD_SYNTAX, "bad subject\n");
		} else if (!read_triples(reader, ctx, &ate_dot)) {
			if (!(ret = (s_type == '[')) && ate_dot) {
				ret = r_err(reader, SERD_ERR_BAD_SYNTAX,
				            "unexpected end of statement\n");
			}
		} else if (!ate_dot) {
			read_ws_star(reader);
			ret = (eat_byte_check(reader, '.') == '.');
		}
		break;
	}
	return ret;
}

static void
skip_until(SerdReader* reader, uint8_t byte)
{
	for (int c = 0; (c = peek_byte(reader)) && c != byte;) {
		eat_byte_safe(reader, c);
	}
}

SerdStatus
read_turtleTrigDoc(SerdReader* reader)
{
	while (!reader->source.eof) {
		const size_t orig_stack_size = reader->stack.size;
		if (!read_n3_statement(reader)) {
			if (reader->strict) {
				serd_stack_pop_to(&reader->stack, orig_stack_size);
				return SERD_ERR_UNKNOWN;
			}
			skip_until(reader, '\n');
			reader->status = SERD_SUCCESS;
		}
		serd_stack_pop_to(&reader->stack, orig_stack_size);
	}
	return reader->status;
}

SerdStatus
read_nquadsDoc(SerdReader* reader)
{
	while (!reader->source.eof) {
		const size_t orig_stack_size = reader->stack.size;

		SerdStatementFlags flags   = 0;
		ReadContext        ctx     = { 0, 0, 0, 0, &flags };
		bool               ate_dot = false;
		int                s_type  = 0;
		read_ws_star(reader);
		if (peek_byte(reader) == EOF) {
			break;
		} else if (peek_byte(reader) == '@') {
			r_err(reader, SERD_ERR_BAD_SYNTAX,
			      "syntax does not support directives\n");
			return SERD_ERR_BAD_SYNTAX;
		}

		// subject predicate object
		if (!(ctx.subject = read_subject(reader, ctx, &ctx.subject, &s_type)) ||
		    !read_ws_star(reader) ||
		    !(ctx.predicate = read_IRIREF(reader)) ||
		    !read_ws_star(reader) ||
		    !read_object(reader, &ctx, false, &ate_dot)) {
			return SERD_ERR_UNKNOWN;
		}

		if (!ate_dot) {  // graphLabel?
			read_ws_star(reader);
			switch (peek_byte(reader)) {
			case '.':
				break;
			case '_':
				ctx.graph = read_BLANK_NODE_LABEL(reader, &ate_dot);
				break;
			default:
				if (!(ctx.graph = read_IRIREF(reader))) {
					return SERD_ERR_UNKNOWN;
				}
			}

			// Terminating '.'
			read_ws_star(reader);
			eat_byte_check(reader, '.');
		}

		if (!emit_statement(reader, ctx, ctx.object)) {
			break;
		}

		serd_stack_pop_to(&reader->stack, orig_stack_size);
	}
	return reader->status;
}
