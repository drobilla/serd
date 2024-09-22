// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_turtle.h"
#include "byte_source.h"
#include "read_ntriples.h"
#include "reader.h"
#include "stack.h"
#include "string_utils.h"
#include "symbols.h"
#include "token_header.h"
#include "try.h"
#include "turtle.h"

#include <serd/event.h>
#include <serd/node_flags.h>
#include <serd/node_type.h>
#include <serd/reader.h>
#include <serd/status.h>
#include <zix/attributes.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static SerdStatus
read_collection(SerdReader* reader, ReadContext ctx, TokenHeader** dest);

static SerdStatus
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot);

// whitespace ::= #x9 | #xA | #xD | #x20 | comment
static SerdStatus
read_whitespace(SerdReader* const reader)
{
  const int c = peek_byte(reader);

  return (c == '\t' || c == '\n' || c == '\r' || c == ' ')
           ? serd_byte_source_advance(&reader->source)
         : (c == '#') ? read_comment(reader)
                      : SERD_FAILURE;
}

bool
read_turtle_ws_star(SerdReader* const reader)
{
  while (!read_whitespace(reader)) {
  }

  return true;
}

static int
skip_ws_peek(SerdReader* const reader)
{
  read_turtle_ws_star(reader);
  return peek_byte(reader);
}

static bool
peek_delim(SerdReader* const reader, const uint8_t delim)
{
  return skip_ws_peek(reader) == delim;
}

static bool
eat_delim(SerdReader* const reader, const uint8_t delim)
{
  if (peek_delim(reader, delim)) {
    skip_byte(reader, delim);
    return read_turtle_ws_star(reader);
  }

  return false;
}

