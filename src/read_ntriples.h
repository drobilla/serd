// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_READ_NTRIPLES_H
#define SERD_SRC_READ_NTRIPLES_H

#include "serd/node.h"
#include "serd/reader.h"
#include "serd/status.h"

#include <stdbool.h>
#include <stdint.h>

// Utilities

static inline bool
codepoint_in_range(const uint32_t c, const uint32_t min, const uint32_t max)
{
  return c >= min && c <= max;
}

static inline bool
is_PN_CHARS_BASE(const uint32_t c)
{
  return (codepoint_in_range(c, 'A', 'Z') || codepoint_in_range(c, 'a', 'z') ||
          codepoint_in_range(c, 0x000C0U, 0x000D6U) ||
          codepoint_in_range(c, 0x000D8U, 0x000F6U) ||
          codepoint_in_range(c, 0x000F8U, 0x002FFU) ||
          codepoint_in_range(c, 0x00370U, 0x0037DU) ||
          codepoint_in_range(c, 0x0037FU, 0x01FFFU) ||
          codepoint_in_range(c, 0x0200CU, 0x0200DU) ||
          codepoint_in_range(c, 0x02070U, 0x0218FU) ||
          codepoint_in_range(c, 0x02C00U, 0x02FEFU) ||
          codepoint_in_range(c, 0x03001U, 0x0D7FFU) ||
          codepoint_in_range(c, 0x0F900U, 0x0FDCFU) ||
          codepoint_in_range(c, 0x0FDF0U, 0x0FFFDU) ||
          codepoint_in_range(c, 0x10000U, 0xEFFFFU));
}

/**
   Read one (possibly multi-byte) character.

   The caller must have already eaten the first byte, `c`.
*/
SerdStatus
read_character(SerdReader* reader, SerdNode* dest, uint8_t c);

// Terminals

/**
   Read a language tag starting after the '@'.

   RDF 1.1 NTriples: [144s] LANGTAG
*/
SerdStatus
read_LANGTAG(SerdReader* reader);

/**
   Read an end of line.

   RDF 1.1 NTriples: [7] EOL
*/
SerdStatus
read_EOL(SerdReader* reader);

/**
   Read an absolute IRI.

   This is a stricter subset of [8] IRIREF in the NTriples grammar, since a
   scheme is required.  Handling this in the parser results in better error
   messages.
*/
SerdStatus
read_IRI(SerdReader* reader, SerdNode** dest);

/**
   Read an IRI reference suffix into an existing node.

   RDF 1.1 NTriples: [8] IRIREF
*/
SerdStatus
read_IRIREF_suffix(SerdReader* reader, SerdNode* node);

/**
   Read a string that is single-quoted with the given character.

   RDF 1.1 NTriples: [9]  STRING_LITERAL_QUOTE
   RDF 1.1 Turtle:   [23] STRING_LITERAL_SINGLE_QUOTE
*/
SerdStatus
read_STRING_LITERAL(SerdReader* reader, SerdNode* ref, uint8_t q);

/**
   Read a blank node label that comes after "_:".

   RDF 1.1 NTriples: [141s] BLANK_NODE_LABEL
*/
SerdStatus
read_BLANK_NODE_LABEL(SerdReader* reader, SerdNode** dest, bool* ate_dot);

/**
   Read an escape like "u201C", starting after the initial backslash.

   RDF 1.1 NTriples: [10] UCHAR
*/
SerdStatus
read_UCHAR(SerdReader* reader, SerdNode* node, uint32_t* code_point);

/**
   Read an escape like "n", starting after the initial backslash.

   RDF 1.1 NTriples: [153s] ECHAR
*/
SerdStatus
read_ECHAR(SerdReader* reader, SerdNode* dest);

/**
   Read a basic prefixed name character.

   RDF 1.1 NTriples: [157s] PN_CHARS_BASE
*/
SerdStatus
read_PN_CHARS_BASE(SerdReader* reader, SerdNode* dest);

/**
   Read an initial prefixed name character.

   RDF 1.1 NTriples: [158s] PN_CHARS_U
*/
SerdStatus
read_PN_CHARS_U(SerdReader* reader, SerdNode* dest);

/**
   Read any prefixed name character.

   RDF 1.1 NTriples: [160s] PN_CHARS
*/
SerdStatus
read_PN_CHARS(SerdReader* reader, SerdNode* dest);

/**
   Read a single hexadecimal digit.

   RDF 1.1 NTriples: [162s] HEX
*/
uint8_t
read_HEX(SerdReader* reader);

/**
   Read a variable name, starting after the '?' or '$'.

   This is an extension that serd uses in certain contexts to support patterns.

   Restricted version of SPARQL 1.1: [166] VARNAME
*/
SerdStatus
read_VARNAME(SerdReader* reader, SerdNode** dest);

// Nonterminals

/**
   Read a comment that starts with '#' and ends with the line.

   Not described by a rule in the grammar since RDF 1.1.
*/
SerdStatus
read_comment(SerdReader* reader);

/**
   Read a subject (IRI or blank).

   RDF 1.1 NTriples: [3] subject
*/
SerdStatus
read_nt_subject(SerdReader* reader, SerdNode** dest);

/**
   Read a predicate (IRI).

   RDF 1.1 NTriples: [4] predicate
*/
SerdStatus
read_nt_predicate(SerdReader* reader, SerdNode** dest);

/**
   Read an object (IRI or blank or literal).

   RDF 1.1 NTriples: [5] object
*/
SerdStatus
read_nt_object(SerdReader* reader, SerdNode** dest, bool* ate_dot);

/**
   Read a variable that starts with '?' or '$'.

   This is an extension that serd uses in certain contexts to support
   patterns.

   Restricted version of SPARQL 1.1: [108] Var
*/
SerdStatus
read_Var(SerdReader* reader, SerdNode** dest);

/**
   Read a complete NTriples document.

   RDF 1.1 NTriples: [1] ntriplesDoc
*/
SerdStatus
read_ntriplesDoc(SerdReader* reader);

#endif // SERD_SRC_READ_NTRIPLES_H
