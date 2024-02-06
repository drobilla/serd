// Copyright 2011-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "read_turtle.h"
#include "byte_source.h"
#include "namespaces.h"
#include "node.h"
#include "node_impl.h"
#include "ntriples.h"
#include "read_ntriples.h"
#include "reader.h"
#include "stack.h"
#include "string_utils.h"
#include "try.h"
#include "turtle.h"

#include "serd/caret_view.h"
#include "serd/env.h"
#include "serd/event.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/status.h"
#include "serd/uri.h"
#include "zix/attributes.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/// Like TRY() but tolerates SERD_FAILURE
#define TRY_LAX(st, exp)                 \
  do {                                   \
    if (((st) = (exp)) > SERD_FAILURE) { \
      return (st);                       \
    }                                    \
  } while (0)

static SerdStatus
read_collection(SerdReader* reader, ReadContext ctx, SerdNode** dest);

static SerdStatus
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot);

// whitespace ::= #x9 | #xA | #xD | #x20 | comment
ZIX_NODISCARD static SerdStatus
read_whitespace(SerdReader* const reader)
{
  const int c = peek_byte(reader);

  return (c == '\t' || c == '\n' || c == '\r' || c == ' ')
           ? serd_byte_source_advance(reader->source)
         : (c == '#') ? read_comment(reader)
                      : SERD_FAILURE;
}

SerdStatus
read_turtle_ws_star(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;

  while (!(st = read_whitespace(reader))) {
  }

  return accept_failure(st);
}

ZIX_NODISCARD static SerdStatus
eat_delim(SerdReader* const reader, const uint8_t delim)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, read_turtle_ws_star(reader));
  if (peek_byte(reader) == delim) {
    TRY(st, skip_byte(reader, delim));
    return read_turtle_ws_star(reader);
  }

  return SERD_FAILURE;
}

// STRING_LITERAL_LONG_QUOTE and STRING_LITERAL_LONG_SINGLE_QUOTE
// Initial triple quotes are already eaten by caller
static SerdStatus
read_STRING_LITERAL_LONG(SerdReader* const reader,
                         SerdNode* const   ref,
                         const uint8_t     q)
{
  SerdStatus st = SERD_SUCCESS;
  while (tolerate_status(reader, st)) {
    const int c = peek_byte(reader);
    if (c == '\\') {
      TRY(st, skip_byte(reader, c));
      st = read_string_escape(reader, ref);
    } else if (c == EOF) {
      st = r_err(reader, SERD_NO_DATA, "unexpected end of file");
    } else if (c == q) {
      TRY(st, skip_byte(reader, q));
      const int q2 = peek_byte(reader);
      TRY(st, skip_byte(reader, q2));
      const int q3 = peek_byte(reader);
      if (q2 == q && q3 == q) { // End of string
        TRY(st, skip_byte(reader, q3));
        break;
      }

      if (q2 == '\\') {
        TRY(st, push_byte(reader, ref, c));
        st = read_string_escape(reader, ref);
      } else {
        if (!(st = push_byte(reader, ref, c))) {
          st = read_character(reader, ref, (uint8_t)q2);
        }
      }
    } else {
      TRY(st, skip_byte(reader, c));
      st = read_character(reader, ref, (uint8_t)c);
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

static SerdStatus
read_String(SerdReader* const reader, SerdNode* const node)
{
  SerdStatus st = SERD_SUCCESS;

  const int q1 = peek_byte(reader);

  TRY(st, skip_byte(reader, q1));
  const int q2 = peek_byte(reader);
  if (q2 != q1) { // Short string (not triple quoted)
    return read_STRING_LITERAL(reader, node, (uint8_t)q1);
  }

  TRY(st, skip_byte(reader, q2));
  const int q3 = peek_byte(reader);
  if (q3 != q1) { // Empty short string ("" or '')
    return SERD_SUCCESS;
  }

  // Long string
  TRY(st, skip_byte(reader, q3));
  node->flags |= SERD_IS_LONG;

  return read_STRING_LITERAL_LONG(reader, node, (uint8_t)q1);
}

static SerdStatus
read_PERCENT(SerdReader* const reader, SerdNode* const dest)
{
  SerdStatus st        = SERD_SUCCESS;
  uint8_t    digits[2] = {'\0', '\0'};

  TRY(st, eat_push_byte(reader, dest, '%'));
  TRY(st, read_HEX(reader, &digits[0]));
  TRY(st, read_HEX(reader, &digits[1]));
  TRY(st, push_byte(reader, dest, digits[0]));
  TRY(st, push_byte(reader, dest, digits[1]));

  return st;
}

static SerdStatus
read_PN_LOCAL_ESC(SerdReader* const reader, SerdNode* const dest)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, skip_byte(reader, '\\'));

  const int c = peek_byte(reader);

  return is_PN_LOCAL_ESC(c) ? eat_push_byte(reader, dest, c)
                            : r_err(reader, SERD_BAD_SYNTAX, "invalid escape");
}

static SerdStatus
read_PLX(SerdReader* const reader, SerdNode* const dest)
{
  const int c = peek_byte(reader);

  return (c == '%')    ? read_PERCENT(reader, dest)
         : (c == '\\') ? read_PN_LOCAL_ESC(reader, dest)
                       : SERD_FAILURE;
}

static SerdStatus
read_PN_LOCAL(SerdReader* const reader,
              SerdNode* const   dest,
              bool* const       ate_dot)
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
    st = eat_push_byte(reader, dest, c);
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
      st = eat_push_byte(reader, dest, c);
    } else if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
      return r_err(reader, st, "bad escape");
    } else if (st != SERD_SUCCESS && (st = read_PN_CHARS(reader, dest))) {
      break;
    }
    trailing_unescaped_dot = (c == '.');
  }

  if (trailing_unescaped_dot) {
    // Ate trailing dot, pop it from stack/node and inform caller
    serd_node_buffer(dest)[--dest->length] = '\0';
    serd_stack_pop(&reader->stack, 1);
    *ate_dot = true;
  }

  return accept_failure(st);
}

