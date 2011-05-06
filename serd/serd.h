/*
  Copyright 2011 David Robillard <http://drobilla.net>

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

/**
   @file serd.h API for Serd, a lightweight RDF syntax library.
*/

#ifndef SERD_SERD_H
#define SERD_SERD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef SERD_SHARED
#    ifdef __WIN32__
#        define SERD_LIB_IMPORT __declspec(dllimport)
#        define SERD_LIB_EXPORT __declspec(dllexport)
#    else
#        define SERD_LIB_IMPORT __attribute__((visibility("default")))
#        define SERD_LIB_EXPORT __attribute__((visibility("default")))
#    endif
#    ifdef SERD_INTERNAL
#        define SERD_API SERD_LIB_EXPORT
#    else
#        define SERD_API SERD_LIB_IMPORT
#    endif
#else
#    define SERD_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
   @defgroup serd Serd
   A lightweight RDF syntax library.
   @{
*/

/**
   Environment.

   Represents the state required to resolve a CURIE or relative URI, e.g. the
   base URI and set of namespace prefixes at a particular point in a
   serialisation.
*/
typedef struct SerdEnvImpl SerdEnv;

/**
   RDF reader.

   parses RDF by calling user-provided sink functions as input is consumed
   (much like an XML SAX parser).
*/
typedef struct SerdReaderImpl SerdReader;

/**
   RDF writer.

   Provides a number of functions to allow writing RDF syntax out to some
   stream.  These functions are deliberately compatible with the sink functions
   used by SerdReader, so a reader can be directly connected to a writer to
   re-serialise a document.
*/
typedef struct SerdWriterImpl SerdWriter;

/**
   Return status code.
*/
typedef enum {
	SERD_SUCCESS        = 0,  /**< No error */
	SERD_FAILURE        = 1,  /**< Non-fatal failure */
	SERD_ERR_UNKNOWN    = 2,  /**< Unknown error */
	SERD_ERR_BAD_SYNTAX = 3,  /**< Invalid syntax */
	SERD_ERR_BAD_ARG    = 3,  /**< Invalid argument */
	SERD_ERR_NOT_FOUND  = 4   /**< Not found */
} SerdStatus;

/**
   RDF syntax type.
*/
typedef enum {
	/**
	   Turtle - Terse RDF Triple Language (UTF-8).
	   @see <a href="http://www.w3.org/TeamSubmission/turtle/">Turtle</a>
	*/
	SERD_TURTLE = 1,

	/**
	   NTriples - Line-based RDF triples (ASCII).
	   @see <a href="http://www.w3.org/TR/rdf-testcases#ntriples">NTriples</a>
	*/
	SERD_NTRIPLES = 2
} SerdSyntax;

/**
   Type of a syntactic RDF node.

   This is more precise than the type of an abstract RDF node.  An abstract
   node is either a resource, literal, or blank.  In syntax there are two ways
   to refer to both a resource (by URI or CURIE) and a blank (by ID or
   anonymously).

   Serd represents all nodes as an unquoted UTF-8 string "value" associated
   with a @ref SerdType, which is precise enough to preserve the syntactic
   information required for streaming abbreviation.  A non-abbreviating sink
   may simply consider @ref SERD_ANON_BEGIN and @ref SERD_ANON equivalent to
   @ref SERD_BLANK_ID.
*/
typedef enum {
	/**
	   The type of a nonexistent node.

	   This type is occasionally useful, but is never emitted by the reader.
	*/
	SERD_NOTHING = 0,

	/**
	   Literal value.

	   A literal optionally has either an associated language, or an associated
	   datatype (not both).
	*/
	SERD_LITERAL = 1,

	/**
	   URI (absolute or relative).

	   Value is an unquoted URI string, which is either a relative reference
	   with respect to the current base URI, or an absolute URI.  A URI is an
	   ID with universal scope.
	   @see <a href="http://tools.ietf.org/html/rfc3986">RFC3986</a>.
	*/
	SERD_URI = 2,

	/**
	   CURIE, a shortened URI.

	   Value is an unquoted CURIE string relative to the current environment,
	   e.g. "rdf:type".
	   @see <a href="http://www.w3.org/TR/curie">CURIE Syntax 1.0</a>
	*/
	SERD_CURIE = 3,

	/**
	   A blank node ID.

	   Value is a blank node ID, e.g. "id3", which is valid only within this
	   serialisation.
	   @see <a href="http://www.w3.org/TeamSubmission/turtle#nodeID">Turtle
	   <tt>nodeID</tt></a>
	*/
	SERD_BLANK_ID = 4,

	/**
	   The first reference to an anonymous (inlined) blank node.

	   Value is identical to a @ref SERD_BLANK_ID value (i.e. this type may be
	   safely considered equivalent to @ref SERD_BLANK_ID).
	*/
	SERD_ANON_BEGIN = 5,

	/**
	   An anonymous blank node.

	   Value is identical to a @ref SERD_BLANK_ID value (i.e. this type may be
	   safely considered equivalent to @ref SERD_BLANK_ID).
	*/
	SERD_ANON = 6
} SerdType;

/**
   A syntactic RDF node.
*/
typedef struct {
	const uint8_t* buf;      /**< Buffer */
	size_t         n_bytes;  /**< Size in bytes (including null) */
	size_t         n_chars;  /**< Length in characters */
	SerdType       type;     /**< Node type */
} SerdNode;

/**
   An unterminated string fragment.
*/
typedef struct {
	const uint8_t* buf;  /**< Start of chunk */
	size_t         len;  /**< Length of chunk in bytes */
} SerdChunk;

/**
   A parsed URI.

   This struct directly refers to chunks in other strings, it does not own any
   memory itself.  Thus, URIs can be parsed and/or resolved against a base URI
   in-place without allocating memory.
*/
typedef struct {
	SerdChunk scheme;     /**< Scheme */
	SerdChunk authority;  /**< Authority */
	SerdChunk path_base;  /**< Path prefix if relative */
	SerdChunk path;       /**< Path suffix */
	SerdChunk query;      /**< Query */
	SerdChunk fragment;   /**< Fragment */
} SerdURI;

/**
   Syntax style options.

   The style of the writer output can be controlled by ORing together
   values from this enumeration.  Note that some options are only supported
   for some syntaxes (e.g. NTriples does not support any options except
   @ref SERD_STYLE_ASCII, which is required).
*/
typedef enum {
	SERD_STYLE_ABBREVIATED = 1,       /**< Abbreviate triples when possible. */
	SERD_STYLE_ASCII       = 1 << 1,  /**< Escape all non-ASCII characters. */
	SERD_STYLE_RESOLVED    = 1 << 2,  /**< Resolve URIs against base URI. */
	SERD_STYLE_CURIED      = 1 << 3   /**< Shorten URIs into CURIEs. */
} SerdStyle;

/**
   @name URI
   @{
*/

static const SerdURI SERD_URI_NULL = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};

