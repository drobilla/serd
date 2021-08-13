// Copyright 2011-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

/// @file serd.h API for Serd, a lightweight RDF syntax library

#ifndef SERD_SERD_H
#define SERD_SERD_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h> // IWYU pragma: keep

#if defined(_WIN32) && !defined(SERD_STATIC) && defined(SERD_INTERNAL)
#  define SERD_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(SERD_STATIC)
#  define SERD_API __declspec(dllimport)
#elif defined(__GNUC__)
#  define SERD_API __attribute__((visibility("default")))
#else
#  define SERD_API
#endif

#ifdef __GNUC__
#  define SERD_ALWAYS_INLINE_FUNC __attribute__((always_inline))
#  define SERD_CONST_FUNC __attribute__((const))
#  define SERD_MALLOC_FUNC __attribute__((malloc))
#  define SERD_PURE_FUNC __attribute__((pure))
#else
#  define SERD_ALWAYS_INLINE_FUNC
#  define SERD_CONST_FUNC
#  define SERD_MALLOC_FUNC
#  define SERD_PURE_FUNC
#endif

#if defined(__clang__) && __clang_major__ >= 7
#  define SERD_NONNULL _Nonnull
#  define SERD_NULLABLE _Nullable
#  define SERD_ALLOCATED _Null_unspecified
#else
#  define SERD_NONNULL
#  define SERD_NULLABLE
#  define SERD_ALLOCATED
#endif

#define SERD_PURE_API \
  SERD_API            \
  SERD_PURE_FUNC

#define SERD_CONST_API \
  SERD_API             \
  SERD_CONST_FUNC

#define SERD_MALLOC_API \
  SERD_API              \
  SERD_MALLOC_FUNC

#if defined(__MINGW32__)
#  define SERD_LOG_FUNC(fmt, a) __attribute__((format(gnu_printf, fmt, a)))
#elif defined(__GNUC__)
#  define SERD_LOG_FUNC(fmt, a) __attribute__((format(printf, fmt, a)))
#else
#  define SERD_LOG_FUNC(fmt, a)
#endif

#ifdef __cplusplus
extern "C" {
#  if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#  endif
#endif

/**
   @defgroup serd Serd C API
   @{
*/

/**
   @defgroup serd_version Version

   Serd uses a single [semantic version number](https://semver.org) which
   reflects changes to the C library ABI.

   @{
*/

/**
   The major version number of the serd library.

   Semver: Increments when incompatible API changes are made.
*/
#define SERD_MAJOR_VERSION 1

/**
   The minor version number of the serd library.

   Semver: Increments when functionality is added in a backwards compatible
   manner.
*/
#define SERD_MINOR_VERSION 1

/**
   The micro version number of the serd library.

   Semver: Increments when changes are made that do not affect the API, such as
   performance improvements or bug fixes.
*/
#define SERD_MICRO_VERSION 1

/**
   @}
   @defgroup serd_string_view String View
   @{
*/

/**
   An immutable slice of a string.

   This type is used for many string parameters, to allow referring to slices
   of strings in-place and to avoid redundant string measurement.
*/
typedef struct {
  const char* SERD_NONNULL buf; ///< Start of string
  size_t                   len; ///< Length of string in bytes
} SerdStringView;

/// Return a view of an empty string
SERD_ALWAYS_INLINE_FUNC
SERD_CONST_FUNC
static inline SerdStringView
serd_empty_string(void)
{
  const SerdStringView view = {"", 0U};
  return view;
}

/**
   Return a view of a substring, or a premeasured string.

   This makes either a view of a slice of a string (which may not be null
   terminated), or a view of a string that has already been measured.  This is
   faster than serd_string() for dynamic strings since it does not call
   `strlen`, so should be used when the length of the string is already known.

   @param str Pointer to the start of the substring.

   @param len Length of the substring in bytes, not including the trailing null
   terminator if present.
*/
SERD_ALWAYS_INLINE_FUNC
SERD_CONST_FUNC
static inline SerdStringView
serd_substring(const char* const SERD_NONNULL str, const size_t len)
{
  const SerdStringView view = {str, len};
  return view;
}

/**
   Return a view of an entire string by measuring it.

   This makes a view of the given string by measuring it with `strlen`.

   @param str Pointer to the start of a null-terminated C string, or null.
*/
SERD_ALWAYS_INLINE_FUNC
SERD_PURE_FUNC
static inline SerdStringView
serd_string(const char* const SERD_NONNULL str)
{
  const SerdStringView view = {str, strlen(str)};
  return view;
}

/**
   Return a view of an entire string by measuring it.

   This makes a view of the given string by measuring it with `strlen`.

   @param str Pointer to the start of a null-terminated C string, or null.
*/
SERD_PURE_FUNC
static inline SerdStringView
serd_optional_string(const char* const SERD_NULLABLE str)
{
  return str ? serd_string(str) : serd_empty_string();
}

/**
   @}
*/

/// A mutable buffer in memory
typedef struct {
  void* SERD_NULLABLE buf; ///< Buffer
  size_t              len; ///< Size of buffer in bytes
} SerdBuffer;

/**
   Free memory allocated by Serd.

   This function exists because some systems require memory allocated by a
   library to be freed by code in the same library.  It is otherwise equivalent
   to the standard C free() function.
*/
SERD_API
void
serd_free(void* SERD_NULLABLE ptr);

/**
   @defgroup serd_status Status Codes
   @{
*/

/// Return status code
typedef enum {
  SERD_SUCCESS,        ///< No error
  SERD_FAILURE,        ///< Non-fatal failure
  SERD_ERR_UNKNOWN,    ///< Unknown error
  SERD_ERR_BAD_SYNTAX, ///< Invalid syntax
  SERD_ERR_BAD_ARG,    ///< Invalid argument
  SERD_ERR_BAD_CURSOR, ///< Use of invalidated cursor
  SERD_ERR_NOT_FOUND,  ///< Not found
  SERD_ERR_ID_CLASH,   ///< Encountered clashing blank node IDs
  SERD_ERR_BAD_CURIE,  ///< Invalid CURIE or unknown namespace prefix
  SERD_ERR_INTERNAL,   ///< Unexpected internal error
  SERD_ERR_OVERFLOW,   ///< Stack overflow
  SERD_ERR_BAD_TEXT,   ///< Invalid text encoding
  SERD_ERR_BAD_WRITE,  ///< Error writing to file/stream
  SERD_ERR_NO_DATA,    ///< Unexpected end of input
  SERD_ERR_BAD_CALL,   ///< Invalid call
  SERD_ERR_BAD_URI,    ///< Invalid or unresolved URI
  SERD_ERR_BAD_DATA,   ///< Invalid data
  SERD_ERR_BAD_INDEX,  ///< No optimal model index available
} SerdStatus;

/**
   A status code with an associated byte count.

   This is returned by functions which write to a buffer to inform the caller
   about the size written, or in case of overflow, size required.
*/
typedef struct {
  /**
     Status code.

     This reports the status of the operation as usual, and also dictates the
     meaning of `count`.
  */
  SerdStatus status;

  /**
     Number of bytes written or required.

     On success, this is the total number of bytes written.  On
     #SERD_ERR_OVERFLOW, this is the number of bytes of output space that are
     required for success.
  */
  size_t count;
} SerdWriteResult;

/// Return a string describing a status code
SERD_CONST_API
const char* SERD_NONNULL
serd_strerror(SerdStatus status);

/**
   @}
   @defgroup serd_string String Utilities
   @{
*/

/**
   Return `path` as a canonical absolute path.

   This expands all symbolic links, relative references, and removes extra
   directory separators.  Null is returned on error, including if the path does
   not exist.

   @return A new string that must be freed with serd_free(), or null.
*/
SERD_API
char* SERD_NULLABLE
serd_canonical_path(const char* SERD_NONNULL path);

/**
   Compare two strings ignoring case.

   @return Less than, equal to, or greater than zero if `s1` is less than,
   equal to, or greater than `s2`, respectively.
*/
SERD_PURE_API
int
serd_strncasecmp(const char* SERD_NONNULL s1,
                 const char* SERD_NONNULL s2,
                 size_t                   n);

/**
   @}
   @defgroup serd_io_functions I/O Function Types

   These function types define the low-level interface that serd uses to read
   and write input.  They are deliberately compatible with the standard C
   functions for reading and writing from files.

   @{
*/

/**
   Function for reading input bytes from a stream.

   This has identical semantics to `fread`, but may set `errno` for more
   informative error reporting than supported by #SerdStreamErrorFunc.

   @param buf Output buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to read from (FILE* for fread).
   @return Number of elements (bytes) read, which is short on error.
*/
typedef size_t (*SerdReadFunc)(void* SERD_NONNULL buf,
                               size_t             size,
                               size_t             nmemb,
                               void* SERD_NONNULL stream);

/**
   Function for writing output bytes to a stream.

   This has identical semantics to `fwrite`, but may set `errno` for more
   informative error reporting than supported by #SerdStreamErrorFunc.

   @param buf Input buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to write to (FILE* for fread).
   @return Number of elements (bytes) written, which is short on error.
*/
typedef size_t (*SerdWriteFunc)(const void* SERD_NONNULL buf,
                                size_t                   size,
                                size_t                   nmemb,
                                void* SERD_NONNULL       stream);

/**
   Function to detect I/O stream errors.

   Identical semantics to `ferror`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdStreamErrorFunc)(void* SERD_NONNULL stream);

/**
   Function to close an I/O stream.

   Identical semantics to `fclose`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdStreamCloseFunc)(void* SERD_NONNULL stream);

/**
   @}
   @defgroup serd_syntax Syntax Utilities
   @{
*/

/// Syntax supported by serd
typedef enum {
  SERD_SYNTAX_EMPTY = 0U, ///< Empty syntax
  SERD_TURTLE       = 1U, ///< Terse triples http://www.w3.org/TR/turtle
  SERD_NTRIPLES     = 2U, ///< Flat triples http://www.w3.org/TR/n-triples/
  SERD_NQUADS       = 3U, ///< Flat quads http://www.w3.org/TR/n-quads/
  SERD_TRIG         = 4U, ///< Terse quads http://www.w3.org/TR/trig/
} SerdSyntax;

/**
   Get a syntax by name.

   Case-insensitive, supports "Turtle", "NTriples", "NQuads", and "TriG".

   @return The syntax with the given name, or the empty syntax if the name is
   unknown.
*/
SERD_PURE_API
SerdSyntax
serd_syntax_by_name(const char* SERD_NONNULL name);

/**
   Guess a syntax from a filename.

   This uses the file extension to guess the syntax of a file, for example a
   filename that ends with ".ttl" will be considered Turtle.

   @return The likely syntax of the given file, or the empty syntax if the
   extension is unknown.
*/
SERD_PURE_API
SerdSyntax
serd_guess_syntax(const char* SERD_NONNULL filename);

/**
   Return whether a syntax can represent multiple graphs in one document.

   @return True for #SERD_NQUADS and #SERD_TRIG, false otherwise.
*/
SERD_CONST_API
bool
serd_syntax_has_graphs(SerdSyntax syntax);

/**
   @}
   @defgroup serd_data Data
   @{
   @defgroup serd_uri URI
   @{
*/

/**
   A parsed view of a URI.

   This representation is designed for fast streaming.  It makes it possible to
   create relative URI references or resolve them into absolute URIs in-place
   without any string allocation.

   Each component refers to slices in other strings, so a URI view must outlive
   any strings it was parsed from.  Note that the components are not
   necessarily null-terminated.

   The scheme, authority, path, query, and fragment simply point to the string
   value of those components, not including any delimiters.  The path_prefix is
   a special component for storing relative or resolved paths.  If it points to
   a string (usually a base URI the URI was resolved against), then this string
   is prepended to the path.  Otherwise, the length is interpreted as the
   number of up-references ("../") that must be prepended to the path.
*/
typedef struct {
  SerdStringView scheme;      ///< Scheme
  SerdStringView authority;   ///< Authority
  SerdStringView path_prefix; ///< Path prefix for relative/resolved paths
  SerdStringView path;        ///< Path suffix
  SerdStringView query;       ///< Query
  SerdStringView fragment;    ///< Fragment
} SerdURIView;

static const SerdURIView SERD_URI_NULL =
  {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}};

