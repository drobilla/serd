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

#include "reader.h"

static SerdStatus read_object(SerdReader* reader,
                              ReadContext ctx,
                              Ref*        dest,
                              Ref*        datatype,
                              Ref*        lang);

static SerdStatus read_array(SerdReader* reader, ReadContext ctx, Ref* dest);

static SerdStatus
read_term(SerdReader* const reader,
          const char* const term,
          const unsigned    len,
          Ref*              dest)
{
	*dest = push_node(reader, SERD_LITERAL, "", 0);

	SerdStatus st;
	for (unsigned i = 0; i < len; ++i) {
		const char c = eat_byte(reader);
		if (!c || c != term[i]) {
			return SERD_ERR_BAD_SYNTAX;
		} else if ((st = push_byte(reader, *dest, c))) {
			return st;
		}
	}

	return SERD_SUCCESS;
}

static SerdStatus
read_hex(SerdReader* const reader, const Ref dest)
{
	SerdStatus st;
	if (!(st = push_byte(reader, dest, eat_byte(reader)))) {
		for (unsigned i = 0; i < 4; ++i) {
			if (!is_xdigit(peek_byte(reader))) {
				return SERD_ERR_BAD_SYNTAX;
			}
			push_byte(reader, dest, eat_byte(reader));
		}
	}
	return st;
}

static SerdStatus
read_escape(SerdReader* const reader, const Ref dest)
{
	switch (peek_byte(reader)) {
	case '\"':
	case '/':
	case '\\':
	case 'b':
	case 'f':
	case 'n':
	case 'r':
	case 't': return push_byte(reader, dest, eat_byte(reader));
	case 'u': return read_hex(reader, dest);
	default: return SERD_ERR_BAD_SYNTAX;
	}
}

static SerdStatus
pop_err(SerdReader* const reader, const SerdStatus st, const Ref ref)
{
	pop_node(reader, ref);
	return st;
}

static SerdStatus
read_string(SerdReader* const reader, Ref* dest)
{
	*dest = push_node(reader, SERD_LITERAL, "", 0);
	if (!eat_byte_check(reader, '\"')) {
		return pop_err(reader, SERD_ERR_BAD_SYNTAX, *dest);
	}

	SerdStatus st;
	for (char c; (c = eat_byte(reader));) {
		switch (c) {
		case '\"': return SERD_SUCCESS;
		case '\\':
			if ((st = push_byte(reader, *dest, c)) ||
			    (st = read_escape(reader, *dest))) {
				return pop_err(reader, SERD_ERR_BAD_SYNTAX, *dest);
			}
			break;
		default:
			if (c >= 0 && c <= 0x1F) {
				return pop_err(reader, SERD_ERR_BAD_SYNTAX, *dest);
			} else if ((st = push_byte(reader, *dest, c))) {
				return pop_err(reader, st, *dest);
			}
		}
	}
	return pop_err(reader, SERD_ERR_BAD_SYNTAX, *dest);
}

static SerdStatus
expand_uri(SerdReader* const     reader,
           const SerdNode* const node,
           SerdChunk* const      uri_prefix,
           SerdChunk* const      uri_suffix)
{
	if (serd_stack_is_empty(&reader->env_stack)) {
		fprintf(stderr, "=> FAILURE\n");
		return SERD_FAILURE;
	}

	SerdEnv* env = *(SerdEnv**)(reader->env_stack.buf + reader->env_stack.size -
	                            sizeof(SerdEnv*));

	return serd_env_expand(env, node, uri_prefix, uri_suffix);
}

static SerdStatus
expand_term(SerdReader* const reader,
            SerdNode* const   node,
            Ref*              kref,
            const SerdNode**  datatype)
{
	if (serd_stack_is_empty(&reader->env_stack)) {
		return SERD_FAILURE;
	}

	SerdEnv* env = *(SerdEnv**)(reader->env_stack.buf + reader->env_stack.size -
	                            sizeof(SerdEnv*));

	SerdChunk  prefix = { 0, 0 };
	SerdChunk  suffix = { 0, 0 };
	SerdStatus st = serd_env_expand_term(env, node, &prefix, &suffix, datatype);

	*kref = push_node(reader,
	                  *datatype ? SERD_LITERAL : SERD_URI,
	                  (const char*)prefix.buf,
	                  prefix.len);
	push_bytes(reader, *kref, suffix.buf, suffix.len);

	return st;
}