// Read the remainder of a PN_PREFIX after some initial characters
static SerdStatus
read_PN_PREFIX_tail(SerdReader* const reader, SerdNode* const dest)
{
  SerdStatus st = SERD_SUCCESS;
  int        c  = 0;
  while ((c = peek_byte(reader)) > 0) { // Middle: (PN_CHARS | '.')*
    if (c == '.') {
      st = eat_push_byte(reader, dest, c);
    } else if ((st = read_PN_CHARS(reader, dest))) {
      break;
    }
  }

  if (st <= SERD_FAILURE &&
      serd_node_string(dest)[serd_node_length(dest) - 1] == '.') {
    if ((st = read_PN_CHARS(reader, dest))) {
      return r_err(reader,
                   st > SERD_FAILURE ? st : SERD_BAD_SYNTAX,
                   "prefix ends with '.'");
    }
  }

  return st;
}

static SerdStatus
read_PN_PREFIX(SerdReader* const reader, SerdNode* const dest)
{
  const SerdStatus st = read_PN_CHARS_BASE(reader, dest);

  return st ? st : read_PN_PREFIX_tail(reader, dest);
}

typedef struct {
  SerdReader* reader;
  SerdNode*   node;
  SerdStatus  status;
} WriteNodeContext;

static size_t
write_to_stack(const void* const ZIX_NONNULL buf,
               const size_t                  size,
               const size_t                  nmemb,
               void* const ZIX_NONNULL       stream)
{
  WriteNodeContext* const ctx  = (WriteNodeContext*)stream;
  const uint8_t* const    utf8 = (const uint8_t*)buf;

  if (!ctx->status) {
    ctx->status = push_bytes(ctx->reader, ctx->node, utf8, nmemb * size);
  }

  return nmemb;
}