// STRING_LITERAL_LONG_QUOTE and STRING_LITERAL_LONG_SINGLE_QUOTE
// Initial triple quotes are already eaten by caller
static SerdStatus
read_STRING_LITERAL_LONG(SerdReader* const  reader,
                         TokenHeader* const ref,
                         const uint8_t      q)
{
  SerdStatus st = SERD_SUCCESS;
  while (tolerate_status(reader, st)) {
    const int c = peek_byte(reader);
    if (c == '\\') {
      if (!(st = skip_byte(reader, c))) {
        st = read_string_escape(reader, ref);
      }
    } else if (c == q) {
      skip_byte(reader, q);
      const int q2 = eat_byte_safe(reader, peek_byte(reader));
      const int q3 = peek_byte(reader);
      if (q2 == q && q3 == q) { // End of string
        skip_byte(reader, q3);
        break;
      }

      if (q2 == '\\') {
        if (!(st = push_byte(reader, ref, c))) {
          st = read_string_escape(reader, ref);
        }
      } else {
        ref->flags |= SERD_HAS_QUOTE;
        if (!(st = push_byte(reader, ref, c))) {
          st = read_character(reader, ref, (uint8_t)q2);
        }
      }
    } else if (c > 0) {
      st = read_character(reader, ref, (uint8_t)eat_byte_safe(reader, c));
    } else {
      return r_err_eof(reader, SERD_BAD_SYNTAX);
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

static SerdStatus
read_String(SerdReader* const reader, TokenHeader* const node)
{
  const int q1 = eat_byte_safe(reader, peek_byte(reader));
  const int q2 = peek_byte(reader);
  if (q2 < 0) {
    return r_err_eof(reader, SERD_BAD_SYNTAX);
  }

  if (q2 != q1) { // Short string (not triple quoted)
    return read_STRING_LITERAL(reader, node, (uint8_t)q1);
  }

  skip_byte(reader, q2);
  const int q3 = peek_byte(reader);
  if (q3 < 0) {
    return r_err_eof(reader, SERD_BAD_SYNTAX);
  }

  if (q3 != q1) { // Empty short string ("" or '')
    return SERD_SUCCESS;
  }

  skip_byte(reader, q3);
  return read_STRING_LITERAL_LONG(reader, node, (uint8_t)q1);
}

static SerdStatus
read_PERCENT(SerdReader* const reader, TokenHeader* const dest)
{
  SerdStatus st = push_byte(reader, dest, eat_byte_safe(reader, '%'));
  if (st) {
    return st;
  }

  const uint8_t h1 = read_HEX(reader);
  const uint8_t h2 = read_HEX(reader);
  if (!h1 || !h2) {
    return SERD_BAD_SYNTAX;
  }

  if (!(st = push_byte(reader, dest, h1))) {
    st = push_byte(reader, dest, h2);
  }

  return st;
}

static SerdStatus
read_PN_LOCAL_ESC(SerdReader* const reader, TokenHeader* const dest)
{
  skip_byte(reader, '\\');

  const int c = peek_byte(reader);

  return is_PN_LOCAL_ESC(c) ? push_byte(reader, dest, eat_byte_safe(reader, c))
                            : r_err(reader, SERD_BAD_SYNTAX, "bad escape");
}

static SerdStatus
read_PLX(SerdReader* const reader, TokenHeader* const dest)
{
  const int c = peek_byte(reader);

  return (c == '%')    ? read_PERCENT(reader, dest)
         : (c == '\\') ? read_PN_LOCAL_ESC(reader, dest)
                       : SERD_FAILURE;
}

static SerdStatus
read_PN_LOCAL(SerdReader* const  reader,
              TokenHeader* const dest,
              bool* const        ate_dot)
{
  int        c                      = peek_byte(reader);
  SerdStatus st                     = SERD_SUCCESS;
  bool       trailing_unescaped_dot = false;
  switch (c) {
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
  case ':':
  case '_':
    st = push_byte(reader, dest, eat_byte_safe(reader, c));
    break;
  default:
    if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
      return r_err(reader, st, "bad escape");
    }

    if (st != SERD_SUCCESS && (st = read_PN_CHARS_BASE(reader, dest))) {
      return st;
    }
  }

  while ((c = peek_byte(reader)) > 0) { // Middle: (PN_CHARS | '.' | ':')*
    if (c == '.' || c == ':') {
      st = push_byte(reader, dest, eat_byte_safe(reader, c));
    } else if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
      return r_err(reader, st, "bad escape");
    } else if (st != SERD_SUCCESS && (st = read_PN_CHARS(reader, dest))) {
      break;
    }
    trailing_unescaped_dot = (c == '.');
  }

  if (trailing_unescaped_dot) {
    // Ate trailing dot, pop it from stack/node and inform caller
    *ate_dot = pop_last_node_char(reader, dest);
  }

  return (st > SERD_FAILURE) ? st : SERD_SUCCESS;
}

// Read the remainder of a PN_PREFIX after some initial characters
static SerdStatus
read_PN_PREFIX_tail(SerdReader* const  reader,
                    TokenHeader* const dest,
                    bool* const        ate_dot)
{
  SerdStatus st                     = SERD_SUCCESS;
  bool       trailing_unescaped_dot = false;

  while (!st) { // Middle: (PN_CHARS | '.')*
    const int c = peek_byte(reader);
    if (c == '.') {
      st = push_byte(reader, dest, eat_byte_safe(reader, c));
      trailing_unescaped_dot = true;
    } else if (!(st = read_PN_CHARS(reader, dest))) {
      trailing_unescaped_dot = false;
    }
  }

  if (trailing_unescaped_dot) {
    *ate_dot = pop_last_node_char(reader, dest);
  }

  return st;
}

static SerdStatus
read_PN_PREFIX(SerdReader* const  reader,
               TokenHeader* const dest,
               bool* const        ate_dot)
{
  const SerdStatus st = read_PN_CHARS_BASE(reader, dest);

  return st ? st : read_PN_PREFIX_tail(reader, dest, ate_dot);
}

static SerdStatus
read_IRIREF(SerdReader* const reader, TokenHeader** const dest)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, eat_byte_check(reader, '<'));

  if (!(*dest = push_node_head(reader, SERD_URI))) {
    return SERD_BAD_STACK;
  }

  return read_IRIREF_suffix(reader, *dest);
}

static SerdStatus
read_PrefixedName(SerdReader* const  reader,
                  TokenHeader* const dest,
                  bool* const        ate_dot)
{
  SerdStatus st = SERD_SUCCESS;
  TRY_FAILING(st, read_PN_PREFIX(reader, dest, ate_dot));

  if (peek_byte(reader) != ':') {
    return SERD_FAILURE;
  }

  TRY(st, push_byte(reader, dest, eat_byte_safe(reader, ':')));
  TRY_FAILING(st, read_PN_LOCAL(reader, dest, ate_dot));
  return push_node_termination(reader);
}