/// Return true iff `string` starts with a valid URI scheme
SERD_PURE_API
bool
serd_uri_string_has_scheme(const char* SERD_NONNULL string);

/// Parse `string` and return a URI view that points into it
SERD_PURE_API
SerdURIView
serd_parse_uri(const char* SERD_NONNULL string);

/**
   Get the unescaped path and hostname from a file URI.

   The returned path and `*hostname` must be freed with serd_free().

   @param uri A file URI.
   @param hostname If non-NULL, set to the hostname, if present.
   @return A filesystem path.
*/
SERD_API
char* SERD_NULLABLE
serd_parse_file_uri(const char* SERD_NONNULL          uri,
                    char* SERD_NONNULL* SERD_NULLABLE hostname);

/**
   Return reference `r` resolved against `base`.

   This will make `r` an absolute URI if possible.

   @see [RFC3986 5.2.2](http://tools.ietf.org/html/rfc3986#section-5.2.2)

   @param r URI reference to make absolute, for example "child/path".

   @param base Base URI, for example "http://example.org/base/".

   @return An absolute URI, for example "http://example.org/base/child/path",
   or `r` if it is not a URI reference that can be resolved against `base`.
*/
SERD_PURE_API
SerdURIView
serd_resolve_uri(SerdURIView r, SerdURIView base);

/**
   Return `r` as a reference relative to `base` if possible.

   @see [RFC3986 5.2.2](http://tools.ietf.org/html/rfc3986#section-5.2.2)

   @param r URI to make relative, for example
   "http://example.org/base/child/path".

   @param base Base URI, for example "http://example.org/base".

   @return A relative URI reference, for example "child/path", `r` if it can
   not be made relative to `base`, or a null URI if `r` could be made relative
   to base, but the path prefix is already being used (most likely because `r`
   was previously a relative URI reference that was resolved against some
   base).
*/
SERD_PURE_API
SerdURIView
serd_relative_uri(SerdURIView r, SerdURIView base);

/**
   Return whether `r` can be written as a reference relative to `base`.

   For example, with `base` "http://example.org/base/", this returns true if
   `r` is also "http://example.org/base/", or something like
   "http://example.org/base/child" ("child")
   "http://example.org/base/child/grandchild#fragment"
   ("child/grandchild#fragment"),
   "http://example.org/base/child/grandchild?query" ("child/grandchild?query"),
   and so on.

   @return True if `r` and `base` are equal or if `r` is a child of `base`.
*/
SERD_PURE_API
bool
serd_uri_is_within(SerdURIView r, SerdURIView base);

/**
   Return the length of `uri` as a string.

   This can be used to get the expected number of bytes that will be written by
   serd_write_uri().

   @return A string length in bytes, not including the null terminator.
*/
SERD_PURE_API
size_t
serd_uri_string_length(SerdURIView uri);

/**
   Write `uri` as a string to `sink`.

   This will call `sink` several times to emit the URI.

   @param uri URI to write as a string.
   @param sink Sink to write string output to.
   @param stream Opaque user argument to pass to `sink`.

   @return The length of the written URI string (not including a null
   terminator), which may be less than `serd_uri_string_length(uri)` on error.
*/
SERD_API
size_t
serd_write_uri(SerdURIView                uri,
               SerdWriteFunc SERD_NONNULL sink,
               void* SERD_NONNULL         stream);

/**
   Write a file URI to `sink` from a path and optional hostname.

   Backslashes in Windows paths will be converted, and other characters will be
   percent encoded as necessary.

   If `path` is relative, `hostname` is ignored.

   @param path File system path.
   @param hostname Optional hostname.
   @param sink Sink to write string output to.
   @param stream Opaque user argument to pass to `sink`.

   @return The length of the written URI string (not including a null
   terminator).
*/
SERD_API
size_t
serd_write_file_uri(SerdStringView             path,
                    SerdStringView             hostname,
                    SerdWriteFunc SERD_NONNULL sink,
                    void* SERD_NONNULL         stream);

/**
   @}
   @defgroup serd_node Node
   @{
*/

/// An RDF node
typedef struct SerdNodeImpl SerdNode;

/**
   Type of a node.

   An RDF node, in the abstract sense, can be either a resource, literal, or a
   blank.  This type is more precise, because syntactically there are two ways
   to refer to a resource (by URI or CURIE).  Serd also has support for
   variable nodes to support some features, which are not RDF nodes.

   There are also two ways to refer to a blank node in syntax (by ID or
   anonymously), but this is handled by statement flags rather than distinct
   node types.
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
     @see [RFC3986](http://tools.ietf.org/html/rfc3986)
  */
  SERD_URI = 2,

  /**
     A blank node.

     Value is a blank node ID without any syntactic prefix, like "id3", which
     is meaningful only within this serialisation.  @see [RDF 1.1
     Turtle](http://www.w3.org/TR/turtle/#grammar-production-BLANK_NODE_LABEL)
  */
  SERD_BLANK = 3,

  /**
     A variable node.

     Value is a variable name without any syntactic prefix, like "name",
     which is meaningful only within this serialisation.  @see [SPARQL 1.1
     Query Language](https://www.w3.org/TR/sparql11-query/#rVar)
  */
  SERD_VARIABLE = 4,
} SerdNodeType;

/// Flags that describe the details of a node
typedef enum {
  SERD_IS_LONG      = 1U << 0U, ///< Literal node should be triple-quoted
  SERD_HAS_DATATYPE = 1U << 1U, ///< Literal node has datatype
  SERD_HAS_LANGUAGE = 1U << 2U, ///< Literal node has language
} SerdNodeFlag;

/// Bitwise OR of SerdNodeFlag values
typedef uint32_t SerdNodeFlags;

/**
   @defgroup serd_node_construction Construction

   This is the low-level node construction API, which can be used to construct
   nodes into existing buffers.  Advanced applications can use this to
   specially manage node memory, for example by allocating nodes on the stack,
   or with a special allocator.

   Note that nodes are "plain old data", so there is no need to destroy a
   constructed node, and nodes may be trivially copied, for example with
   memcpy().

   @{
*/

/**
   Construct a node into an existing buffer.

   This is the universal node constructor which can construct any node.  An
   error will be returned if the parameters do not make sense.  In particular,
   #SERD_HAS_DATATYPE or #SERD_HAS_LANGUAGE (but not both) may only be given if
   `type` is #SERD_LITERAL, and `meta` must be syntactically valid based on
   that flag.

   This function may also be used to determine the size of buffer required by
   passing a null buffer with zero size.

   @param buf_size The size of `buf` in bytes, or zero to only measure.

   @param buf Buffer where the node will be written, or null to only measure.

   @param type The type of the node to construct.

   @param string The string body of the node.

   @param flags Flags that describe the details of the node.

   @param meta The string value of the literal's metadata.  If
   #SERD_HAS_DATATYPE is set, then this must be an absolute datatype URI.  If
   #SERD_HAS_LANGUAGE is set, then this must be a language tag like "en-ca".
   Otherwise, it is ignored.

   @return A result with a `status` and a `count` of bytes written.  If the
   buffer is too small for the node, then `status` will be #SERD_ERR_OVERFLOW,
   and `count` will be set to the number of bytes required to successfully
   construct the node.
*/
SERD_API
SerdWriteResult
serd_node_construct(size_t              buf_size,
                    void* SERD_NULLABLE buf,
                    SerdNodeType        type,
                    SerdStringView      string,
                    SerdNodeFlags       flags,
                    SerdStringView      meta);

/**
   Construct a simple "token" node.

   "Token" is just a shorthand used in this API to refer to a node that is not
   a typed or tagged literal, that is, a node that is just one string.  This
   can be used to create URIs, blank nodes, variables, and simple string
   literals.

   Note that string literals constructed with this function will have no flags
   set, and so will be written as "short" literals (not triple-quoted).  To
   construct long literals, use the more advanced serd_construct_literal() with
   the #SERD_IS_LONG flag.

   See the serd_node_construct() documentation for details on buffer usage and
   the return value.
*/
SERD_API
SerdWriteResult
serd_node_construct_token(size_t              buf_size,
                          void* SERD_NULLABLE buf,
                          SerdNodeType        type,
                          SerdStringView      string);

/**
   Construct a URI node from a parsed URI.

   This is similar to serd_node_construct_token(), but will write a parsed URI
   into the new node.  This can be used to resolve a relative URI reference or
   expand a CURIE directly into a node without needing to allocate the URI
   string separately.
*/
SerdWriteResult
serd_node_construct_uri(size_t              buf_size,
                        void* SERD_NULLABLE buf,
                        SerdURIView         uri);

/**
   Construct a file URI node from a path and optional hostname.

   This is similar to serd_node_construct_token(), but will create a new file
   URI from a file path and optional hostname, performing any necessary
   escaping.
*/
SerdWriteResult
serd_node_construct_file_uri(size_t              buf_size,
                             void* SERD_NULLABLE buf,
                             SerdStringView      path,
                             SerdStringView      hostname);

/**
   Construct a literal node with an optional datatype or language.

   Either a datatype (which must be an absolute URI) or a language (which must
   be an RFC5646 language tag) may be given, but not both.

   This is the most general literal constructor, which can be used to construct
   any literal node.  This works like serd_node_construct(), see its
   documentation for details.
*/
SERD_API
SerdWriteResult
serd_node_construct_literal(size_t              buf_size,
                            void* SERD_NULLABLE buf,
                            SerdStringView      string,
                            SerdNodeFlags       flags,
                            SerdStringView      meta);

