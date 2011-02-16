/* Serd, a lightweight RDF syntax library.
 * Copyright 2011 David Robillard <d@drobilla.net>
 *
 * Serd is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Serd is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 * Public Serd API.
 */

#ifndef SERD_SERD_H
#define SERD_SERD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef SERD_SHARED
	#if defined _WIN32 || defined __CYGWIN__
		#define SERD_LIB_IMPORT __declspec(dllimport)
		#define SERD_LIB_EXPORT __declspec(dllexport)
	#else
		#define SERD_LIB_IMPORT __attribute__ ((visibility("default")))
		#define SERD_LIB_EXPORT __attribute__ ((visibility("default")))
	#endif
	#ifdef SERD_INTERNAL
		#define SERD_API SERD_LIB_EXPORT
	#else
		#define SERD_API SERD_LIB_IMPORT
	#endif
#else
	#define SERD_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup serd Serd
 * A lightweight RDF syntax library.
 * @{
 */

typedef struct SerdEnvImpl*    SerdEnv;    /**< Namespace prefixes. */
typedef struct SerdReaderImpl* SerdReader; /**< RDF reader. */
typedef struct SerdWriterImpl* SerdWriter; /**< RDF writer. */

/** RDF syntax */
typedef enum {
	SERD_TURTLE   = 1,  ///< <http://www.w3.org/TeamSubmission/turtle/>
	SERD_NTRIPLES = 2   ///< <http://www.w3.org/TR/rdf-testcases/#ntriples>
} SerdSyntax;

/** Type of a syntactic RDF node.
 * This is more precise than the type of an abstract RDF node. An abstract
 * node is either a resource, literal, or blank. In syntax there are two
 * ways to refer to a resource (URI or CURIE), and two ways to refer to a
 * blank node (with a blank ID, or anonymously). Serd represents nodes as
 * an unquoted UTF-8 string "value" associated with a @ref SerdType,
 * which preserves syntactic information allowing for lossless abbreviation.
 * A non-abbreviating sink may simply consider @ref SERD_ANON_BEGIN and
 * @ref SERD_ANON equivalent to SERD_BLANK_ID.
 */
typedef enum {
	/** The type of a NULL node. */
	SERD_NOTHING = 0,

	/** Literal value. A literal optionally has either an associated language,
	 * or an associated datatype (not both).
	 */
	SERD_LITERAL = 1,

	/** URI. Value is a valid URI string (either absolute or relative), which
	 * is valid universally. See <http://tools.ietf.org/html/rfc3986>.
	 */
	SERD_URI = 2,

	/** CURIE, a shortened URI. Value is an unquoted UTF-8 CURIE string
	 * relative to the current environment, e.g. "rdf:type", which is valid
	 * only within this serialisation. See <http://www.w3.org/TR/curie>.
	 */
	SERD_CURIE = 3,

	/** A blank node ID. Value is a blank node identifier (e.g. "blank3"),
	 * which is valid only within this serialisation.
	 * See <http://www.w3.org/TeamSubmission/turtle#nodeID>.
	 */
	SERD_BLANK_ID = 4,

	/** The first reference to an anonymous (inlined) blank node.
	 * Value is identical to a @ref SERD_BLANK_ID value.
	 */
	SERD_ANON_BEGIN = 5,

	/** An anonymous blank node.
	 * Value is identical to a @ref SERD_BLANK_ID value.
	 */
	SERD_ANON = 6
} SerdType;

/** @name SerdURI
 * A parsed URI.
 * @{
 */

/** An unterminated string fragment. */
typedef struct {
	const uint8_t* buf;  ///< Start of chunk
	size_t         len;  ///< Length of chunk in bytes
} SerdChunk;

/** A parsed URI.
 * This struct directly refers to chunks in other strings, it does not own
 * any memory itself. Thus, URIs can be parsed and/or resolved against a
 * base URI in-place without allocating memory.
 */
typedef struct {
	SerdChunk scheme;     ///< Scheme
	SerdChunk authority;  ///< Authority
	SerdChunk path_base;  ///< Path prefix if relative
	SerdChunk path;       ///< Path suffix
	SerdChunk query;      ///< Query
	SerdChunk fragment;   ///< Fragment
} SerdURI;

static const SerdURI SERD_URI_NULL = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};

