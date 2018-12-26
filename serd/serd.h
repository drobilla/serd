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

/**
   @file serd.h API for Serd, a lightweight RDF syntax library.
*/

#ifndef SERD_SERD_H
#define SERD_SERD_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef SERD_SHARED
#    ifdef _WIN32
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
   World.

   The World represents all library state shared between various objects.
*/
typedef struct SerdWorldImpl SerdWorld;

/**
   Nodes.

   A hashing container for nodes that can be used for interning and simplified
   memory management.
*/
typedef struct SerdNodesImpl SerdNodes;

/**
   Statement.

   A subject, predicate, and object, with optional graph context.
*/
typedef struct SerdStatementImpl SerdStatement;

/**
   Cursor, the origin of a statement in a document.
*/
typedef struct SerdCursorImpl SerdCursor;

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
   An interface that receives a stream of RDF data.
*/
typedef struct SerdSinkImpl SerdSink;

/**
   A sink for bytes that receives string output.
*/
typedef struct SerdByteSinkImpl SerdByteSink;

/**
   Return status code.
*/
typedef enum {
	SERD_SUCCESS,         /**< No error */
	SERD_FAILURE,         /**< Non-fatal failure */
	SERD_ERR_UNKNOWN,     /**< Unknown error */
	SERD_ERR_BAD_SYNTAX,  /**< Invalid syntax */
	SERD_ERR_BAD_ARG,     /**< Invalid argument */
	SERD_ERR_NOT_FOUND,   /**< Not found */
	SERD_ERR_ID_CLASH,    /**< Encountered clashing blank node IDs */
	SERD_ERR_BAD_CURIE,   /**< Invalid CURIE (e.g. prefix does not exist) */
	SERD_ERR_INTERNAL,    /**< Unexpected internal error (should not happen) */
	SERD_ERR_OVERFLOW     /**< Stack overflow */
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
	SERD_NTRIPLES = 2,

	/**
	   NQuads - Line-based RDF quads (UTF-8).
	   @see <a href="https://www.w3.org/TR/n-quads/">NQuads</a>
	*/
	SERD_NQUADS = 3,

	/**
	   TriG - Terse RDF quads (UTF-8).
	   @see <a href="https://www.w3.org/TR/trig/">Trig</a>
	*/
	SERD_TRIG = 4
} SerdSyntax;

/**
   Flags indicating inline abbreviation information for a statement.
*/
typedef enum {
	SERD_EMPTY_S = 1 << 0,  /**< Empty blank node subject */
	SERD_ANON_S  = 1 << 1,  /**< Start of anonymous subject */
	SERD_ANON_O  = 1 << 2,  /**< Start of anonymous object */
	SERD_LIST_S  = 1 << 3,  /**< Start of list subject */
	SERD_LIST_O  = 1 << 4   /**< Start of list object */
} SerdStatementFlag;

/**
   Bitwise OR of SerdStatementFlag values.
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
	SERD_BLANK = 4
} SerdType;

/**
   Flags indicating certain string properties relevant to serialisation.
*/
typedef enum {
	SERD_HAS_NEWLINE  = 1,      /**< Contains line breaks ('\\n' or '\\r') */
	SERD_HAS_QUOTE    = 1 << 1, /**< Contains quotes ('"') */
	SERD_HAS_DATATYPE = 1 << 2, /**< Literal node has datatype */
	SERD_HAS_LANGUAGE = 1 << 3  /**< Literal node has language */
} SerdNodeFlag;

/**
   Field in a statement.
*/
typedef enum {
	SERD_SUBJECT   = 0,  /**< Subject */
	SERD_PREDICATE = 1,  /**< Predicate ("key") */
	SERD_OBJECT    = 2,  /**< Object    ("value") */
	SERD_GRAPH     = 3   /**< Graph     ("context") */
} SerdField;

/**
   Bitwise OR of SerdNodeFlag values.
*/
typedef uint32_t SerdNodeFlags;

/**
   A syntactic RDF node.
*/
typedef struct SerdNodeImpl SerdNode;

/**
   An unterminated immutable slice of a string.
*/
typedef struct {
	const char* buf;  /**< Start of chunk */
	size_t      len;  /**< Length of chunk in bytes */
} SerdStringView;