/**
   Construct a canonical xsd:boolean literal.

   The constructed node will be either "true" or "false", with datatype
   xsd:boolean.

   This is a convenience wrapper for serd_node_construct_literal() that
   constructs a node directly from a `bool`.
*/
SerdWriteResult
serd_node_construct_boolean(size_t              buf_size,
                            void* SERD_NULLABLE buf,
                            bool                value);

/**
   Construct a canonical xsd:decimal literal.

   The constructed node will be an xsd:decimal literal, like "12.34", with
   datatype xsd:decimal.

   The node will always contain a '.', start with a digit, and end with a digit
   (a leading and/or trailing '0' will be added if necessary), for example,
   "1.0".  It will never be in scientific notation.

   This is a convenience wrapper for serd_node_construct_literal() that
   constructs a node directly from a `double`.
*/
SerdWriteResult
serd_node_construct_decimal(size_t              buf_size,
                            void* SERD_NULLABLE buf,
                            double              value);

/**
   Construct a canonical xsd:double literal.

   The constructed node will be an xsd:double literal, like "1.23E45", with
   datatype xsd:double.  A canonical xsd:double is always in scientific
   notation.

   This is a convenience wrapper for serd_node_construct_literal() that
   constructs a node directly from a `double`.
*/
SerdWriteResult
serd_node_construct_double(size_t              buf_size,
                           void* SERD_NULLABLE buf,
                           double              value);

/**
   Construct a canonical xsd:float literal.

   The constructed node will be an xsd:float literal, like "1.23E45", with
   datatype xsd:float.  A canonical xsd:float is always in scientific notation.

   Uses identical formatting to serd_node_construct_double(), except with at
   most 9 significant digits (under 14 characters total).

   This is a convenience wrapper for serd_node_construct_literal() that
   constructs a node directly from a `float`.
*/
SerdWriteResult
serd_node_construct_float(size_t              buf_size,
                          void* SERD_NULLABLE buf,
                          float               value);

/**
   Construct a canonical xsd:integer literal.

   The constructed node will be an xsd:integer literal like "1234", with the
   given datatype, or datatype xsd:integer if none is given.  It is the
   caller's responsibility to ensure that the value is within the range of the
   given datatype.
*/
SerdWriteResult
serd_node_construct_integer(size_t              buf_size,
                            void* SERD_NULLABLE buf,
                            int64_t             value,
                            SerdStringView      datatype);

/**
   Construct a canonical xsd:base64Binary literal.

   The constructed node will be an xsd:base64Binary literal like "Zm9vYmFy",
   with datatype xsd:base64Binary.
*/
SerdWriteResult
serd_node_construct_base64(size_t                   buf_size,
                           void* SERD_NULLABLE      buf,
                           size_t                   value_size,
                           const void* SERD_NONNULL value,
                           SerdStringView           datatype);

/**
   @}
   @defgroup serd_node_allocation Dynamic Allocation

   This is a convenient higher-level node construction API which allocates
   nodes on the heap.  The returned nodes must be freed with serd_node_free().

   Note that in most cases it is better to use a #SerdNodes instead of managing
   individual node allocations.

   @{
*/

/**
   Create a new node of any type.

   This is a wrapper for serd_node_construct() that allocates a new node on the
   heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_new(SerdNodeType   type,
              SerdStringView string,
              SerdNodeFlags  flags,
              SerdStringView meta);

/**
   Create a new simple "token" node.

   This is a wrapper for serd_node_construct_token() that allocates a new node
   on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_token(SerdNodeType type, SerdStringView string);

/**
   Create a new string literal node.

   This is a trivial wrapper for serd_new_token() that passes `SERD_LITERAL`
   for the type.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_string(SerdStringView string);

/**
   Create a new URI node from a string.

   This is a wrapper for serd_node_construct_uri() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_uri(SerdStringView string);

/**
   Create a new URI node from a parsed URI.

   This is a wrapper for serd_node_construct_uri() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_parsed_uri(SerdURIView uri);

/**
   Create a new file URI node from a path and optional hostname.

   This is a wrapper for serd_node_construct_file_uri() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_file_uri(SerdStringView path, SerdStringView hostname);

/**
   Create a new literal node.

   This is a wrapper for serd_node_construct_literal() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_literal(SerdStringView string,
                 SerdNodeFlags  flags,
                 SerdStringView meta);

/**
   Create a new canonical xsd:boolean node.

   This is a wrapper for serd_node_construct_boolean() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_boolean(bool b);

/**
   Create a new canonical xsd:decimal literal.

   This is a wrapper for serd_node_construct_decimal() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_decimal(double d);

/**
   Create a new canonical xsd:double literal.

   This is a wrapper for serd_node_construct_double() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_double(double d);

/**
   Create a new canonical xsd:float literal.

   This is a wrapper for serd_node_construct_float() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_float(float f);

/**
   Create a new canonical xsd:integer literal.

   This is a wrapper for serd_node_construct_integer() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_integer(int64_t i, SerdStringView datatype);

/**
   Create a new canonical xsd:base64Binary literal.

   This is a wrapper for serd_node_construct_base64() that allocates a new
   node on the heap.

   @return A newly allocated node that must be freed with serd_node_free(), or
   null.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_base64(const void* SERD_NONNULL buf,
                size_t                   size,
                SerdStringView           datatype);

/**
   @}
*/

/**
   Return the value of `node` as a boolean.

   This will work for booleans, and numbers of any datatype if they are 0 or
   1.

   @return The value of `node` as a `bool`, or `false` on error.
*/
SERD_API
bool
serd_get_boolean(const SerdNode* SERD_NONNULL node);

/**
   Return the value of `node` as a double.

   This will coerce numbers of any datatype to double, if the value fits.

   @return The value of `node` as a `double`, or NaN on error.
*/
SERD_API
double
serd_get_double(const SerdNode* SERD_NONNULL node);

/**
   Return the value of `node` as a float.

   This will coerce numbers of any datatype to float, if the value fits.

   @return The value of `node` as a `float`, or NaN on error.
*/
SERD_API
float
serd_get_float(const SerdNode* SERD_NONNULL node);

/**
   Return the value of `node` as a long (signed 64-bit integer).

   This will coerce numbers of any datatype to long, if the value fits.

   @return The value of `node` as a `int64_t`, or 0 on error.
*/
SERD_API
int64_t
serd_get_integer(const SerdNode* SERD_NONNULL node);

/**
   Return the maximum size of a decoded base64 node in bytes.

   This returns an upper bound on the number of bytes that would be decoded by
   serd_get_base64().  This is calculated as a simple constant-time arithmetic
   expression based on the length of the encoded string, so may be larger than
   the actual size of the data due to things like additional whitespace.
*/
SERD_PURE_API
size_t
serd_get_base64_size(const SerdNode* SERD_NONNULL node);

/**
   Decode a base64 node.

   This function can be used to decode a node created with serd_new_base64().

   @param node A literal node which is an encoded base64 string.

   @param buf_size The size of `buf` in bytes.

   @param buf Buffer where decoded data will be written.

   @return On success, #SERD_SUCCESS is returned along with the number of bytes
   written.  If the output buffer is too small, then #SERD_ERR_OVERFLOW is
   returned along with the number of bytes required for successful decoding.
*/
SERD_API
SerdWriteResult
serd_get_base64(const SerdNode* SERD_NONNULL node,
                size_t                       buf_size,
                void* SERD_NONNULL           buf);

/// Return a deep copy of `node`
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_copy(const SerdNode* SERD_NULLABLE node);

/// Free any data owned by `node`
SERD_API
void
serd_node_free(SerdNode* SERD_NULLABLE node);

/// Return the type of a node (SERD_URI, SERD_BLANK, or SERD_LITERAL)
SERD_PURE_API
SerdNodeType
serd_node_type(const SerdNode* SERD_NONNULL node);

/// Return the node's string
SERD_CONST_API
const char* SERD_NONNULL
serd_node_string(const SerdNode* SERD_NONNULL node);

/// Return the length of the node's string in bytes (excluding terminator)
SERD_PURE_API
size_t
serd_node_length(const SerdNode* SERD_NULLABLE node);

/**
   Return a view of the string in a node.

   This is a convenience wrapper for serd_node_string() and serd_node_length()
   that can be used to get both in a single call.
*/
SERD_PURE_API
SerdStringView
serd_node_string_view(const SerdNode* SERD_NONNULL node);

/**
   Return a parsed view of the URI in a node.

   It is best to check the node type before calling this function, though it is
   safe to call on non-URI nodes.  In that case, it will return a null view
   with all fields zero.

   Note that this parses the URI string contained in the node, so it is a good
   idea to keep the value if you will be using it several times in the same
   scope.
*/
SERD_PURE_API
SerdURIView
serd_node_uri_view(const SerdNode* SERD_NONNULL node);

/// Return the flags (string properties) of a node
SERD_PURE_API
SerdNodeFlags
serd_node_flags(const SerdNode* SERD_NONNULL node);

/// Return the datatype of the literal node, if present
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_node_datatype(const SerdNode* SERD_NONNULL node);

/// Return the language tag of the literal node, if present
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_node_language(const SerdNode* SERD_NONNULL node);

/// Return true iff `a` is equal to `b`
SERD_PURE_API
bool
serd_node_equals(const SerdNode* SERD_NULLABLE a,
                 const SerdNode* SERD_NULLABLE b);

/**
   Compare two nodes.

   Returns less than, equal to, or greater than zero if `a` is less than, equal
   to, or greater than `b`, respectively.  NULL is treated as less than any
   other node.

   Nodes are ordered first by type, then by string value, then by language or
   datatype, if present.
*/
SERD_PURE_API
int
serd_node_compare(const SerdNode* SERD_NONNULL a,
                  const SerdNode* SERD_NONNULL b);

/**
   @}
   @defgroup serd_nodes Nodes
   @{
*/

/// Hashing node container for interning and simplified memory management
typedef struct SerdNodesImpl SerdNodes;

/// Create a new node set
SERD_API
SerdNodes* SERD_ALLOCATED
serd_nodes_new(void);

/**
   Free `nodes` and all nodes that are stored in it.

   Note that this invalidates any node pointers previously returned from
   `nodes`.
*/
SERD_API
void
serd_nodes_free(SerdNodes* SERD_NULLABLE nodes);

/// Return the number of interned nodes
SERD_PURE_API
size_t
serd_nodes_size(const SerdNodes* SERD_NONNULL nodes);

/**
   Return the existing interned copy of a node if it exists.

   This either returns an equivalent to the given node, or null if this node
   has not been interned.
*/
SERD_API
const SerdNode* SERD_NULLABLE
serd_nodes_get(const SerdNodes* SERD_NONNULL nodes,
               const SerdNode* SERD_NULLABLE node);