static SerdStatus
resolve_IRIREF(SerdReader* const reader,
               SerdNode* const   dest,
               const size_t      string_start_offset)
{
  // If the URI is already absolute, we don't need to do anything
  if (serd_uri_string_has_scheme(serd_node_string(dest))) {
    return SERD_SUCCESS;
  }

  // Parse the URI reference so we can resolve it
  SerdURIView uri = serd_parse_uri(serd_node_string(dest));

  // Resolve relative URI reference to a full URI
  uri = serd_resolve_uri(uri, serd_env_base_uri_view(reader->env));
  if (!uri.scheme.length) {
    return r_err(reader,
                 SERD_BAD_SYNTAX,
                 "failed to resolve relative URI reference <%s>",
                 serd_node_string(dest));
  }

  // Push a new temporary node for constructing the resolved URI
  SerdNode* const temp = push_node(reader, SERD_URI, "", 0);
  if (!temp) {
    return SERD_BAD_STACK;
  }

  // Write resolved URI to the temporary node
  WriteNodeContext ctx = {reader, temp, SERD_SUCCESS};
  temp->length         = serd_write_uri(uri, write_to_stack, &ctx);
  if (!ctx.status) {
    // Replace the destination with the new expanded node
    memmove(dest, temp, serd_node_total_size(temp));
    serd_stack_pop_to(&reader->stack, string_start_offset + dest->length);
    push_node_termination(reader);
  }

  return ctx.status;
}

static SerdStatus
read_IRIREF(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, eat_byte_check(reader, '<'));

  if (!(*dest = push_node(reader, SERD_URI, "", 0))) {
    return SERD_BAD_STACK;
  }

  const size_t string_start_offset = reader->stack.size;

  st = read_IRIREF_suffix(reader, *dest);

  return !tolerate_status(reader, st) ? st
         : (reader->flags & SERD_READ_RELATIVE)
           ? SERD_SUCCESS
           : resolve_IRIREF(reader, *dest, string_start_offset);
}

static SerdStatus
read_PrefixedName(SerdReader* const reader,
                  SerdNode* const   dest,
                  const bool        read_prefix,
                  bool* const       ate_dot,
                  const size_t      string_start_offset)
{
  SerdStatus st = SERD_SUCCESS;
  if (read_prefix) {
    TRY_LAX(st, read_PN_PREFIX(reader, dest));
  }

  if (peek_byte(reader) != ':') {
    return SERD_FAILURE;
  }

  TRY(st, skip_byte(reader, ':'));

  if ((reader->flags & SERD_READ_PREFIXED)) {
    dest->type = SERD_CURIE;
    TRY(st, push_byte(reader, dest, ':'));
  } else {
    // Search environment for the prefix URI
    const ZixStringView name = serd_node_string_view(dest);
    const ZixStringView uri  = serd_env_get_prefix(reader->env, name);
    if (!uri.length) {
      return r_err(reader, st, "unknown prefix \"%s\"", name.data);
    }

    // Pop back to the start of the string and replace it
    serd_stack_pop_to(&reader->stack, string_start_offset);
    serd_node_set_header(dest, 0U, 0U, SERD_URI);
    TRY(st, push_bytes(reader, dest, (const uint8_t*)uri.data, uri.length));
  }

  if ((st = read_PN_LOCAL(reader, dest, ate_dot)) > SERD_FAILURE) {
    return st;
  }

  return push_node_termination(reader);
}

static SerdStatus
read_0_9(SerdReader* const reader, SerdNode* const str, const bool at_least_one)
{
  unsigned   count = 0;
  SerdStatus st    = SERD_SUCCESS;
  for (int c = 0; is_digit((c = peek_byte(reader))); ++count) {
    TRY(st, eat_push_byte(reader, str, c));
  }

  if (at_least_one && count == 0) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected digit");
  }

  return st;
}