static SerdStatus
read_0_9(SerdReader* const  reader,
         TokenHeader* const str,
         const bool         at_least_one)
{
  unsigned   count = 0;
  SerdStatus st    = SERD_SUCCESS;
  for (int c = 0; is_digit((c = peek_byte(reader))); ++count) {
    TRY(st, push_byte(reader, str, eat_byte_safe(reader, c)));
  }

  if (at_least_one && count == 0) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected digit");
  }

  return st;
}

ZIX_NODISCARD static SerdStatus
read_number_sign(SerdReader* const   reader,
                 TokenHeader** const dest,
                 const int           c)
{
  return (c == '-' || c == '+')
           ? push_byte(reader, *dest, eat_byte_safe(reader, c))
           : SERD_SUCCESS;
}

ZIX_NODISCARD static SerdStatus
read_number(SerdReader* const   reader,
            TokenHeader** const dest,
            TokenHeader** const datatype,
            bool* const         ate_dot)
{
  if (!(*dest = push_node_head(reader, SERD_LITERAL))) {
    return SERD_BAD_STACK;
  }

  SerdStatus st = SERD_SUCCESS;
  int        c  = peek_byte(reader);
  TRY(st, read_number_sign(reader, dest, c));

  bool has_decimal = false;
  if ((c = peek_byte(reader)) == '.') {
    has_decimal = true;
    // decimal case 2 (e.g. ".0" or "-.0" or "+.0")
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    TRY(st, read_0_9(reader, *dest, true));
  } else {
    // all other cases ::= ( '-' | '+' ) [0-9]+ ( . )? ( [0-9]+ )? ...
    TRY(st, read_0_9(reader, *dest, true));
    if ((c = peek_byte(reader)) == '.') {
      // Annoyingly, dot can be end of statement, so tentatively eat
      skip_byte(reader, c);
      c = peek_byte(reader);
      if (!is_digit(c) && c != 'e' && c != 'E') {
        *ate_dot = true; // Force caller to deal with silly grammar
      } else {
        has_decimal = true;
        TRY(st, push_byte(reader, *dest, '.'));
        read_0_9(reader, *dest, false);
      }
    }
  }

  c = peek_byte(reader);
  if (c == 'e' || c == 'E') {
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    c = peek_byte(reader);
    TRY(st, read_number_sign(reader, dest, c));
    TRY(st, read_0_9(reader, *dest, true));
    *datatype = push_node(reader, SERD_URI, serd_symbols[XSD_DOUBLE]);
  } else if (has_decimal) {
    *datatype = push_node(reader, SERD_URI, serd_symbols[XSD_DECIMAL]);
  } else {
    *datatype = push_node(reader, SERD_URI, serd_symbols[XSD_INTEGER]);
  }

  (*dest)->flags |= SERD_HAS_DATATYPE;
  return *datatype ? SERD_SUCCESS : SERD_BAD_STACK;
}

SerdStatus
read_turtle_iri(SerdReader* const   reader,
                TokenHeader** const dest,
                bool* const         ate_dot)
{
  if (peek_byte(reader) == '<') {
    return read_IRIREF(reader, dest);
  }

  *dest = push_node_head(reader, SERD_CURIE);
  return *dest ? read_PrefixedName(reader, *dest, ate_dot) : SERD_BAD_STACK;
}

static SerdStatus
read_literal(SerdReader* const   reader,
             TokenHeader** const dest,
             TokenHeader** const meta,
             bool* const         ate_dot)
{
  if (!(*dest = push_node_head(reader, SERD_LITERAL))) {
    return SERD_BAD_STACK;
  }

  SerdStatus st = SERD_SUCCESS;
  if ((st = read_String(reader, *dest))) {
    return st;
  }

  const int next = peek_byte(reader);
  if (next == '@') {
    skip_byte(reader, '@');
    (*dest)->flags |= SERD_HAS_LANGUAGE;
    st = read_LANGTAG(reader, meta);
  } else if (next == '^') {
    skip_byte(reader, '^');
    TRY(st, eat_byte_check(reader, '^'));
    (*dest)->flags |= SERD_HAS_DATATYPE;
    st = read_turtle_iri(reader, meta, ate_dot);
  }

  return st;
}