/**
   Intern `node`.

   Multiple calls with equivalent nodes will return the same pointer.

   @return A node that is different than, but equivalent to, `node`.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_intern(SerdNodes* SERD_NONNULL       nodes,
                  const SerdNode* SERD_NULLABLE node);

/**
   Make a simple "token" node.

   "Token" is just a shorthand used in this API to refer to a node that is not
   a typed or tagged literal, that is, a node that is just one string.  This
   can be used to make URIs, blank nodes, variables, and simple string
   literals.

   Note that string literals constructed with this function will have no flags
   set, and so will be written as "short" literals (not triple-quoted).  To
   construct long literals, use the more advanced serd_nodes_literal() with the
   #SERD_IS_LONG flag.

   A new node will be added if an equivalent node is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_token(SerdNodes* SERD_NONNULL nodes,
                 SerdNodeType            type,
                 SerdStringView          string);

/**
   Make a string node.

   A new node will be added if an equivalent node is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_string(SerdNodes* SERD_NONNULL nodes, SerdStringView string);

/**
   Make a URI node from a string.

   A new node will be constructed with serd_node_construct_token() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_uri(SerdNodes* SERD_NONNULL nodes, SerdStringView string);

/**
   Make a URI node from a parsed URI.

   A new node will be constructed with serd_node_construct_uri() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_parsed_uri(SerdNodes* SERD_NONNULL nodes, SerdURIView uri);

/**
   Make a file URI node from a path and optional hostname.

   A new node will be constructed with serd_node_construct_file_uri() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_file_uri(SerdNodes* SERD_NONNULL nodes,
                    SerdStringView          path,
                    SerdStringView          hostname);

/**
   Make a literal node with optional datatype or language.

   This can create complex literals with an associated datatype URI or language
   tag, and control whether a literal should be written as a short or long
   (triple-quoted) string.

   @param nodes The node set to get this literal from.

   @param string The string value of the literal.

   @param flags Flags to describe the literal and its metadata.  Note that at
   most one of #SERD_HAS_DATATYPE and #SERD_HAS_LANGUAGE may be set.

   @param meta The string value of the literal's metadata.  If
   #SERD_HAS_DATATYPE is set, then this must be an absolute datatype URI.  If
   #SERD_HAS_LANGUAGE is set, then this must be an RFC 5646 language tag like
   "en-ca".  Otherwise, it is ignored.

   @return A newly allocated literal node that must be freed with
   serd_node_free(), or null if the arguments are invalid or allocation failed.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_literal(SerdNodes* SERD_NONNULL nodes,
                   SerdStringView          string,
                   SerdNodeFlags           flags,
                   SerdStringView          meta);

/**
   Make a canonical xsd:boolean node.

   A new node will be constructed with serd_node_construct_boolean() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_boolean(SerdNodes* SERD_NONNULL nodes, bool value);

/**
   Make a canonical xsd:decimal node.

   A new node will be constructed with serd_node_construct_decimal() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_decimal(SerdNodes* SERD_NONNULL nodes, double value);

/**
   Make a canonical xsd:double node.

   A new node will be constructed with serd_node_construct_double() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_double(SerdNodes* SERD_NONNULL nodes, double value);

/**
   Make a canonical xsd:float node.

   A new node will be constructed with serd_node_construct_float() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_float(SerdNodes* SERD_NONNULL nodes, float value);

/**
   Make a canonical xsd:integer node.

   A new node will be constructed with serd_node_construct_integer() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_integer(SerdNodes* SERD_NONNULL nodes,
                   int64_t                 value,
                   SerdStringView          datatype);

/**
   Make a canonical xsd:base64Binary node.

   A new node will be constructed with serd_node_construct_base64() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_base64(SerdNodes* SERD_NONNULL  nodes,
                  const void* SERD_NONNULL value,
                  size_t                   value_size,
                  SerdStringView           datatype);

/**
   Make a blank node.

   A new node will be constructed with serd_node_construct_token() if an
   equivalent one is not already in the set.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_blank(SerdNodes* SERD_NONNULL nodes, SerdStringView string);

/**
   Dereference `node`.

   Decrements the reference count of `node`, and frees the internally stored
   equivalent node if this was the last reference.  Does nothing if no node
   equivalent to `node` is stored in `nodes`.
*/
SERD_API
void
serd_nodes_deref(SerdNodes* SERD_NONNULL       nodes,
                 const SerdNode* SERD_NULLABLE node);

/**
   @}
   @defgroup serd_caret Caret
   @{
*/

/// The origin of a statement in a text document
typedef struct SerdCaretImpl SerdCaret;

/**
   Create a new caret.

   Note that, to minimise model overhead, the caret does not own the name
   node, so `name` must have a longer lifetime than the caret for it to be
   valid.  That is, serd_caret_name() will return exactly the pointer
   `name`, not a copy.

   @param name The name of the document or stream (usually a file URI)
   @param line The line number in the document (1-based)
   @param col The column number in the document (1-based)
   @return A new caret that must be freed with serd_caret_free()
*/
SERD_API
SerdCaret* SERD_ALLOCATED
serd_caret_new(const SerdNode* SERD_NONNULL name, unsigned line, unsigned col);

/// Return a copy of `caret`
SERD_API
SerdCaret* SERD_ALLOCATED
serd_caret_copy(const SerdCaret* SERD_NULLABLE caret);

/// Free `caret`
SERD_API
void
serd_caret_free(SerdCaret* SERD_NULLABLE caret);

/// Return true iff `lhs` is equal to `rhs`
SERD_PURE_API
bool
serd_caret_equals(const SerdCaret* SERD_NULLABLE lhs,
                  const SerdCaret* SERD_NULLABLE rhs);

/**
   Return the document name.

   This is typically a file URI, but may be a descriptive string node for
   statements that originate from streams.
*/
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_caret_name(const SerdCaret* SERD_NONNULL caret);

/// Return the one-relative line number in the document
SERD_PURE_API
unsigned
serd_caret_line(const SerdCaret* SERD_NONNULL caret);

/// Return the zero-relative column number in the line
SERD_PURE_API
unsigned
serd_caret_column(const SerdCaret* SERD_NONNULL caret);

/**
   @}
   @defgroup serd_statement Statement
   @{
*/

/// A subject, predicate, and object, with optional graph context
typedef struct SerdStatementImpl SerdStatement;

/// Index of a node in a statement
typedef enum {
  SERD_SUBJECT   = 0U, ///< Subject
  SERD_PREDICATE = 1U, ///< Predicate ("key")
  SERD_OBJECT    = 2U, ///< Object ("value")
  SERD_GRAPH     = 3U, ///< Graph ("context")
} SerdField;

/**
   Create a new statement.

   Note that, to minimise model overhead, statements do not own their nodes, so
   they must have a longer lifetime than the statement for it to be valid.  For
   statements in models, this is the lifetime of the model.  For user-created
   statements, the simplest way to handle this is to use `SerdNodes`.

   @param s The subject
   @param p The predicate ("key")
   @param o The object ("value")
   @param g The graph ("context")
   @param caret Optional caret at the origin of this statement
   @return A new statement that must be freed with serd_statement_free()
*/
SERD_API
SerdStatement* SERD_ALLOCATED
serd_statement_new(const SerdNode* SERD_NONNULL   s,
                   const SerdNode* SERD_NONNULL   p,
                   const SerdNode* SERD_NONNULL   o,
                   const SerdNode* SERD_NULLABLE  g,
                   const SerdCaret* SERD_NULLABLE caret);

/// Return a copy of `statement`
SERD_API
SerdStatement* SERD_ALLOCATED
serd_statement_copy(const SerdStatement* SERD_NULLABLE statement);

/// Free `statement`
SERD_API
void
serd_statement_free(SerdStatement* SERD_NULLABLE statement);

/// Return the given node of the statement
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_statement_node(const SerdStatement* SERD_NONNULL statement,
                    SerdField                         field);

/// Return the subject of the statement
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_statement_subject(const SerdStatement* SERD_NONNULL statement);

/// Return the predicate of the statement
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_statement_predicate(const SerdStatement* SERD_NONNULL statement);

/// Return the object of the statement
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_statement_object(const SerdStatement* SERD_NONNULL statement);

/// Return the graph of the statement
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_statement_graph(const SerdStatement* SERD_NONNULL statement);

/// Return the source location where the statement originated, or NULL
SERD_PURE_API
const SerdCaret* SERD_NULLABLE
serd_statement_caret(const SerdStatement* SERD_NONNULL statement);

/**
   Return true iff `a` is equal to `b`, ignoring statement caret metadata.

   Only returns true if nodes are equivalent, does not perform wildcard
   matching.
*/
SERD_PURE_API
bool
serd_statement_equals(const SerdStatement* SERD_NULLABLE a,
                      const SerdStatement* SERD_NULLABLE b);

/**
   Return true iff the statement matches the given pattern.

   Nodes match if they are equivalent, or if one of them is NULL.  The
   statement matches if every node matches.
*/
SERD_PURE_API
bool
serd_statement_matches(const SerdStatement* SERD_NONNULL statement,
                       const SerdNode* SERD_NULLABLE     subject,
                       const SerdNode* SERD_NULLABLE     predicate,
                       const SerdNode* SERD_NULLABLE     object,
                       const SerdNode* SERD_NULLABLE     graph);

/**
   @}
   @}
   @defgroup serd_world World
   @{
*/

/// Global library state
typedef struct SerdWorldImpl SerdWorld;

/**
   Create a new Serd World.

   It is safe to use multiple worlds in one process, though no objects can be
   shared between worlds.
*/
SERD_MALLOC_API
SerdWorld* SERD_ALLOCATED
serd_world_new(void);

/// Free `world`
SERD_API
void
serd_world_free(SerdWorld* SERD_NULLABLE world);

/**
   Return the nodes cache in `world`.

   The returned cache is owned by the world and contains various nodes used
   frequently by the implementation.  For convenience, it may be used to store
   additional nodes which will be freed when the world is freed.
*/
SERD_PURE_API
SerdNodes* SERD_NONNULL
serd_world_nodes(SerdWorld* SERD_NONNULL world);

/**
   Return a unique blank node.

   The returned node is valid only until the next time serd_world_get_blank()
   is called or the world is destroyed.
*/
SERD_API
const SerdNode* SERD_NONNULL
serd_world_get_blank(SerdWorld* SERD_NONNULL world);

/**
   @}
   @defgroup serd_logging Logging
   @{
*/

/// Log entry level, compatible with syslog
typedef enum {
  SERD_LOG_LEVEL_EMERGENCY, ///< Emergency, system is unusable
  SERD_LOG_LEVEL_ALERT,     ///< Action must be taken immediately
  SERD_LOG_LEVEL_CRITICAL,  ///< Critical condition
  SERD_LOG_LEVEL_ERROR,     ///< Error
  SERD_LOG_LEVEL_WARNING,   ///< Warning
  SERD_LOG_LEVEL_NOTICE,    ///< Normal but significant condition
  SERD_LOG_LEVEL_INFO,      ///< Informational message
  SERD_LOG_LEVEL_DEBUG,     ///< Debug message
} SerdLogLevel;