/**
   Return true iff @a utf8 starts with a valid URI scheme.
*/
SERD_API
bool
serd_uri_string_has_scheme(const uint8_t* utf8);

/**
   Parse @a utf8, writing result to @a out.
*/
SERD_API
SerdStatus
serd_uri_parse(const uint8_t* utf8, SerdURI* out);

/**
   Set @a out to @a uri resolved against @a base.
*/
SERD_API
void
serd_uri_resolve(const SerdURI* uri, const SerdURI* base, SerdURI* out);

/**
   Sink function for raw string output.
*/
typedef size_t (*SerdSink)(const void* buf, size_t len, void* stream);

/**
   Serialise @a uri with a series of calls to @a sink.
*/
SERD_API
size_t
serd_uri_serialise(const SerdURI* uri, SerdSink sink, void* stream);

/**
   @}
   @name Node
   @{
*/

static const SerdNode SERD_NODE_NULL = { 0, 0, 0, SERD_NOTHING };

/**
   Make a (shallow) node from @a str.

   This measures, but does not copy, @a str.  No memory is allocated.
*/
SERD_API
SerdNode
serd_node_from_string(SerdType type, const uint8_t* str);

/**
   Make a deep copy of @a node.

   @return a node that the caller must free with @ref serd_node_free.
*/
SERD_API
SerdNode
serd_node_copy(const SerdNode* node);

/**
   Return true iff @a a is equal to @a b.
*/
SERD_API
bool
serd_node_equals(const SerdNode* a, const SerdNode* b);

/**
   Simple wrapper for serd_node_new_uri to resolve a URI node.
*/
SERD_API
SerdNode
serd_node_new_uri_from_node(const SerdNode* uri_node,
                            const SerdURI*  base,
                            SerdURI*        out);

/**
   Simple wrapper for serd_node_new_uri to resolve a URI string.
*/
SERD_API
SerdNode
serd_node_new_uri_from_string(const uint8_t* str,
                              const SerdURI* base,
                              SerdURI*       out);

/**
   Create a new node by serialising @a uri into a new string.

   @param uri The URI to parse and serialise.

   @param base Base URI to resolve @a uri against (or NULL for no resolution).

   @param out Set to the parsing of the new URI (i.e. points only to
   memory owned by the new returned node).
*/
SERD_API
SerdNode
serd_node_new_uri(const SerdURI* uri, const SerdURI* base, SerdURI* out);

/**
   Free any data owned by @a node.

   Note that if @a node is itself dynamically allocated (which is not the case
   for nodes created internally by serd), it will not be freed.
*/
SERD_API
void
serd_node_free(SerdNode* node);

/**
   @}
   @name Event Handlers
   @{
*/

/**
   Sink (callback) for base URI changes.

   Called whenever the base URI of the serialisation changes.
*/
typedef SerdStatus (*SerdBaseSink)(void*           handle,
                                   const SerdNode* uri);

/**
   Sink (callback) for namespace definitions.

   Called whenever a prefix is defined in the serialisation.
*/
typedef SerdStatus (*SerdPrefixSink)(void*           handle,
                                     const SerdNode* name,
                                     const SerdNode* uri);