static SerdStatus
read_verb(SerdReader* const reader, TokenHeader** const dest)
{
  if (peek_byte(reader) == '<') {
    return read_IRIREF(reader, dest);
  }

  const size_t orig_stack_size = reader->stack.size;

  TokenHeader* node = NULL;
  if (!(node = push_node_head(reader, SERD_CURIE))) {
    return SERD_BAD_STACK;
  }

  // Try to read as a prefixed name
  bool       ate_dot = false;
  SerdStatus st      = read_PrefixedName(reader, node, &ate_dot);

  if (st == SERD_FAILURE) {
    // Check if this is actually the "a" shorthand
    const char* const str = (const char*)(node + 1U);
    const size_t      len = node->length;
    if (len == 1 && str[0] == 'a') {
      node = push_node(reader, SERD_URI, serd_symbols[RDF_TYPE]);
      st   = node ? SERD_SUCCESS : SERD_BAD_STACK;
    } else {
      st = SERD_BAD_SYNTAX;
    }
  }

  if (st) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);
    *dest = NULL;
    return r_err(reader, st, "bad verb");
  }

  *dest = node;
  return SERD_SUCCESS;
}

static SerdStatus
read_anon(SerdReader* const   reader,
          ReadContext         ctx,
          const bool          subject,
          TokenHeader** const dest)
{
  skip_byte(reader, '[');

  const SerdEventFlags old_flags = *ctx.flags;
  const bool           empty     = peek_delim(reader, ']');

  if (subject) {
    *ctx.flags |= empty ? SERD_EMPTY_S : SERD_ANON_S;
  } else {
    *ctx.flags |= empty ? SERD_EMPTY_O : SERD_ANON_O;
  }

  if (!*dest) {
    if (!(*dest = blank_id(reader))) {
      return SERD_BAD_STACK;
    }
  }

  // Emit statement with this anonymous object first
  SerdStatus st = SERD_SUCCESS;
  if (ctx.subject) {
    TRY(st, emit_statement(reader, ctx, *dest, NULL));
  }

  // Switch the subject to the anonymous node and read its description
  ctx.subject = *dest;
  if (!empty) {
    bool ate_dot_in_list = false;
    TRY(st, read_predicateObjectList(reader, ctx, &ate_dot_in_list));

    if (ate_dot_in_list) {
      return r_err(reader, SERD_BAD_SYNTAX, "'.' inside blank");
    }

    read_turtle_ws_star(reader);
    *ctx.flags = old_flags;

    st = emit_event(reader, serd_end_event(stack_token_view(*dest).string));
  }

  return st > SERD_FAILURE ? st : eat_byte_check(reader, ']');
}

static bool
node_has_string(const TokenHeader* const node, const ZixStringView string)
{
  return node->length == string.length &&
         !memcmp(node + 1U, string.data, string.length);
}

// Read a "named" object: a boolean literal or a prefixed name
static SerdStatus
read_named_object(SerdReader* const   reader,
                  TokenHeader** const dest,
                  TokenHeader** const datatype,
                  bool* const         ate_dot)
{
  static const ZixStringView true_string  = ZIX_STATIC_STRING("true");
  static const ZixStringView false_string = ZIX_STATIC_STRING("false");

  /* This function deals with nodes that start with some letters.  Unlike
     everything else, the cases here aren't nicely distinguished by leading
     characters, so this is more tedious to deal with in a non-tokenizing
     parser like this one.

     Deal with this here by trying to read a prefixed node, then if it turns
     out to actually be "true" or "false", switch it to a boolean literal. */

  if (!(*dest = push_node_head(reader, SERD_CURIE))) {
    return SERD_BAD_STACK;
  }

  TokenHeader* node = *dest;
  SerdStatus   st   = SERD_SUCCESS;

  // Attempt to read a prefixed name
  st = read_PrefixedName(reader, node, ate_dot);

  // Check if this is actually a special boolean node
  if (st == SERD_FAILURE && (node_has_string(node, true_string) ||
                             node_has_string(node, false_string))) {
    node->flags = SERD_HAS_DATATYPE;
    node->type  = SERD_LITERAL;
    *datatype   = push_node(reader, SERD_URI, serd_symbols[XSD_BOOLEAN]);
    return *datatype ? SERD_SUCCESS : SERD_BAD_STACK;
  }

  // Any other failure is a syntax error
  if (st) {
    st = st > SERD_FAILURE ? st : SERD_BAD_SYNTAX;
    return r_err(reader, st, "expected prefixed name or boolean");
  }

  return SERD_SUCCESS;
}