/**
   A structured log field.

   Fields are used to add metadata to log messages.  Syslog-compatible keys
   should be used where possible, otherwise, keys should be namespaced to
   prevent clashes.

   Serd itself uses the following keys:

   - ERRNO - The `errno` of the original system error if any (decimal string)
   - SERD_COL - The 1-based column number in the file (decimal string)
   - SERD_FILE - The file which caused this message (string)
   - SERD_LINE - The 1-based line number in the file (decimal string)
   - SERD_CHECK - The check/warning/etc that triggered this message (string)
*/
typedef struct {
  const char* SERD_NONNULL key;   ///< Field name
  const char* SERD_NONNULL value; ///< Field value
} SerdLogField;

/**
   Function for handling log messages.

   By default, the log is printed to `stderr`.  This can be overridden by
   passing a function of this type to serd_set_log_func().

   @param handle Pointer to opaque user data.
   @param level Log level.
   @param n_fields Number of entries in `fields`.
   @param fields An array of `n_fields` extra log fields.
   @param message Log message.
*/
typedef SerdStatus (*SerdLogFunc)(void* SERD_NULLABLE               handle,
                                  SerdLogLevel                      level,
                                  size_t                            n_fields,
                                  const SerdLogField* SERD_NULLABLE fields,
                                  SerdStringView                    message);

/// A #SerdLogFunc that does nothing (for suppressing log output)
SERD_CONST_API
SerdStatus
serd_quiet_log_func(void* SERD_NULLABLE               handle,
                    SerdLogLevel                      level,
                    size_t                            n_fields,
                    const SerdLogField* SERD_NULLABLE fields,
                    SerdStringView                    message);

/**
   Set a function to be called with log messages (typically errors).

   If no custom logging function is set, then messages are printed to stderr.

   @param world World that will send log entries to the given function.

   @param log_func Log function to call for every log message.  Each call to
   this function represents a complete log message with an implicit trailing
   newline.

   @param handle Opaque handle that will be passed to every invocation of
   `log_func`.
*/
SERD_API
void
serd_set_log_func(SerdWorld* SERD_NONNULL   world,
                  SerdLogFunc SERD_NULLABLE log_func,
                  void* SERD_NULLABLE       handle);

/**
   Write a message to the log with a `va_list`.

   This is the fundamental and most powerful function for writing entries to
   the log, the others are convenience wrappers that ultimately call this.

   This writes a single complete entry to the log, and so may not be used to
   print parts of a line like a more general printf-like function.  There
   should be no trailing newline in `fmt`.  Arguments following `fmt` should
   correspond to conversion specifiers in the format string as in printf from
   the standard C library.

   @param world World to log to.
   @param level Log level.
   @param n_fields Number of entries in `fields`.
   @param fields An array of `n_fields` extra log fields.
   @param fmt Format string.
   @param args Arguments for `fmt`.

   @return A status code, which is always #SERD_SUCCESS with the default log
   function.  If a custom log function is set with serd_set_log_func() and it
   returns an error, then that error is returned here.
*/
SERD_API
SERD_LOG_FUNC(5, 0)
SerdStatus
serd_vxlogf(const SerdWorld* SERD_NONNULL     world,
            SerdLogLevel                      level,
            size_t                            n_fields,
            const SerdLogField* SERD_NULLABLE fields,
            const char* SERD_NONNULL          fmt,
            va_list                           args);

/**
   Write a message to the log with extra fields.

   This is a convenience wrapper for serd_vxlogf() that takes the format
   arguments directly.
*/
SERD_API
SERD_LOG_FUNC(5, 6)
SerdStatus
serd_xlogf(const SerdWorld* SERD_NONNULL     world,
           SerdLogLevel                      level,
           size_t                            n_fields,
           const SerdLogField* SERD_NULLABLE fields,
           const char* SERD_NONNULL          fmt,
           ...);

/**
   Write a simple message to the log.

   This is a convenience wrapper for serd_vxlogf() which sets no extra fields.
*/
SERD_API
SERD_LOG_FUNC(3, 0)
SerdStatus
serd_vlogf(const SerdWorld* SERD_NONNULL world,
           SerdLogLevel                  level,
           const char* SERD_NONNULL      fmt,
           va_list                       args);

/**
   Write a simple message to the log.

   This is a convenience wrapper for serd_vlogf() that takes the format
   arguments directly.
*/
SERD_API
SERD_LOG_FUNC(3, 4)
SerdStatus
serd_logf(const SerdWorld* SERD_NONNULL world,
          SerdLogLevel                  level,
          const char* SERD_NONNULL      fmt,
          ...);

/**
   Write a message to the log with a caret position.

   This is a convenience wrapper for serd_vxlogf() which sets `SERD_FILE`,
   `SERD_LINE`, and `SERD_COL` to the position of the given caret.  Entries are
   typically printed with a GCC-style prefix like "file.ttl:16:4".
*/
SERD_API
SERD_LOG_FUNC(4, 0)
SerdStatus
serd_vlogf_at(const SerdWorld* SERD_NONNULL  world,
              SerdLogLevel                   level,
              const SerdCaret* SERD_NULLABLE caret,
              const char* SERD_NONNULL       fmt,
              va_list                        args);

/**
   Write a message to the log with a caret position.

   This is a convenience wrapper for serd_vlogf_at() that takes the format
   arguments directly.
*/
SERD_API
SERD_LOG_FUNC(4, 5)
SerdStatus
serd_logf_at(const SerdWorld* SERD_NONNULL  world,
             SerdLogLevel                   level,
             const SerdCaret* SERD_NULLABLE caret,
             const char* SERD_NONNULL       fmt,
             ...);

/**
   @}
   @defgroup serd_streaming Data Streaming
   @{
*/

/**
   @defgroup serd_event Events
   @{
*/

/// Type of a SerdEvent
typedef enum {
  SERD_BASE      = 1, ///< Base URI changed
  SERD_PREFIX    = 2, ///< New URI prefix
  SERD_STATEMENT = 3, ///< Statement
  SERD_END       = 4, ///< End of anonymous node
} SerdEventType;

/// Flags indicating inline abbreviation information for a statement
typedef enum {
  SERD_EMPTY_S = 1U << 0U, ///< Empty blank node subject
  SERD_EMPTY_G = 1U << 1U, ///< Empty blank node graph
  SERD_ANON_S  = 1U << 2U, ///< Start of anonymous subject
  SERD_ANON_O  = 1U << 3U, ///< Start of anonymous object
  SERD_LIST_S  = 1U << 4U, ///< Start of list subject
  SERD_LIST_O  = 1U << 5U, ///< Start of list object
  SERD_TERSE_S = 1U << 6U, ///< Start of terse subject
  SERD_TERSE_O = 1U << 7U, ///< Start of terse object
} SerdStatementFlag;

/// Bitwise OR of SerdStatementFlag values
typedef uint32_t SerdStatementFlags;

/**
   Event for base URI changes.

   Emitted whenever the base URI changes.
*/
typedef struct {
  SerdEventType                type; ///< #SERD_BASE
  const SerdNode* SERD_NONNULL uri;  ///< Base URI
} SerdBaseEvent;

/**
   Event for namespace definitions.

   Emitted whenever a prefix is defined.
*/
typedef struct {
  SerdEventType                type; ///< #SERD_PREFIX
  const SerdNode* SERD_NONNULL name; ///< Prefix name
  const SerdNode* SERD_NONNULL uri;  ///< Namespace URI
} SerdPrefixEvent;

/**
   Event for statements.

   Emitted for every statement.
*/
typedef struct {
  SerdEventType                     type;      ///< #SERD_STATEMENT
  SerdStatementFlags                flags;     ///< Flags for pretty-printing
  const SerdStatement* SERD_NONNULL statement; ///< Statement
} SerdStatementEvent;

/**
   Event for the end of anonymous node descriptions.

   This is emitted to indicate that the given anonymous node will no longer be
   described.  This is used by the writer which may, for example, need to
   write a delimiter.
*/
typedef struct {
  SerdEventType                type; ///< #SERD_END
  const SerdNode* SERD_NONNULL node; ///< Anonymous node that is finished
} SerdEndEvent;

/**
   An event in a data stream.

   Streams of data are represented as a series of events.  Events represent
   everything that can occur in an RDF document, and are used to plumb together
   different components.  For example, when parsing a document, a reader emits
   a stream of events which can be sent to a writer to rewrite a document, or
   to an inserter to build a model in memory.
*/
typedef union {
  SerdEventType      type;      ///< Event type (always set)
  SerdBaseEvent      base;      ///< Base URI changed
  SerdPrefixEvent    prefix;    ///< New namespace prefix
  SerdStatementEvent statement; ///< Statement
  SerdEndEvent       end;       ///< End of anonymous node
} SerdEvent;

/// Function for handling events
typedef SerdStatus (*SerdEventFunc)(void* SERD_NULLABLE           handle,
                                    const SerdEvent* SERD_NONNULL event);
/**
   @}
   @defgroup serd_sink Sink
   @{
*/

/// An interface that receives a stream of RDF data
typedef struct SerdSinkImpl SerdSink;

/// Function to free an opaque handle
typedef void (*SerdFreeFunc)(void* SERD_NULLABLE ptr);

/**
   Create a new sink.

   @param handle Opaque handle that will be passed to sink functions.
   @param event_func Function that will be called for every event.
   @param free_handle Free function to call on handle in serd_sink_free().
*/
SERD_API
SerdSink* SERD_ALLOCATED
serd_sink_new(void* SERD_NULLABLE         handle,
              SerdEventFunc SERD_NULLABLE event_func,
              SerdFreeFunc SERD_NULLABLE  free_handle);

/// Free `sink`
SERD_API
void
serd_sink_free(SerdSink* SERD_NULLABLE sink);

/// Send an event to the sink
SERD_API
SerdStatus
serd_sink_write_event(const SerdSink* SERD_NONNULL  sink,
                      const SerdEvent* SERD_NONNULL event);

/// Set the base URI
SERD_API
SerdStatus
serd_sink_write_base(const SerdSink* SERD_NONNULL sink,
                     const SerdNode* SERD_NONNULL uri);

/// Set a namespace prefix
SERD_API
SerdStatus
serd_sink_write_prefix(const SerdSink* SERD_NONNULL sink,
                       const SerdNode* SERD_NONNULL name,
                       const SerdNode* SERD_NONNULL uri);

/// Write a statement
SERD_API
SerdStatus
serd_sink_write_statement(const SerdSink* SERD_NONNULL      sink,
                          SerdStatementFlags                flags,
                          const SerdStatement* SERD_NONNULL statement);

/// Write a statement from individual nodes
SERD_API
SerdStatus
serd_sink_write(const SerdSink* SERD_NONNULL  sink,
                SerdStatementFlags            flags,
                const SerdNode* SERD_NONNULL  subject,
                const SerdNode* SERD_NONNULL  predicate,
                const SerdNode* SERD_NONNULL  object,
                const SerdNode* SERD_NULLABLE graph);