/**
   A mutable buffer in memory.
*/
typedef struct {
	void*  buf;  /**< Buffer */
	size_t len;  /**< Size of buffer in bytes */
} SerdBuffer;

/**
   An error description.
*/
typedef struct {
	SerdStatus        status;  /**< Error code */
	const SerdCursor* cursor;  /**< Origin of error, or NULL */
	const char*       fmt;     /**< Message format string (printf style) */
	va_list*          args;    /**< Arguments for fmt */
} SerdError;

/**
   A parsed URI.

   This struct directly refers to slices in other strings, it does not own any
   memory itself.  Thus, URIs can be parsed and/or resolved against a base URI
   in-place without allocating memory.
*/
typedef struct {
	SerdStringView scheme;     /**< Scheme */
	SerdStringView authority;  /**< Authority */
	SerdStringView path_base;  /**< Path prefix if relative */
	SerdStringView path;       /**< Path suffix */
	SerdStringView query;      /**< Query */
	SerdStringView fragment;   /**< Fragment */
} SerdURI;

/**
   Syntax style options.

   The style of the writer output can be controlled by ORing together
   values from this enumeration.  Note that some options are only supported
   for some syntaxes (e.g. NTriples does not support abbreviation and is
   always ASCII).
*/
typedef enum {
	SERD_STYLE_ASCII = 1 << 0,  /**< Escape all non-ASCII characters. */
} SerdStyle;

/**
   Bitwise OR of SerdStyle values.
*/
typedef uint32_t SerdStyleFlags;

/**
   Free memory allocated by Serd.

   This function exists because some systems require memory allocated by a
   library to be freed by code in the same library.  It is otherwise equivalent
   to the standard C free() function.
*/
SERD_API
void
serd_free(void* ptr);

/**
   @name String Utilities
   @{
*/

/**
   Return a string describing a status code.
*/
SERD_API
const char*
serd_strerror(SerdStatus status);

/**
   Measure a UTF-8 string.
   @return Length of `str` in bytes.
   @param str A null-terminated UTF-8 string.
   @param flags (Output) Set to the applicable flags.
*/
SERD_API
size_t
serd_strlen(const char* str, SerdNodeFlags* flags);

/**
   Parse a string to a double.

   The API of this function is similar to the standard C strtod function,
   except this function is locale-independent and always matches the lexical
   format used in the Turtle grammar (the decimal point is always ".").  The
   end parameter is an offset from the start of `str` to avoid the
   const-correctness issues of the strtod API.
*/
SERD_API
double
serd_strtod(const char* str, size_t* end);

/**
   Decode a base64 string.
   This function can be used to deserialise a blob node created with
   serd_new_blob().

   @param str Base64 string to decode.
   @param len The length of `str`.
   @param size Set to the size of the returned blob in bytes.
   @return A newly allocated blob which must be freed with serd_free().
*/
SERD_API
void*
serd_base64_decode(const char* str, size_t len, size_t* size);

/**
   @}
   @name Byte Streams
   @{
*/

/**
   Function to detect I/O stream errors.

   Identical semantics to `ferror`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdStreamErrorFunc)(void* stream);

/**
   Source function for raw string input.

   Identical semantics to `fread`, but may set errno for more informative error
   reporting than supported by SerdStreamErrorFunc.

   @param buf Output buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to read from (FILE* for fread).
   @return Number of elements (bytes) read, which is short on error.
*/
typedef size_t (*SerdReadFunc)(void*  buf,
                               size_t size,
                               size_t nmemb,
                               void*  stream);

/**
   Sink function for raw string output.

   Identical semantics to `fwrite`, but may set errno for more informative
   error reporting than supported by SerdStreamErrorFunc.

   @param buf Input buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to write to (FILE* for fread).
   @return Number of elements (bytes) written, which is short on error.
*/
typedef size_t (*SerdWriteFunc)(const void* buf,
                                size_t      size,
                                size_t      nmemb,
                                void*       stream);