static SerdStatus
read_uri(SerdReader* const reader, Ref* dest)
{
	const SerdStatus st = read_string(reader, dest);
	if (!st) {
		SerdChunk prefix = { NULL, 0 };
		SerdChunk suffix = { NULL, 0 };
		SerdNode* node   = deref(reader, *dest);

		fprintf(stderr, "EXPAND URI %s\n", node->buf);
		node->type = SERD_CURIE;
		if (!expand_uri(reader, node, &prefix, &suffix)) {
			fprintf(stderr, " => EXPAND 1\n");
			return SERD_SUCCESS;
		}

		node->type = SERD_URI;
		if (!expand_uri(reader, node, &prefix, &suffix)) {
			fprintf(stderr, " => EXPAND 2\n");
			return SERD_SUCCESS;
		}

		fprintf(stderr, "=> NO EXPAND\n");
	}
	return st;
}

static SerdStatus
read_digit(SerdReader* const reader, const Ref dest)
{
	return (is_digit(peek_byte(reader))
	                ? push_byte(reader, dest, eat_byte(reader))
	                : SERD_ERR_BAD_SYNTAX);
}

static SerdStatus
read_digits(SerdReader* const reader, const Ref dest)
{
	SerdStatus st = read_digit(reader, dest);
	while (!st) {
		if ((st = read_digit(reader, dest)) == SERD_ERR_BAD_SYNTAX) {
			return SERD_SUCCESS;
		}
	}
	return st;
}

static SerdStatus
read_sign(SerdReader* const reader, const Ref dest)
{
	switch (peek_byte(reader)) {
	case '\0': return SERD_ERR_BAD_SYNTAX;
	case '+':
	case '-': return push_byte(reader, dest, eat_byte(reader));
	}
	return SERD_SUCCESS;
}

// Slightly more lax than the JSON standard, tolerates leading 0 and +
static SerdStatus
read_number(SerdReader* const reader, Ref* dest, Ref* datatype)
{
#define XSD_DOUBLE NS_XSD "double"
#define XSD_INTEGER NS_XSD "integer"

	*dest = push_node(reader, SERD_LITERAL, "", 0);

	SerdStatus st;
	bool       has_decimal = false;
	if ((st = read_sign(reader, *dest))) {
		return st;
	}

	if (peek_byte(reader) == '0') {
		if ((st = push_byte(reader, *dest, eat_byte(reader)))) {
			return st;
		} else if (peek_byte(reader) != '.' && peek_byte(reader) != 'e') {
			return SERD_SUCCESS;
		}
	} else if ((st = read_digits(reader, *dest))) {
		return st;
	}

	if (peek_byte(reader) == '.') {
		has_decimal = true;
		if ((st = push_byte(reader, *dest, eat_byte(reader))) ||
		    (st = read_digits(reader, *dest))) {
			return st;
		}
	} else {
		*datatype = push_node(
		        reader, SERD_URI, XSD_INTEGER, sizeof(XSD_INTEGER) - 1);
	}

	if (peek_byte(reader) == 'E' || peek_byte(reader) == 'e') {
		if ((st = push_byte(reader, *dest, eat_byte(reader))) ||
		    (st = read_sign(reader, *dest)) ||
		    (st = read_digits(reader, *dest))) {
			return st;
		}
	}

	*datatype = has_decimal ? push_node(reader,
	                                    SERD_URI,
	                                    XSD_DOUBLE,
	                                    sizeof(XSD_DOUBLE) - 1)
	                        : push_node(reader,
	                                    SERD_URI,
	                                    XSD_INTEGER,
	                                    sizeof(XSD_INTEGER) - 1);

	return SERD_SUCCESS;
}

static void
skip_ws(SerdReader* const reader)
{
	for (char c; (c = peek_byte(reader));) {
		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case ' ': eat_byte(reader); break;
		default: return;
		}
	}
}

static SerdStatus
read_sep(SerdReader* const reader, const char sep)
{
	skip_ws(reader);
	if (eat_byte_check(reader, sep) == sep) {
		skip_ws(reader);
		return SERD_SUCCESS;
	}
	return SERD_ERR_BAD_SYNTAX;
}