static SerdStatus
read_number(SerdReader* const reader,
            SerdNode** const  dest,
            bool* const       ate_dot)
{
#define XSD_DECIMAL NS_XSD "decimal"
#define XSD_DOUBLE NS_XSD "double"
#define XSD_INTEGER NS_XSD "integer"

  if (!(*dest = push_node(reader, SERD_LITERAL, "", 0))) {
    return SERD_BAD_STACK;
  }

  SerdStatus st          = SERD_SUCCESS;
  int        c           = peek_byte(reader);
  bool       has_decimal = false;

  if (c == '-' || c == '+') {
    TRY(st, eat_push_byte(reader, *dest, c));
  }

  if ((c = peek_byte(reader)) == '.') {
    has_decimal = true;
    // decimal case 2 (e.g. ".0" or "-.0" or "+.0")
    TRY(st, eat_push_byte(reader, *dest, c));
    TRY(st, read_0_9(reader, *dest, true));
  } else {
    // all other cases ::= ( '-' | '+' ) [0-9]+ ( . )? ( [0-9]+ )? ...
    TRY(st, read_0_9(reader, *dest, true));
    if ((c = peek_byte(reader)) == '.') {
      has_decimal = true;

      // Annoyingly, dot can be end of statement, so tentatively eat
      TRY(st, skip_byte(reader, c));
      c = peek_byte(reader);
      if (!is_digit(c) && c != 'e' && c != 'E') {
        *ate_dot = true;     // Force caller to deal with stupid grammar
        return SERD_SUCCESS; // Next byte is not a number character
      }

      TRY(st, push_byte(reader, *dest, '.'));
      read_0_9(reader, *dest, false);
    }
  }

  SerdNode* meta = NULL;

  c = peek_byte(reader);
  if (c == 'e' || c == 'E') {
    // double
    TRY(st, eat_push_byte(reader, *dest, c));
    c = peek_byte(reader);
    if (c == '+' || c == '-') {
      TRY(st, eat_push_byte(reader, *dest, c));
    }
    TRY(st, read_0_9(reader, *dest, true));
    meta = push_node(reader, SERD_URI, XSD_DOUBLE, sizeof(XSD_DOUBLE) - 1);
    (*dest)->flags |= SERD_HAS_DATATYPE;
  } else if (has_decimal) {
    meta = push_node(reader, SERD_URI, XSD_DECIMAL, sizeof(XSD_DECIMAL) - 1);
    (*dest)->flags |= SERD_HAS_DATATYPE;
  } else {
    meta = push_node(reader, SERD_URI, XSD_INTEGER, sizeof(XSD_INTEGER) - 1);
  }

  (*dest)->meta = meta;
  (*dest)->flags |= SERD_HAS_DATATYPE;
  return meta ? SERD_SUCCESS : SERD_BAD_STACK;
}

SerdStatus
read_turtle_iri(SerdReader* const reader,
                SerdNode** const  dest,
                bool* const       ate_dot)
{
  if (peek_byte(reader) == '<') {
    return read_IRIREF(reader, dest);
  }

  if (!(*dest = push_node(reader, SERD_CURIE, "", 0))) {
    return SERD_BAD_STACK;
  }

  return read_PrefixedName(reader, *dest, true, ate_dot, reader->stack.size);
}