/**
   Create a new byte sink.

   @param write_func Function called with bytes to consume.
   @param stream Context parameter passed to `sink`.
   @param block_size Number of bytes to write per call.
*/
SERD_API
SerdByteSink*
serd_byte_sink_new(SerdWriteFunc write_func, void* stream, size_t block_size);

/**
   Write to `sink`.

   Compatible with SerdWriteFunc.
*/
SERD_API
size_t
serd_byte_sink_write(const void*   buf,
                     size_t        size,
                     size_t        nmemb,
                     SerdByteSink* sink);

/**
   Flush any pending output in `sink` to the underlying write function.
*/
SERD_API
void
serd_byte_sink_flush(SerdByteSink* sink);

/**
   Free `sink`.
*/
SERD_API
void
serd_byte_sink_free(SerdByteSink* sink);

/**
   @}
   @name Syntax Utilities
   @{
*/

/**
   Get a syntax by name.

   Case-insensitive, supports "Turtle", "NTriples", "NQuads", and "TriG".  Zero
   is returned if the name is not recognized.
*/
SERD_API
SerdSyntax
serd_syntax_by_name(const char* name);

/**
   Guess a syntax from a filename.

   This uses the file extension to guess the syntax of a file.  Zero is
   returned if the extension is not recognized.
*/
SERD_API
SerdSyntax
serd_guess_syntax(const char* filename);

/**
   Guess a syntax from a filename.

   This uses the file extension to guess the syntax of a file.  Zero is
   returned if the extension is not recognized.
*/
SERD_API
bool
serd_syntax_has_graphs(SerdSyntax syntax);

/**
   @}
   @name URI
   @{
*/

static const SerdURI SERD_URI_NULL = {
	{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}
};

/**
   Get the unescaped path and hostname from a file URI.
   @param uri A file URI.
   @param hostname If non-NULL, set to the hostname, if present.
   @return The path component of the URI.

   The returned path and `*hostname` must be freed with serd_free().
*/
SERD_API
char*
serd_file_uri_parse(const char* uri, char** hostname);

/**
   Return true iff `utf8` starts with a valid URI scheme.
*/
SERD_API
bool
serd_uri_string_has_scheme(const char* utf8);

/**
   Parse `utf8`, writing result to `out`.
*/
SERD_API
SerdStatus
serd_uri_parse(const char* utf8, SerdURI* out);

/**
   Set target `t` to reference `r` resolved against `base`.

   @see http://tools.ietf.org/html/rfc3986#section-5.2.2
*/
SERD_API
void
serd_uri_resolve(const SerdURI* r, const SerdURI* base, SerdURI* t);

/**
   Serialise `uri` with a series of calls to `sink`.
*/
SERD_API
size_t
serd_uri_serialise(const SerdURI* uri, SerdWriteFunc sink, void* stream);

/**
   Serialise `uri` relative to `base` with a series of calls to `sink`.

   The `uri` is written as a relative URI iff if it a child of `base` and @c
   root.  The optional `root` parameter must be a prefix of `base` and can be
   used keep up-references ("../") within a certain namespace.
*/
SERD_API
size_t
serd_uri_serialise_relative(const SerdURI* uri,
                            const SerdURI* base,
                            const SerdURI* root,
                            SerdWriteFunc  sink,
                            void*          stream);

/**
   @}
   @name Node
   @{
*/

/**
   Create a new "simple" node that is just a string.

   This can be used to create blank, CURIE, or URI nodes from an already
   measured string or slice of a buffer, which avoids a strlen compared to the
   friendly constructors.  This may not be used for literals since those must
   be measured to set the SERD_HAS_NEWLINE and SERD_HAS_QUOTE flags.
*/
SERD_API
SerdNode*
serd_new_simple_node(SerdType type, const char* str, size_t len);

/**
   Create a new plain literal string node from `str`.
*/
SERD_API
SerdNode*
serd_new_string(const char* str);

/**
   Create a new plain literal string node from a prefix of `str`.
*/
SERD_API
SerdNode*
serd_new_substring(const char* str, size_t len);