/// Mark the end of an anonymous node
SERD_API
SerdStatus
serd_sink_write_end(const SerdSink* SERD_NONNULL sink,
                    const SerdNode* SERD_NONNULL node);

/**
   @}
   @defgroup serd_canon Canon
   @{
*/

/// Flags that control canonical node transformation
typedef enum {
  SERD_CANON_LAX = 1U << 0U, ///< Tolerate and pass through invalid input
} SerdCanonFlag;

/// Bitwise OR of SerdCanonFlag values
typedef uint32_t SerdCanonFlags;

/**
   Return a new sink that transforms literals to canonical form where possible.

   The returned sink acts like `target` in all respects, except literal nodes
   in statements may be modified from the original.
*/
SERD_API
SerdSink* SERD_ALLOCATED
serd_canon_new(const SerdWorld* SERD_NULLABLE world,
               const SerdSink* SERD_NONNULL   target,
               SerdCanonFlags                 flags);

/**
   @}
   @defgroup serd_filter Filter
   @{
*/

/**
   Return a new sink that filters out statements that do not match a pattern.

   The returned sink acts like `target` in all respects, except that some
   statements may be dropped.

   @param target The target sink to pass the filtered data to.

   @param subject The optional subject of the filter pattern.

   @param predicate The optional predicate of the filter pattern.

   @param object The optional object of the filter pattern.

   @param graph The optional graph of the filter pattern.

   @param inclusive If true, then only statements that match the pattern are
   passed through.  Otherwise, only statements that do *not* match the pattern
   are passed through.
*/
SERD_API
SerdSink* SERD_ALLOCATED
serd_filter_new(const SerdSink* SERD_NONNULL  target,
                const SerdNode* SERD_NULLABLE subject,
                const SerdNode* SERD_NULLABLE predicate,
                const SerdNode* SERD_NULLABLE object,
                const SerdNode* SERD_NULLABLE graph,
                bool                          inclusive);

/**
   @}
   @}
   @defgroup serd_env Environment
   @{
*/

/// Lexical environment for relative URIs or CURIEs (base URI and namespaces)
typedef struct SerdEnvImpl SerdEnv;

/// Create a new environment
SERD_API
SerdEnv* SERD_ALLOCATED
serd_env_new(SerdStringView base_uri);

/// Copy an environment
SERD_API
SerdEnv* SERD_ALLOCATED
serd_env_copy(const SerdEnv* SERD_NULLABLE env);

/// Return true iff `a` is equal to `b`
SERD_PURE_API
bool
serd_env_equals(const SerdEnv* SERD_NULLABLE a, const SerdEnv* SERD_NULLABLE b);

/// Free `env`
SERD_API
void
serd_env_free(SerdEnv* SERD_NULLABLE env);

/// Get the current base URI
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_env_base_uri(const SerdEnv* SERD_NULLABLE env);

/// Set the current base URI
SERD_API
SerdStatus
serd_env_set_base_uri(SerdEnv* SERD_NONNULL env, SerdStringView uri);

/**
   Set a namespace prefix.

   A namespace prefix is used to expand CURIE nodes, for example, with the
   prefix "xsd" set to "http://www.w3.org/2001/XMLSchema#", "xsd:decimal" will
   expand to "http://www.w3.org/2001/XMLSchema#decimal".
*/
SERD_API
SerdStatus
serd_env_set_prefix(SerdEnv* SERD_NONNULL env,
                    SerdStringView        name,
                    SerdStringView        uri);

/**
   Qualify `uri` into a prefix and suffix (like a CURIE) if possible.

   @param env Environment with prefixes to use.

   @param uri URI to qualify.

   @param prefix On success, pointed to a prefix string slice, which is only
   valid until the next time `env` is mutated.

   @param suffix On success, pointed to a suffix string slice, which is only
   valid until the next time `env` is mutated.

   @return #SERD_SUCCESS, or #SERD_FAILURE if `uri` can not be qualified with
   `env`.
*/
SERD_API
SerdStatus
serd_env_qualify(const SerdEnv* SERD_NULLABLE env,
                 SerdStringView               uri,
                 SerdStringView* SERD_NONNULL prefix,
                 SerdStringView* SERD_NONNULL suffix);

/**
   Expand `curie` to an absolute URI if possible.

   For example, if `env` has the prefix "rdf" set to
   <http://www.w3.org/1999/02/22-rdf-syntax-ns#>, then calling this with curie
   "rdf:type" will produce <http://www.w3.org/1999/02/22-rdf-syntax-ns#type>.

   Returns null if `node` can not be expanded.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_env_expand_curie(const SerdEnv* SERD_NULLABLE env, SerdStringView curie);

/**
   Expand `node` to an absolute URI if possible.

   Returns null if `node` can not be expanded.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_env_expand_node(const SerdEnv* SERD_NULLABLE  env,
                     const SerdNode* SERD_NULLABLE node);

/// Write all prefixes in `env` to `sink`
SERD_API
void
serd_env_write_prefixes(const SerdEnv* SERD_NONNULL  env,
                        const SerdSink* SERD_NONNULL sink);

/**
   Create a node from a string representation in `syntax`.

   The string should be a node as if written as an object in the given syntax,
   without any extra quoting or punctuation, which is the format returned by
   serd_node_to_syntax().  These two functions, when used with #SERD_TURTLE,
   can be used to round-trip any node to a string and back.

   @param str String representation of a node.

   @param syntax Syntax to use.  Should be either SERD_TURTLE or SERD_NTRIPLES
   (the others are redundant).  Note that namespaced (CURIE) nodes and relative
   URIs can not be expressed in NTriples.

   @param env Environment of `str`.  This must define any abbreviations needed
   to parse the string.

   @return A newly allocated node that must be freed with serd_node_free().
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_from_syntax(const char* SERD_NONNULL str,
                      SerdSyntax               syntax,
                      SerdEnv* SERD_NULLABLE   env);

/**
   Return a string representation of `node` in `syntax`.

   The returned string represents that node as if written as an object in the
   given syntax, without any extra quoting or punctuation.

   @param node Node to write as a string.

   @param syntax Syntax to use.  Should be either SERD_TURTLE or SERD_NTRIPLES
   (the others are redundant).  Note that namespaced (CURIE) nodes and relative
   URIs can not be expressed in NTriples.

   @param env Environment for the output string.  This can be used to
   abbreviate things nicely by setting namespace prefixes.

   @return A newly allocated string that must be freed with serd_free().
*/
SERD_API
char* SERD_ALLOCATED
serd_node_to_syntax(const SerdNode* SERD_NONNULL node,
                    SerdSyntax                   syntax,
                    const SerdEnv* SERD_NULLABLE env);

/**
   @}
   @defgroup serd_byte_source Byte Source
   @{
*/

/// A source for bytes that provides text input
typedef struct SerdByteSourceImpl SerdByteSource;

/**
   Create a new byte source that reads from a string.

   @param string Null-terminated UTF-8 string to read from.
   @param name Optional name of stream for error messages (string or URI).
*/
SERD_API
SerdByteSource* SERD_ALLOCATED
serd_byte_source_new_string(const char* SERD_NONNULL      string,
                            const SerdNode* SERD_NULLABLE name);

/**
   Create a new byte source that reads from a file.

   An arbitrary `FILE*` can be used via serd_byte_source_new_function() as
   well, this is just a convenience function that opens the file properly, sets
   flags for optimized I/O if possible, and automatically sets the name of the
   source to the file path.

   @param path Path of file to open and read from.
   @param page_size Number of bytes to read per call.
*/
SERD_API
SerdByteSource* SERD_ALLOCATED
serd_byte_source_new_filename(const char* SERD_NONNULL path, size_t page_size);

/**
   Create a new byte source that reads from a user-specified function

   The `stream` will be passed to the `read_func`, which is compatible with the
   standard C `fread` if `stream` is a `FILE*`.  Note that the reader only ever
   reads individual bytes at a time, that is, the `size` parameter will always
   be 1 (but `nmemb` may be higher).

   @param read_func Stream read function, like `fread`.
   @param error_func Stream error function, like `ferror`.
   @param close_func Stream close function, like `fclose`.
   @param stream Context parameter passed to `read_func` and `error_func`.
   @param name Optional name of stream for error messages (string or URI).
   @param page_size Number of bytes to read per call.
*/
SERD_API
SerdByteSource* SERD_ALLOCATED
serd_byte_source_new_function(SerdReadFunc SERD_NONNULL         read_func,
                              SerdStreamErrorFunc SERD_NONNULL  error_func,
                              SerdStreamCloseFunc SERD_NULLABLE close_func,
                              void* SERD_NULLABLE               stream,
                              const SerdNode* SERD_NULLABLE     name,
                              size_t                            page_size);

/// Free `source`
SERD_API
void
serd_byte_source_free(SerdByteSource* SERD_NULLABLE source);

/**
   @}
   @defgroup serd_reader Reader
   @{
*/

/// Streaming parser that reads a text stream and writes to a statement sink
typedef struct SerdReaderImpl SerdReader;

/// Reader options
typedef enum {
  /**
     Tolerate invalid input where possible.

     This will attempt to ignore invalid input and continue reading.  Invalid
     Unicode characters will be replaced with the replacement character, and
     various other syntactic problems will be ignored.  If there are more
     severe problems, the reader will try to skip the statement and continue
     parsing.  This should work reasonably well for line-based syntaxes like
     NTriples and NQuads, but abbreviated Turtle or TriG may not recover.

     Note that this flag should be used carefully, since it can result in data
     loss.
  */
  SERD_READ_LAX = 1U << 0U,

  /**
     Support reading variable nodes.

     As an extension, serd supports reading variables nodes with SPARQL-like
     syntax, for example "?foo" or "$bar".  This can be used for storing
     graph patterns and templates.
  */
  SERD_READ_VARIABLES = 1U << 1U,

  /**
     Read relative URI references exactly without resolving them.

     Normally, the reader expands all relative URIs against the base URI.  This
     flag disables that, so that URI references are passed to the sink exactly
     as they are in the input.
  */
  SERD_READ_RELATIVE = 1U << 2U,

  /**
     Read blank node labels without adding a prefix unique to the document.

     Normally, the reader adds a prefix like "f1", "f2", and so on, to blank
     node labels, to separate the namespaces from separate input documents.
     This flag disables that, so that blank node labels will be read without
     any prefix added.

     Note that this flag should be used carefully, since it can result in data
     corruption.  Specifically, if data from separate documents parsed with
     this flag is combined, the IDs from each document may clash.
  */
  SERD_READ_GLOBAL = 1U << 3U,

  /**
     Read generated blank node labels exactly without adjusting them.

     Normally, the reader will adapt blank node labels in the input that clash
     with its scheme for generating new ones, for example mapping "_:b123" to
     "_:B123".  This flag disables that, so that blank node labels are passed
     to the sink exactly as they are in the input.

     Note that this flag should be used carefully, since it can result in data
     corruption.  Specifically, if the input is a syntax like Turtle with
     anonymous nodes, the generated IDs for those nodes may clash with IDs from
     the input document.
  */
  SERD_READ_GENERATED = 1U << 4U,
} SerdReaderFlag;