static SerdStatus
read_literal(SerdReader* const reader,
             SerdNode** const  dest,
             bool* const       ate_dot)
{
  SerdStatus st = SERD_SUCCESS;

  if (!(*dest = push_node(reader, SERD_LITERAL, "", 0))) {
    return SERD_BAD_STACK;
  }

  if ((st = read_String(reader, *dest))) {
    return st;
  }

  SerdNode* datatype = NULL;
  SerdNode* lang     = NULL;
  const int next     = peek_byte(reader);
  if (next == '@') {
    TRY(st, skip_byte(reader, '@'));
    TRY(st, read_LANGTAG(reader, &lang));
    (*dest)->meta = lang;
    (*dest)->flags |= SERD_HAS_LANGUAGE;
  } else if (next == '^') {
    TRY(st, skip_byte(reader, '^'));
    TRY(st, eat_byte_check(reader, '^'));
    TRY(st, read_turtle_iri(reader, &datatype, ate_dot));
    (*dest)->meta = datatype;
    (*dest)->flags |= SERD_HAS_DATATYPE;
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_verb(SerdReader* reader, SerdNode** const dest)
{
  const size_t orig_stack_size = reader->stack.size;
  const int    first           = peek_byte(reader);

  if (first == '$' || first == '?') {
    return read_Var(reader, dest);
  }

  if (first == '<') {
    return read_IRIREF(reader, dest);
  }

  /* Either a qname, or "a".  Read the prefix first, and if it is in fact
     "a", produce that instead.
  */
  if (!(*dest = push_node(reader, SERD_CURIE, "", 0))) {
    return SERD_BAD_STACK;
  }

  const size_t curie_offset = reader->stack.size;

  SerdStatus st = SERD_SUCCESS;
  TRY_LAX(st, read_PN_PREFIX(reader, *dest));

  bool      ate_dot = false;
  SerdNode* node    = *dest;
  const int next    = peek_byte(reader);
  if (node->length == 1 && serd_node_string(node)[0] == 'a' && next != ':' &&
      !is_PN_CHARS_BASE(next)) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);
    *dest = reader->rdf_type;
    return SERD_SUCCESS;
  }

  if ((st = read_PrefixedName(reader, *dest, false, &ate_dot, curie_offset)) ||
      ate_dot) {
    *dest = NULL;
    return r_err(
      reader, st > SERD_FAILURE ? st : SERD_BAD_SYNTAX, "expected verb");
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_anon(SerdReader* const reader,
          ReadContext       ctx,
          const bool        subject,
          SerdNode** const  dest)
{
  const SerdStatementEventFlags old_flags = *ctx.flags;
  SerdStatus                    st        = SERD_SUCCESS;

  TRY(st, skip_byte(reader, '['));
  TRY(st, read_turtle_ws_star(reader));

  const bool empty = peek_byte(reader) == ']';
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
  if (ctx.subject) {
    TRY(st, emit_statement(reader, ctx, *dest));
  }

  // Switch the subject to the anonymous node and read its description
  ctx.subject = *dest;
  if (!empty) {
    bool ate_dot_in_list = false;
    TRY(st, read_predicateObjectList(reader, ctx, &ate_dot_in_list));

    if (ate_dot_in_list) {
      return r_err(reader, SERD_BAD_SYNTAX, "'.' inside blank");
    }

    TRY(st, read_turtle_ws_star(reader));
    *ctx.flags = old_flags;
    st         = serd_sink_write_end(reader->sink, *dest);
  }

  return st > SERD_FAILURE ? st : eat_byte_check(reader, ']');
}

static bool
node_has_string(const SerdNode* const node, const ZixStringView string)
{
  return node->length == string.length &&
         !memcmp(serd_node_string(node), string.data, string.length);
}

// Read a "named" object: a boolean literal or a prefixed name
static SerdStatus
read_named_object(SerdReader* const reader,
                  SerdNode** const  dest,
                  bool* const       ate_dot)
{
  static const char* const   XSD_BOOLEAN     = NS_XSD "boolean";
  static const size_t        XSD_BOOLEAN_LEN = 40;
  static const ZixStringView true_string     = ZIX_STATIC_STRING("true");
  static const ZixStringView false_string    = ZIX_STATIC_STRING("false");

  /* This function deals with nodes that start with some letters.  Unlike
     everything else, the cases here aren't nicely distinguished by leading
     characters, so this is more tedious to deal with in a non-tokenizing
     parser like this one.

     Deal with this here by trying to read a prefixed node, then if it turns
     out to actually be "true" or "false", switch it to a boolean literal. */

  if (!(*dest = push_node(reader, SERD_CURIE, "", 0))) {
    return SERD_BAD_STACK;
  }

  SerdNode*  node = *dest;
  SerdStatus st   = SERD_SUCCESS;

  // Attempt to read a prefixed name
  st = read_PrefixedName(reader, node, true, ate_dot, reader->stack.size);

  // Check if this is actually a special boolean node
  if (st == SERD_FAILURE && (node_has_string(node, true_string) ||
                             node_has_string(node, false_string))) {
    node->flags = SERD_HAS_DATATYPE;
    node->type  = SERD_LITERAL;
    node->meta  = push_node(reader, SERD_URI, XSD_BOOLEAN, XSD_BOOLEAN_LEN);
    return node->meta ? SERD_SUCCESS : SERD_BAD_STACK;
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
read_object(SerdReader* const  reader,
            ReadContext* const ctx,
            bool* const        ate_dot)
{
  const size_t  orig_stack_size = reader->stack.size;
  SerdCaretView orig_caret      = reader->source->caret;

  assert(ctx->subject);

  SerdStatus st     = SERD_FAILURE;
  bool       simple = true;
  SerdNode*  o      = 0;
  const int  c      = peek_byte(reader);

  switch (c) {
  case EOF:
  case ')':
    return r_err(reader, SERD_BAD_SYNTAX, "expected object");
  case '$':
  case '?':
    st = read_Var(reader, &o);
    break;
  case '[':
    simple = false;
    st     = read_anon(reader, *ctx, false, &o);
    break;
  case '(':
    simple = false;
    st     = read_collection(reader, *ctx, &o);
    break;
  case '_':
    st = read_BLANK_NODE_LABEL(reader, &o, ate_dot);
    break;
  case '<':
    st = read_IRIREF(reader, &o);
    break;
  case ':':
    st = read_turtle_iri(reader, &o, ate_dot);
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
    st = read_number(reader, &o, ate_dot);
    break;
  case '\"':
  case '\'':
    ++orig_caret.column;
    st = read_literal(reader, &o, ate_dot);
    break;
  default:
    // Either a boolean literal or a prefixed name
    st = read_named_object(reader, &o, ate_dot);
  }

  if (!st && simple && o) {
    st = emit_statement_at(reader, *ctx, o, orig_caret);
  }

  serd_stack_pop_to(&reader->stack, orig_stack_size);
#ifndef NDEBUG
  assert(reader->stack.size == orig_stack_size);
#endif
  return st;
}

static SerdStatus
read_objectList(SerdReader* const reader, ReadContext ctx, bool* const ate_dot)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, read_object(reader, &ctx, ate_dot));

  while (!*ate_dot && !(st = eat_delim(reader, ','))) {
    TRY_FAILING(st, read_object(reader, &ctx, ate_dot));
  }

  return accept_failure(st);
}

static SerdStatus
read_predicateObjectList(SerdReader* const reader,
                         ReadContext       ctx,
                         bool* const       ate_dot)
{
  const size_t orig_stack_size = reader->stack.size;

  SerdStatus st = SERD_SUCCESS;
  while (!st && !(st = read_verb(reader, &ctx.predicate)) &&
         !(st = read_turtle_ws_star(reader)) &&
         !(st = read_objectList(reader, ctx, ate_dot)) && !*ate_dot) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);

    bool ate_semi = false;
    while (!(st = read_turtle_ws_star(reader))) {
      const int c = peek_byte(reader);
      if (c == '.' || c == ']' || c == '}') {
        return SERD_SUCCESS;
      }

      if (c != ';') {
        break;
      }

      TRY(st, skip_byte(reader, c));
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
read_collection(SerdReader* const reader,
                ReadContext       ctx,
                SerdNode** const  dest)
{
  SerdStatus st = SERD_SUCCESS;

  TRY(st, skip_byte(reader, '('));
  TRY(st, read_turtle_ws_star(reader));

  bool end = peek_byte(reader) == ')';
  if (!(*dest = end ? reader->rdf_nil : blank_id(reader))) {
    return SERD_BAD_STACK;
  }

  if (ctx.subject) { // Reading a collection object
    *ctx.flags |= (end ? 0 : SERD_LIST_O);
    TRY(st, emit_statement(reader, ctx, *dest));
    *ctx.flags &= ~((unsigned)SERD_LIST_O);
  } else { // Reading a collection subject
    *ctx.flags |= (end ? 0 : SERD_LIST_S);
  }

  if (end) {
    return end_collection(reader, st);
  }

  /* The order of node allocation here is necessarily not in stack order,
     so we create two nodes and recycle them throughout. */
  SerdNode* n1 =
    push_node_padded(reader, genid_length(reader), SERD_BLANK, "", 0);

  SerdNode* node = n1;
  SerdNode* rest = 0;

  if (!n1) {
    return SERD_BAD_STACK;
  }

  ctx.subject = *dest;
  while (peek_byte(reader) != ')') {
    // _:node rdf:first object
    ctx.predicate = reader->rdf_first;
    bool ate_dot  = false;
    if ((st = read_object(reader, &ctx, &ate_dot)) || ate_dot) {
      return end_collection(reader, st);
    }

    TRY(st, read_turtle_ws_star(reader));
    if (!(end = (peek_byte(reader) == ')'))) {
      /* Give rest a new ID.  Done as late as possible to ensure it is
         used and > IDs generated by read_object above. */
      if (!rest) {
        rest = blank_id(reader); // First pass, push
        assert(rest);            // Can't overflow since read_object() popped
      } else {
        set_blank_id(reader, rest, genid_length(reader) + 1U);
      }
    }

    // _:node rdf:rest _:rest
    ctx.predicate = reader->rdf_rest;
    TRY(st, emit_statement(reader, ctx, (end ? reader->rdf_nil : rest)));

    ctx.subject = rest;        // _:node = _:rest
    rest        = node;        // _:rest = (old)_:node
    node        = ctx.subject; // invariant
  }

  return end_collection(reader, st);
}

SerdStatus
read_turtle_subject(SerdReader* const reader,
                    ReadContext       ctx,
                    SerdNode** const  dest,
                    int* const        s_type)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;
  switch ((*s_type = peek_byte(reader))) {
  case '$':
  case '?':
    st = read_Var(reader, dest);
    break;
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

  SerdStatus st = SERD_SUCCESS;
  TRY(st, read_turtle_ws_star(reader));

  const int c = peek_byte(reader);
  if (c == '.') {
    *ate_dot = !skip_byte(reader, c);
    return SERD_FAILURE;
  }

  if (c == '}') {
    return SERD_FAILURE;
  }

  st = read_predicateObjectList(reader, ctx, ate_dot);

  ctx.subject = ctx.predicate = 0;
  return accept_failure(st);
}

SerdStatus
read_turtle_base(SerdReader* const reader, const bool sparql, const bool token)
{
  SerdStatus st = SERD_SUCCESS;
  if (token) {
    TRY(st, eat_string(reader, "base", 4));
  }

  TRY(st, read_turtle_ws_star(reader));

  SerdNode* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));
  TRY(st, push_node_termination(reader));
  TRY(st, serd_sink_write_base(reader->sink, uri));

  TRY(st, read_turtle_ws_star(reader));
  if (!sparql) {
    return eat_byte_check(reader, '.');
  }

  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_BAD_SYNTAX, "full stop after SPARQL BASE");
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

  TRY(st, read_turtle_ws_star(reader));
  SerdNode* name = push_node(reader, SERD_LITERAL, "", 0);
  if (!name) {
    return SERD_BAD_STACK;
  }

  TRY_LAX(st, read_PN_PREFIX(reader, name));
  TRY(st, push_node_termination(reader));

  TRY(st, eat_byte_check(reader, ':'));
  TRY(st, read_turtle_ws_star(reader));

  SerdNode* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));
  TRY(st, push_node_termination(reader));

  if (!(st = serd_sink_write_prefix(reader->sink, name, uri))) {
    if (!sparql) {
      TRY(st, read_turtle_ws_star(reader));
      st = eat_byte_check(reader, '.');
    }
  }

  return st;
}