/**
   Create a new literal node from substrings.

   This is a low-level constructor which can be used for constructing a literal
   from slices of a buffer (for example, directly from a Turtle literal)
   without copying.  In most cases, applications should use the simpler
   serd_new_plain_literal() or serd_new_typed_literal().

   Either `datatype_uri` or `lang` can be given, but not both, unless
   `datatype_uri` is rdf:langString in which case it is ignored.

   @param str Literal body string.
   @param str_len Length of `str` in bytes.
   @param datatype_uri Full datatype URI, or NULL.
   @param datatype_uri_len Length of `datatype_uri` in bytes.
   @param lang Language string.
   @param lang_len Length of `lang` in bytes.
*/
SERD_API
SerdNode*
serd_new_literal(const char* str,
                 size_t      str_len,
                 const char* datatype_uri,
                 size_t      datatype_uri_len,
                 const char* lang,
                 size_t      lang_len);

/**
   Create a new literal node from `str`.

   A plain literal has no datatype, but may have a language tag.  The `lang`
   may be NULL, in which case this is equivalent to `serd_new_string()`.
*/
SERD_API
SerdNode*
serd_new_plain_literal(const char* str, const char* lang);

/**
   Create a new typed literal node from `str`.

   A typed literal has no language tag, but may have a datatype.  The
   `datatype` may be NULL, in which case this is equivalent to
   `serd_new_string()`.
*/
SERD_API
SerdNode*
serd_new_typed_literal(const char* str, const SerdNode* datatype);

/**
   Create a new blank node.
*/
SERD_API
SerdNode*
serd_new_blank(const char* str);

/**
   Create a new CURIE node.
*/
SERD_API
SerdNode*
serd_new_curie(const char* str);

/**
   Return a deep copy of `node`.
*/
SERD_API
SerdNode*
serd_node_copy(const SerdNode* node);

/**
   Return true iff `a` is equal to `b`.
*/
SERD_API
bool
serd_node_equals(const SerdNode* a, const SerdNode* b);

/**
   Compare two nodes.

   Returns less than, equal to, or greater than zero if `a` is less than, equal
   to, or greater than `b`, respectively.  NULL is treated as less than any
   other node.
*/
SERD_API
int
serd_node_compare(const SerdNode* a, const SerdNode* b);

/**
   Create a new URI from a string.
*/
SERD_API
SerdNode*
serd_new_uri(const char* str);

/**
   Create a new URI from a string, resolved against a base URI.
*/
SERD_API
SerdNode*
serd_new_resolved_uri(const char* str, const SerdNode* base);

/**
   Resolve `node` against `base`.

   If `node` is not a relative URI, an equivalent new node is returned.
*/
SERD_API
SerdNode*
serd_node_resolve(const SerdNode* node, const SerdNode* base);

/**
   Create a new file URI node from a file system path and optional hostname.

   Backslashes in Windows paths will be converted, and other characters will be
   percent encoded as necessary.

   If `path` is relative, `hostname` is ignored.
   If `out` is not NULL, it will be set to the parsed URI.
*/
SERD_API
SerdNode*
serd_new_file_uri(const char* path, const char* hostname);

/**
   Create a new URI from a string, relative to a base URI.

   @param str URI string.

   @param base Base URI to make `str` relative to, if possible.

   @param root Optional root URI for resolution.

   The URI is made relative iff if it a child of `base` and `root`.  The
   optional `root` parameter must be a prefix of `base` and can be used keep
   up-references ("../") within a certain namespace.
*/
SERD_API
SerdNode*
serd_new_relative_uri(const char*     str,
                      const SerdNode* base,
                      const SerdNode* root);

/**
   Create a new node by serialising `d` into an xsd:decimal string.

   The resulting node will always contain a `.', start with a digit, and end
   with a digit (i.e. will have a leading and/or trailing `0' if necessary).
   It will never be in scientific notation.  A maximum of `frac_digits` digits
   will be written after the decimal point, but trailing zeros will
   automatically be omitted (except one if `d` is a round integer).

   Note that about 16 and 8 fractional digits are required to precisely
   represent a double and float, respectively.

   @param d The value for the new node.

   @param frac_digits The maximum number of digits after the decimal place.

   @param datatype Datatype of node, may be NULL in which case the node has
   type xsd:decimal.
*/
SERD_API
SerdNode*
serd_new_decimal(double d, unsigned frac_digits, const SerdNode* datatype);