/**
   Sink (callback) for statements.

   Called for every RDF statement in the serialisation.
*/
typedef SerdStatus (*SerdStatementSink)(void*           handle,
                                        const SerdNode* graph,
                                        const SerdNode* subject,
                                        const SerdNode* predicate,
                                        const SerdNode* object,
                                        const SerdNode* object_datatype,
                                        const SerdNode* object_lang);

/**
   Sink (callback) for anonymous node end markers.

   This is called to indicate that the anonymous node with the given
   @a value will no longer be referred to by any future statements
   (i.e. the anonymous serialisation of the node is finished).
*/
typedef SerdStatus (*SerdEndSink)(void*           handle,
                                  const SerdNode* node);

/**
   @}
   @name Environment
   @{
*/

/**
   Create a new environment.
*/
SERD_API
SerdEnv*
serd_env_new();

/**
   Free @a ns.
*/
SERD_API
void
serd_env_free(SerdEnv* env);

/**
   Get the current base URI.
*/
SERD_API
const SerdNode*
serd_env_get_base_uri(SerdEnv* state,
                      SerdURI* out);

/**
   Set the current base URI.
*/
SERD_API
SerdStatus
serd_env_set_base_uri(SerdEnv*        state,
                      const SerdNode* uri_node);

/**
   Set a namespace prefix.
*/
SERD_API
SerdStatus
serd_env_set_prefix(SerdEnv*        env,
                    const SerdNode* name,
                    const SerdNode* uri);

/**
   Qualify @a uri into a CURIE if possible.
*/
SERD_API
bool
serd_env_qualify(const SerdEnv*  env,
                 const SerdNode* uri,
                 SerdNode*       prefix,
                 SerdChunk*      suffix);

/**
   Expand @a curie.
*/
SERD_API
SerdStatus
serd_env_expand(const SerdEnv*  env,
                const SerdNode* curie,
                SerdChunk*      uri_prefix,
                SerdChunk*      uri_suffix);

/**
   Expand @a node, which must be a CURIE or URI, to a full URI.
*/
SERD_API
SerdNode
serd_env_expand_node(SerdEnv*        env,
                     const SerdNode* node);

/**
   Call @a func for each prefix defined in @a env.
*/
SERD_API
void
serd_env_foreach(const SerdEnv* env,
                 SerdPrefixSink func,
                 void*          handle);

/**
   @}
   @name Reader
   @{
*/

/**
   Create a new RDF reader.
*/
SERD_API
SerdReader*
serd_reader_new(SerdSyntax        syntax,
                void*             handle,
                SerdBaseSink      base_sink,
                SerdPrefixSink    prefix_sink,
                SerdStatementSink statement_sink,
                SerdEndSink       end_sink);

/**
   Set a prefix to be added to all blank node identifiers.

   This is useful when multiple files are to be parsed into the same output
   (e.g. a store, or other files).  Since Serd preserves blank node IDs, this
   could cause conflicts where two non-equivalent blank nodes are merged,
   resulting in corrupt data.  By setting a unique blank node prefix for each
   parsed file, this can be avoided, while preserving blank node names.
*/
SERD_API
void
serd_reader_set_blank_prefix(SerdReader*    reader,
                             const uint8_t* prefix);

/**
   Read @a file.
*/
SERD_API
SerdStatus
serd_reader_read_file(SerdReader*    reader,
                      FILE*          file,
                      const uint8_t* name);

/**
   Read @a utf8.
*/
SERD_API
SerdStatus
serd_reader_read_string(SerdReader* me, const uint8_t* utf8);

/**
   Free @a reader.
*/
SERD_API
void
serd_reader_free(SerdReader* reader);

/**
   @}
   @name Writer
   @{
*/

/**
   Create a new RDF writer.
*/
SERD_API
SerdWriter*
serd_writer_new(SerdSyntax     syntax,
                SerdStyle      style,
                SerdEnv*       env,
                const SerdURI* base_uri,
                SerdSink       sink,
                void*          stream);

/**
   Free @a writer.
*/
SERD_API
void
serd_writer_free(SerdWriter* writer);

/**
   Set the current output base URI (and emit directive if applicable).
*/
SERD_API
SerdStatus
serd_writer_set_base_uri(SerdWriter*    writer,
                         const SerdURI* uri);

/**
   Set a namespace prefix (and emit directive if applicable).
*/
SERD_API
SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri);

/**
   Write a statement.
*/
SERD_API
SerdStatus
serd_writer_write_statement(SerdWriter*     writer,
                            const SerdNode* graph,
                            const SerdNode* subject,
                            const SerdNode* predicate,
                            const SerdNode* object,
                            const SerdNode* object_datatype,
                            const SerdNode* object_lang);

/**
   Mark the end of an anonymous node's description.
*/
SERD_API
SerdStatus
serd_writer_end_anon(SerdWriter*     writer,
                     const SerdNode* node);

/**
   Finish a write.
*/
SERD_API
SerdStatus
serd_writer_finish(SerdWriter* writer);

/**
   @}
   @}
*/

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* SERD_SERD_H */