// Read an object and emit statements, possibly recursively
static SerdStatus
read_object(SerdReader* const        reader,
            const ReadContext* const ctx,
            TokenHeader** const      o,
            TokenHeader** const      meta,
            bool* const              ate_dot)
{
  const size_t orig_stack_size = reader->stack.size;

  assert(ctx->subject);

  SerdStatus st     = SERD_FAILURE;
  bool       simple = true;
  const int  c      = peek_byte(reader);
  if (c < 0) {
    return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");
  }

  switch (c) {
  case ')':
    return r_err(reader, SERD_BAD_SYNTAX, "expected object");
  case '[':
    simple = false;
    st     = read_anon(reader, *ctx, false, o);
    break;
  case '(':
    simple = false;
    st     = read_collection(reader, *ctx, o);
    break;
  case '_':
    st = read_BLANK_NODE_LABEL(reader, o, ate_dot);
    break;
  case '<':
    st = read_IRIREF(reader, o);
    break;
  case ':':
    st = read_turtle_iri(reader, o, ate_dot);
    break;
  case '+':
  case '-':
  case '.':
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
    st = read_number(reader, o, meta, ate_dot);
    break;
  case '\"':
  case '\'':
    st = read_literal(reader, o, meta, ate_dot);
    break;
  default:
    // Either a boolean literal or a prefixed name
    st = read_named_object(reader, o, meta, ate_dot);
  }

  if (!st && simple && *o) {
    st = emit_statement(reader, *ctx, *o, *meta);
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);
  return st;
}

static SerdStatus
read_objectList(SerdReader* const reader, ReadContext ctx, bool* const ate_dot)
{
  SerdStatus st = SERD_SUCCESS;

  TokenHeader* object = NULL;
  TokenHeader* meta   = NULL;
  TRY(st, read_object(reader, &ctx, &object, &meta, ate_dot));

  while (st <= SERD_FAILURE && !*ate_dot && eat_delim(reader, ',')) {
    object = NULL;
    meta   = NULL;
    st     = read_object(reader, &ctx, &object, &meta, ate_dot);
  }

  return st;
}

static SerdStatus
read_predicateObjectList(SerdReader* const reader,
                         ReadContext       ctx,
                         bool* const       ate_dot)
{
  const size_t orig_stack_size = reader->stack.size;

  SerdStatus st = SERD_SUCCESS;
  while (!(st = read_verb(reader, &ctx.predicate)) &&
         read_turtle_ws_star(reader) &&
         !(st = read_objectList(reader, ctx, ate_dot)) && !*ate_dot) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);

    bool ate_semi = false;
    for (int c = 0; (c = skip_ws_peek(reader)) > 0;) {
      if (c == '.' || c == ']' || c == '}') {
        return SERD_SUCCESS;
      }

      if (c != ';') {
        break;
      }

      skip_byte(reader, c);
      ate_semi = true;
    }

    if (!ate_semi) {
      return r_err(reader, SERD_BAD_SYNTAX, "missing ';' or '.'");
    }
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);
  ctx.predicate = 0;
  return st;
}

static SerdStatus
end_collection(SerdReader* const reader, const SerdStatus st)
{
  return st ? st : eat_byte_check(reader, ')');
}