/**
   Create a new node by serialising `i` into an xsd:integer string.

   @param i Integer value to serialise.

   @param datatype Datatype of node, may be NULL in which case the node has
   type xsd:integer.
*/
SERD_API
SerdNode*
serd_new_integer(int64_t i, const SerdNode* datatype);

/**
   Create a node by serialising `buf` into an xsd:base64Binary string.
   This function can be used to make a serialisable node out of arbitrary
   binary data, which can be decoded using serd_base64_decode().

   @param buf Raw binary input data.

   @param size Size of `buf`.

   @param wrap_lines Wrap lines at 76 characters to conform to RFC 2045.

   @param datatype Datatype of node, may be NULL in which case the node has
   type xsd:base64Binary.
*/
SERD_API
SerdNode*
serd_new_blob(const void*     buf,
              size_t          size,
              bool            wrap_lines,
              const SerdNode* datatype);

/**
   Return the type of a node (SERD_URI, SERD_BLANK, or SERD_LITERAL).
*/
SERD_API
SerdType
serd_node_get_type(const SerdNode* node);

/**
   Return the string value of a node.
*/
SERD_API
const char*
serd_node_get_string(const SerdNode* node);

/**
   Return the length of the string value of a node in bytes.
*/
SERD_API
size_t
serd_node_get_length(const SerdNode* node);

/**
   Return the datatype of a literal node, or NULL.
*/
SERD_API
const SerdNode*
serd_node_get_datatype(const SerdNode* node);

/**
   Return the language tag of a literal node, or NULL.
*/
SERD_API
const SerdNode*
serd_node_get_language(const SerdNode* node);

/**
   Return the flags (string properties) of a node.
*/
SERD_API
SerdNodeFlags
serd_node_get_flags(const SerdNode* node);

/**
   Free any data owned by `node`.

   Note that if `node` is itself dynamically allocated (which is not the case
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
   Sink (callback) for errors.

   @param handle Handle for user data.
   @param error Error description.
*/
typedef SerdStatus (*SerdErrorSink)(void*            handle,
                                    const SerdError* error);

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
typedef SerdStatus (*SerdStatementSink)(void*                handle,
                                        SerdStatementFlags   flags,
                                        const SerdStatement* statement);

/**
   Sink (callback) for anonymous node end markers.

   This is called to indicate that the anonymous node with the given
   `value` will no longer be referred to by any future statements
   (i.e. the anonymous serialisation of the node is finished).
*/
typedef SerdStatus (*SerdEndSink)(void*           handle,
                                  const SerdNode* node);

/**
   @}
   @name World
   @{
*/

/**
   Create a new Serd World.

   It is safe to use multiple worlds in one process, though no objects can be
   shared between worlds.
*/
SERD_API
SerdWorld*
serd_world_new(void);

/**
   Free `world`.
*/
SERD_API
void
serd_world_free(SerdWorld* world);

/**
   Return the nodes cache in `world`.

   The returned cache is owned by the world and contains various nodes used
   frequently by the implementation.  For convenience, it may be used to store
   additional nodes which will be freed when the world is freed.
*/
SERD_API
SerdNodes*
serd_world_get_nodes(SerdWorld* world);

/**
   Return a unique blank node.

   The returned node is valid only until the next time serd_world_get_blank()
   is called or the world is destroyed.
*/
SERD_API
const SerdNode*
serd_world_get_blank(SerdWorld* world);

/**
   Set a function to be called when errors occur.

   The `error_sink` will be called with `handle` as its first argument.  If
   no error function is set, errors are printed to stderr.
*/
SERD_API
void
serd_world_set_error_sink(SerdWorld*    world,
                          SerdErrorSink error_sink,
                          void*         handle);

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
   Copy an environment.
*/
SERD_API
SerdEnv*
serd_env_copy(const SerdEnv* env);

/**
   Return true iff `a` is equal to `b`.
*/
SERD_API
bool
serd_env_equals(const SerdEnv* a, const SerdEnv* b);

/**
   Free `ns`.
*/
SERD_API
void
serd_env_free(SerdEnv* env);

