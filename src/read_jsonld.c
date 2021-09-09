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
#include "namespaces.h"
#include "reader.h"
#include "string_utils.h"

static SerdStatus
read_node_object(SerdReader* reader, ReadContext ctx);

static SerdStatus
read_array(SerdReader* reader, ReadContext ctx, SerdNode** dest);

static SerdNode*
push_js_object(SerdReader*     reader,
               ReadContext     ctx,
               const SerdNode* value,
               const SerdNode* datatype,
               const SerdNode* lang)
{
  (void)ctx; // FIXME: ?

  SerdNode* node = push_node(
    reader, value->type, serd_node_string(value), serd_node_length(value));

  if (!datatype && !lang) {
    return node;
  } else if (datatype && lang) {
    return NULL;
  }

  if (datatype) {
    node->flags |= SERD_HAS_DATATYPE;
    push_node(reader,
              datatype->type,
              serd_node_string(datatype),
              serd_node_length(datatype));
  } else if (lang) {
    node->flags |= SERD_HAS_LANGUAGE;
    push_node(
      reader, lang->type, serd_node_string(lang), serd_node_length(lang));
  }

  return node;
}

static SerdStatus
maybe_emit_statement(SerdReader*           reader,
                     ReadContext           ctx,
                     SerdNode* const       value,
                     const SerdNode* const datatype,
                     const SerdNode* const lang)
{
  SerdStatus st = SERD_SUCCESS;

  // fprintf(stderr, "MAYBE EMIT STATEMENT VALUE %s\n",
  // serd_node_string(value));
  if (!value) {
    // fprintf(stderr, "NO EMIT\n");
    return SERD_SUCCESS;
  }

  if (value->flags & (SERD_HAS_DATATYPE | SERD_HAS_LANGUAGE)) {
    st = emit_statement(reader, ctx, value);
  } else {
    SerdNode* o = push_js_object(reader, ctx, value, datatype, lang);
    st          = emit_statement(reader, ctx, o);
  }

  // fprintf(stderr, "EMIT STATUS: %s\n", serd_strerror(st));
  return st;
}