/// Bitwise OR of SerdReaderFlag values
typedef uint32_t SerdReaderFlags;

/// Create a new RDF reader
SERD_API
SerdReader* SERD_ALLOCATED
serd_reader_new(SerdWorld* SERD_NONNULL      world,
                SerdSyntax                   syntax,
                SerdReaderFlags              flags,
                SerdEnv* SERD_NONNULL        env,
                const SerdSink* SERD_NONNULL sink,
                size_t                       stack_size);

/// Prepare to read from a byte source
SERD_API
SerdStatus
serd_reader_start(SerdReader* SERD_NONNULL     reader,
                  SerdByteSource* SERD_NONNULL byte_source);

/**
   Read a single "chunk" of data during an incremental read.

   This function will read a single top level description, and return.  This
   may be a directive, statement, or several statements; essentially it reads
   until a '.' is encountered.  This is particularly useful for reading
   directly from a pipe or socket.
*/
SERD_API
SerdStatus
serd_reader_read_chunk(SerdReader* SERD_NONNULL reader);

/**
   Read a complete document from the source.

   This function will continue pulling from the source until a complete
   document has been read.  Note that this may block when used with streams,
   for incremental reading use serd_reader_read_chunk().
*/
SERD_API
SerdStatus
serd_reader_read_document(SerdReader* SERD_NONNULL reader);

/**
   Finish reading from the source.

   This should be called before starting to read from another source.
*/
SERD_API
SerdStatus
serd_reader_finish(SerdReader* SERD_NONNULL reader);

/**
   Free `reader`.

   The reader will be finished via `serd_reader_finish()` if necessary.
*/
SERD_API
void
serd_reader_free(SerdReader* SERD_NULLABLE reader);

/**
   @}
   @defgroup serd_byte_sink Byte Sink
   @{
*/

/// A sink for bytes that receives text output
typedef struct SerdByteSinkImpl SerdByteSink;

/**
   Create a new byte sink that writes to a buffer.

   The `buffer` is owned by the caller, but will be expanded as necessary.
   Note that the string in the buffer will not be null terminated until the
   byte sink is closed.

   @param buffer Buffer to write output to.
*/
SERD_API
SerdByteSink* SERD_ALLOCATED
serd_byte_sink_new_buffer(SerdBuffer* SERD_NONNULL buffer);

/**
   Create a new byte sink that writes to a file.

   An arbitrary `FILE*` can be used via serd_byte_sink_new_function() as well,
   this is just a convenience function that opens the file properly and sets
   flags for optimized I/O if possible.

   @param path Path of file to open and write to.
   @param block_size Number of bytes to write per call.
*/
SERD_API
SerdByteSink* SERD_ALLOCATED
serd_byte_sink_new_filename(const char* SERD_NONNULL path, size_t block_size);

/**
   Create a new byte sink that writes to a user-specified function.

   The `stream` will be passed to the `write_func`, which is compatible with
   the standard C `fwrite` if `stream` is a `FILE*`.

   @param write_func Stream write function, like `fwrite`.
   @param close_func Stream close function, like `fclose`.
   @param stream Context parameter passed to `sink`.
   @param block_size Number of bytes to write per call.
*/
SERD_API
SerdByteSink* SERD_ALLOCATED
serd_byte_sink_new_function(SerdWriteFunc SERD_NONNULL        write_func,
                            SerdStreamCloseFunc SERD_NULLABLE close_func,
                            void* SERD_NULLABLE               stream,
                            size_t                            block_size);

/// Flush any pending output in `sink` to the underlying write function
SERD_API
void
serd_byte_sink_flush(SerdByteSink* SERD_NONNULL sink);

/**
   Close `sink`, including the underlying file if necessary.

   If `sink` was created with serd_byte_sink_new_filename(), then the file is
   closed.  If there was an error, then SERD_ERR_UNKNOWN is returned and
   `errno` is set.
*/
SERD_API
SerdStatus
serd_byte_sink_close(SerdByteSink* SERD_NONNULL sink);

/// Free `sink`, flushing and closing first if necessary
SERD_API
void
serd_byte_sink_free(SerdByteSink* SERD_NULLABLE sink);

/**
   @}
   @defgroup serd_writer Writer
   @{
*/

/// Streaming writer that writes a text stream as it receives events
typedef struct SerdWriterImpl SerdWriter;

/**
   Writer style options.

   These flags allow more precise control of writer output style.  Note that
   some options are only supported for some syntaxes, for example, NTriples
   does not support abbreviation and is always ASCII.
*/
typedef enum {
  /**
     Escape all non-ASCII characters.

     Although all the supported syntaxes are UTF-8 by definition, this can be
     used to escape all non-ASCII characters so that data will survive
     transmission through ASCII-only channels.
  */
  SERD_WRITE_ASCII = 1U << 0U,

  /**
     Write expanded URIs instead of prefixed names.

     This will avoid shortening URIs into CURIEs entirely, even if the output
     syntax supports prefixed names.  This can be useful for making chunks of
     syntax context-free.
  */
  SERD_WRITE_EXPANDED = 1U << 1U,

  /**
     Write URI references exactly as they are received.

     Normally, the writer resolves URIs against the base URI, so it can
     potentially writem them as relative URI references.  This flag disables
     that, so URI nodes are written exactly as they are received.

     When fed by a reader with #SERD_READ_RELATIVE enabled, this will write URI
     references exactly as they are in the input.
  */
  SERD_WRITE_VERBATIM = 1U << 2U,

  /**
     Write terser output without newlines.

     For Turtle and TriG, this enables a terser form of output which only has
     newlines at the top level.  This can result in very long lines, but is
     more compact and useful for making these abbreviated syntaxes line-based.
  */
  SERD_WRITE_TERSE = 1U << 3U,

  /**
     Tolerate lossy output.

     This will tolerate input that can not be written without loss, in
     particular invalid UTF-8 text.  Note that this flag should be used
     carefully, since it can result in data loss.
  */
  SERD_WRITE_LAX = 1U << 4U,

  /**
     Write rdf:type as a normal predicate.

     This disables the special "a" syntax in Turtle and TriG.
  */
  SERD_WRITE_RDF_TYPE = 1U << 5U,
} SerdWriterFlag;

/// Bitwise OR of SerdWriterFlag values
typedef uint32_t SerdWriterFlags;

/// Create a new RDF writer
SERD_API
SerdWriter* SERD_ALLOCATED
serd_writer_new(SerdWorld* SERD_NONNULL     world,
                SerdSyntax                  syntax,
                SerdWriterFlags             flags,
                const SerdEnv* SERD_NONNULL env,
                SerdByteSink* SERD_NONNULL  byte_sink);

/// Free `writer`
SERD_API
void
serd_writer_free(SerdWriter* SERD_NULLABLE writer);

/// Return a sink interface that emits statements via `writer`
SERD_CONST_API
const SerdSink* SERD_NONNULL
serd_writer_sink(SerdWriter* SERD_NONNULL writer);

/**
   A convenience sink function for writing to a string.

   This function can be used as a SerdSink to write to a SerdBuffer which is
   resized as necessary with realloc().  The `stream` parameter must point to
   an initialized SerdBuffer.  When the write is finished, the string should be
   retrieved with serd_buffer_sink_finish().
*/
SERD_API
size_t
serd_buffer_sink(const void* SERD_NONNULL buf,
                 size_t                   size,
                 size_t                   nmemb,
                 void* SERD_NONNULL       stream);

/**
   Finish writing to a buffer with serd_buffer_sink().

   The returned string is the result of the serialisation, which is null
   terminated (by this function) and owned by the caller.
*/
SERD_API
char* SERD_NONNULL
serd_buffer_sink_finish(SerdBuffer* SERD_NONNULL stream);

/**
   Set the current output base URI, and emit a directive if applicable.

   Note this function can be safely casted to SerdBaseSink.
*/
SERD_API
SerdStatus
serd_writer_set_base_uri(SerdWriter* SERD_NONNULL      writer,
                         const SerdNode* SERD_NULLABLE uri);

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
serd_writer_set_root_uri(SerdWriter* SERD_NONNULL writer, SerdStringView uri);

/**
   Finish a write.

   This flushes any pending output, for example terminating punctuation, so
   that the output is a complete document.
*/
SERD_API
SerdStatus
serd_writer_finish(SerdWriter* SERD_NONNULL writer);

/**
   @}
   @defgroup serd_storage Storage
   @{
*/

/**
   @defgroup serd_cursor Cursor
   @{
*/

/**
   A cursor that iterates over statements in a model.

   A cursor is a smart iterator that visits all statements that match a
   pattern.
*/
typedef struct SerdCursorImpl SerdCursor;

/// Return a new copy of `cursor`
SERD_API
SerdCursor* SERD_ALLOCATED
serd_cursor_copy(const SerdCursor* SERD_NULLABLE cursor);

/// Return the statement pointed to by `cursor`
SERD_API
const SerdStatement* SERD_NULLABLE
serd_cursor_get(const SerdCursor* SERD_NULLABLE cursor);

/**
   Increment cursor to point to the next statement.

   @return Failure if `cursor` was already at the end.
*/
SERD_API
SerdStatus
serd_cursor_advance(SerdCursor* SERD_NONNULL cursor);

/// Return true if the cursor has reached its end
SERD_PURE_API
bool
serd_cursor_is_end(const SerdCursor* SERD_NULLABLE cursor);

/**
   Return true iff `lhs` equals `rhs`.

   Two cursors are equivalent if they point to the same statement in the same
   index in the same model, or are both the end of the same model.  Note that
   two cursors can point to the same statement but not be equivalent, since
   they may have reached the statement via different indices.
*/
SERD_PURE_API
bool
serd_cursor_equals(const SerdCursor* SERD_NULLABLE lhs,
                   const SerdCursor* SERD_NULLABLE rhs);

/// Free `cursor`
SERD_API
void
serd_cursor_free(SerdCursor* SERD_NULLABLE cursor);

/**
   @}
   @defgroup serd_range Range
   @{
*/

/// Flags that control the style of a model description
typedef enum {
  SERD_NO_INLINE_OBJECTS = 1U << 0U, ///< Disable object inlining
  SERD_NO_TYPE_FIRST     = 1U << 1U, ///< Disable writing rdf:type ("a") first
} SerdDescribeFlag;

/// Bitwise OR of SerdDescribeFlag values
typedef uint32_t SerdDescribeFlags;