static SerdStatus
maybe_read_sep(SerdReader* const reader, const char sep)
{
	skip_ws(reader);
	if (peek_byte(reader) == sep) {
		eat_byte(reader);
		skip_ws(reader);
		return SERD_SUCCESS;
	}
	return SERD_FAILURE;
}

static SerdStatus
read_value(SerdReader* const reader,
           ReadContext       ctx,
           Ref*              dest,
           Ref*              datatype,
           Ref*              lang)
{
#define XSD_DECIMAL NS_XSD "decimal"
#define XSD_BOOLEAN NS_XSD "boolean"
	skip_ws(reader);
	switch (peek_byte(reader)) {
	case '\0': return SERD_ERR_BAD_SYNTAX;
	case '\"': return read_string(reader, dest);
	case '[': return read_array(reader, ctx, dest);
	case 'f':
		*datatype = push_node(
		        reader, SERD_URI, XSD_BOOLEAN, sizeof(XSD_BOOLEAN) - 1);
		return read_term(reader, "false", 5, dest);
	case 'n': return read_term(reader, "null", 4, dest);
	case 't':
		*datatype = push_node(
		        reader, SERD_URI, XSD_BOOLEAN, sizeof(XSD_BOOLEAN) - 1);
		return read_term(reader, "true", 4, dest);
	case '{': return read_object(reader, ctx, dest, datatype, lang);

	case '-':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9': return read_number(reader, dest, datatype);

	default: return SERD_ERR_BAD_SYNTAX;
	}
}

static SerdStatus
read_array(SerdReader* const reader, ReadContext ctx, Ref* dest)
{
	SerdStatus st;
	if ((st = read_sep(reader, '['))) {
		return st;
	} else if (peek_byte(reader) == ']') {
		read_sep(reader, ']');
		return SERD_SUCCESS;
	}

	Ref vref = 0;
	while (!st) {
		if ((st = read_value(reader, ctx, &vref, &ctx.datatype, &ctx.lang))) {
			return st;
		}
		emit_statement(reader, ctx, vref, ctx.datatype, ctx.lang);
		skip_ws(reader);
		if (peek_byte(reader) == ',') {
			read_sep(reader, ',');
		} else {
			break;
		}
	}

	return st || read_sep(reader, ']');
}

