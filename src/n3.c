// Copyright 2011-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "byte_source.h"
#include "env.h"
#include "namespaces.h"
#include "node.h"
#include "ntriples.h"
#include "read_ntriples.h"
#include "reader.h"
#include "stack.h"
#include "string_utils.h"
#include "try.h"
#include "turtle.h"

#include "serd/attributes.h"
#include "serd/caret.h"
#include "serd/env.h"
#include "serd/node.h"
#include "serd/reader.h"
#include "serd/sink.h"
#include "serd/statement.h"
#include "serd/status.h"
#include "serd/string.h"
#include "serd/syntax.h"
#include "serd/uri.h"
#include "zix/string_view.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SerdStatus
read_collection(SerdReader* reader, ReadContext ctx, SerdNode** dest);

static SerdStatus
read_predicateObjectList(SerdReader* reader, ReadContext ctx, bool* ate_dot);

// whitespace ::= #x9 | #xA | #xD | #x20 | comment
static SerdStatus
read_whitespace(SerdReader* const reader)
{
  switch (peek_byte(reader)) {
  case '\t':
  case '\n':
  case '\r':
  case ' ':
    return serd_byte_source_advance(reader->source);
  case '#':
    return read_comment(reader);
  default:
    break;
  }

  return SERD_FAILURE;
}

static bool
read_ws_star(SerdReader* const reader)
{
  while (!read_whitespace(reader)) {
  }

  return true;
}

static bool
peek_delim(SerdReader* const reader, const uint8_t delim)
{
  read_ws_star(reader);
  return peek_byte(reader) == delim;
}

static bool
eat_delim(SerdReader* const reader, const uint8_t delim)
{
  if (peek_delim(reader, delim)) {
    skip_byte(reader, delim);
    return read_ws_star(reader);
  }

  return false;
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
      skip_byte(reader, c);
      uint32_t code = 0;
      if ((st = read_ECHAR(reader, ref)) &&
          (st = read_UCHAR(reader, ref, &code))) {
        return r_err(reader, st, "invalid escape '\\%c'", peek_byte(reader));
      }
    } else if (c == EOF) {
      st = r_err(reader, SERD_NO_DATA, "unexpected end of file");
    } else if (c == q) {
      skip_byte(reader, q);
      const int q2 = eat_byte_safe(reader, peek_byte(reader));
      const int q3 = peek_byte(reader);
      if (q2 == q && q3 == q) { // End of string
        skip_byte(reader, q3);
        break;
      }

      if (!(st = push_byte(reader, ref, c))) {
        st = read_character(reader, ref, (uint8_t)q2);
      }
    } else {
      st = read_character(reader, ref, (uint8_t)eat_byte_safe(reader, c));
    }
  }

  return tolerate_status(reader, st) ? SERD_SUCCESS : st;
}

static SerdStatus
read_String(SerdReader* const reader, SerdNode* const node)
{
  const int q1 = eat_byte_safe(reader, peek_byte(reader));
  const int q2 = peek_byte(reader);
  if (q2 == EOF) {
    return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");
  }

  if (q2 != q1) { // Short string (not triple quoted)
    return read_STRING_LITERAL(reader, node, (uint8_t)q1);
  }

  skip_byte(reader, q2);
  const int q3 = peek_byte(reader);
  if (q3 == EOF) {
    return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");
  }

  if (q3 != q1) { // Empty short string ("" or '')
    return SERD_SUCCESS;
  }

  // Long string
  skip_byte(reader, q3);
  node->flags |= SERD_IS_LONG;

  return read_STRING_LITERAL_LONG(reader, node, (uint8_t)q1);
}

static SerdStatus
read_PERCENT(SerdReader* const reader, SerdNode* const dest)
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
read_PN_LOCAL_ESC(SerdReader* const reader, SerdNode* const dest)
{
  skip_byte(reader, '\\');

  const int c = peek_byte(reader);

  return is_PN_LOCAL_ESC(c) ? push_byte(reader, dest, eat_byte_safe(reader, c))
                            : r_err(reader, SERD_BAD_SYNTAX, "invalid escape");
}