/**
   Describe a range of statements by writing to a sink.

   This will consume the given cursor, and emit at least every statement it
   visits.  More statements from the model may be written in order to describe
   anonymous blank nodes that are associated with a subject in the range.

   The default is to write statements in an order suited for pretty-printing
   with Turtle or TriG with as many anonymous nodes as possible.  If
   `SERD_NO_INLINE_OBJECTS` is given, a simple sorted stream is written
   instead, which is faster since no searching is required, but can result in
   ugly output for Turtle or Trig.
*/
SERD_API
SerdStatus
serd_describe_range(const SerdCursor* SERD_NULLABLE range,
                    const SerdSink* SERD_NONNULL    sink,
                    SerdDescribeFlags               flags);

/**
   @}
   @defgroup serd_model Model
   @{
*/

/// An indexed set of statements
typedef struct SerdModelImpl SerdModel;

/**
   Statement ordering.

   Statements themselves always have the same fields in the same order
   (subject, predicate, object, graph), but a model can keep indices for
   different orderings to provide good performance for different kinds of
   queries.
*/
typedef enum {
  SERD_ORDER_SPO,  ///<         Subject,   Predicate, Object
  SERD_ORDER_SOP,  ///<         Subject,   Object,    Predicate
  SERD_ORDER_OPS,  ///<         Object,    Predicate, Subject
  SERD_ORDER_OSP,  ///<         Object,    Subject,   Predicate
  SERD_ORDER_PSO,  ///<         Predicate, Subject,   Object
  SERD_ORDER_POS,  ///<         Predicate, Object,    Subject
  SERD_ORDER_GSPO, ///< Graph,  Subject,   Predicate, Object
  SERD_ORDER_GSOP, ///< Graph,  Subject,   Object,    Predicate
  SERD_ORDER_GOPS, ///< Graph,  Object,    Predicate, Subject
  SERD_ORDER_GOSP, ///< Graph,  Object,    Subject,   Predicate
  SERD_ORDER_GPSO, ///< Graph,  Predicate, Subject,   Object
  SERD_ORDER_GPOS, ///< Graph,  Predicate, Object,    Subject
} SerdStatementOrder;

/// Flags that control model storage and indexing
typedef enum {
  SERD_STORE_GRAPHS = 1U << 0U, ///< Store and index the graph of statements
  SERD_STORE_CARETS = 1U << 1U, ///< Store original caret of statements
} SerdModelFlag;

/// Bitwise OR of SerdModelFlag values
typedef uint32_t SerdModelFlags;

/**
   Create a new model.

   @param world The world in which to make this model.

   @param default_order The order for the default index, which is always
   present and responsible for owning all the statements in the model.  This
   should almost always be #SERD_ORDER_SPO or #SERD_ORDER_GSPO (which
   support writing pretty documents), but advanced applications that do not want
   either of these indices can use a different order.  Additional indices can
   be added with serd_model_add_index().

   @param flags Options that control what data is stored in the model.
*/
SERD_API
SerdModel* SERD_ALLOCATED
serd_model_new(SerdWorld* SERD_NONNULL world,
               SerdStatementOrder      default_order,
               SerdModelFlags          flags);

/// Return a deep copy of `model`
SERD_API
SerdModel* SERD_ALLOCATED
serd_model_copy(const SerdModel* SERD_NONNULL model);

/// Return true iff `a` is equal to `b`, ignoring statement cursor metadata
SERD_API
bool
serd_model_equals(const SerdModel* SERD_NULLABLE a,
                  const SerdModel* SERD_NULLABLE b);

/// Close and free `model`
SERD_API
void
serd_model_free(SerdModel* SERD_NULLABLE model);

/**
   Add an index for a particular statement order to the model.

   @return Failure if this index already exists.
*/
SERD_API
SerdStatus
serd_model_add_index(SerdModel* SERD_NONNULL model, SerdStatementOrder order);

/**
   Add an index for a particular statement order to the model.

   @return Failure if this index does not exist.
*/
SERD_API
SerdStatus
serd_model_drop_index(SerdModel* SERD_NONNULL model, SerdStatementOrder order);

/// Get the world associated with `model`
SERD_PURE_API
SerdWorld* SERD_NONNULL
serd_model_world(SerdModel* SERD_NONNULL model);

/// Get all nodes interned in `model`
SERD_PURE_API
const SerdNodes* SERD_NONNULL
serd_model_nodes(const SerdModel* SERD_NONNULL model);

/// Get the default statement order of `model`
SERD_PURE_API
SerdStatementOrder
serd_model_default_order(const SerdModel* SERD_NONNULL model);

/// Get the flags enabled on `model`
SERD_PURE_API
SerdModelFlags
serd_model_flags(const SerdModel* SERD_NONNULL model);

/// Return the number of statements stored in `model`
SERD_PURE_API
size_t
serd_model_size(const SerdModel* SERD_NONNULL model);

/// Return true iff there are no statements stored in `model`
SERD_PURE_API
bool
serd_model_empty(const SerdModel* SERD_NONNULL model);

/**
   Return a cursor at the start of every statement in the model.

   The returned cursor will advance over every statement in the model's default
   order.
*/
SERD_API
SerdCursor* SERD_ALLOCATED
serd_model_begin(const SerdModel* SERD_NONNULL model);

/**
   Return a cursor past the end of the model.

   This returns the "universal" end cursor, which is equivalent to any cursor
   for this model that has reached its end.
*/
SERD_CONST_API
const SerdCursor* SERD_NONNULL
serd_model_end(const SerdModel* SERD_NONNULL model);

/// Return a cursor over all statements in the model in a specific order
SERD_API
SerdCursor* SERD_ALLOCATED
serd_model_begin_ordered(const SerdModel* SERD_NONNULL model,
                         SerdStatementOrder            order);

/**
   Search for statements that match a pattern.

   @return An iterator to the first match, or NULL if no matches found.
*/
SERD_API
SerdCursor* SERD_ALLOCATED
serd_model_find(const SerdModel* SERD_NONNULL model,
                const SerdNode* SERD_NULLABLE s,
                const SerdNode* SERD_NULLABLE p,
                const SerdNode* SERD_NULLABLE o,
                const SerdNode* SERD_NULLABLE g);

/**
   Search for a single node that matches a pattern.

   Exactly one of `s`, `p`, `o` must be NULL.
   This function is mainly useful for predicates that only have one value.

   @return The first matching node, or NULL if no matches are found.
*/
SERD_API
const SerdNode* SERD_NULLABLE
serd_model_get(const SerdModel* SERD_NONNULL model,
               const SerdNode* SERD_NULLABLE s,
               const SerdNode* SERD_NULLABLE p,
               const SerdNode* SERD_NULLABLE o,
               const SerdNode* SERD_NULLABLE g);

/**
   Search for a single statement that matches a pattern.

   This function is mainly useful for predicates that only have one value.

   @return The first matching statement, or NULL if none are found.
*/
SERD_API
const SerdStatement* SERD_NULLABLE
serd_model_get_statement(const SerdModel* SERD_NONNULL model,
                         const SerdNode* SERD_NULLABLE s,
                         const SerdNode* SERD_NULLABLE p,
                         const SerdNode* SERD_NULLABLE o,
                         const SerdNode* SERD_NULLABLE g);

/// Return true iff a statement exists
SERD_API
bool
serd_model_ask(const SerdModel* SERD_NONNULL model,
               const SerdNode* SERD_NULLABLE s,
               const SerdNode* SERD_NULLABLE p,
               const SerdNode* SERD_NULLABLE o,
               const SerdNode* SERD_NULLABLE g);

/// Return the number of matching statements
SERD_API
size_t
serd_model_count(const SerdModel* SERD_NONNULL model,
                 const SerdNode* SERD_NULLABLE s,
                 const SerdNode* SERD_NULLABLE p,
                 const SerdNode* SERD_NULLABLE o,
                 const SerdNode* SERD_NULLABLE g);

/**
   Add a statement to a model from nodes.

   This function fails if there are any active iterators on `model`.
*/
SERD_API
SerdStatus
serd_model_add(SerdModel* SERD_NONNULL       model,
               const SerdNode* SERD_NONNULL  s,
               const SerdNode* SERD_NONNULL  p,
               const SerdNode* SERD_NONNULL  o,
               const SerdNode* SERD_NULLABLE g);

/**
   Add a statement to a model from nodes with a document origin.

   This function fails if there are any active iterators on `model`.
*/
SERD_API
SerdStatus
serd_model_add_with_caret(SerdModel* SERD_NONNULL        model,
                          const SerdNode* SERD_NONNULL   s,
                          const SerdNode* SERD_NONNULL   p,
                          const SerdNode* SERD_NONNULL   o,
                          const SerdNode* SERD_NULLABLE  g,
                          const SerdCaret* SERD_NULLABLE caret);

/**
   Add a statement to a model.

   This function fails if there are any active iterators on `model`.
   If statement is null, then SERD_FAILURE is returned.
*/
SERD_API
SerdStatus
serd_model_insert(SerdModel* SERD_NONNULL           model,
                  const SerdStatement* SERD_NONNULL statement);

/**
   Add a range of statements to a model.

   This function fails if there are any active iterators on `model`.
*/
SERD_API
SerdStatus
serd_model_insert_statements(SerdModel* SERD_NONNULL  model,
                             SerdCursor* SERD_NONNULL range);

/**
   Remove a statement from a model via an iterator.

   Calling this function invalidates all other iterators on this model.

   @param model The model which `iter` points to.

   @param cursor Cursor pointing to the element to erase.  This cursor is
   advanced to the next statement on return.
*/
SERD_API
SerdStatus
serd_model_erase(SerdModel* SERD_NONNULL  model,
                 SerdCursor* SERD_NONNULL cursor);

/**
   Remove a range of statements from a model.

   This can be used with serd_model_find() to erase all statements in a model
   that match a pattern.

   Calling this function invalidates all iterators on `model`.

   @param model The model which `range` points to.

   @param range Range to erase, which will be empty on return.
*/
SERD_API
SerdStatus
serd_model_erase_statements(SerdModel* SERD_NONNULL  model,
                            SerdCursor* SERD_NONNULL range);

/**
   Remove everything from a model.

   Calling this function invalidates all iterators on `model`.

   @param model The model to clear.
*/
SERD_API
SerdStatus
serd_model_clear(SerdModel* SERD_NONNULL model);

/**
   @}
   @defgroup serd_inserter Inserter
   @{
*/

/**
   Create an inserter for writing statements to a model.

   Once created, an inserter is just a sink with no additional interface.

   @param model The model to insert received statements into.

   @param default_graph Optional default graph, which will be set on received
   statements that have no graph.  This allows, for example, loading a Turtle
   document into an isolated graph in the model.

   @return A newly allocated sink which must be freed with serd_sink_free().
*/
SERD_API
SerdSink* SERD_ALLOCATED
serd_inserter_new(SerdModel* SERD_NONNULL       model,
                  const SerdNode* SERD_NULLABLE default_graph);

/**
   @}
   @}
   @}
*/

#ifdef __cplusplus
#  if defined(__GNUC__)
#    pragma GCC diagnostic pop
#  endif
} /* extern "C" */
#endif

#endif /* SERD_SERD_H */