static SerdStatus
read_collection(SerdReader* const   reader,
                ReadContext         ctx,
                TokenHeader** const dest)
{
  SerdStatus st = SERD_SUCCESS;

  skip_byte(reader, '(');

  bool end = peek_delim(reader, ')');
  if (!(*dest = end ? reader->rdf_nil : blank_id(reader))) {
    return SERD_BAD_STACK;
  }

  if (ctx.subject) { // Reading a collection object
    *ctx.flags |= (end ? 0 : SERD_LIST_O);
    TRY(st, emit_statement(reader, ctx, *dest, NULL));
    *ctx.flags &= (SerdEventFlags) ~((unsigned)SERD_LIST_O);
  } else { // Reading a collection subject
    *ctx.flags |= (end ? 0 : SERD_LIST_S);
  }

  if (end) {
    return end_collection(reader, st);
  }

  /* The order of node allocation here is necessarily not in stack order,
     so we create two nodes and recycle them throughout. */
  TokenHeader* const n1 =
    push_node_space(reader, SERD_BLANK, genid_size(reader));
  if (!n1) {
    return SERD_BAD_STACK;
  }

  ctx.subject = *dest;

  TokenHeader* node = n1;
  TokenHeader* rest = NULL;
  while (!peek_delim(reader, ')')) {
    // _:node rdf:first object
    ctx.predicate        = reader->rdf_first;
    TokenHeader* object  = NULL;
    TokenHeader* meta    = NULL;
    bool         ate_dot = false;
    if ((st = read_object(reader, &ctx, &object, &meta, &ate_dot)) || ate_dot) {
      return end_collection(reader, st);
    }

    if (!(end = peek_delim(reader, ')'))) {
      /* Give rest a new ID.  Done as late as possible to ensure it is
         used and > IDs generated by read_object above. */
      if (!rest) {
        rest = blank_id(reader); // First pass, push
        assert(rest);            // Can't overflow since read_object() popped
      } else {
        set_blank_id(reader, rest, genid_size(reader));
      }
    }

    // _:node rdf:rest _:rest
    ctx.predicate = reader->rdf_rest;
    st = emit_statement(reader, ctx, (end ? reader->rdf_nil : rest), NULL);
    if (st) {
      break;
    }

    ctx.subject = rest;        // _:node = _:rest
    rest        = node;        // _:rest = (old)_:node
    node        = ctx.subject; // invariant
  }

  return end_collection(reader, st);
}

SerdStatus
read_turtle_subject(SerdReader* const   reader,
                    ReadContext         ctx,
                    TokenHeader** const dest,
                    int* const          s_type)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;
  switch ((*s_type = peek_byte(reader))) {
  case '[':
    st = read_anon(reader, ctx, true, dest);
    break;
  case '(':
    st = read_collection(reader, ctx, dest);
    break;
  case '_':
    st = read_BLANK_NODE_LABEL(reader, dest, &ate_dot);
    break;
  default:
    TRY(st, read_turtle_iri(reader, dest, &ate_dot));
  }

  if (ate_dot) {
    return r_err(reader, SERD_BAD_SYNTAX, "subject ends with '.'");
  }

  return st;
}

SerdStatus
read_turtle_triples(SerdReader* const reader,
                    ReadContext       ctx,
                    bool* const       ate_dot)
{
  assert(ctx.subject);

  read_turtle_ws_star(reader);

  const int c = peek_byte(reader);
  if (c == '.') {
    *ate_dot = !skip_byte(reader, c);
    return SERD_FAILURE;
  }

  if (c == '}') {
    return SERD_FAILURE;
  }

  const SerdStatus st = read_predicateObjectList(reader, ctx, ate_dot);

  ctx.subject = ctx.predicate = 0;
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

SerdStatus
read_turtle_base(SerdReader* const reader, const bool sparql, const bool token)
{
  SerdStatus st = SERD_SUCCESS;
  if (token) {
    TRY(st, eat_string(reader, "base", 4));
  }

  read_turtle_ws_star(reader);

  TokenHeader* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));
  TRY(st, emit_event(reader, serd_base_event(stack_token_view(uri).string)));

  read_turtle_ws_star(reader);
  if (!sparql) {
    return eat_byte_check(reader, '.');
  }

  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_BAD_SYNTAX, "'.' after SPARQL BASE");
  }

  return SERD_SUCCESS;
}