static SerdStatus
read_PLX(SerdReader* const reader, SerdNode* const dest)
{
  const int c = peek_byte(reader);
  switch (c) {
  case '%':
    return read_PERCENT(reader, dest);
  case '\\':
    return read_PN_LOCAL_ESC(reader, dest);
  default:
    return SERD_FAILURE;
  }
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
    st = push_byte(reader, dest, eat_byte_safe(reader, c));
    break;
  default:
    if ((st = read_PLX(reader, dest)) > SERD_FAILURE) {
      return r_err(reader, st, "bad escape");
    } else if (st != SERD_SUCCESS && read_PN_CHARS_BASE(reader, dest)) {
      return SERD_FAILURE;
    }
  }

  while ((c = peek_byte(reader))) { // Middle: (PN_CHARS | '.' | ':')*
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
    --dest->length;
    serd_stack_pop(&reader->stack, 1);
    *ate_dot = true;
  }

  return (st > SERD_FAILURE) ? st : SERD_SUCCESS;
}

// Read the remainder of a PN_PREFIX after some initial characters
static SerdStatus
read_PN_PREFIX_tail(SerdReader* const reader, SerdNode* const dest)
{
  SerdStatus st = SERD_SUCCESS;
  int        c  = 0;
  while ((c = peek_byte(reader))) { // Middle: (PN_CHARS | '.')*
    if (c == '.') {
      st = push_byte(reader, dest, eat_byte_safe(reader, c));
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
  SerdStatus st = SERD_SUCCESS;

  if (!(st = read_PN_CHARS_BASE(reader, dest))) {
    return read_PN_PREFIX_tail(reader, dest);
  }

  return st;
}

typedef struct {
  SerdReader* reader;
  SerdNode*   node;
  SerdStatus  status;
} WriteNodeContext;

static size_t
write_to_stack(const void* const SERD_NONNULL buf,
               const size_t                   size,
               const size_t                   nmemb,
               void* const SERD_NONNULL       stream)
{
  WriteNodeContext* const ctx  = (WriteNodeContext*)stream;
  const uint8_t* const    utf8 = (const uint8_t*)buf;

  ctx->status = push_bytes(ctx->reader, ctx->node, utf8, nmemb * size);

  return nmemb;
}

static SerdStatus
resolve_IRIREF(SerdReader* const reader,
               SerdNode* const   dest,
               const size_t      string_start_offset)
{
  // If the URI is already absolute, we don't need to do anything
  SerdURIView uri = serd_parse_uri(serd_node_string(dest));
  if (uri.scheme.length) {
    return SERD_SUCCESS;
  }

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
    memmove(dest, temp, sizeof(SerdNode) + serd_node_pad_length(temp->length));
    serd_stack_pop_to(&reader->stack, string_start_offset + dest->length);
  }

  return ctx.status;
}

static SerdStatus
read_IRIREF(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st = SERD_SUCCESS;
  if ((st = eat_byte_check(reader, '<'))) {
    return st;
  }

  if (!(*dest = push_node(reader, SERD_URI, "", 0))) {
    return SERD_BAD_STACK;
  }

  const size_t string_start_offset = reader->stack.size;

  st = read_IRIREF_suffix(reader, *dest);
  if (!tolerate_status(reader, st)) {
    return st;
  }

  return (reader->flags & SERD_READ_RELATIVE)
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
  if (read_prefix && ((st = read_PN_PREFIX(reader, dest)) > SERD_FAILURE)) {
    return st;
  }

  if (peek_byte(reader) != ':') {
    return SERD_FAILURE;
  }

  if ((st = push_byte(reader, dest, eat_byte_safe(reader, ':'))) ||
      (st = read_PN_LOCAL(reader, dest, ate_dot)) > SERD_FAILURE) {
    return st;
  }

  // Expand to absolute URI
  const ZixStringView curie = serd_node_string_view(dest);
  ZixStringView       prefix;
  ZixStringView       suffix;
  if ((st = serd_env_expand_in_place(reader->env, curie, &prefix, &suffix))) {
    return r_err(
      reader, st, "failed to expand URI \"%s\"", serd_node_string(dest));
  }

  // Push a new temporary node for constructing the full URI
  SerdNode* const temp = push_node(reader, SERD_URI, "", 0);
  if ((st = push_bytes(
         reader, temp, (const uint8_t*)prefix.data, prefix.length)) ||
      (st = push_bytes(
         reader, temp, (const uint8_t*)suffix.data, suffix.length))) {
    return st;
  }

  // Replace the destination with the new expanded node
  const size_t total_size =
    sizeof(SerdNode) + serd_node_pad_length(temp->length);

  memmove(dest, temp, total_size);

  serd_stack_pop_to(&reader->stack,
                    string_start_offset + serd_node_length(dest));

  return SERD_SUCCESS;
}

static SerdStatus
read_0_9(SerdReader* const reader, SerdNode* const str, const bool at_least_one)
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

static SerdStatus
read_number(SerdReader* const reader,
            SerdNode** const  dest,
            bool* const       ate_dot)
{
#define XSD_DECIMAL NS_XSD "decimal"
#define XSD_DOUBLE NS_XSD "double"
#define XSD_INTEGER NS_XSD "integer"

  *dest = push_node(reader, SERD_LITERAL, "", 0);

  SerdStatus st          = SERD_SUCCESS;
  int        c           = peek_byte(reader);
  bool       has_decimal = false;
  if (!*dest) {
    return SERD_BAD_STACK;
  }

  if (c == '-' || c == '+') {
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
  }

  if ((c = peek_byte(reader)) == '.') {
    has_decimal = true;
    // decimal case 2 (e.g. ".0" or "-.0" or "+.0")
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    TRY(st, read_0_9(reader, *dest, true));
  } else {
    // all other cases ::= ( '-' | '+' ) [0-9]+ ( . )? ( [0-9]+ )? ...
    TRY(st, read_0_9(reader, *dest, true));
    if ((c = peek_byte(reader)) == '.') {
      has_decimal = true;

      // Annoyingly, dot can be end of statement, so tentatively eat
      skip_byte(reader, c);
      c = peek_byte(reader);
      if (!is_digit(c) && c != 'e' && c != 'E') {
        *ate_dot = true;     // Force caller to deal with stupid grammar
        return SERD_SUCCESS; // Next byte is not a number character
      }

      TRY(st, push_byte(reader, *dest, '.'));
      read_0_9(reader, *dest, false);
    }
  }
  c = peek_byte(reader);
  if (c == 'e' || c == 'E') {
    // double
    TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
    switch ((c = peek_byte(reader))) {
    case '+':
    case '-':
      TRY(st, push_byte(reader, *dest, eat_byte_safe(reader, c)));
      break;
    default:
      break;
    }
    TRY(st, read_0_9(reader, *dest, true));
    push_node(reader, SERD_URI, XSD_DOUBLE, sizeof(XSD_DOUBLE) - 1);
    (*dest)->flags |= SERD_HAS_DATATYPE;
  } else if (has_decimal) {
    push_node(reader, SERD_URI, XSD_DECIMAL, sizeof(XSD_DECIMAL) - 1);
    (*dest)->flags |= SERD_HAS_DATATYPE;
  } else {
    push_node(reader, SERD_URI, XSD_INTEGER, sizeof(XSD_INTEGER) - 1);
    (*dest)->flags |= SERD_HAS_DATATYPE;
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_iri(SerdReader* const reader, SerdNode** const dest, bool* const ate_dot)
{
  if (peek_byte(reader) == '<') {
    return read_IRIREF(reader, dest);
  }

  if (!(*dest = push_node(reader, SERD_LITERAL, "", 0))) {
    return SERD_BAD_STACK;
  }

  return read_PrefixedName(reader, *dest, true, ate_dot, reader->stack.size);
}

static SerdStatus
read_literal(SerdReader* const reader,
             SerdNode** const  dest,
             bool* const       ate_dot)
{
  if (!(*dest = push_node(reader, SERD_LITERAL, "", 0))) {
    return SERD_BAD_STACK;
  }

  SerdStatus st = read_String(reader, *dest);
  if (st) {
    return st;
  }

  SerdNode* datatype = NULL;
  switch (peek_byte(reader)) {
  case '@':
    skip_byte(reader, '@');
    (*dest)->flags |= SERD_HAS_LANGUAGE;
    TRY(st, read_LANGTAG(reader));
    break;
  case '^':
    skip_byte(reader, '^');
    TRY(st, eat_byte_check(reader, '^'));
    (*dest)->flags |= SERD_HAS_DATATYPE;
    TRY(st, read_iri(reader, &datatype, ate_dot));
    break;
  }
  return SERD_SUCCESS;
}

static SerdStatus
read_verb(SerdReader* reader, SerdNode** dest)
{
  const size_t orig_stack_size = reader->stack.size;

  switch (peek_byte(reader)) {
  case '$':
  case '?':
    return read_Var(reader, dest);
  case '<':
    return read_IRIREF(reader, dest);
  }

  /* Either a qname, or "a".  Read the prefix first, and if it is in fact
     "a", produce that instead.
  */
  if (!(*dest = push_node(reader, SERD_URI, "", 0))) {
    return SERD_BAD_STACK;
  }

  const size_t string_start_offset = reader->stack.size;
  SerdStatus   st                  = read_PN_PREFIX(reader, *dest);
  if (st > SERD_FAILURE) {
    return st;
  }

  bool      ate_dot = false;
  SerdNode* node    = *dest;
  const int next    = peek_byte(reader);
  if (node->length == 1 && serd_node_string(node)[0] == 'a' && next != ':' &&
      !is_PN_CHARS_BASE(next)) {
    serd_stack_pop_to(&reader->stack, orig_stack_size);
    *dest = reader->rdf_type;
    return SERD_SUCCESS;
  }

  if ((st = read_PrefixedName(
         reader, *dest, false, &ate_dot, string_start_offset)) ||
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
  skip_byte(reader, '[');

  const SerdStatementFlags old_flags = *ctx.flags;
  const bool               empty     = peek_delim(reader, ']');

  if (subject) {
    *ctx.flags |= empty ? SERD_EMPTY_S : SERD_ANON_S;
  } else {
    *ctx.flags |= SERD_ANON_O;
  }

  if (!*dest) {
    if (!(*dest = blank_id(reader))) {
      return SERD_BAD_STACK;
    }
  }

  // Emit statement with this anonymous object first
  SerdStatus st = SERD_SUCCESS;
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

    read_ws_star(reader);
    *ctx.flags = old_flags;
  }

  if (!(subject && empty)) {
    TRY(st, serd_sink_write_end(reader->sink, *dest));
  }

  return eat_byte_check(reader, ']');
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
  static const char* const XSD_BOOLEAN     = NS_XSD "boolean";
  static const size_t      XSD_BOOLEAN_LEN = 40;

  /* This function deals with nodes that start with some letters.  Unlike
     everything else, the cases here aren't nicely distinguished by leading
     characters, so this is more tedious to deal with in a non-tokenizing
     parser like this one.

     Deal with this here by trying to read a prefixed node, then if it turns
     out to actually be "true" or "false", switch it to a boolean literal. */

  if (!(*dest = push_node(reader, SERD_URI, "", 0))) {
    return SERD_BAD_STACK;
  }

  SerdNode*  node = *dest;
  SerdStatus st   = SERD_SUCCESS;

  // Attempt to read a prefixed name
  st = read_PrefixedName(reader, node, true, ate_dot, reader->stack.size);

  // Check if this is actually a special boolean node
  if (st == SERD_FAILURE && (node_has_string(node, zix_string("true")) ||
                             node_has_string(node, zix_string("false")))) {
    node->flags = SERD_HAS_DATATYPE;
    node->type  = SERD_LITERAL;
    return push_node(reader, SERD_URI, XSD_BOOLEAN, XSD_BOOLEAN_LEN)
             ? SERD_SUCCESS
             : SERD_BAD_STACK;
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
  const size_t orig_stack_size = reader->stack.size;
  SerdCaret    orig_caret      = reader->source->caret;

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
    st = read_iri(reader, &o, ate_dot);
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
    ++orig_caret.col;
    st = read_literal(reader, &o, ate_dot);
    break;
  default:
    // Either a boolean literal or a prefixed name
    st = read_named_object(reader, &o, ate_dot);
  }

  if (!st && simple && o) {
    st = emit_statement_at(reader, *ctx, o, &orig_caret);
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

  while (st <= SERD_FAILURE && !*ate_dot && eat_delim(reader, ',')) {
    st = read_object(reader, &ctx, ate_dot);
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
  while (!(st = read_verb(reader, &ctx.predicate)) && read_ws_star(reader) &&
         !(st = read_objectList(reader, ctx, ate_dot))) {
    if (*ate_dot) {
      serd_stack_pop_to(&reader->stack, orig_stack_size);
      return SERD_SUCCESS;
    }

    bool ate_semi = false;
    int  c        = 0;
    do {
      read_ws_star(reader);
      switch (c = peek_byte(reader)) {
      case EOF:
        serd_stack_pop_to(&reader->stack, orig_stack_size);
        return r_err(reader, SERD_BAD_SYNTAX, "unexpected end of file");
      case '.':
      case ']':
      case '}':
        serd_stack_pop_to(&reader->stack, orig_stack_size);
        return SERD_SUCCESS;
      case ';':
        skip_byte(reader, c);
        ate_semi = true;
      }
    } while (c == ';');

    if (!ate_semi) {
      serd_stack_pop_to(&reader->stack, orig_stack_size);
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

  skip_byte(reader, '(');

  bool end = peek_delim(reader, ')');
  *dest    = end ? reader->rdf_nil : blank_id(reader);

  if (!*dest) {
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
  while (!peek_delim(reader, ')')) {
    // _:node rdf:first object
    ctx.predicate = reader->rdf_first;
    bool ate_dot  = false;
    if ((st = read_object(reader, &ctx, &ate_dot)) || ate_dot) {
      return end_collection(reader, st);
    }

    if (!(end = peek_delim(reader, ')'))) {
      /* Give rest a new ID.  Done as late as possible to ensure it is
         used and > IDs generated by read_object above. */
      if (!rest) {
        rest = blank_id(reader); // First pass, push
        assert(rest);            // Can't overflow since read_object() popped
      } else {
        set_blank_id(reader, rest, genid_length(reader) + 1);
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

static SerdStatus
read_subject(SerdReader* const reader,
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
    if ((st = read_iri(reader, dest, &ate_dot))) {
      return st;
    }
  }

  if (ate_dot) {
    return r_err(reader, SERD_BAD_SYNTAX, "subject ends with '.'");
  }

  return st;
}

static SerdStatus
read_labelOrSubject(SerdReader* const reader, SerdNode** const dest)
{
  SerdStatus st      = SERD_SUCCESS;
  bool       ate_dot = false;

  switch (peek_byte(reader)) {
  case '[':
    skip_byte(reader, '[');
    read_ws_star(reader);
    if ((st = eat_byte_check(reader, ']'))) {
      return st;
    }
    *dest = blank_id(reader);
    return *dest ? SERD_SUCCESS : SERD_BAD_STACK;
  case '_':
    return read_BLANK_NODE_LABEL(reader, dest, &ate_dot);
  default:
    if (!read_iri(reader, dest, &ate_dot)) {
      return SERD_SUCCESS;
    } else {
      return r_err(reader, SERD_BAD_SYNTAX, "expected label or subject");
    }
  }
}

static SerdStatus
read_triples(SerdReader* const reader, ReadContext ctx, bool* const ate_dot)
{
  SerdStatus st = SERD_FAILURE;
  if (ctx.subject) {
    read_ws_star(reader);
    switch (peek_byte(reader)) {
    case '.':
      *ate_dot = eat_byte_safe(reader, '.');
      return SERD_FAILURE;
    case '}':
      return SERD_FAILURE;
    }
    st = read_predicateObjectList(reader, ctx, ate_dot);
  }

  ctx.subject = ctx.predicate = 0;
  return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static SerdStatus
read_base(SerdReader* const reader, const bool sparql, const bool token)
{
  SerdStatus st = SERD_SUCCESS;
  if (token) {
    TRY(st, eat_string(reader, "base", 4));
  }

  read_ws_star(reader);

  SerdNode* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));

  if (reader->stack.size + sizeof(SerdNode) > reader->stack.buf_size) {
    return SERD_BAD_STACK;
  }

  serd_node_zero_pad(uri);
  TRY(st, serd_env_set_base_uri(reader->env, serd_node_string_view(uri)));
  TRY(st, serd_sink_write_base(reader->sink, uri));

  read_ws_star(reader);
  if (!sparql) {
    return eat_byte_check(reader, '.');
  }

  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_BAD_SYNTAX, "full stop after SPARQL BASE");
  }

  return SERD_SUCCESS;
}

static SerdStatus
read_prefixID(SerdReader* const reader, const bool sparql, const bool token)
{
  SerdStatus st = SERD_SUCCESS;
  if (token) {
    TRY(st, eat_string(reader, "prefix", 6));
  }

  read_ws_star(reader);
  SerdNode* name = push_node(reader, SERD_LITERAL, "", 0);
  if (!name) {
    return SERD_BAD_STACK;
  }

  if ((st = read_PN_PREFIX(reader, name)) > SERD_FAILURE) {
    return st;
  }

  if ((st = eat_byte_check(reader, ':'))) {
    return st;
  }

  read_ws_star(reader);
  SerdNode* uri = NULL;
  TRY(st, read_IRIREF(reader, &uri));

  if (reader->stack.size + sizeof(SerdNode) > reader->stack.buf_size) {
    return SERD_BAD_STACK;
  }

  serd_node_zero_pad(name);
  serd_node_zero_pad(uri);

  TRY(st,
      serd_env_set_prefix(
        reader->env, serd_node_string_view(name), serd_node_string_view(uri)));

  st = serd_sink_write_prefix(reader->sink, name, uri);

  if (!sparql) {
    read_ws_star(reader);
    st = eat_byte_check(reader, '.');
  }

  return st;
}

static SerdStatus
read_wrappedGraph(SerdReader* const reader, ReadContext* const ctx)
{
  SerdStatus st = SERD_SUCCESS;
  if ((st = eat_byte_check(reader, '{'))) {
    return st;
  }

  read_ws_star(reader);
  while (peek_byte(reader) != '}') {
    const size_t orig_stack_size = reader->stack.size;
    bool         ate_dot         = false;
    int          s_type          = 0;

    ctx->subject = 0;
    if ((st = read_subject(reader, *ctx, &ctx->subject, &s_type))) {
      return r_err(reader, st, "expected subject");
    }

    if (read_triples(reader, *ctx, &ate_dot) && s_type != '[') {
      return r_err(reader, SERD_BAD_SYNTAX, "missing predicate object list");
    }

    serd_stack_pop_to(&reader->stack, orig_stack_size);
    read_ws_star(reader);
    if (peek_byte(reader) == '.') {
      skip_byte(reader, '.');
    }
    read_ws_star(reader);
  }

  skip_byte(reader, '}');
  read_ws_star(reader);
  if (peek_byte(reader) == '.') {
    return r_err(reader, SERD_BAD_SYNTAX, "graph followed by '.'");
  }

  return SERD_SUCCESS;
}

static int
tokcmp(const SerdNode* const node, const char* const tok, const size_t n)
{
  return ((!node || node->length != n)
            ? -1
            : serd_strncasecmp(serd_node_string(node), tok, n));
}

static SerdStatus
read_turtle_directive(SerdReader* const reader)
{
  skip_byte(reader, '@');

  switch (peek_byte(reader)) {
  case 'b':
    return read_base(reader, false, true);
  case 'p':
    return read_prefixID(reader, false, true);
  default:
    break;
  }

  return r_err(reader, SERD_BAD_SYNTAX, "expected \"base\" or \"prefix\"");
}

static SerdStatus
read_sparql_directive(SerdReader* const     reader,
                      ReadContext* const    ctx,
                      const SerdNode* const token)
{
  if (!tokcmp(token, "base", 4)) {
    return read_base(reader, true, false);
  }

  if (!tokcmp(token, "prefix", 6)) {
    return read_prefixID(reader, true, false);
  }

  if (!tokcmp(token, "graph", 5)) {
    SerdStatus st = SERD_SUCCESS;
    read_ws_star(reader);
    TRY(st, read_labelOrSubject(reader, &ctx->graph));
    read_ws_star(reader);
    return read_wrappedGraph(reader, ctx);
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
  if ((st = read_subject(reader, *ctx, &token, &s_type)) > SERD_FAILURE) {
    return st;
  }

  // Try to interpret as a SPARQL "PREFIX" or "BASE" directive
  if (st && (st = read_sparql_directive(reader, ctx, token)) != SERD_FAILURE) {
    return st;
  }

  // Try to interpret as a named TriG graph like "graphname { ..."
  read_ws_star(reader);
  if (peek_byte(reader) == '{') {
    if (s_type == '(' || (s_type == '[' && !*ctx->flags)) {
      return r_err(reader, SERD_BAD_SYNTAX, "invalid graph name");
    }

    ctx->graph = token;
    (*ctx->flags) |= (s_type == '[' ? SERD_EMPTY_G : 0U);
    return read_wrappedGraph(reader, ctx);
  }

  if (st) {
    return r_err(reader, SERD_BAD_SYNTAX, "expected directive or subject");
  }

  // Our token is really a subject, read some triples
  bool ate_dot = false;
  ctx->subject = token;
  if ((st = read_triples(reader, *ctx, &ate_dot)) > SERD_FAILURE) {
    return st;
  }

  // "Failure" is only allowed for anonymous subjects like "[ ... ] ."
  if (st && s_type != '[') {
    return r_err(reader, SERD_BAD_SYNTAX, "expected triples");
  }

  // Ensure that triples are properly terminated
  return ate_dot ? st : eat_byte_check(reader, '.');
}

SerdStatus
read_n3_statement(SerdReader* const reader)
{
  SerdStatementFlags flags = 0;
  ReadContext        ctx   = {0, 0, 0, 0, &flags};

  // Handle nice cases we can distinguish from the next byte
  read_ws_star(reader);
  switch (peek_byte(reader)) {
  case EOF:
    return SERD_FAILURE;

  case '\0':
    eat_byte(reader);
    return SERD_FAILURE;

  case '@':
    return read_turtle_directive(reader);

  case '{':
    return (reader->syntax == SERD_TRIG)
             ? read_wrappedGraph(reader, &ctx)
             : r_err(reader, SERD_BAD_SYNTAX, "syntax does not support graphs");

  default:
    break;
  }

  // No such luck, figure out what to read from the first token
  return read_block(reader, &ctx);
}

SerdStatus
read_turtleTrigDoc(SerdReader* const reader)
{
  while (!reader->source->eof) {
    const size_t     orig_stack_size = reader->stack.size;
    const SerdStatus st              = read_n3_statement(reader);

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
