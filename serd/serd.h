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
   base URI and set of namespace prefixes at a particular point.
*/
typedef struct SerdEnvImpl SerdEnv;

/**
   RDF reader.

   Parses RDF by calling user-provided sink functions as input is consumed
   (much like an XML SAX parser).
*/
typedef struct SerdReaderImpl SerdReader;

/**
   RDF writer.

   Provides a number of functions to allow writing RDF syntax out to some
   stream.  These functions are deliberately compatible with the sink functions
   used by SerdReader, so a reader can be directly connected to a writer to
   re-serialise a document with minimal overhead.
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
   Flags indication inline abbreviation information for a statement.
*/
typedef enum {
	SERD_EMPTY_S      = 1 << 1,  /**< Empty blank node subject */
	SERD_EMPTY_O      = 1 << 2,  /**< Empty blank node object */
	SERD_ANON_S_BEGIN = 1 << 3,  /**< Start of anonymous subject */
	SERD_ANON_O_BEGIN = 1 << 4,  /**< Start of anonymous object */
	SERD_ANON_CONT    = 1 << 5   /**< Continuation of anonymous node */
} SerdStatementFlag;

/**
   Bitwise OR of SerdNodeFlag values.
*/
typedef uint32_t SerdStatementFlags;

/**
   Type of a syntactic RDF node.

   This is more precise than the type of an abstract RDF node.  An abstract
   node is either a resource, literal, or blank.  In syntax there are two ways
   to refer to a resource (by URI or CURIE) and two ways to refer to a blank
   (by ID or anonymously).  Anonymous (inline) blank nodes are expressed using
   SerdStatementFlags rather than this type.
*/
typedef enum {
	/**
	   The type of a nonexistent node.

	   This type is useful as a sentinel, but is never emitted by the reader.
	*/
	SERD_NOTHING = 0,

	/**
	   Literal value.

	   A literal optionally has either a language, or a datatype (not both).
	*/
	SERD_LITERAL = 1,

	/**
	   URI (absolute or relative).

	   Value is an unquoted URI string, which is either a relative reference
	   with respect to the current base URI (e.g. "foo/bar"), or an absolute
	   URI (e.g. "http://example.org/foo").
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
	   A blank node.

	   Value is a blank node ID, e.g. "id3", which is meaningful only within
	   this serialisation.
	   @see <a href="http://www.w3.org/TeamSubmission/turtle#nodeID">Turtle
	   <tt>nodeID</tt></a>
	*/
	SERD_BLANK = 4,

} SerdType;

/**
   Flags indicating certain string properties relevant to serialisation.
*/
typedef enum {
	SERD_HAS_NEWLINE = 1,      /**< Contains line breaks ('\\n' or '\\r') */
	SERD_HAS_QUOTE   = 1 << 1  /**< Contains quotes ('"') */
} SerdNodeFlag;

/**
   Bitwise OR of SerdNodeFlag values.
*/
typedef uint32_t SerdNodeFlags;

/**
   A syntactic RDF node.
*/
typedef struct {
	const uint8_t* buf;      /**< Value string */
	size_t         n_bytes;  /**< Size in bytes (not including null) */
	size_t         n_chars;  /**< Length in characters (not including null)*/
	SerdNodeFlags  flags;    /**< Node flags (e.g. string properties) */
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
   for some syntaxes (e.g. NTriples does not support abbreviation and is
   always ASCII).
*/
typedef enum {
	SERD_STYLE_ABBREVIATED = 1,       /**< Abbreviate triples when possible. */
	SERD_STYLE_ASCII       = 1 << 1,  /**< Escape all non-ASCII characters. */
	SERD_STYLE_RESOLVED    = 1 << 2,  /**< Resolve URIs against base URI. */
	SERD_STYLE_CURIED      = 1 << 3   /**< Shorten URIs into CURIEs. */
} SerdStyle;

/**
   UTF-8 strlen.
   @return Length of @c str in characters (except NULL).
   @param str A null-terminated UTF-8 string.
   @param n_bytes (Output) Set to the size of @c str in bytes (except NULL).
   @param flags (Output) Set to the applicable flags.
*/
SERD_API
size_t
serd_strlen(const uint8_t* str, size_t* n_bytes, SerdNodeFlags* flags);

/**
   @name URI
   @{
*/

static const SerdURI SERD_URI_NULL = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};

/**
   Return the local path for @c uri, or NULL if @c uri is not a file URI.
*/
SERD_API
const uint8_t*
serd_uri_to_path(const uint8_t* uri);

/**
   Return true iff @c utf8 starts with a valid URI scheme.
*/
SERD_API
bool
serd_uri_string_has_scheme(const uint8_t* utf8);

/**
   Parse @c utf8, writing result to @c out.
*/
SERD_API
SerdStatus
serd_uri_parse(const uint8_t* utf8, SerdURI* out);

/**
   Set @c out to @c uri resolved against @c base.
*/
SERD_API
void
serd_uri_resolve(const SerdURI* uri, const SerdURI* base, SerdURI* out);

/**
   Sink function for raw string output.
*/
typedef size_t (*SerdSink)(const void* buf, size_t len, void* stream);

/**
   Serialise @c uri with a series of calls to @c sink.
*/
SERD_API
size_t
serd_uri_serialise(const SerdURI* uri, SerdSink sink, void* stream);

/**
   @}
   @name Node
   @{
*/