SerdStatus
read_turtle_prefixID(SerdReader* const reader,
                     const bool        sparql,
                     const bool        token)
{
  SerdStatus st = SERD_SUCCESS;
  if (token) {
    TRY(st, eat_string(reader, "prefix", 6));
  }

  read_turtle_ws_star(reader);
  TokenHeader* const name = push_node_head(reader, SERD_LITERAL);
  if (!name) {
    return SERD_BAD_STACK;
  }

  bool ate_dot = false;
  TRY_FAILING(st, read_PN_PREFIX(reader, name, &ate_dot));
  if (ate_dot || (st = eat_byte_check(reader, ':'))) {
    return r_err(reader,
                 st > SERD_FAILURE ? st : SERD_BAD_SYNTAX,
                 "expected a prefix name");
  }

  read_turtle_ws_star(reader);

  TokenHeader* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));

  st = emit_event(reader,
                  serd_prefix_event(stack_token_view(name).string,
                                    stack_token_view(uri).string));

  if (!sparql) {
    read_turtle_ws_star(reader);
    st = eat_byte_check(reader, '.');
  }

  return st;
}

SerdStatus
read_turtle_directive(SerdReader* const reader)
{
  skip_byte(reader, '@');

  const int next = peek_byte(reader);

  if (next == 'b') {
    return read_turtle_base(reader, false, true);
  }

  if (next == 'p') {
    return read_turtle_prefixID(reader, false, true);
  }

  return r_err(reader, SERD_BAD_SYNTAX, "expected \"base\" or \"prefix\"");
}

static SerdStatus
read_sparql_directive(SerdReader* const reader, const TokenHeader* const token)
{
  static const ZixStringView base_token   = ZIX_STATIC_STRING("base");
  static const ZixStringView prefix_token = ZIX_STATIC_STRING("prefix");

  if (token_equals(token, base_token)) {
    return read_turtle_base(reader, true, false);
  }

  if (token_equals(token, prefix_token)) {
    return read_turtle_prefixID(reader, true, false);
  }

  return SERD_FAILURE;
}

static SerdStatus
read_block(SerdReader* const reader, ReadContext* const ctx)
{
  SerdStatus st = SERD_SUCCESS;

  // Try to read a subject, though it may actually be a directive or graph name
  TokenHeader* token  = NULL;
  int          s_type = 0;
  TRY_FAILING(st, read_turtle_subject(reader, *ctx, &token, &s_type));

  // Try to interpret as a SPARQL "PREFIX" or "BASE" directive
  if (st && (st = read_sparql_directive(reader, token)) != SERD_FAILURE) {
    return st;
  }

  if (st) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected directive or subject");
  }

  // Our token is really a subject, read some triples
  bool ate_dot = false;
  ctx->subject = token;
  TRY_FAILING(st, read_turtle_triples(reader, *ctx, &ate_dot));

  // "Failure" is only allowed for anonymous subjects like "[ ... ] ."
  if (st && s_type != '[') {
    return r_err(reader, SERD_BAD_SYNTAX, "expected triples");
  }

  // Ensure that triples are properly terminated
  return ate_dot ? st : eat_byte_check(reader, '.');
}

SerdStatus
read_turtle_statement(SerdReader* const reader)
{
  SerdEventFlags flags = 0U;
  ReadContext    ctx   = {NULL, NULL, NULL, &flags};

  // Skip whitespace and get the first byte
  read_turtle_ws_star(reader);
  const int c = peek_byte(reader);
  if (c < 0) {
    return SERD_FAILURE; // EOF
  }

  // Handle nice cases we can distinguish from the next byte
  switch (c) {
  case '\0':
    eat_byte(reader);
    return SERD_FAILURE;

  case '@':
    return read_turtle_directive(reader);

  default:
    break;
  }

  // No such luck, figure out what to read from the first token
  return read_block(reader, &ctx);
}

SerdStatus
read_turtleDoc(SerdReader* const reader)
{
  while (!reader->source.eof) {
    const size_t     orig_stack_size = reader->stack.size;
    const SerdStatus st              = read_turtle_statement(reader);

    if (st > SERD_FAILURE) {
      if (!tolerate_status(reader, st)) {
        serd_stack_pop_to(&reader->stack, orig_stack_size);
        return st;
      }
      serd_reader_skip_until_byte(reader, '\n');
    }

    serd_stack_pop_to(&reader->stack, orig_stack_size);
  }

  return SERD_SUCCESS;
}