/** Return true iff @a utf8 starts with a valid URI scheme. */
SERD_API
bool
serd_uri_string_has_scheme(const uint8_t* utf8);

/** Parse @a utf8, writing result to @a out. */
SERD_API
bool
serd_uri_parse(const uint8_t* utf8, SerdURI* out);

/** Set @a out to @a uri resolved against @a base. */
SERD_API
void
serd_uri_resolve(const SerdURI* uri, const SerdURI* base, SerdURI* out);

/** Sink function for raw string output. */
typedef size_t (*SerdSink)(const void* buf, size_t len, void* stream);

/** Serialise @a uri with a series of calls to @a sink. */
SERD_API
size_t
serd_uri_serialise(const SerdURI* uri, SerdSink sink, void* stream);

/** @} */
/** @name SerdNode
 * @brief An RDF node.
 * @{
 */

/** A syntactic RDF node. */
typedef struct {
	SerdType       type;
	size_t         n_bytes;  ///< Size in bytes including trailing null byte
	size_t         n_chars;  ///< Length in characters
	const uint8_t* buf;      ///< Buffer
} SerdNode;

static const SerdNode SERD_NODE_NULL = { SERD_NOTHING, 0, 0, 0 };

/** Make a (shallow) node from @a str.
 * This measures, but does not copy, @a str.  No memory is allocated.
 */
SERD_API
SerdNode
serd_node_from_string(SerdType type, const uint8_t* str);

/** Make a deep copy of @a node.
 * @return a node that the caller must free with @ref serd_node_free.
 */
SERD_API
SerdNode
serd_node_copy(const SerdNode* node);

/** Simple wrapper for serd_node_new_uri to resolve a URI node. */
SERD_API
SerdNode
serd_node_new_uri_from_node(const SerdNode* uri_node,
                            const SerdURI*  base,
                            SerdURI*        out);

/** Simple wrapper for serd_node_new_uri to resolve a URI string. */
SERD_API
SerdNode
serd_node_new_uri_from_string(const uint8_t* str,
                              const SerdURI* base,
                              SerdURI*       out);

/** Create a new node by serialising @a uri into a new string.
 * @param uri The URI to parse and serialise.
 * @param base Base URI to resolve @a uri against (or NULL for no resolution).
 * @param out (Output) set to the parsing of the new URI (i.e. points only to
 *        memory owned by the new returned node).
 */
SERD_API
SerdNode
serd_node_new_uri(const SerdURI* uri, const SerdURI* base, SerdURI* out);

/** Free any data owned by @a node.
 * Note that if @a node is itself dynamically allocated (which is not the case
 * for nodes created internally by serd), it will not be freed.
 */
SERD_API
void
serd_node_free(SerdNode* node);

/** @} */
/** @name SerdHandlers
 * @brief Event handlers (user-provided worker functions).
 * @{
 */

/** Sink for base URI changes. */
typedef bool (*SerdBaseSink)(void*           handle,
                             const SerdNode* uri);

/** Sink for namespace definitions. */
typedef bool (*SerdPrefixSink)(void*           handle,
                               const SerdNode* name,
                               const SerdNode* uri);

/** Sink for statements. */
typedef bool (*SerdStatementSink)(void*           handle,
                                  const SerdNode* graph,
                                  const SerdNode* subject,
                                  const SerdNode* predicate,
                                  const SerdNode* object,
                                  const SerdNode* object_datatype,
                                  const SerdNode* object_lang);

/** @} */
/** @name SerdEnv
 * @brief An environment (a prefix => URI dictionary).
 * @{
 */

/** Create a new environment. */
SERD_API
SerdEnv
serd_env_new();

/** Free @a ns. */
SERD_API
void
serd_env_free(SerdEnv env);

/** Add namespace @a uri to @a ns using prefix @a name. */
SERD_API
void
serd_env_add(SerdEnv         env,
             const SerdNode* name,
             const SerdNode* uri);

