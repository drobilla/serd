/* Serd, an RDF serialisation library.
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

/* @file
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
 * A lightweight RDF serialisation library.
 * @{
 */

typedef struct SerdEnvImpl*    SerdEnv;    /**< Namespace prefixes. */
typedef struct SerdReaderImpl* SerdReader; /**< RDF reader. */
typedef struct SerdWriterImpl* SerdWriter; /**< RDF writer. */

/** RDF syntax */
typedef enum {
	SERD_TURTLE   = 1, /**< <http://www.w3.org/TeamSubmission/turtle/> */
	SERD_NTRIPLES = 2  /**< <http://www.w3.org/TR/rdf-testcases/#ntriples> */
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

/** A chunk of memory (unterminated string). */
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

/** Resolve @a uri relative to @a base, writing result to @a out. */
SERD_API
bool
serd_uri_resolve(const SerdURI* uri, const SerdURI* base, SerdURI* out);

/** Write @a uri to @a file. */
SERD_API
bool
serd_uri_write(const SerdURI* uri, FILE* file);

/** Sink function for raw string output. */
typedef size_t (*SerdSink)(const void* buf, size_t len, void* stream);

/** Serialise @a uri with a series of calls to @a sink. */
SERD_API
size_t
serd_uri_serialise(const SerdURI* uri, SerdSink sink, void* stream);

/** @} */
/** @name SerdString
 * @brief A measured UTF-8 string.
 * @{
 */

/** Measured UTF-8 string. */
typedef struct {
	size_t  n_bytes;  ///< Size in bytes including trailing null byte
	size_t  n_chars;  ///< Length in characters
	uint8_t buf[];    ///< Buffer
} SerdString;

/** Create a new UTF-8 string from @a utf8. */
SERD_API
SerdString*
serd_string_new(const uint8_t* utf8);

/** Copy @a string. */
SERD_API
SerdString*
serd_string_copy(const SerdString* str);

/** Free @a str. */
SERD_API
void
serd_string_free(SerdString* str);

/** Serialise @a uri to a string. */
SERD_API
SerdString*
serd_string_new_from_uri(const SerdURI* uri,
                         SerdURI*       out);

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
serd_env_add(SerdEnv           env,
             const SerdString* name,
             const SerdString* uri);

/** Expand @a qname. */
SERD_API
bool
serd_env_expand(const SerdEnv     env,
                const SerdString* qname,
                SerdChunk*        uri_prefix,
                SerdChunk*        uri_suffix);

/** @} */
/** @name SerdReader
 * @brief Reader of RDF syntax.
 * @{
 */

/** Sink for base URI changes. */
typedef bool (*SerdBaseSink)(void*             handle,
                             const SerdString* uri);

/** Sink for namespace definitions. */
typedef bool (*SerdPrefixSink)(void*             handle,
                               const SerdString* name,
                               const SerdString* uri);

/** Sink for statements. */
typedef bool (*SerdStatementSink)(void*             handle,
                                  const SerdString* graph,
                                  const SerdString* subject,
                                  SerdType      subject_type,
                                  const SerdString* predicate,
                                  SerdType      predicate_type,
                                  const SerdString* object,
                                  SerdType      object_type,
                                  const SerdString* object_lang,
                                  const SerdString* object_datatype);

/** Sink for anonymous node end markers.
 * This is called to indicate that the anonymous node with the given
 * @a value will no longer be referred to by any future statements
 * (i.e. the anonymous serialisation of the node is finished).
 */
typedef bool (*SerdEndSink)(void*             handle,
                            const SerdString* value);

/** Create a new RDF reader. */
SERD_API
SerdReader
serd_reader_new(SerdSyntax        syntax,
                void*             handle,
                SerdBaseSink      base_sink,
                SerdPrefixSink    prefix_sink,
                SerdStatementSink statement_sink,
                SerdEndSink       end_sink);

/** Read @a file. */
SERD_API
bool
serd_reader_read_file(SerdReader     reader,
                      FILE*          file,
                      const uint8_t* name);

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
	SERD_STYLE_ASCII       = 1 << 1  /**< Escape all non-ASCII characters. */
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

/** Set the current output base URI. */
SERD_API
void
serd_writer_set_base_uri(SerdWriter     writer,
                         const SerdURI* uri);

/** Set the current output base URI. */
SERD_API
void
serd_writer_set_prefix(SerdWriter        writer,
                       const SerdString* name,
                       const SerdString* uri);

/** Write a statement. */
SERD_API
bool
serd_writer_write_statement(SerdWriter        writer,
                            const SerdString* graph,
                            const SerdString* subject,
                            SerdType          subject_type,
                            const SerdString* predicate,
                            SerdType          predicate_type,
                            const SerdString* object,
                            SerdType          object_type,
                            const SerdString* object_datatype,
                            const SerdString* object_lang);

/** Mark the end of an anonymous node's description. */
SERD_API
bool
serd_writer_end_anon(SerdWriter        writer,
                     const SerdString* subject);

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
