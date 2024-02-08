// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_NTRIPLES_H
#define SERD_SRC_READ_NTRIPLES_H

#include "serd/node.h"
#include "serd/reader.h"
#include "serd/status.h"
#include "zix/attributes.h"

#include <stdbool.h>
#include <stdint.h>

// Utilities

/**
   Read one (possibly multi-byte) character.

   The caller must have already eaten the first byte, `c`.
*/
ZIX_NODISCARD SerdStatus
read_character(SerdReader* reader, SerdNode* dest, uint8_t c);

/**
   Read one string literal escape.

   The caller must have already eaten the first byte, a backslash.
*/
ZIX_NODISCARD SerdStatus
read_string_escape(SerdReader* reader, SerdNode* ref);

// Terminals

/**
   Read a language tag starting after the '@'.

   RDF 1.1 NTriples: [144s] LANGTAG
*/
ZIX_NODISCARD SerdStatus
read_LANGTAG(SerdReader* reader, SerdNode** dest);

/**
   Read an end of line.

   RDF 1.1 NTriples: [7] EOL
*/
ZIX_NODISCARD SerdStatus
read_EOL(SerdReader* reader);

/**
   Read an IRI reference suffix into an existing node.

   RDF 1.1 NTriples: [8] IRIREF
*/
ZIX_NODISCARD SerdStatus
read_IRIREF_suffix(SerdReader* reader, SerdNode* node);

/**
   Read a string that is single-quoted with the given character.

   RDF 1.1 NTriples: [9]  STRING_LITERAL_QUOTE
   RDF 1.1 Turtle:   [23] STRING_LITERAL_SINGLE_QUOTE
*/
ZIX_NODISCARD SerdStatus
read_STRING_LITERAL(SerdReader* reader, SerdNode* ref, uint8_t q);

/**
   Read a blank node label that comes after "_:".

   RDF 1.1 NTriples: [141s] BLANK_NODE_LABEL
*/
ZIX_NODISCARD SerdStatus
read_BLANK_NODE_LABEL(SerdReader* reader, SerdNode** dest, bool* ate_dot);

/**
   Read an escape like "u201C", starting after the initial backslash.

   RDF 1.1 NTriples: [10] UCHAR
*/
ZIX_NODISCARD SerdStatus
read_UCHAR(SerdReader* reader, SerdNode* node, uint32_t* code_point);

/**
   Read an escape like "n", starting after the initial backslash.

   RDF 1.1 NTriples: [153s] ECHAR
*/
ZIX_NODISCARD SerdStatus
read_ECHAR(SerdReader* reader, SerdNode* dest);

/**
   Read a basic prefixed name character.

   RDF 1.1 NTriples: [157s] PN_CHARS_BASE
*/
ZIX_NODISCARD SerdStatus
read_PN_CHARS_BASE(SerdReader* reader, SerdNode* dest);

/**
   Read any prefixed name character.

   RDF 1.1 NTriples: [160s] PN_CHARS
*/
ZIX_NODISCARD SerdStatus
read_PN_CHARS(SerdReader* reader, SerdNode* dest);

/**
   Read a single hexadecimal digit.

   RDF 1.1 NTriples: [162s] HEX
*/
ZIX_NODISCARD SerdStatus
read_HEX(SerdReader* reader, uint8_t* dest);

// Nonterminals

/**
   Read a comment that starts with '#' and ends with the line.

   Not described by a rule in the grammar since RDF 1.1.
*/
ZIX_NODISCARD SerdStatus
read_comment(SerdReader* reader);

/**
   Read a subject (IRI or blank).

   RDF 1.1 NTriples: [3] subject
*/
ZIX_NODISCARD SerdStatus
read_nt_subject(SerdReader* reader, SerdNode** dest, bool* ate_dot);

/**
   Read a predicate (IRI).

   RDF 1.1 NTriples: [4] predicate
*/
ZIX_NODISCARD SerdStatus
read_nt_predicate(SerdReader* reader, SerdNode** dest);

/**
   Read an object (IRI or blank or literal).

   RDF 1.1 NTriples: [5] object
*/
ZIX_NODISCARD SerdStatus
read_nt_object(SerdReader* reader, SerdNode** dest, bool* ate_dot);

/**
   Read a variable that starts with '?' or '$'.

   This is an extension that serd uses in certain contexts to support
   patterns.

   Restricted version of SPARQL 1.1: [108] Var
*/
ZIX_NODISCARD SerdStatus
read_Var(SerdReader* reader, SerdNode** dest);

/**
   Read a single NTriples line.

   May read a statement, but may also just skip some input like comments or
   extra whitespace.
*/
SerdStatus
read_ntriples_line(SerdReader* reader);

#endif // SERD_SRC_READ_NTRIPLES_H