/** Qualify @a into a CURIE if possible. */
SERD_API
bool
serd_env_qualify(const SerdEnv   env,
                 const SerdNode* uri,
                 SerdNode*       prefix,
                 SerdChunk*      suffix);

/** Expand @a curie. */
SERD_API
bool
serd_env_expand(const SerdEnv   env,
                const SerdNode* curie,
                SerdChunk*      uri_prefix,
                SerdChunk*      uri_suffix);

/** Call @a func for each prefix defined in @a env. */
SERD_API
void
serd_env_foreach(const SerdEnv  env,
                 SerdPrefixSink func,
                 void*          handle);

/** @} */
/** @name SerdReader
 * @brief Reader of RDF syntax.
 * @{
 */

/** Sink for anonymous node end markers.
 * This is called to indicate that the anonymous node with the given
 * @a value will no longer be referred to by any future statements
 * (i.e. the anonymous serialisation of the node is finished).
 */
typedef bool (*SerdEndSink)(void*           handle,
                            const SerdNode* node);

/** Create a new RDF reader. */
SERD_API
SerdReader
serd_reader_new(SerdSyntax        syntax,
                void*             handle,
                SerdBaseSink      base_sink,
                SerdPrefixSink    prefix_sink,
                SerdStatementSink statement_sink,
                SerdEndSink       end_sink);

/** Set a prefix to be added to all blank node identifiers.
 * This is useful when multiple files are to be parsed into the same output
 * (e.g. a store, or other files).  Since Serd preserves blank node IDs, this
 * could cause conflicts where two non-equivalent blank nodes are merged,
 * resulting in corrupt data.  By setting a unique blank node prefix for
 * each parsed file, this can be avoided, while preserving blank node names.
 */
SERD_API
void
serd_reader_set_blank_prefix(SerdReader     reader,
                             const uint8_t* prefix);

/** Read @a file. */
SERD_API
bool
serd_reader_read_file(SerdReader     reader,
                      FILE*          file,
                      const uint8_t* name);

/** Read @a utf8. */
SERD_API
bool
serd_reader_read_string(SerdReader me, const uint8_t* utf8);

/** Free @a reader. */
SERD_API
void
serd_reader_free(SerdReader reader);

/** @} */
/** @name SerdWriter
 * @brief Writer of RDF syntax.
 * @{
 */

typedef enum {
	SERD_STYLE_ABBREVIATED = 1,      /**< Abbreviate triples when possible. */
	SERD_STYLE_ASCII       = 1 << 1, /**< Escape all non-ASCII characters. */
	SERD_STYLE_RESOLVED    = 1 << 2  /**< Resolve relative URIs against base. */
} SerdStyle;

/** Create a new RDF writer. */
SERD_API
SerdWriter
serd_writer_new(SerdSyntax     syntax,
                SerdStyle      style,
                SerdEnv        env,
                const SerdURI* base_uri,
                SerdSink       sink,
                void*          stream);

/** Free @a writer. */
SERD_API
void
serd_writer_free(SerdWriter writer);

/** Set the current output base URI (and emit directive if applicable). */
SERD_API
void
serd_writer_set_base_uri(SerdWriter     writer,
                         const SerdURI* uri);

/** Set a namespace prefix (and emit directive if applicable). */
SERD_API
void
serd_writer_set_prefix(SerdWriter      writer,
                       const SerdNode* name,
                       const SerdNode* uri);

/** Write a statement. */
SERD_API
bool
serd_writer_write_statement(SerdWriter      writer,
                            const SerdNode* graph,
                            const SerdNode* subject,
                            const SerdNode* predicate,
                            const SerdNode* object,
                            const SerdNode* object_datatype,
                            const SerdNode* object_lang);

/** Mark the end of an anonymous node's description. */
SERD_API
bool
serd_writer_end_anon(SerdWriter      writer,
                     const SerdNode* node);

/** Finish a write. */
SERD_API
void
serd_writer_finish(SerdWriter writer);

/** @} */

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SERD_SERD_H */