/**
   Get the current base URI.
*/
SERD_API
const SerdNode*
serd_env_get_base_uri(const SerdEnv* env);

/**
   Set the current base URI.
*/
SERD_API
SerdStatus
serd_env_set_base_uri(SerdEnv* env, const SerdNode* uri);

/**
   Set a namespace prefix.
*/
SERD_API
SerdStatus
serd_env_set_prefix(SerdEnv* env, const SerdNode* name, const SerdNode* uri);

/**
   Set a namespace prefix.
*/
SERD_API
SerdStatus
serd_env_set_prefix_from_strings(SerdEnv*    env,
                                 const char* name,
                                 const char* uri);

/**
   Qualify `uri` into a CURIE if possible.

   Returns null if `node` can not be qualified.
*/
SERD_API
SerdNode*
serd_env_qualify(const SerdEnv* env, const SerdNode* uri);

/**
   Expand `node`, which must be a CURIE or URI, to a full URI.

   Returns null if `node` can not be expanded.
*/
SERD_API
SerdNode*
serd_env_expand(const SerdEnv* env, const SerdNode* node);

/**
   Write all prefixes in `env` to `sink`.
*/
SERD_API
void
serd_env_write_prefixes(const SerdEnv* env, const SerdSink* sink);

/**
   @}
   @name Sink
   @{
*/

/**
   Create a new sink.

   Initially, the sink has no set functions and will do nothing.  Use the
   serd_sink_set_*_func functions to set handlers for various events.

   @param handle Opaque handle that will be passed to sink functions.
*/
SERD_API
SerdSink*
serd_sink_new(void* handle);

/**
   Free `sink`.
*/
SERD_API
void
serd_sink_free(SerdSink* sink);

/**
   Set a function to be called when the base URI changes.
*/
SERD_API
SerdStatus
serd_sink_set_base_func(SerdSink* sink, SerdBaseSink base_func);

/**
   Set a function to be called when a namespace prefix is defined.
*/
SERD_API
SerdStatus
serd_sink_set_prefix_func(SerdSink* sink, SerdPrefixSink prefix_func);

/**
   Set a function to be called when a statement is emitted.
*/
SERD_API
SerdStatus
serd_sink_set_statement_func(SerdSink* sink, SerdStatementSink statement_func);

/**
   Set a function to be called when an anonymous node ends.
*/
SERD_API
SerdStatus
serd_sink_set_end_func(SerdSink* sink, SerdEndSink end_func);

/**
   Set the base URI.

   Simple wrapper for the `SerdBaseSink` of `sink`.
*/
SERD_API
SerdStatus
serd_sink_write_base(const SerdSink* sink, const SerdNode* uri);

/**
   Set a namespace prefix.

   Simple wrapper for the `SerdPrefixSink` of `sink`.
*/
SERD_API
SerdStatus
serd_sink_write_prefix(const SerdSink* sink,
                       const SerdNode* name,
                       const SerdNode* uri);

/**
   Write a statement.

   Simple wrapper for the `SerdStatementSink` of `sink`.
*/
SERD_API
SerdStatus
serd_sink_write_statement(const SerdSink*      sink,
                          SerdStatementFlags   flags,
                          const SerdStatement* statement);

/**
   Write a statement from individual nodes.

   Simple wrapper for the `SerdStatementSink` of `sink`.
*/
SERD_API
SerdStatus
serd_sink_write(const SerdSink*    sink,
                SerdStatementFlags flags,
                const SerdNode*    subject,
                const SerdNode*    predicate,
                const SerdNode*    object,
                const SerdNode*    graph);

/**
   Mark the end of an anonymous node.

   Simple wrapper for the `SerdEndSink` of `sink`.
*/
SERD_API
SerdStatus
serd_sink_write_end(const SerdSink* sink, const SerdNode* node);

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
serd_reader_new(SerdWorld*      world,
                SerdSyntax      syntax,
                const SerdSink* sink,
                size_t          stack_size);

/**
   Enable or disable strict parsing.

   The reader is non-strict (lax) by default, which will tolerate URIs with
   invalid characters.  Setting strict will fail when parsing such files.  An
   error is printed for invalid input in either case.
*/
SERD_API
void
serd_reader_set_strict(SerdReader* reader, bool strict);

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
serd_reader_add_blank_prefix(SerdReader* reader,
                             const char* prefix);