static SerdStatus
read_list(SerdReader* const reader, ReadContext ctx, Ref* dest)
{
	SerdStatus st;
	if ((st = read_sep(reader, '['))) {
		return st;
	} else if (peek_byte(reader) == ']') {
		*dest = reader->rdf_nil;
		read_sep(reader, ']');
		return SERD_SUCCESS;
	}

	// subject predicate _:head
	*dest = blank_id(reader);
	emit_statement(reader, ctx, *dest, 0, 0);

	/* The order of node allocation here is necessarily not in stack order,
	   so we create two nodes and recycle them throughout. */
	Ref n1   = push_node_padded(reader, genid_size(reader), SERD_BLANK, "", 0);
	Ref n2   = 0;
	Ref node = n1;
	Ref rest = 0;

	ctx.subject = *dest;
	bool end    = peek_byte(reader) == ']';
	while (!end) {
		// _:node rdf:first object
		ctx.predicate = reader->rdf_first;
		Ref vref      = 0;
		if ((st = read_value(reader, ctx, &vref, &ctx.datatype, &ctx.lang))) {
			return st;
		}
		emit_statement(reader, ctx, vref, ctx.datatype, ctx.lang);

		skip_ws(reader);
		if (!(end = peek_byte(reader) != ',')) {
			read_sep(reader, ',');
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
		emit_statement(reader, ctx, (end ? reader->rdf_nil : rest), 0, 0);

		ctx.subject = rest;         // _:node = _:rest
		rest        = node;         // _:rest = (old)_:node
		node        = ctx.subject;  // invariant
	}

	return st || read_sep(reader, ']');
}

static SerdStatus
set_term(SerdReader* const     reader,
         SerdEnv* const        env,
         const SerdNode* const key,
         const SerdNode* const value,
         const SerdNode* const type)
{
	fprintf(stderr,
	        "SET TERM %s %s %s\n",
	        key->buf,
	        value->buf,
	        type ? (const char*)type->buf : "(null)");
	SerdStatus st = serd_env_set_term(env, key, value, type);
	if (!st && reader->prefix_sink) {
		st = reader->prefix_sink(reader->handle, key, value);
	}

	return st;
}

static SerdStatus
read_context_value(SerdReader* const     reader,
                   SerdEnv* const        env,
                   const SerdNode* const key)
{
	SerdStatus st = SERD_SUCCESS;
	if (peek_byte(reader) == '{') {
		st = read_sep(reader, '{');

		Ref idref   = 0;
		Ref typeref = 0;
		while (!st) {
			Ref kref = 0;
			if ((st = read_string(reader, &kref)) ||
			    (st = read_sep(reader, ':'))) {
				return pop_err(reader, st, kref);
			}

			Ref vref = 0;
			if ((st = read_uri(reader, &vref))) {
				return pop_err(reader, st, vref);
			}

			SerdNode* ckey   = deref(reader, kref);
			SerdNode* cvalue = deref(reader, vref);

			fprintf(stderr, "CTX KEY: %s\n", ckey->buf);
			fprintf(stderr, "CTX VAL: %s\n", cvalue->buf);

			if (!strcmp((const char*)ckey->buf, "@id")) {
				idref = vref;
			} else if (!strcmp((const char*)ckey->buf, "@type")) {
				if (!strcmp((const char*)cvalue->buf, "@id")) {
					typeref = push_node(reader,
					                    SERD_URI,
					                    NS_RDFS "Resource",
					                    strlen(NS_RDFS "Resource"));
				} else {
					typeref = vref;
				}
			}

			if (maybe_read_sep(reader, ',')) {
				break;
			}
		}

		if ((st = read_sep(reader, '}'))) {
			return st;
		}

		set_term(
		        reader, env, key, deref(reader, idref), deref(reader, typeref));

	} else {
		fprintf(stderr, "SIMPLE\n");
		Ref vref = 0;
		if (!(st = read_uri(reader, &vref))) {
			SerdNode* value = deref(reader, vref);

			value->type = SERD_URI;
			set_term(reader, env, key, value, NULL);
		}
	}

	return st;
}

static SerdStatus
read_context(SerdReader* const reader, ReadContext* ctx)
{
	SerdStatus st;
	skip_ws(reader);
	if (peek_byte(reader) != '{') {
		Ref cref = 0;
		read_uri(reader, &cref);
		pop_node(reader, cref);
		return SERD_SUCCESS;  // TODO: Context references?
	}

	if ((st = read_sep(reader, '{'))) {
		return st;
	}

	SerdEnv*  env = serd_env_new(NULL);
	SerdEnv** frame =
	        (SerdEnv**)serd_stack_push(&reader->env_stack, sizeof(SerdEnv*));
	*frame = env;

	while (true) {
		Ref kref = 0;
		if ((st = read_string(reader, &kref)) || (st = read_sep(reader, ':'))) {
			return pop_err(reader, st, kref);
		}

		SerdNode* key = deref(reader, kref);
		if ((st = read_context_value(reader, env, key))) {
			return pop_err(reader, st, kref);
		}

		if (maybe_read_sep(reader, ',')) {
			break;
		}
	}

	if ((st = read_sep(reader, '}'))) {
		return st;
	}

	return SERD_SUCCESS;
}

static SerdStatus
start_object(SerdReader* const reader, ReadContext ctx, const Ref ref)
{
	if (deref(reader, ref)->type == SERD_BLANK) {
		set_blank_id(reader, ref, genid_size(reader));
	}
	if (ctx.subject && ctx.predicate) {
		emit_statement(reader, ctx, ref, 0, 0);
	}
	return SERD_SUCCESS;
}

static SerdStatus
read_object(SerdReader* const reader,
            ReadContext       ctx,
            Ref*              dest,
            Ref*              datatype,
            Ref*              lang)
{
	SerdStatus st;
	if ((st = read_sep(reader, '{'))) {
		return st;
	}

	Ref  id   = push_node_padded(reader, genid_size(reader), SERD_BLANK, "", 0);
	Ref  kref = 0;  // Key
	Ref  vref = 0;  // Value
	bool is_literal = false;

	while (true) {
		if ((st = read_string(reader, &kref)) || (st = read_sep(reader, ':'))) {
			return pop_err(reader, st, kref);
		}

		SerdNode* key = deref(reader, kref);
		if (!strcmp((const char*)key->buf, "@list")) {
			read_list(reader, ctx, dest);
		} else if (!strcmp((const char*)key->buf, "@value")) {
			is_literal = true;
			pop_node(reader, kref);
			if ((st = read_string(reader, dest))) {
				return pop_err(reader, st, kref);
			}
		} else if (is_literal) {
			if (!strcmp((const char*)key->buf, "@type")) {
				if ((st = read_uri(reader, datatype))) {
					return pop_err(reader, st, kref);
				}
			} else if (!strcmp((const char*)key->buf, "@language")) {
				if ((st = read_string(reader, lang))) {
					return pop_err(reader, st, kref);
				}
			} else {
				r_err(reader,
				      SERD_ERR_BAD_SYNTAX,
				      "invalid literal property\n");
				return pop_err(reader, SERD_ERR_BAD_SYNTAX, kref);
			}
		} else if (!strcmp((const char*)key->buf, "@id")) {
			pop_node(reader, kref);
			pop_node(reader, id);
			if ((st = read_uri(reader, &id))) {
				return SERD_ERR_BAD_SYNTAX;
			}
		} else if (!strcmp((const char*)key->buf, "@context")) {
			pop_node(reader, kref);
			read_context(reader, &ctx);
		} else {
			if (ctx.subject != id) {
				start_object(reader, ctx, id);
				ctx.subject = id;
			}

			SerdType vtype = SERD_LITERAL;
			if (!strcmp((const char*)key->buf, "@type")) {
				pop_node(reader, kref);
				kref  = push_node(reader, SERD_URI, NS_RDF "type", 47);
				vtype = SERD_URI;
			} else {
				const SerdNode* type          = NULL;
				Ref             expanded_kref = 0;
				expand_term(reader, key, &expanded_kref, &type);
				if (expanded_kref) {
					key       = deref(reader, expanded_kref);
					key->type = SERD_URI;
					fprintf(stderr, "KEY: %s\n", key->buf);
					kref = expanded_kref;
				}
				/* if (!type) { */
				/* 	vtype = SERD_URI; */
				/* } */

				if (type) {
					fprintf(stderr, "DATATYPE: %s\n", type->buf);
					if (!strcmp((const char*)type->buf, NS_RDFS "Resource")) {
						vtype = SERD_URI;
					} else {
						ctx.datatype = push_node(reader,
						                         type->type,
						                         (const char*)type->buf,
						                         type->n_bytes);
					}
				} else {
					fprintf(stderr, "NO DATATYPE\n");
					/* vtype = SERD_URI; */
				}

				key->type = SERD_URI;
				/* SerdChunk prefix = { NULL, 0 }; */
				/* SerdChunk suffix = { NULL, 0 }; */
				/* if (!expand_uri(reader, key, &prefix, &suffix)) { */
				/* 	key->type = SERD_CURIE; */
				/* } else { */
				/* 	key->type = SERD_URI; */
				/* } */
			}

			ctx.predicate = kref;
			if (vtype == SERD_URI) {
				st = read_uri(reader, &vref);
			} else {
				st = read_value(reader, ctx, &vref, &ctx.datatype, &ctx.lang);
			}

			if (st) {
				pop_node(reader, kref);
				return pop_node(reader, ctx.subject);
			}

			emit_statement(reader, ctx, vref, ctx.datatype, ctx.lang);
			pop_node(reader, vref);
			pop_node(reader, kref);
		}

		if (maybe_read_sep(reader, ',')) {
			break;
		}
	}
	// pop_node(reader, ctx->subject); // FIXME

	if ((st = read_sep(reader, '}'))) {
		return st;
	}

	return SERD_SUCCESS;
}

bool
read_js_statement(SerdReader* reader)
{
	return false;
}

bool
read_js_doc(SerdReader* reader)
{
	SerdStatementFlags flags = 0;
	ReadContext        ctx   = { 0, 0, 0, 0, 0, 0, &flags };

	reader->env_stack = serd_stack_new(128);
	Ref obj           = 0;
	Ref datatype      = 0;
	Ref lang          = 0;
	if (read_object(reader, ctx, &obj, &datatype, &lang)) {
		return false;
	}
	serd_stack_free(&reader->env_stack);

	return true;
}