static SerdStatus
read_term(SerdReader* const reader,
          const char* const term,
          const unsigned    len,
          SerdNode**        dest)
{
  *dest = push_node(reader, SERD_LITERAL, "", 0);

  SerdStatus st;
  for (unsigned i = 0; i < len; ++i) {
    const int c = eat_byte(reader);
    if (!c || c != term[i]) {
      return SERD_ERR_BAD_SYNTAX;
    }

    if ((st = push_byte(reader, *dest, c))) {
      return st;
    }
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_hex(SerdReader* const reader, SerdNode* dest)
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
read_escape(SerdReader* const reader, SerdNode* dest)
{
  switch (peek_byte(reader)) {
  case '\"':
  case '/':
  case '\\':
  case 'b':
  case 'f':
  case 'n':
  case 'r':
  case 't':
    return push_byte(reader, dest, eat_byte(reader));
  case 'u':
    return read_hex(reader, dest);
  default:
    return SERD_ERR_BAD_SYNTAX;
  }
}

static SerdStatus
pop_err(SerdReader* const reader, const SerdStatus st, SerdNode* ref)
{
  (void)reader; // FIXME
  (void)ref;    // FIXME
  /* pop_node(reader, ref); */
  return st;
}

static SerdStatus
read_string(SerdReader* const reader, SerdNode** dest)
{
  SerdStatus st = SERD_SUCCESS;

  *dest = push_node(reader, SERD_LITERAL, "", 0);
  if ((st = eat_byte_check(reader, '\"'))) {
    return st;
  }

  for (int c; (c = eat_byte(reader));) {
    /* //fprintf(stderr, "s: %c\n", c); */
    switch (c) {
    case '\"':
      // fprintf(stderr, "GOOD!\n");
      return SERD_SUCCESS;
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

  // fprintf(stderr, "ERR\n");
  return pop_err(reader, SERD_ERR_BAD_SYNTAX, *dest);
}

/* static SerdStatus */
/* expand_uri(SerdReader* const     reader, */
/*            const SerdNode* const node, */
/*            SerdStringView* const uri_prefix, */
/*            SerdStringView* const uri_suffix) */
/* { */
/*   if (serd_stack_is_empty(&reader->env_stack)) { */
/*     return SERD_FAILURE; */
/*   } */

/*   SerdEnv* env = *(SerdEnv**)(reader->env_stack.buf + reader->env_stack.size
 * - */
/*                               sizeof(SerdEnv*)); */

/*   return serd_env_expand_in_place(env, node, uri_prefix, uri_suffix); */
/* } */

static SerdStatus
expand_term(SerdReader* const reader,
            SerdNode* const   node,
            SerdNode**        kref,
            const SerdNode**  datatype)
{
  SerdStatus st = SERD_SUCCESS;

  if (serd_stack_is_empty(&reader->env_stack)) {
    return SERD_FAILURE;
  }

  SerdEnv* env = *(SerdEnv**)(reader->env_stack.buf + reader->env_stack.size -
                              sizeof(SerdEnv*));

  SerdStringView prefix = {0, 0};
  SerdStringView suffix = {0, 0};

  if ((st = serd_env_expand_term_in_place(
         env, node, &prefix, &suffix, datatype))) {
    return st;
  }

  assert(prefix.buf);
  *kref = push_node(reader,
                    (datatype && *datatype) ? SERD_LITERAL : SERD_URI, // FIXME
                    prefix.buf,
                    prefix.len);

  push_bytes(reader, *kref, (const uint8_t*)suffix.buf, suffix.len);

  return st;
}

static SerdStatus
read_uri(SerdReader* const reader, SerdNode** dest)
{
  SerdStatus st = SERD_SUCCESS;

  if ((st = read_string(reader, dest))) {
    return st;
  }

  /* SerdStringView prefix = {NULL, 0}; */
  /* SerdStringView suffix = {NULL, 0}; */
  SerdNode* node = *dest;

  /* fprintf(stderr, "EXPAND %s\n", serd_node_string(node)); */

  node->type = SERD_URI;
  if (!(st = expand_term(reader, node, dest, NULL))) {
    node->type = SERD_URI; // FIXME: ?
    // fprintf(stderr, "=> %s\n", serd_node_string(*dest));
    return SERD_SUCCESS;
  }

  /* node->type = SERD_LITERAL; // FIXME: ? */
  /* if (!expand_term(reader, node, &prefix, &suffix)) { */
  /*   return SERD_SUCCESS; */
  /* } */

  /* node->type = SERD_URI; */
  /* if (!expand_term(reader, node, &prefix, &suffix)) { */
  /*   return SERD_SUCCESS; */
  /* } */

  // fprintf(stderr, "ERR %s\n", serd_strerror(st));
  //  return st; // FIXME
  return SERD_SUCCESS;
}

static SerdStatus
read_digit(SerdReader* const reader, SerdNode* dest)
{
  return (is_digit(peek_byte(reader))
            ? push_byte(reader, dest, eat_byte(reader))
            : SERD_ERR_BAD_SYNTAX);
}

static SerdStatus
read_digits(SerdReader* const reader, SerdNode* dest)
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
read_sign(SerdReader* const reader, SerdNode* dest)
{
  switch (peek_byte(reader)) {
  case '\0':
    return SERD_ERR_BAD_SYNTAX;
  case '+':
  case '-':
    return push_byte(reader, dest, eat_byte(reader));
  }
  return SERD_SUCCESS;
}

// Slightly more lax than the JSON standard, tolerates leading 0 and +
static SerdStatus
read_number(SerdReader* const reader, SerdNode** dest)
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
  }

  if (peek_byte(reader) == 'E' || peek_byte(reader) == 'e') {
    if ((st = push_byte(reader, *dest, eat_byte(reader))) ||
        (st = read_sign(reader, *dest)) || (st = read_digits(reader, *dest))) {
      return st;
    }
  }

  (*dest)->flags |= SERD_HAS_DATATYPE;
  SerdNode* datatype =
    has_decimal
      ? push_node(reader, SERD_URI, XSD_DOUBLE, sizeof(XSD_DOUBLE) - 1)
      : push_node(reader, SERD_URI, XSD_INTEGER, sizeof(XSD_INTEGER) - 1);

  return datatype ? SERD_SUCCESS : SERD_ERR_OVERFLOW;
}

static void
skip_ws(SerdReader* const reader)
{
  for (int c; (c = peek_byte(reader));) {
    switch (c) {
    case '\t':
    case '\r':
    case '\n':
    case ' ':
      eat_byte(reader);
      break;
    default:
      return;
    }
  }
}

static SerdStatus
read_sep(SerdReader* const reader, const int sep)
{
  SerdStatus st = SERD_SUCCESS;

  if ((st = eat_byte_check(reader, sep))) {
    return st;
  }

  skip_ws(reader);
  return SERD_SUCCESS;
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
read_value(SerdReader* const reader, ReadContext ctx, SerdNode** dest)
{
  static const char* const XSD_BOOLEAN     = NS_XSD "boolean";
  static const size_t      XSD_BOOLEAN_LEN = 40;

  SerdStatus st = SERD_SUCCESS;

  skip_ws(reader);
  switch (peek_byte(reader)) {
  case '\0':
    return SERD_ERR_BAD_SYNTAX;
  case '\"':
    return read_string(reader, dest);
  case '[':
    return read_array(reader, ctx, dest);
  case 'f':
    if ((st = read_term(reader, "false", 5, dest))) {
      return st;
    }

    (*dest)->flags |= SERD_HAS_DATATYPE;
    if (!(push_node(reader, SERD_URI, XSD_BOOLEAN, XSD_BOOLEAN_LEN))) {
      return SERD_ERR_OVERFLOW;
    }

    return SERD_SUCCESS;
  case 'n':
    return read_term(reader, "null", 4, dest);
  case 't':
    if ((st = read_term(reader, "true", 4, dest))) {
      return st;
    }

    (*dest)->flags |= SERD_HAS_DATATYPE;
    if (!(push_node(reader, SERD_URI, XSD_BOOLEAN, XSD_BOOLEAN_LEN))) {
      return SERD_ERR_OVERFLOW;
    }

    return SERD_SUCCESS;
  case '{':
    return read_node_object(reader, ctx);

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
  case '9':
    return read_number(reader, dest);

  default:
    return SERD_ERR_BAD_SYNTAX;
  }
}

static SerdStatus
read_array(SerdReader* const reader, ReadContext ctx, SerdNode** dest)
{
  (void)dest; // FIXME

  SerdStatus st;
  if ((st = read_sep(reader, '['))) {
    return st;
  } else if (peek_byte(reader) == ']') {
    read_sep(reader, ']');
    return SERD_SUCCESS;
  }

  SerdNode* vref = 0;
  while (!st) {
    if ((st = read_value(reader, ctx, &vref))) {
      /* fprintf(stderr, "ARRAY VALUE ERR %s\n", serd_strerror(st)); */
      return st;
    }
    emit_statement(reader, ctx, vref);
    skip_ws(reader);
    if (peek_byte(reader) == ',') {
      read_sep(reader, ',');
    } else {
      break;
    }
  }

  return st ? st : read_sep(reader, ']');
}

static SerdStatus
read_uri_array(SerdReader* const reader, ReadContext ctx, SerdNode** dest)
{
  (void)dest; // FIXME

  SerdStatus st;
  if ((st = read_sep(reader, '['))) {
    return st;
  } else if (peek_byte(reader) == ']') {
    read_sep(reader, ']');
    return SERD_SUCCESS;
  }

  SerdNode* vref = 0;
  while (!st) {
    if ((st = read_uri(reader, &vref))) {
      /* fprintf(stderr, "ARRAY VALUE ERR %s\n", serd_strerror(st)); */
      return st;
    }
    /* fprintf(stderr, "ARRAY VALUE %s\n", serd_node_string(vref)); */
    st = emit_statement(reader, ctx, vref);
    assert(!st);
    skip_ws(reader);
    if (peek_byte(reader) == ',') {
      read_sep(reader, ',');
    } else {
      break;
    }
  }

  return st ? st : read_sep(reader, ']');
}

static SerdStatus
read_list(SerdReader* const reader, ReadContext ctx)
{
  SerdNode*  out_node = NULL;
  SerdNode** dest     = &out_node;

  SerdStatus st;
  if ((st = read_sep(reader, '['))) {
    return st;
  } else if (peek_byte(reader) == ']') {
    *dest = reader->rdf_nil;
    read_sep(reader, ']');
    return emit_statement(reader, ctx, *dest);
  }

  // subject predicate _:head

  if (!(*dest = blank_id(reader))) {
    return SERD_ERR_OVERFLOW;
  }

  emit_statement(reader, ctx, *dest);

  /* The order of node allocation here is necessarily not in stack order,
     so we create two nodes and recycle them throughout. */
  SerdNode* n1 =
    push_node_padded(reader, genid_length(reader), SERD_BLANK, "", 0);

  SerdNode* n2   = NULL;
  SerdNode* node = n1;
  SerdNode* rest = NULL;

  ctx.subject = *dest;
  bool end    = peek_byte(reader) == ']';
  while (!end) {
    // _:node rdf:first object
    ctx.predicate = reader->rdf_first;

    SerdNode* vref = 0;
    if ((st = read_value(reader, ctx, &vref))) {
      return st;
    }

    emit_statement(reader, ctx, vref);

    skip_ws(reader);
    if (!(end = peek_byte(reader) != ',')) {
      read_sep(reader, ',');
      /* Give rest a new ID.  Done as late as possible to ensure it is
         used and > IDs generated by read_node_object above. */
      if (!rest) {
        rest = n2 = blank_id(reader); // First pass, push
        assert(rest); // Can't overflow since read_node_object() popped
      } else {
        set_blank_id(reader, rest, genid_length(reader) + 1);
      }
    }

    // _:node rdf:rest _:rest
    ctx.predicate = reader->rdf_rest;
    emit_statement(reader, ctx, (end ? reader->rdf_nil : rest));

    ctx.subject = rest;        // _:node = _:rest
    rest        = node;        // _:rest = (old)_:node
    node        = ctx.subject; // invariant
  }

  return st ? st : read_sep(reader, ']');
}

static SerdStatus
set_term(SerdReader* const     reader,
         SerdEnv* const        env,
         const SerdNode* const key,
         const SerdNode* const value,
         const SerdNode* const type)
{
  SerdNode* expanded = serd_env_expand(env, value);

  SerdStatus st = serd_env_define_term(env,
                                       serd_node_string_view(key),
                                       serd_node_string_view(expanded),
                                       type ? serd_node_string_view(type)
                                            : SERD_EMPTY_STRING());

  return st ? st : serd_sink_write_prefix(reader->sink, key, expanded);
}

static SerdStatus
read_context_value(SerdReader* const     reader,
                   SerdEnv* const        env,
                   const SerdNode* const key)
{
  SerdStatus st = SERD_SUCCESS;
  if (peek_byte(reader) == '{') {
    st = read_sep(reader, '{');

    SerdNode* idref   = NULL;
    SerdNode* typeref = NULL;
    while (!st) {
      SerdNode* ckey = NULL;
      if ((st = read_string(reader, &ckey)) || (st = read_sep(reader, ':'))) {
        return pop_err(reader, st, ckey);
      }

      SerdNode* cvalue = NULL;
      if ((st = read_uri(reader, &cvalue))) {
        return pop_err(reader, st, cvalue);
      }

      const char* ckey_str   = serd_node_string(ckey);
      const char* cvalue_str = serd_node_string(cvalue);
      if (!strcmp(ckey_str, "@id")) {
        idref = cvalue;
      } else if (!strcmp(ckey_str, "@type")) {
        if (!strcmp(cvalue_str, "@id")) {
          typeref = push_node(
            reader, SERD_URI, NS_RDFS "Resource", strlen(NS_RDFS "Resource"));
        } else {
          typeref = cvalue;
        }
      }

      if (maybe_read_sep(reader, ',')) {
        break;
      }
    }

    if ((st = read_sep(reader, '}'))) {
      return st;
    }

    if (idref) {
      set_term(reader, env, key, idref, typeref);
    }

  } else {
    SerdNode* value = NULL;
    if (!(st = read_uri(reader, &value))) {
      value->type = SERD_URI;
      set_term(reader, env, key, value, NULL);
    }
  }

  return st;
}

static SerdStatus
read_context(SerdReader* const reader, ReadContext* ctx)
{
  (void)ctx; // FIXME: ?

  SerdStatus st;
  skip_ws(reader);
  if (peek_byte(reader) != '{') {
    SerdNode* cref = NULL;
    read_uri(reader, &cref);
    return SERD_SUCCESS; // TODO: Context references?
  }

  if ((st = read_sep(reader, '{'))) {
    return st;
  }

  SerdEnv*  env = serd_env_new(SERD_EMPTY_STRING());
  SerdEnv** frame =
    (SerdEnv**)serd_stack_push(&reader->env_stack, sizeof(SerdEnv*));
  *frame = env;

  while (true) {
    SerdNode* key = NULL;
    if ((st = read_string(reader, &key)) || (st = read_sep(reader, ':'))) {
      return pop_err(reader, st, key);
    }

    if ((st = read_context_value(reader, env, key))) {
      return pop_err(reader, st, key);
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
start_object(SerdReader* const reader, ReadContext ctx, SerdNode* node)
{
  if (node->type == SERD_BLANK) {
    set_blank_id(reader, node, genid_length(reader) + 1);
  }

  if (ctx.subject && ctx.predicate) {
    emit_statement(reader, ctx, node);
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_node_type(SerdReader* const reader,
               ReadContext       ctx,
               SerdNode* const   node_id)
{
  (void)ctx;
  (void)node_id;

  skip_ws(reader);

  /* fprintf(stderr, "READ NODE TYPE\n"); */
  if (peek_byte(reader) == '[') {
    ctx.subject   = node_id;
    ctx.predicate = reader->rdf_type;
    read_uri_array(reader, ctx, NULL);
  } else {
    SerdNode* value = NULL;
    read_uri(reader, &value);

    ctx.subject   = node_id;
    ctx.predicate = reader->rdf_type;
    maybe_emit_statement(reader, ctx, value, NULL, NULL);
  }

  return SERD_ERR_INTERNAL;
}

static SerdStatus
read_node_object(SerdReader* const reader, ReadContext ctx)
{
  SerdStatus st;
  if ((st = read_sep(reader, '{'))) {
    return st;
  }

  SerdNode* id = blank_id(reader);
  //    push_node_padded(reader, genid_length(reader), SERD_BLANK, "", 0);

  if (!id) {
    return SERD_ERR_OVERFLOW;
  }

  SerdNode* key        = NULL;
  SerdNode* value      = NULL;
  SerdNode* datatype   = NULL;
  SerdNode* lang       = NULL;
  bool      is_literal = false;

  while (true) {
    if ((st = read_string(reader, &key)) || (st = read_sep(reader, ':'))) {
      return pop_err(reader, st, key);
    }

    /* "@context" */
    /* "@id" */
    /* "@included" */
    /* "@graph" */
    /* "@nest" */
    /* "@type" */
    /* "@reverse" */
    /* "@index"; */

    const char* key_str = serd_node_string(key);
    /* fprintf(stderr, "KEY: %s\n", key_str); */
    if (!strcmp(key_str, "@list")) {
      st = read_list(reader, ctx);
    } else if (!strcmp(key_str, "@value")) {
      is_literal = true;
      if ((st = read_string(reader, &value))) {
        return pop_err(reader, st, key);
      }
    } else if (!strcmp(key_str, "@type")) {
      st = read_node_type(reader, ctx, id);
    } else if (is_literal) {
      if (!strcmp(key_str, "@type")) {
        if ((st = read_uri(reader, &datatype))) {
          return pop_err(reader, st, key);
        }
      } else if (!strcmp(key_str, "@language")) {
        if ((st = read_string(reader, &lang))) {
          return pop_err(reader, st, key);
        }
      } else {
        r_err(reader, SERD_ERR_BAD_SYNTAX, "invalid literal property\n");
        return pop_err(reader, SERD_ERR_BAD_SYNTAX, key);
      }
    } else if (!strcmp(key_str, "@id")) {
      if ((st = read_uri(reader, &id))) {
        return SERD_ERR_BAD_SYNTAX;
      }
      // fprintf(stderr, "ID %s\n", serd_node_string(id));
    } else if (!strcmp(key_str, "@context")) {
      read_context(reader, &ctx);
    } else {
      // fprintf(stderr, "OBJECT\n");
      if (ctx.subject != id) {
        start_object(reader, ctx, id);
        ctx.subject = id;
      }

      SerdNodeType vtype = SERD_LITERAL;
      if (!strcmp(key_str, "@type")) {
        key   = push_node(reader, SERD_URI, NS_RDF "type", 47);
        vtype = SERD_URI;
      } else {
        const SerdNode* type         = NULL;
        SerdNode*       expanded_key = NULL;
        expand_term(reader, key, &expanded_key, &type);
        if (expanded_key) {
          key       = expanded_key;
          key->type = SERD_URI;
        }

        if (type) {
          const char* type_str = serd_node_string(type);
          if (!strcmp(type_str, NS_RDFS "Resource")) {
            vtype = SERD_URI;
          } else {
            datatype = push_node(reader, type->type, type_str, type->length);
          }
        }

        key->type = SERD_URI;
      }

      ctx.predicate = key;
      // fprintf(stderr, "VTYPE: %d\n", vtype);
      if (vtype == SERD_URI) {
        st = read_uri(reader, &value);
        //        assert(value);
      } else {
        st = read_value(reader, ctx, &value);
        //        assert(value);
      }

      if (st) {
        return st;
      }

      // fprintf(stderr, "MAYBE EMIT %s\n", serd_node_string(value));
      //      assert(value);
      maybe_emit_statement(reader, ctx, value, datatype, lang);
      value = NULL;
    }

    if (maybe_read_sep(reader, ',')) {
      break;
    }
  }

  if ((st = read_sep(reader, '}'))) {
    return st;
  }

  //  maybe_emit_statement(reader, ctx, value, datatype, lang);

  return SERD_SUCCESS;
}

SerdStatus
read_jsonLd_statement(SerdReader* reader)
{
  (void)reader;

  return SERD_FAILURE;
}

SerdStatus
read_jsonLdDoc(SerdReader* reader)
{
  SerdStatementFlags flags = 0;
  ReadContext        ctx   = {0, 0, 0, 0, &flags};

  reader->env_stack = serd_stack_new(128, sizeof(SerdEnv*));

  SerdStatus st = SERD_SUCCESS;
  if ((st = read_node_object(reader, ctx))) {
    return st;
  }

  return SERD_SUCCESS;
}