/**
   Prepare to read from the file at a local file `uri`.
*/
SERD_API
SerdStatus
serd_reader_start_file(SerdReader* reader, const char* uri, bool bulk);

/**
   Prepare to read from a stream.

   The `read_func` is guaranteed to only be called for `page_size` elements
   with size 1 (i.e. `page_size` bytes).
*/
SERD_API
SerdStatus
serd_reader_start_stream(SerdReader*         reader,
                         SerdReadFunc        read_func,
                         SerdStreamErrorFunc error_func,
                         void*               stream,
                         const SerdNode*     name,
                         size_t              page_size);

/**
   Prepare to read from a string.
*/
SERD_API
SerdStatus
serd_reader_start_string(SerdReader*     reader,
                         const char*     utf8,
                         const SerdNode* name);

/**
   Read a single "chunk" of data during an incremental read.

   This function will read a single top level description, and return.  This
   may be a directive, statement, or several statements; essentially it reads
   until a '.' is encountered.  This is particularly useful for reading
   directly from a pipe or socket.
*/
SERD_API
SerdStatus
serd_reader_read_chunk(SerdReader* reader);

/**
   Read a complete document from the source.

   This function will continue pulling from the source until a complete
   document has been read.  Note that this may block when used with streams,
   for incremental reading use serd_reader_read_chunk().
*/
SERD_API
SerdStatus
serd_reader_read_document(SerdReader* reader);

/**
   Finish reading from the source.

   This will close the open file, if applicable, and ensure the reader has
   processed all input.
*/
SERD_API
SerdStatus
serd_reader_finish(SerdReader* reader);

/**
   Free `reader`.

   The reader will be finished via `serd_reader_finish()` if necessary.
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
serd_writer_new(SerdWorld*      world,
                SerdSyntax      syntax,
                SerdStyleFlags  style,
                SerdEnv*        env,
                SerdWriteFunc   write_func,
                void*           stream);

/**
   Free `writer`.
*/
SERD_API
void
serd_writer_free(SerdWriter* writer);

/**
   Return a sink interface that emits statements via `writer`.
*/
SERD_API
const SerdSink*
serd_writer_get_sink(SerdWriter* writer);

/**
   Return the env used by `writer`.
*/
SERD_API
SerdEnv*
serd_writer_get_env(SerdWriter* writer);

/**
   A convenience sink function for writing to a string.

   This function can be used as a SerdSink to write to a SerdBuffer which is
   resized as necessary with realloc().  The `stream` parameter must point to
   an initialized SerdBuffer.  When the write is finished, the string should be
   retrieved with serd_buffer_sink_finish().
*/
SERD_API
size_t
serd_buffer_sink(const void* buf, size_t size, size_t nmemb, void* stream);

/**
   Finish a serialisation to a buffer with serd_buffer_sink().

   The returned string is the result of the serialisation, which is NULL
   terminated (by this function) and owned by the caller.
*/
SERD_API
char*
serd_buffer_sink_finish(SerdBuffer* stream);

/**
   Set a prefix to be removed from matching blank node identifiers.
*/
SERD_API
void
serd_writer_chop_blank_prefix(SerdWriter* writer,
                              const char* prefix);

/**
   Set the current output base URI (and emit directive if applicable).

   Note this function can be safely casted to SerdBaseSink.
*/
SERD_API
SerdStatus
serd_writer_set_base_uri(SerdWriter*     writer,
                         const SerdNode* uri);

/**
   Set the current root URI.

   The root URI should be a prefix of the base URI.  The path of the root URI
   is the highest path any relative up-reference can refer to.  For example,
   with root <file:///foo/root> and base <file:///foo/root/base>,
   <file:///foo/root> will be written as <../>, but <file:///foo> will be
   written non-relatively as <file:///foo>.  If the root is not explicitly set,
   it defaults to the base URI, so no up-references will be created at all.
*/
SERD_API
SerdStatus
serd_writer_set_root_uri(SerdWriter*     writer,
                         const SerdNode* uri);