static const SerdNode SERD_NODE_NULL = { 0, 0, 0, 0, SERD_NOTHING };

/**
   Make a (shallow) node from @c str.

   This measures, but does not copy, @c str.  No memory is allocated.
*/
SERD_API
SerdNode
serd_node_from_string(SerdType type, const uint8_t* str);

/**
   Make a deep copy of @c node.

   @return a node that the caller must free with @ref serd_node_free.
*/
SERD_API
SerdNode
serd_node_copy(const SerdNode* node);

/**
   Return true iff @c a is equal to @c b.
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
   Create a new node by serialising @c uri into a new string.

   @param uri The URI to parse and serialise.

   @param base Base URI to resolve @c uri against (or NULL for no resolution).

   @param out Set to the parsing of the new URI (i.e. points only to
   memory owned by the new returned node).
*/
SERD_API
SerdNode
serd_node_new_uri(const SerdURI* uri, const SerdURI* base, SerdURI* out);

/**
   Free any data owned by @c node.

   Note that if @c node is itself dynamically allocated (which is not the case
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
typedef SerdStatus (*SerdStatementSink)(void*              handle,
                                        SerdStatementFlags flags,
                                        const SerdNode*    graph,
                                        const SerdNode*    subject,
                                        const SerdNode*    predicate,
                                        const SerdNode*    object,
                                        const SerdNode*    object_datatype,
                                        const SerdNode*    object_lang);

/**
   Sink (callback) for anonymous node end markers.

   This is called to indicate that the anonymous node with the given
   @c value will no longer be referred to by any future statements
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
serd_env_new(const SerdNode* base_uri);

/**
   Free @c ns.
*/
SERD_API
void
serd_env_free(SerdEnv* env);

/**
   Get the current base URI.
*/
SERD_API
const SerdNode*
serd_env_get_base_uri(const SerdEnv* env,
                      SerdURI*       out);

/**
   Set the current base URI.
*/
SERD_API
SerdStatus
serd_env_set_base_uri(SerdEnv*        env,
                      const SerdNode* uri);

/**
   Set a namespace prefix.
*/
SERD_API
SerdStatus
serd_env_set_prefix(SerdEnv*        env,
                    const SerdNode* name,
                    const SerdNode* uri);

/**
   Qualify @c uri into a CURIE if possible.
*/
SERD_API
bool
serd_env_qualify(const SerdEnv*  env,
                 const SerdNode* uri,
                 SerdNode*       prefix,
                 SerdChunk*      suffix);

/**
   Expand @c curie.
*/
SERD_API
SerdStatus
serd_env_expand(const SerdEnv*  env,
                const SerdNode* curie,
                SerdChunk*      uri_prefix,
                SerdChunk*      uri_suffix);

/**
   Expand @c node, which must be a CURIE or URI, to a full URI.
*/
SERD_API
SerdNode
serd_env_expand_node(const SerdEnv*  env,
                     const SerdNode* node);

/**
   Call @c func for each prefix defined in @c env.
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
                void              (*free_handle)(void*),
                SerdBaseSink      base_sink,
                SerdPrefixSink    prefix_sink,
                SerdStatementSink statement_sink,
                SerdEndSink       end_sink);

/**
   Return the @c handle passed to @ref serd_reader_new.
*/
SERD_API
void*
serd_reader_get_handle(const SerdReader* reader);
		
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
serd_reader_add_blank_prefix(SerdReader*    reader,
                             const uint8_t* prefix);

/**
   Read a file at a given @c uri.
*/
SERD_API
SerdStatus
serd_reader_read_file(SerdReader*    reader,
                      const uint8_t* uri);

/**
   Read @c file.
*/
SERD_API
SerdStatus
serd_reader_read_file_handle(SerdReader*    reader,
                             FILE*          file,
                             const uint8_t* name);

/**
   Read @c utf8.
*/
SERD_API
SerdStatus
serd_reader_read_string(SerdReader* me, const uint8_t* utf8);

/**
   Free @c reader.
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
   Free @c writer.
*/
SERD_API
void
serd_writer_free(SerdWriter* writer);

/**
   Set a prefix to be removed from matching blank node identifiers.
*/
SERD_API
void
serd_writer_chop_blank_prefix(SerdWriter*    writer,
                              const uint8_t* prefix);

/**
   Set the current output base URI (and emit directive if applicable).

   Note this function can be safely casted to SerdBaseSink.
*/
SERD_API
SerdStatus
serd_writer_set_base_uri(SerdWriter*     writer,
                         const SerdNode* uri);

/**
   Set a namespace prefix (and emit directive if applicable).

   Note this function can be safely casted to SerdPrefixSink.
*/
SERD_API
SerdStatus
serd_writer_set_prefix(SerdWriter*     writer,
                       const SerdNode* name,
                       const SerdNode* uri);

/**
   Write a statement.

   Note this function can be safely casted to SerdStatementSink.
*/
SERD_API
SerdStatus
serd_writer_write_statement(SerdWriter*        writer,
                            SerdStatementFlags flags,
                            const SerdNode*    graph,
                            const SerdNode*    subject,
                            const SerdNode*    predicate,
                            const SerdNode*    object,
                            const SerdNode*    object_datatype,
                            const SerdNode*    object_lang);

/**
   Mark the end of an anonymous node's description.

   Note this function can be safely casted to SerdEndSink.
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