SerdStatus
read_turtle_directive(SerdReader* const reader)
{
  SerdStatus st = SERD_SUCCESS;
  TRY(st, skip_byte(reader, '@'));

  const int c = peek_byte(reader);
  return (c == 'b') ? read_turtle_base(reader, false, true)
         : (c == 'p')
           ? read_turtle_prefixID(reader, false, true)
           : r_err(reader, SERD_BAD_SYNTAX, "expected \"base\" or \"prefix\"");
}

static SerdStatus
read_sparql_directive(SerdReader* const reader, const SerdNode* const token)
{
  if (token_equals(token, "base", 4)) {
    return read_turtle_base(reader, true, false);
  }

  if (token_equals(token, "prefix", 6)) {
    return read_turtle_prefixID(reader, true, false);
  }

  return SERD_FAILURE;
}

static SerdStatus
read_block(SerdReader* const reader, ReadContext* const ctx)
{
  SerdStatus st = SERD_SUCCESS;

  // Try to read a subject, though it may actually be a directive or graph name
  SerdNode* token  = NULL;
  int       s_type = 0;
  TRY_LAX(st, read_turtle_subject(reader, *ctx, &token, &s_type));

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
  TRY_LAX(st, read_turtle_triples(reader, *ctx, &ate_dot));

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
  SerdStatementEventFlags flags = 0U;
  ReadContext             ctx   = {0, 0, 0, 0, &flags};
  SerdStatus              st    = SERD_SUCCESS;

  // Read first character and handle simple cases based on it
  TRY(st, read_turtle_ws_star(reader));
  const int c = peek_byte(reader);
  if (c < 0) {
    return SERD_FAILURE;
  }

  if (c == '\0') {
    TRY(st, skip_byte(reader, c));
    return SERD_FAILURE;
  }

  if (c == '@') {
    return read_turtle_directive(reader);
  }

  // No such luck, figure out what to read from the first token
  return read_block(reader, &ctx);
}

SerdStatus
read_turtleDoc(SerdReader* const reader)
{
  while (!reader->source->eof) {
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