/**
   Finish a write.
*/
SERD_API
SerdStatus
serd_writer_finish(SerdWriter* writer);

/**
   @}
   @name Nodes
   @{
*/

/**
   Create a new node set.
*/
SERD_API
SerdNodes*
serd_nodes_new(void);

/**
   Free `nodes` and all nodes that are stored in it.

   Note that this invalidates any pointers previously returned from
   `serd_nodes_intern()` or `serd_nodes_manage()` calls on `nodes`.
*/
SERD_API
void
serd_nodes_free(SerdNodes* nodes);

/**
   Intern `node`.

   Multiple calls with equivalent nodes will return the same pointer.

   @return A node that is different than, but equivalent to, `node`.
*/
SERD_API
const SerdNode*
serd_nodes_intern(SerdNodes* nodes, const SerdNode* node);

/**
   Manage `node`.

   Like `serd_nodes_intern`, but takes ownership of `node`, freeing it and
   returning a previously interned/managed equivalent node if necessary.

   @return A node that is equivalent to `node`.
*/
SERD_API
const SerdNode*
serd_nodes_manage(SerdNodes* nodes, SerdNode* node);

/**
   Dereference `node`.

   Decrements the reference count of `node`, and frees the internally stored
   equivalent node if this was the last reference.  Does nothing if no node
   equivalent to `node` is stored in `nodes`.
*/
SERD_API
void
serd_nodes_deref(SerdNodes* nodes, const SerdNode* node);

/**
   @}
   @name Statement
   @{
*/

/**
   Return the given node in `statement`.
*/
SERD_API
const SerdNode*
serd_statement_get_node(const SerdStatement* statement, SerdField field);

/**
   Return the subject in `statement`.
*/
SERD_API
const SerdNode*
serd_statement_get_subject(const SerdStatement* statement);

/**
   Return the predicate in `statement`.
*/
SERD_API
const SerdNode*
serd_statement_get_predicate(const SerdStatement* statement);

/**
   Return the object in `statement`.
*/
SERD_API
const SerdNode*
serd_statement_get_object(const SerdStatement* statement);

/**
   Return the graph in `statement`.
*/
SERD_API
const SerdNode*
serd_statement_get_graph(const SerdStatement* statement);

/**
   Return the source location where `statement` originated, or NULL.
*/
SERD_API
const SerdCursor*
serd_statement_get_cursor(const SerdStatement* statement);

/**
   @}
   @name Cursor
   @{
*/

/**
   Create a new cursor

   Note that, to minimise model overhead, the cursor does not own the name
   node, so `name` must have a longer lifetime than the cursor for it to be
   valid.  That is, serd_cursor_get_name() will return exactly the pointer
   `name`, not a copy.  For cursors from models, this is the lifetime of the
   model.  For user-created cursors, the simplest way to handle this is to use
   `SerdNodes`.

   @param name The name of the document or stream (usually a file URI)
   @param line The line number in the document (1-based)
   @param col The column number in the document (1-based)
   @return A new cursor that must be freed with serd_cursor_free()
*/
SERD_API
SerdCursor*
serd_cursor_new(const SerdNode* name, unsigned line, unsigned col);

/// Return a copy of `cursor`
SERD_API
SerdCursor*
serd_cursor_copy(const SerdCursor* cursor);

/// Free `cursor`
SERD_API
void
serd_cursor_free(SerdCursor* cursor);

/// Return true iff `lhs` is equal to `rhs`
SERD_API
bool
serd_cursor_equals(const SerdCursor* lhs, const SerdCursor* rhs);

/**
   Return the document name.

   This is typically a file URI, but may be a descriptive string node for
   statements that originate from streams.
*/
SERD_API
const SerdNode*
serd_cursor_get_name(const SerdCursor* cursor);

/**
   Return the one-relative line number in the document.
*/
SERD_API
unsigned
serd_cursor_get_line(const SerdCursor* cursor);

/**
   Return the zero-relative column number in the line.
*/
SERD_API
unsigned
serd_cursor_get_column(const SerdCursor* cursor);

/**
   @}
   @}
*/

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* SERD_SERD_H */
