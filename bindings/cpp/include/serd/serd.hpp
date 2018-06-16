/*
  Copyright 2019-2021 David Robillard <d@drobilla.net>

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

#ifndef SERD_SERD_HPP
#define SERD_SERD_HPP

#include "serd/Flags.hpp"                 // IWYU pragma: export
#include "serd/Optional.hpp"              // IWYU pragma: export
#include "serd/StringView.hpp"            // IWYU pragma: export
#include "serd/detail/Copyable.hpp"       // IWYU pragma: export
#include "serd/detail/DynamicWrapper.hpp" // IWYU pragma: export
#include "serd/detail/StaticWrapper.hpp"  // IWYU pragma: export
#include "serd/detail/Wrapper.hpp"        // IWYU pragma: export

#include "serd/serd.h"

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace serd {

/**
   @defgroup serdpp Serd C++ API
   @{
*/

/**
   @defgroup serdpp_status Status Codes
   @{
*/

/// @copydoc SerdStatus
enum class Status {
  success        = SERD_SUCCESS,        ///< @copydoc SERD_SUCCESS
  failure        = SERD_FAILURE,        ///< @copydoc SERD_FAILURE
  err_unknown    = SERD_ERR_UNKNOWN,    ///< @copydoc SERD_ERR_UNKNOWN
  err_bad_syntax = SERD_ERR_BAD_SYNTAX, ///< @copydoc SERD_ERR_BAD_SYNTAX
  err_bad_arg    = SERD_ERR_BAD_ARG,    ///< @copydoc SERD_ERR_BAD_ARG
  err_bad_iter   = SERD_ERR_BAD_ITER,   ///< @copydoc SERD_ERR_BAD_ITER
  err_not_found  = SERD_ERR_NOT_FOUND,  ///< @copydoc SERD_ERR_NOT_FOUND
  err_id_clash   = SERD_ERR_ID_CLASH,   ///< @copydoc SERD_ERR_ID_CLASH
  err_bad_curie  = SERD_ERR_BAD_CURIE,  ///< @copydoc SERD_ERR_BAD_CURIE
  err_internal   = SERD_ERR_INTERNAL,   ///< @copydoc SERD_ERR_INTERNAL
  err_overflow   = SERD_ERR_OVERFLOW,   ///< @copydoc SERD_ERR_OVERFLOW
  err_invalid    = SERD_ERR_INVALID,    ///< @copydoc SERD_ERR_INVALID
  err_no_data    = SERD_ERR_NO_DATA,    ///< @copydoc SERD_ERR_NO_DATA
  err_bad_write  = SERD_ERR_BAD_WRITE,  ///< @copydoc SERD_ERR_BAD_WRITE
  err_bad_call   = SERD_ERR_BAD_CALL    ///< @copydoc SERD_ERR_BAD_CALL
};

/// @copydoc serd_strerror
inline const char*
strerror(const Status status)
{
  return serd_strerror(static_cast<SerdStatus>(status));
}

/**
   @}
   @defgroup serdpp_string String Utilities
   @{
*/

/**
   Encode `size` bytes of `buf` into `str`, which must be large enough.

   @param buf Input binary data (vector-like container of bytes).
   @return A base64 encoded representation of the data in `buf`.
*/
template<class Container>
inline std::string
base64_encode(const Container& buf)
{
#if 0
  const size_t length{serd_base64_encoded_length(buf.size(), wrap_lines)};
  std::string  str(length + 1, '\0');

  serd_base64_encode(&str.at(0), buf.data(), buf.size(), wrap_lines);
  return str;
#endif

  (void)buf;
  return ""; // FIXME
}

// FIXME
#if 0
/**
   Decode a base64 string.

   Container must be a vector-like container of bytes.

   @param str Base64 string to decode.
   @return The decoded data represented in `str`.
*/
template<class Container = std::vector<uint8_t>>
inline Container
base64_decode(StringView str)
{
#  if 0
  size_t    size{serd_base64_decoded_size(str.length())};
  Container buf(size, 0);

  serd_base64_decode(&buf.at(0), &size, str.c_str(), str.length());
  buf.resize(size);
  return buf;
#  endif
  (void)str;
  return {}; // FIXME
}
#endif

// TODO: serd_canonical_path

// TODO: grouping?
static inline size_t
stream_write(const void* buf, size_t size, size_t nmemb, void* sink) noexcept
{
  (void)size;
  assert(size == 1);

  std::ostream& os = *static_cast<std::ostream*>(sink);

  try {
    os.write(static_cast<const char*>(buf), std::streamsize(nmemb));
    return os.good() ? nmemb : 0u;
  } catch (...) {
  }
  return 0;
}

/**
   @}
   @defgroup serdpp_syntax Syntax Utilities
   @{
*/

/// @copydoc SerdSyntax
enum class Syntax {
  empty    = SERD_SYNTAX_EMPTY, ///< @copydoc SERD_SYNTAX_EMPTY
  Turtle   = SERD_TURTLE,       ///< @copydoc SERD_TURTLE
  NTriples = SERD_NTRIPLES,     ///< @copydoc SERD_NTRIPLES
  NQuads   = SERD_NQUADS,       ///< @copydoc SERD_NQUADS
  TriG     = SERD_TRIG          ///< @copydoc SERD_TRIG
};

/// @copydoc serd_syntax_by_name
inline Syntax
syntax_by_name(StringView name)
{
  return Syntax(serd_syntax_by_name(name.c_str()));
}

/// @copydoc serd_guess_syntax
inline Syntax
guess_syntax(StringView filename)
{
  return Syntax(serd_guess_syntax(filename.c_str()));
}

/**
   Return whether a syntax can represent multiple graphs.

   @return True for @ref Syntax::NQuads and @ref Syntax::TriG, false otherwise.
*/
inline bool
syntax_has_graphs(const Syntax syntax)
{
  return serd_syntax_has_graphs(static_cast<SerdSyntax>(syntax));
}

/**
   @}
   @defgroup serdpp_data Data
   @{
   @defgroup serdpp_uri URI
   @{
*/

/**
   Get the unescaped path and hostname from a file URI.

   The returned path and `*hostname` must be freed with serd_free().

   @param uri A file URI.
   @param hostname If non-NULL, set to the hostname, if present.
   @return A filesystem path.
*/
inline std::string
parse_file_uri(StringView uri, std::string* hostname = nullptr)
{
  char* c_hostname = nullptr;
  char* c_path     = serd_parse_file_uri(uri.data(), &c_hostname);
  if (hostname && c_hostname) {
    *hostname = c_hostname;
  }

  std::string path{c_path};
  serd_free(c_hostname);
  serd_free(c_path);
  return path;
}

/// @copydoc serd_uri_string_has_scheme
inline bool
uri_string_has_scheme(StringView string)
{
  return serd_uri_string_has_scheme(string.c_str());
}

/**
   A parsed URI.

   This directly refers to slices in other strings, it does not own any memory
   itself.  Thus, URIs can be parsed and/or resolved against a base URI
   in-place without allocating memory.
*/
class URI
{
public:
  /**
     Component of a URI.

     Note that there is a distinction between a component being non-present
     and present but empty.  For example, "file:///path" has an empty
     authority, while "file:/path" has no authority.  A non-present component
     has its `data()` pointer set to null, while an empty component has a
     data pointer, but length zero.
  */
  using Component = StringView;

  /// Construct a URI by parsing a URI string
  explicit URI(StringView str)
    : _uri{serd_parse_uri(str.data())}
  {}

  /// Construct a URI from a C URI view
  explicit URI(const SerdURIView& uri)
    : _uri{uri}
  {}

  /// Return the scheme of this URI
  Component scheme() const { return make_component(_uri.scheme); }

  /// Return the authority of this URI
  Component authority() const { return make_component(_uri.authority); }

  /// Return the path prefix of this URI, which is set if it has been resolved
  Component path_prefix() const { return make_component(_uri.path_prefix); }

  /// Return the path (suffix) of this URI
  Component path() const { return make_component(_uri.path); }

  /// Return the query
  Component query() const { return make_component(_uri.query); }

  /// Return the fragment of this URI
  Component fragment() const { return make_component(_uri.fragment); }

  /// Return this URI resolved against `base`
  URI resolve(const URI& base) const
  {
    return URI{serd_resolve_uri(_uri, base._uri)};
  }

  /// Return URI as a string
  std::string string() const
  {
    std::ostringstream ss;

    serd_write_uri(_uri, stream_write, &ss);
    return ss.str();
  }

  /// Return this URI as a string relative to `base`
  std::string relative_string(const URI& base) const
  {
    std::ostringstream ss;

    const SerdURIView rel = serd_relative_uri(_uri, base._uri);

    serd_write_uri(rel, stream_write, &ss);
    return ss.str();
  }

  /**
     Return this URI as a string relative to `base` but constrained to `root`.

     The returned URI string is relative iff this URI is a child of both `base`
     and `root`.  The `root` must be a prefix of `base` and can be used keep
     up-references ("../") within a certain namespace.
  */
  std::string relative_string(const URI& base, const URI& root) const
  {
    if (serd_uri_is_within(_uri, root._uri)) {
      return relative_string(base);
    }

    return string();
  }

  /// Return a pointer to the underlying C object
  const SerdURIView* cobj() const { return &_uri; }

private:
  static Component make_component(const SerdStringView slice)
  {
    return slice.buf ? Component{slice.buf, slice.len} : Component{};
  }

  SerdURIView _uri;
};

inline std::ostream&
operator<<(std::ostream& os, const URI& uri)
{
  serd_write_uri(*uri.cobj(), stream_write, &os);
  return os;
}

/**
   @}
   @defgroup serdpp_node Node
   @{
*/

/// @copydoc SerdNodeType
enum class NodeType {
  literal = SERD_LITERAL, ///< @copydoc SERD_LITERAL
  URI     = SERD_URI,     ///< @copydoc SERD_URI
  CURIE   = SERD_CURIE,   ///< @copydoc SERD_CURIE
  blank   = SERD_BLANK    ///< @copydoc SERD_BLANK
};

template<class CObj>
using NodeHandle = detail::
  StaticCopyable<CObj, serd_node_copy, serd_node_equals, serd_node_free>;

// template<class CObj>
// class NodeWrapper;

/// A view of an immutable node
// using NodeView = NodeWrapper<const SerdNode>;
class NodeView;

/// Common base class for any wrapped node
template<class CObj>
class NodeWrapper : public NodeHandle<CObj>
{
protected:
  explicit NodeWrapper(CObj* const ptr)
    : NodeHandle<CObj>{ptr}
  {}

public:
  template<class C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  NodeWrapper(const NodeWrapper<C>& node)
    : NodeHandle<CObj>{node}
  {}

  /// @copydoc serd_node_type
  NodeType type() const { return NodeType(serd_node_type(this->cobj())); }

  /// @copydoc serd_node_string
  const char* c_str() const { return serd_node_string(this->cobj()); }

  /// @copydoc serd_node_string
  StringView str() const { return StringView{c_str(), length()}; }

  /// @copydoc serd_node_length
  size_t size() const { return serd_node_length(this->cobj()); }

  /// @copydoc serd_node_length
  size_t length() const { return serd_node_length(this->cobj()); }

  /// @copydoc serd_node_datatype
  Optional<NodeView> datatype() const;

  /// @copydoc serd_node_language
  Optional<NodeView> language() const;

  /// @copydoc serd_node_string_view
  StringView string_view() const { return StringView{c_str(), length()}; }

  /// @copydoc serd_node_uri_view
  SerdURIView uri_view() const { return serd_node_uri_view(this->cobj()); }

  /// Returns a newly allocated copy of the node's string
  explicit operator std::string() const
  {
    return std::string{c_str(), length()};
  }

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  explicit operator StringView() const { return StringView(c_str(), length()); }

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  explicit operator SerdStringView() const
  {
    return SerdStringView{c_str(), length()};
  }

  /// Return a pointer to the first character in the node's string
  const char* begin() const { return c_str(); }

  /// Return a pointer to the null terminator at the end of the node's string
  const char* end() const { return c_str() + length(); }

  /// Return true if the node's string is empty
  bool empty() const { return length() == 0; }
};

/// A non-owning constant view of some other node
class NodeView : public NodeWrapper<const SerdNode>
{
public:
  /// Create a view of a C node pointer
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  NodeView(const SerdNode* const ptr)
    : NodeWrapper{ptr}
  {}

  /// Create a view of some other node
  template<class C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  NodeView(const NodeWrapper<C>& node)
    : NodeWrapper{node}
  {}
};

template<class CObj>
inline Optional<NodeView>
NodeWrapper<CObj>::datatype() const
{
  return NodeView{serd_node_datatype(this->cobj())};
}

template<class CObj>
inline Optional<NodeView>
NodeWrapper<CObj>::language() const
{
  return NodeView{serd_node_language(this->cobj())};
}

/**
   Compare two nodes.

   Nodes are ordered first by type, then by string value, then by language or
   datatype, if present.
*/
inline bool
operator<(const NodeView& lhs, const NodeView& rhs)
{
  return serd_node_compare(lhs.cobj(), rhs.cobj()) < 0;
}

/// An RDF node
class Node : public NodeWrapper<SerdNode>
{
public:
  /// Create a node by taking ownership of a C node
  explicit Node(SerdNode* const node)
    : NodeWrapper<SerdNode>{node}
  {}

  /// Create a node by copying another node
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Node(const NodeView& node)
    : NodeWrapper<SerdNode>{node}
  {}

  /// Create an xsd:boolean node from a ``bool``
  explicit Node(const bool b)
    : NodeWrapper{serd_new_boolean(b)}
  {}

  /// Create an xsd:double node from a ``double``
  explicit Node(const double d)
    : NodeWrapper{serd_new_double(d)}
  {}

  /// Create an xsd:float node from a ``float``
  explicit Node(const float f)
    : NodeWrapper{serd_new_float(f)}
  {}

  /// Create an xsd:integer node from a ``int64_t``
  explicit Node(const int64_t i)
    : NodeWrapper{serd_new_integer(i, nullptr)}
  {}

  Node(const Node& node) = default;
  Node& operator=(const Node& node) = default;

  Node(Node&& node) = default;
  Node& operator=(Node&& node) = default;

  ~Node() = default;

private:
  friend class Optional<Node>;
  friend class Cursor;

  explicit Node(std::nullptr_t)
    : NodeWrapper{nullptr}
  {}
};

inline std::ostream&
operator<<(std::ostream& os, const NodeView& node)
{
  return os << node.c_str();
}

/// Create a new plain literal node with no language from `str`
inline Node
make_string(StringView str)
{
  return Node{serd_new_string(str)};
}

/// @copydoc serd_new_plain_literal
inline Node
make_plain_literal(StringView str, StringView lang)
{
  return Node{serd_new_plain_literal(str, lang)};
}

/// Create a new typed literal node from `str`
inline Node
make_typed_literal(StringView str, const StringView datatype)
{
  return Node{serd_new_typed_literal(str, datatype)};
}

/// @copydoc serd_new_blank
inline Node
make_blank(StringView str)
{
  return Node{serd_new_blank(str)};
}

/// @copydoc serd_new_curie
inline Node
make_curie(StringView str)
{
  return Node{serd_new_curie(str)};
}

/// @copydoc serd_new_uri
inline Node
make_uri(StringView str)
{
  return Node{serd_new_uri(str)};
}

/// @copydoc serd_new_parsed_uri
inline Node
make_uri(SerdURIView uri)
{
  return Node{serd_new_parsed_uri(uri)};
}

/// @copydoc serd_new_parsed_uri
inline Node
make_uri(URI uri)
{
  return Node{serd_new_parsed_uri(*uri.cobj())};
}

/// Create a new file URI node from a local filesystem path
inline Node
make_file_uri(StringView path)
{
  return Node{serd_new_file_uri(path, SERD_EMPTY_STRING())};
}

/// @copydoc serd_new_file_uri
inline Node
make_file_uri(StringView path, StringView hostname)
{
  return Node{serd_new_file_uri(path, hostname)};
}

/// @copydoc serd_new_boolean
inline Node
make_boolean(const bool b)
{
  return Node{serd_new_boolean(b)};
}

/// @copydoc serd_new_decimal
inline Node
make_decimal(double d, Optional<NodeView> datatype = {})
{
  return Node{serd_new_decimal(d, datatype.cobj())};
}

/// @copydoc serd_new_double
inline Node
make_double(double d)
{
  return Node{serd_new_double(d)};
}

/// @copydoc serd_new_float
inline Node
make_float(float f)
{
  return Node{serd_new_float(f)};
}

/// @copydoc serd_new_integer
inline Node
make_integer(int64_t i, Optional<NodeView> datatype = {})
{
  return Node{serd_new_integer(i, datatype.cobj())};
}

/**
   Create a new canonical xsd:base64Binary literal with a specific datatype.

   This function can be used to make a node out of arbitrary binary data, which
   can be decoded using base64_decode().  The datatype argument allows a more
   specific datatype to be set.

   @param buf Raw binary data to encode in node.
   @param size Size of `buf` in bytes.
   @param datatype Datatype of node.
*/
inline Node
make_base64(const void* buf, size_t size, NodeView datatype)
{
  return Node{serd_new_base64(buf, size, datatype.cobj())};
}

/**
   Create a new canonical xsd:base64Binary literal.

   This function can be used to make a node out of arbitrary binary data, which
   can be decoded using base64_decode().

   @param buf Raw binary data to encode in node.
   @param size Size of `buf` in bytes.
*/
inline Node
make_base64(const void* buf, size_t size)
{
  return Node{serd_new_base64(buf, size, nullptr)};
}

/// Prototype for Node get() templates
template<class T>
inline T
get(NodeView node);

/// @copydoc serd_get_boolean
template<>
inline bool
get<bool>(NodeView node)
{
  return serd_get_boolean(node.cobj());
}

/// @copydoc serd_get_double
template<>
inline double
get<double>(NodeView node)
{
  return serd_get_double(node.cobj());
}

/// @copydoc serd_get_float
template<>
inline float
get<float>(NodeView node)
{
  return serd_get_float(node.cobj());
}

/// @copydoc serd_get_integer
template<>
inline int64_t
get<int64_t>(NodeView node)
{
  return serd_get_integer(node.cobj());
}

/**
   @}
   @defgroup serdpp_nodes Nodes
   @{
*/

// TODO

/**
   @}
   @defgroup serdpp_cursor Cursor
   @{
*/

/// Cursor handle
template<class CObj>
using CursorHandle = detail::
  StaticCopyable<CObj, serd_cursor_copy, serd_cursor_equals, serd_cursor_free>;

/// Cursor wrapper
template<class CObj>
class CursorWrapper : public CursorHandle<CObj>
{
public:
  explicit CursorWrapper(CObj* cursor)
    : CursorHandle<CObj>{cursor}
  {}

  template<class C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  CursorWrapper(const CursorWrapper<C>& cursor)
    : CursorHandle<CObj>{cursor.cobj()}
  {}

  /// @copydoc serd_cursor_name
  NodeView name() const { return NodeView(serd_cursor_name(this->cobj())); }

  /// @copydoc serd_cursor_line
  unsigned line() const { return serd_cursor_line(this->cobj()); }

  /// @copydoc serd_cursor_column
  unsigned column() const { return serd_cursor_column(this->cobj()); }
};

/// Cursor view
using CursorView = CursorWrapper<const SerdCursor>;

/// Extra data managed by mutable (user created) Cursor
struct CursorData {
  Node name_node;
};

/// @copydoc SerdCursor
class Cursor
  : private CursorData
  , public CursorWrapper<SerdCursor>
{
public:
  /**
     Create a new cursor.

     @param name The name of the document or stream (usually a file URI)
     @param line The line number in the document (1-based)
     @param col The column number in the document (1-based)
  */
  Cursor(const NodeView& name, const unsigned line, const unsigned col)
    : CursorData{Node{name}}
    , CursorWrapper{serd_cursor_new(name_node.cobj(), line, col)}
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Cursor(const CursorView& cursor)
    : Cursor(cursor.name(), cursor.line(), cursor.column())
  {}

private:
  friend class Optional<Cursor>;
  friend class Statement;

  explicit Cursor(std::nullptr_t)
    : CursorData{Node{nullptr}}
    , CursorWrapper{nullptr}
  {}
};

/**
   @}
   @defgroup serdpp_statement Statement
   @{
*/

/// @copydoc SerdField
enum class Field {
  subject   = SERD_SUBJECT,   ///< @copydoc SERD_SUBJECT
  predicate = SERD_PREDICATE, ///< @copydoc SERD_PREDICATE
  object    = SERD_OBJECT,    ///< @copydoc SERD_OBJECT
  graph     = SERD_GRAPH      ///< @copydoc SERD_GRAPH
};

template<class CObj>
using StatementHandle = detail::StaticCopyable<CObj,
                                               serd_statement_copy,
                                               serd_statement_equals,
                                               serd_statement_free>;

template<class CObj>
class StatementWrapper;

/// View of a constant statement
using StatementView = StatementWrapper<const SerdStatement>;

/// Extra data managed by mutable (user created) Statement
struct StatementData {
  Node             _subject;
  Node             _predicate;
  Node             _object;
  Optional<Node>   _graph;
  Optional<Cursor> _cursor;
};

/// Statement wrapper
template<class CObj>
class StatementWrapper : public StatementHandle<CObj>
{
public:
  explicit StatementWrapper(CObj* statement)
    : StatementHandle<CObj>{statement}
  {}

  template<class C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  StatementWrapper(const StatementWrapper<C>& statement)
    : StatementHandle<CObj>{statement}
  {}

  /// @copydoc serd_statement_node
  NodeView node(Field field) const
  {
    return NodeView{
      serd_statement_node(this->cobj(), static_cast<SerdField>(field))};
  }

  /// @copydoc serd_statement_subject
  NodeView subject() const
  {
    return NodeView{serd_statement_subject(this->cobj())};
  }

  /// @copydoc serd_statement_predicate
  NodeView predicate() const
  {
    return NodeView{serd_statement_predicate(this->cobj())};
  }

  /// @copydoc serd_statement_object
  NodeView object() const
  {
    return NodeView{serd_statement_object(this->cobj())};
  }

  /// @copydoc serd_statement_graph
  Optional<NodeView> graph() const
  {
    return NodeView{serd_statement_graph(this->cobj())};
  }

  /// @copydoc serd_statement_cursor
  Optional<CursorView> cursor() const
  {
    return CursorView{serd_statement_cursor(this->cobj())};
  }

  /// @copydoc serd_statement_matches
  bool matches(Optional<NodeView> subject,
               Optional<NodeView> predicate,
               Optional<NodeView> object,
               Optional<NodeView> graph = {}) const
  {
    return serd_statement_matches(this->cobj(),
                                  subject.cobj(),
                                  predicate.cobj(),
                                  object.cobj(),
                                  graph.cobj());
  }

private:
  template<class CIter>
  friend class IterWrapper;

  StatementWrapper()
    : StatementHandle<CObj>{nullptr}
  {}
};

/// @copydoc SerdStatement
class Statement
  : public StatementData
  , public StatementWrapper<SerdStatement>
{
public:
  Statement(const NodeView&      s,
            const NodeView&      p,
            const NodeView&      o,
            const NodeView&      g,
            Optional<CursorView> cursor = {})
    : StatementData{s, p, o, g, cursor ? *cursor : Optional<Cursor>{}}
    , StatementWrapper{serd_statement_new(_subject.cobj(),
                                          _predicate.cobj(),
                                          _object.cobj(),
                                          _graph.cobj(),
                                          _cursor.cobj())}
  {}

  Statement(const NodeView&      s,
            const NodeView&      p,
            const NodeView&      o,
            Optional<CursorView> cursor = {})
    : StatementData{s, p, o, {}, cursor ? *cursor : Optional<Cursor>{}}
    , StatementWrapper{serd_statement_new(_subject.cobj(),
                                          _predicate.cobj(),
                                          _object.cobj(),
                                          nullptr,
                                          _cursor.cobj())}
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Statement(const StatementView& statement)
    : StatementData{statement.subject(),
                    statement.predicate(),
                    statement.object(),
                    statement.graph() ? *statement.graph() : Optional<Node>{},
                    statement.cursor() ? *statement.cursor()
                                       : Optional<Cursor>{}}
    , StatementWrapper{statement}
  {}
};

/**
   @}
   @}
   @defgroup serdpp_world World
   @{
*/

/// @copydoc SerdLogLevel
enum class LogLevel {
  emergency = SERD_LOG_LEVEL_EMERGENCY, ///< @copydoc SERD_LOG_LEVEL_EMERGENCY
  alert     = SERD_LOG_LEVEL_ALERT,     ///< @copydoc SERD_LOG_LEVEL_ALERT
  critical  = SERD_LOG_LEVEL_CRITICAL,  ///< @copydoc SERD_LOG_LEVEL_CRITICAL
  error     = SERD_LOG_LEVEL_ERROR,     ///< @copydoc SERD_LOG_LEVEL_ERROR
  warning   = SERD_LOG_LEVEL_WARNING,   ///< @copydoc SERD_LOG_LEVEL_WARNING
  notice    = SERD_LOG_LEVEL_NOTICE,    ///< @copydoc SERD_LOG_LEVEL_NOTICE
  info      = SERD_LOG_LEVEL_INFO,      ///< @copydoc SERD_LOG_LEVEL_INFO
  debug     = SERD_LOG_LEVEL_DEBUG      ///< @copydoc SERD_LOG_LEVEL_DEBUG
};

/// Extended fields for a log message
using LogFields = std::map<StringView, StringView>;

/// User-provided callback function for handling a log message
using LogFunc = std::function<Status(LogLevel, LogFields, std::string&&)>;

/// @copydoc SerdWorld
class World : public detail::StaticWrapper<SerdWorld, serd_world_free>
{
public:
  World()
    : StaticWrapper{serd_world_new()}
  {}

  NodeView get_blank() { return NodeView(serd_world_get_blank(cobj())); }

  void set_message_func(LogFunc log_func)
  {
    _log_func = std::move(log_func);
    serd_world_set_log_func(cobj(), s_log_func, this);
  }

  SERD_LOG_FUNC(4, 5)
  Status log(const LogLevel    level,
             const LogFields&  fields,
             const char* const fmt,
             ...)
  {
    va_list args;
    va_start(args, fmt);

    std::vector<SerdLogField> c_fields(fields.size());
    size_t                    index = 0;
    for (const auto& f : fields) {
      c_fields[index].key   = f.first.c_str();
      c_fields[index].value = f.second.c_str();
      ++index;
    }

    const SerdStatus st = serd_world_vlogf(cobj(),
                                           static_cast<SerdLogLevel>(level),
                                           fields.size(),
                                           c_fields.data(),
                                           fmt,
                                           args);

    va_end(args);
    return static_cast<Status>(st);
  }

private:
  SERD_LOG_FUNC(1, 0)
  static std::string format(const char* fmt, va_list args) noexcept
  {
    va_list args_copy;
    va_copy(args_copy, args);

    const auto n_bytes =
      static_cast<unsigned>(vsnprintf(nullptr, 0, fmt, args_copy));

    va_end(args_copy);

#if __cplusplus >= 201703L
    std::string result(n_bytes, '\0');
    vsnprintf(result.data(), n_bytes + 1u, fmt, args);
#else
    std::vector<char> str(n_bytes + 1u, '\0');
    vsnprintf(str.data(), n_bytes + 1u, fmt, args);
    std::string result(str.data(), size_t(n_bytes));
#endif
    return result;
  }

  static SerdStatus s_log_func(void* handle, const SerdLogEntry* entry) noexcept
  {
    const auto* const self = static_cast<const World*>(handle);
    try {
      LogFields fields;
      for (size_t i = 0; i < entry->n_fields; ++i) {
        fields.emplace(entry->fields[i].key, entry->fields[i].value);
      }

      return static_cast<SerdStatus>(
        self->_log_func(static_cast<LogLevel>(entry->level),
                        fields,
                        format(entry->fmt, *entry->args)));
    } catch (...) {
      return SERD_ERR_INTERNAL;
    }
  }

  LogFunc _log_func{};
};

/**
   @}
   @defgroup serdpp_streaming Data Streaming
   @{
*/

/**
   @defgroup serdpp_event Events
   @{
*/

// TODO

/// @copydoc SerdStatementFlag
enum class StatementFlag {
  empty_S = SERD_EMPTY_S, ///< @copydoc SERD_EMPTY_S
  anon_S  = SERD_ANON_S,  ///< @copydoc SERD_ANON_S
  anon_O  = SERD_ANON_O,  ///< @copydoc SERD_ANON_O
  list_S  = SERD_LIST_S,  ///< @copydoc SERD_LIST_S
  list_O  = SERD_LIST_O,  ///< @copydoc SERD_LIST_O
  terse_S = SERD_TERSE_S, ///< @copydoc SERD_TERSE_S
  terse_O = SERD_TERSE_O  ///< @copydoc SERD_TERSE_O
};

/// Bitwise OR of StatementFlag values
using StatementFlags = Flags<StatementFlag>;

/**
   @}
   @defgroup serdpp_sink Sink
   @{
*/

// FIXME: Document
using BaseFunc      = std::function<Status(NodeView)>;
using PrefixFunc    = std::function<Status(NodeView name, NodeView uri)>;
using StatementFunc = std::function<Status(StatementFlags, StatementView)>;
using EndFunc       = std::function<Status(NodeView)>;

/// Sink wrapper
template<class CSink>
class SinkWrapper : public detail::StaticWrapper<CSink, serd_sink_free>
{
public:
  explicit SinkWrapper(CSink* const ptr)
    : detail::StaticWrapper<CSink, serd_sink_free>{ptr}
  {}

  /// @copydoc serd_sink_write_base
  Status base(const NodeView& uri) const
  {
    return Status(serd_sink_write_base(this->cobj(), uri.cobj()));
  }

  /// @copydoc serd_sink_write_prefix
  Status prefix(NodeView name, const NodeView& uri) const
  {
    return Status(
      serd_sink_write_prefix(this->cobj(), name.cobj(), uri.cobj()));
  }

  /// @copydoc serd_sink_write_statement
  Status statement(StatementFlags flags, StatementView statement) const
  {
    return Status(
      serd_sink_write_statement(this->cobj(), flags, statement.cobj()));
  }

  /// @copydoc serd_sink_write
  Status write(StatementFlags     flags,
               const NodeView&    subject,
               const NodeView&    predicate,
               const NodeView&    object,
               Optional<NodeView> graph = {}) const
  {
    return Status(serd_sink_write(this->cobj(),
                                  flags,
                                  subject.cobj(),
                                  predicate.cobj(),
                                  object.cobj(),
                                  graph.cobj()));
  }

  /// @copydoc serd_sink_write_end
  Status end(const NodeView& node) const
  {
    return Status(serd_sink_write_end(this->cobj(), node.cobj()));
  }
};

/// Sink view
class SinkView : public SinkWrapper<const SerdSink>
{
public:
  explicit SinkView(const SerdSink* const ptr)
    : SinkWrapper<const SerdSink>{ptr}
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  SinkView(const SinkWrapper<SerdSink>& sink)
    : SinkWrapper<const SerdSink>{sink.cobj()}
  {}
};

/// @copydoc SerdSink
class Sink : public SinkWrapper<SerdSink>
{
public:
  Sink()
    : SinkWrapper{serd_sink_new(this, s_event, nullptr)}
  {}

  void set_base_func(BaseFunc base_func) { _base_func = std::move(base_func); }

  void set_prefix_func(PrefixFunc prefix_func)
  {
    _prefix_func = std::move(prefix_func);
  }

  void set_statement_func(StatementFunc statement_func)
  {
    _statement_func = std::move(statement_func);
  }

  void set_end_func(EndFunc end_func) { _end_func = std::move(end_func); }

private:
  static SerdStatus s_base(void* handle, const SerdNode* uri) noexcept
  {
    const auto* const sink = static_cast<const Sink*>(handle);
    return sink->_base_func ? SerdStatus(sink->_base_func(NodeView(uri)))
                            : SERD_SUCCESS;
  }

  static SerdStatus s_prefix(void*           handle,
                             const SerdNode* name,
                             const SerdNode* uri) noexcept
  {
    const auto* const sink = static_cast<const Sink*>(handle);
    return sink->_prefix_func
             ? SerdStatus(sink->_prefix_func(NodeView(name), NodeView(uri)))
             : SERD_SUCCESS;
  }

  static SerdStatus s_statement(void*                handle,
                                SerdStatementFlags   flags,
                                const SerdStatement* statement) noexcept
  {
    const auto* const sink = static_cast<const Sink*>(handle);
    return sink->_statement_func
             ? SerdStatus(sink->_statement_func(StatementFlags(flags),
                                                StatementView(statement)))
             : SERD_SUCCESS;
  }

  static SerdStatus s_end(void* handle, const SerdNode* node) noexcept
  {
    const auto* const sink = static_cast<const Sink*>(handle);
    return sink->_end_func ? SerdStatus(sink->_end_func(NodeView(node)))
                           : SERD_SUCCESS;
  }

  static SerdStatus s_event(void* handle, const SerdEvent* event) noexcept
  {
    const auto* const sink = static_cast<const Sink*>(handle);

    switch (event->type) {
    case SERD_BASE:
      return sink->_base_func
               ? SerdStatus(sink->_base_func(NodeView(event->base.uri)))
               : SERD_SUCCESS;
    case SERD_PREFIX:
      return sink->_prefix_func
               ? SerdStatus(sink->_prefix_func(NodeView(event->prefix.name),
                                               NodeView(event->prefix.uri)))
               : SERD_SUCCESS;
    case SERD_STATEMENT:
      return sink->_statement_func
               ? SerdStatus(sink->_statement_func(
                   StatementFlags(event->statement.flags),
                   StatementView(event->statement.statement)))
               : SERD_SUCCESS;
    case SERD_END:
      return sink->_end_func
               ? SerdStatus(sink->_end_func(NodeView(event->end.node)))
               : SERD_SUCCESS;
    }

    return SERD_SUCCESS;
  }

  BaseFunc      _base_func{};
  PrefixFunc    _prefix_func{};
  StatementFunc _statement_func{};
  EndFunc       _end_func{};
};

/**
   @}
   @defgroup serdpp_canon Canon
   @{
*/

/// @copydoc SerdCanonFlag
enum class CanonFlag {
  lax = SERD_CANON_LAX ///< @copydoc SERD_CANON_LAX
};

/// @copydoc SerdCanonFlags
using CanonFlags = Flags<CanonFlag>;

/// @copydoc serd_canon_new
inline SinkWrapper<SerdSink>
make_canon(const World& world, SinkView target, const CanonFlags flags)
{
  return SinkWrapper<SerdSink>{
    serd_canon_new(world.cobj(), target.cobj(), flags)};
}

/**
   @}
   @defgroup serdpp_filter Filter
   @{
*/

/// @copydoc serd_filter_new
inline SinkWrapper<SerdSink>
make_filter(SinkView           target,
            Optional<NodeView> subject,
            Optional<NodeView> predicate,
            Optional<NodeView> object,
            Optional<NodeView> graph,
            const bool         inclusive)
{
  return SinkWrapper<SerdSink>{serd_filter_new(target.cobj(),
                                               subject.cobj(),
                                               predicate.cobj(),
                                               object.cobj(),
                                               graph.cobj(),
                                               inclusive)};
}

/**
   @}
   @}
   @defgroup serdpp_env Environment
   @{
*/

template<class CObj>
using EnvHandle =
  detail::StaticCopyable<CObj, serd_env_copy, serd_env_equals, serd_env_free>;

/// Env wrapper
template<class CObj>
class EnvWrapper : public EnvHandle<CObj>
{
public:
  /// Return the base URI
  NodeView base_uri() const
  {
    return NodeView(serd_env_base_uri(this->cobj()));
  }

  /// Set the base URI
  Status set_base_uri(const StringView& uri)
  {
    return Status(serd_env_set_base_uri(this->cobj(), uri));
  }

  /// Set a namespace prefix
  Status set_prefix(StringView name, StringView uri)
  {
    return Status(serd_env_set_prefix(this->cobj(), name, uri));
  }

  /// Qualify `uri` into a CURIE if possible
  Optional<Node> qualify(const NodeView& uri) const
  {
    return Node{serd_env_qualify(this->cobj(), uri.cobj())};
  }

  /// Expand `node` into an absolute URI if possible
  Optional<Node> expand(const NodeView& node) const
  {
    return Node{serd_env_expand(this->cobj(), node.cobj())};
  }

  /// Send all prefixes to `sink`
  void write_prefixes(SinkView sink) const
  {
    serd_env_write_prefixes(this->cobj(), sink.cobj());
  }

protected:
  explicit EnvWrapper(std::unique_ptr<CObj> ptr)
    : EnvHandle<CObj>{std::move(ptr)}
  {}

  explicit EnvWrapper(CObj* const ptr)
    : EnvHandle<CObj>{ptr}
  {}
};

/// EnvView
using EnvView = EnvWrapper<const SerdEnv>;

/// @copydoc SerdEnv
class Env : public EnvWrapper<SerdEnv>
{
public:
  Env()
    : EnvWrapper{serd_env_new(SERD_EMPTY_STRING())}
  {}

  explicit Env(const NodeView& base)
    : EnvWrapper{serd_env_new(base.string_view())}
  {}
};

/**
   @}
   @defgroup serdpp_syntax_io Reading and Writing
   @{
   @defgroup serdpp_byte_source Byte Source
   @{
*/

/// @copydoc SerdByteSource
class ByteSource
  : public detail::StaticWrapper<SerdByteSource, serd_byte_source_free>
{
public:
  explicit ByteSource(std::istream& stream)
    : StaticWrapper{serd_byte_source_new_function(s_read,
                                                  s_error,
                                                  nullptr,
                                                  this,
                                                  nullptr,
                                                  1)}
    , _stream(&stream)
  {}

  explicit ByteSource(const std::string& string)
    : StaticWrapper{serd_byte_source_new_string(string.c_str(), nullptr)}
    , _stream{nullptr}
  {}

  ByteSource(const ByteSource&) = delete;
  ByteSource& operator=(const ByteSource&) = delete;

  ByteSource(ByteSource&&) = delete;
  ByteSource& operator=(ByteSource&&) = delete;

  ~ByteSource() = default;

  static inline size_t s_read(void*  buf,
                              size_t size,
                              size_t nmemb,
                              void*  source) noexcept
  {
    assert(size == 1);
    (void)size;

    auto* self = static_cast<ByteSource*>(source);

    try {
      self->_stream->read(static_cast<char*>(buf),
                          static_cast<std::streamsize>(nmemb));
      if (self->_stream->fail()) {
        return 0;
      }
    } catch (...) {
    }
    return nmemb;
  }

  static inline int s_error(void* source) noexcept
  {
    auto* self = static_cast<ByteSource*>(source);

    return static_cast<int>(self->_stream->fail());
  }

private:
  std::istream* _stream;
};

/**
   @}
   @defgroup serdpp_reader Reader
   @{
*/

/// @copydoc SerdReaderFlag
enum class ReaderFlag {
  lax = SERD_READ_LAX ///< @copydoc SERD_READ_LAX
};

/// @copydoc SerdReaderFlags
using ReaderFlags = Flags<ReaderFlag>;

/// @copydoc SerdReader
class Reader : public detail::StaticWrapper<SerdReader, serd_reader_free>
{
public:
  Reader(World&            world,
         const Syntax      syntax,
         const ReaderFlags flags,
         Env&              env,
         SinkView          sink,
         size_t            stack_size)
    : StaticWrapper{serd_reader_new(world.cobj(),
                                    SerdSyntax(syntax),
                                    flags,
                                    env.cobj(),
                                    sink.cobj(),
                                    stack_size)}
  {}

  void add_blank_prefix(StringView prefix)
  {
    serd_reader_add_blank_prefix(cobj(), prefix.data());
  }

  Status start(ByteSource& byte_source)
  {
    return Status(serd_reader_start(cobj(), byte_source.cobj()));
  }

  Status read_chunk() { return Status(serd_reader_read_chunk(cobj())); }

  Status read_document() { return Status(serd_reader_read_document(cobj())); }

  Status finish() { return Status(serd_reader_finish(cobj())); }

private:
  static inline size_t s_stream_read(void*  buf,
                                     size_t size,
                                     size_t nmemb,
                                     void*  stream) noexcept
  {
    assert(size == 1);
    (void)size;

    try {
      auto* const s = static_cast<std::istream*>(stream);
      s->read(static_cast<char*>(buf), std::streamsize(nmemb));
      if (s->good()) {
        return nmemb;
      }
    } catch (...) {
    }

    return 0;
  }

  static inline int s_stream_error(void* stream) noexcept
  {
    try {
      auto* const s = static_cast<std::istream*>(stream);
      return (!(s->good()));
    } catch (...) {
    }

    return 1;
  }
};

/**
   @}
   @defgroup serdpp_byte_sink Byte Sink
   @{
*/

/**
   Sink function for string output.

   Similar semantics to `SerdWriteFunc` (and in turn `fwrite`), but takes char*
   for convenience and may set errno for more informative error reporting than
   supported by `SerdStreamErrorFunc`.

   @return Number of elements (bytes) written, which is short on error.
*/
using WriteFunc = std::function<size_t(const char*, size_t)>;

/// @copydoc SerdByteSink
class ByteSink : public detail::StaticWrapper<SerdByteSink, serd_byte_sink_free>
{
public:
  /// Create a byte sink that writes to a function in blocks
  ByteSink(WriteFunc write_func, const size_t block_size)
    : StaticWrapper{serd_byte_sink_new_function(s_write, this, block_size)}
    , _write_func{std::move(write_func)}
  {}

  ByteSink(StringView filename, const size_t block_size)
    : StaticWrapper{serd_byte_sink_new_filename(filename.c_str(), block_size)}
  {}

  /// Create a byte sink that writes to a function one byte at a time
  explicit ByteSink(WriteFunc write_func)
    : ByteSink{std::move(write_func), 1}
  {}

  /// Create a byte sink from a standard output stream
  explicit ByteSink(std::ostream& stream)
    : StaticWrapper{serd_byte_sink_new_function(s_write, this, 1)}
    , _write_func{[&](const char* str, size_t len) {
      stream.write(str, std::streamsize(len));
      return stream.good() ? len : size_t(0);
    }}
  {}

  Status close() { return static_cast<Status>(serd_byte_sink_close(cobj())); }

private:
  static inline size_t s_write(const void* buf,
                               size_t      size,
                               size_t      nmemb,
                               void*       sink) noexcept
  {
    assert(size == 1);
    (void)size;

    auto* self = static_cast<ByteSink*>(sink);

    try {
      return self->_write_func(static_cast<const char*>(buf), nmemb);
    } catch (...) {
    }

    return 0;
  }

  WriteFunc _write_func;
};

/**
   @}
   @defgroup serdpp_writer Writer
   @{
*/

/// @copydoc SerdWriterFlag
enum class WriterFlag {
  ascii = SERD_WRITE_ASCII, ///< @copydoc SERD_WRITE_ASCII
  terse = SERD_WRITE_TERSE, ///< @copydoc SERD_WRITE_TERSE
  lax   = SERD_WRITE_LAX    ///< @copydoc SERD_WRITE_LAX
};

/// @copydoc SerdWriterFlags
using WriterFlags = Flags<WriterFlag>;

/// @copydoc SerdWriter
class Writer : public detail::StaticWrapper<SerdWriter, serd_writer_free>
{
public:
  /**
     Create a writer that writes syntax to the given byte sink.

     @param world The world that this writer is a part of.

     @param syntax Syntax to write.

     @param flags Flags to control writer behaviour.

     @param env Environment used for expansion and abbreviation.  The writer
     uses a reference to this, so the environment must outlive the writer.

     @param byte_sink Byte sink for receiving output.The writer uses a
     reference to this, so the environment must outlive the writer.
  */
  Writer(World&            world,
         const Syntax      syntax,
         const WriterFlags flags,
         Env&              env,
         ByteSink&         byte_sink)
    : StaticWrapper{serd_writer_new(world.cobj(),
                                    SerdSyntax(syntax),
                                    flags,
                                    env.cobj(),
                                    byte_sink.cobj())}
  {}

  /// Return a sink that can be used to write data
  SinkView sink() { return SinkView{serd_writer_sink(cobj())}; }

  /// @copydoc serd_writer_set_root_uri
  Status set_root_uri(const NodeView& uri)
  {
    return Status(serd_writer_set_root_uri(cobj(), uri.cobj()));
  }

  /// @copydoc serd_writer_finish
  Status finish() { return Status(serd_writer_finish(cobj())); }
};

/**
   @}
   @}
   @defgroup serdpp_storage Storage
   @{
*/

/**
   @defgroup serdpp_iterator Iterator
   @{
*/

template<class CObj>
using IterHandle = detail::
  DynamicCopyable<CObj, serd_iter_copy, serd_iter_equals, serd_iter_free>;

template<class CObj>
class IterWrapper : public IterHandle<CObj>
{
public:
  IterWrapper(IterWrapper&&) noexcept = default;
  IterWrapper(const IterWrapper&)     = default;

  IterWrapper& operator=(IterWrapper&&) noexcept = default;
  IterWrapper& operator=(const IterWrapper&) = default;

  ~IterWrapper() = default;

  const StatementView& operator*() const
  {
    _statement = StatementView{serd_iter_get(this->cobj())};
    return _statement;
  }

  const StatementView* operator->() const
  {
    _statement = StatementView{serd_iter_get(this->cobj())};
    return &_statement;
  }

protected:
  IterWrapper(CObj* ptr, detail::Ownership ownership)
    : IterHandle<CObj>{{ptr, ownership}}
  {}

  mutable StatementView _statement{};
};

/// Iter view
class IterView : public IterWrapper<const SerdIter>
{
public:
  explicit IterView(const SerdIter* const ptr)
    : IterWrapper<const SerdIter>{ptr, detail::Ownership::view}
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  IterView(const IterWrapper<SerdIter>& iter)
    : IterWrapper<const SerdIter>{iter.cobj(), detail::Ownership::view}
  {}
};

// using IterView = IterWrapper<const SerdIter>;

/// @copydoc SerdIter
class Iter : public IterWrapper<SerdIter>
{
public:
  Iter(IterView iter)
    : IterWrapper{serd_iter_copy(iter.cobj()), detail::Ownership::owned}
  {}

  Iter(SerdIter* const ptr, const detail::Ownership ownership)
    : IterWrapper{ptr, ownership}
  {}

  Iter(Iter&&)      = default;
  Iter(const Iter&) = default;

  Iter& operator=(Iter&&) = default;
  Iter& operator=(const Iter&) = default;

  ~Iter() = default;

  Iter& operator++()
  {
    serd_iter_next(this->cobj());
    return *this;
  }
};

/**
   @}
   @defgroup serdpp_range Range
   @{
*/

/// @copydoc SerdSerialisationFlag
enum class SerialisationFlag {
  no_inline_objects = SERD_NO_INLINE_OBJECTS ///< Disable object inlining
};

/// Bitwise OR of SerialisationFlag values
using SerialisationFlags = Flags<SerialisationFlag>;

/// @copydoc SerdRange
class Range
  : public detail::StaticCopyable<SerdRange,
                                  serd_range_copy,
                                  serd_range_equals,
                                  serd_range_free>
{
public:
  /// Return an iterator to the first element in the range
  const Iter& begin() const { return _begin; }

  /// Return an iterator one past the last element in the range
  Iter& begin() { return _begin; }

  /// Return an iterator one past the last element in the range
  const Iter& end() const { return _end; }

  /// @copydoc serd_write_range
  Status write(SinkView sink, SerialisationFlags flags = {})
  {
    return Status(serd_write_range(cobj(), sink.cobj(), flags));
  }

  /// @copydoc serd_range_empty
  bool empty() const { return serd_range_empty(cobj()); }

private:
  friend class Model;

  explicit Range(SerdRange* const r)
    : StaticCopyable{r}
    , _begin{serd_range_begin(r), detail::Ownership::view}
    , _end{serd_range_end(r), detail::Ownership::view}
  {}

  Iter _begin;
  Iter _end;
};

/**
   @}
   @defgroup serdpp_model Model
   @{
*/

/// @copydoc SerdModelFlag
enum class ModelFlag {
  index_SPO     = SERD_INDEX_SPO,    ///< @copydoc SERD_INDEX_SPO
  index_SOP     = SERD_INDEX_SOP,    ///< @copydoc SERD_INDEX_SOP
  index_OPS     = SERD_INDEX_OPS,    ///< @copydoc SERD_INDEX_OPS
  index_OSP     = SERD_INDEX_OSP,    ///< @copydoc SERD_INDEX_OSP
  index_PSO     = SERD_INDEX_PSO,    ///< @copydoc SERD_INDEX_PSO
  index_POS     = SERD_INDEX_POS,    ///< @copydoc SERD_INDEX_POS
  index_graphs  = SERD_INDEX_GRAPHS, ///< @copydoc SERD_INDEX_GRAPHS
  store_cursors = SERD_STORE_CURSORS ///< @copydoc SERD_STORE_CURSORS
};

/// Bitwise OR of ModelFlag values
using ModelFlags = Flags<ModelFlag>;

/// @copydoc SerdStatementOrder
enum class StatementOrder {
  SPO,  ///<         Subject,   Predicate, Object
  SOP,  ///<         Subject,   Object,    Predicate
  OPS,  ///<         Object,    Predicate, Subject
  OSP,  ///<         Object,    Subject,   Predicate
  PSO,  ///<         Predicate, Subject,   Object
  POS,  ///<         Predicate, Object,    Subject
  GSPO, ///< Graph,  Subject,   Predicate, Object
  GSOP, ///< Graph,  Subject,   Object,    Predicate
  GOPS, ///< Graph,  Object,    Predicate, Subject
  GOSP, ///< Graph,  Object,    Subject,   Predicate
  GPSO, ///< Graph,  Predicate, Subject,   Object
  GPOS  ///< Graph,  Predicate, Object,    Subject
};

using ModelHandle = detail::StaticCopyable<SerdModel,
                                           serd_model_copy,
                                           serd_model_equals,
                                           serd_model_free>;

/// @copydoc SerdModel
class Model : public ModelHandle
{
public:
  using value_type     = Statement;
  using iterator       = Iter;
  using const_iterator = Iter;

  Model(World& world, ModelFlags flags)
    : ModelHandle{serd_model_new(world.cobj(), flags)}
  {}

  /// @copydoc serd_model_size
  size_t size() const { return serd_model_size(cobj()); }

  /// @copydoc serd_model_empty
  bool empty() const { return serd_model_empty(cobj()); }

  /// @copydoc serd_model_insert
  Status insert(StatementView s)
  {
    return static_cast<Status>(serd_model_insert(cobj(), s.cobj()));
  }

  /// @copydoc serd_model_add
  Status insert(const NodeView&    s,
                const NodeView&    p,
                const NodeView&    o,
                Optional<NodeView> g = {})
  {
    return static_cast<Status>(
      serd_model_add(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
  }

  /// @copydoc serd_model_add_range
  Status insert(Range&& range)
  {
    return static_cast<Status>(serd_model_add_range(cobj(), range.cobj()));
  }

  /**
     Remove a statement from a model via an iterator.

     Calling this function invalidates all iterators on `model` except `iter`.

     @param iter Iterator to the element to erase.

     @returns An iterator to the statement following the erased statement,
     or the end iterator if the statement was the last or an error occurred.
  */
  Iter erase(Iter iter)
  {
    if (!serd_model_erase(cobj(), iter.cobj())) {
      return iter;
    }

    return end();
  }

  Iter erase(const IterView& iter)
  {
    Iter iter_copy{iter};

    if (!serd_model_erase(cobj(), iter_copy.cobj())) {
      return iter_copy;
    }

    return end();
  }

  /**
     Remove a range from a model.

     Calling this function invalidates all iterators on `model` except `iter`.

     @param range Range to erase.
  */
  Status erase(Range range)
  {
    return static_cast<Status>(serd_model_erase_range(cobj(), range.cobj()));
  }

  /// @copydoc serd_model_find
  Iter find(Optional<NodeView> s,
            Optional<NodeView> p,
            Optional<NodeView> o,
            Optional<NodeView> g = {}) const
  {
    return Iter(serd_model_find(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()),
                detail::Ownership::owned);
  }

  /// @copydoc serd_model_range
  Range range(Optional<NodeView> s,
              Optional<NodeView> p,
              Optional<NodeView> o,
              Optional<NodeView> g = {}) const
  {
    return Range(
      serd_model_range(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
  }

  /// @copydoc serd_model_get
  Optional<NodeView> get(Optional<NodeView> s,
                         Optional<NodeView> p,
                         Optional<NodeView> o,
                         Optional<NodeView> g = {}) const
  {
    return NodeView(
      serd_model_get(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
  }

  /// @copydoc serd_model_get_statement
  Optional<StatementView> get_statement(Optional<NodeView> s,
                                        Optional<NodeView> p,
                                        Optional<NodeView> o,
                                        Optional<NodeView> g = {}) const
  {
    return StatementView(
      serd_model_get_statement(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
  }

  /// @copydoc serd_model_ask
  bool ask(Optional<NodeView> s,
           Optional<NodeView> p,
           Optional<NodeView> o,
           Optional<NodeView> g = {}) const
  {
    return serd_model_ask(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj());
  }

  /// @copydoc serd_model_count
  size_t count(Optional<NodeView> s,
               Optional<NodeView> p,
               Optional<NodeView> o,
               Optional<NodeView> g = {}) const
  {
    return serd_model_count(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj());
  }

  /// @copydoc serd_model_all
  Range all() const { return Range(serd_model_all(cobj())); }

  /// @copydoc serd_model_ordered
  Range ordered(StatementOrder order) const
  {
    return Range(
      serd_model_ordered(cobj(), static_cast<SerdStatementOrder>(order)));
  }

  iterator begin() const
  {
    return iterator(serd_model_begin(cobj()), detail::Ownership::owned);
  }

  iterator end() const
  {
    // TODO: cache
    return iterator(serd_iter_copy(serd_model_end(cobj())),
                    detail::Ownership::owned);
  }

private:
  friend class Optional<Model>;

  explicit Model(std::nullptr_t)
    : StaticCopyable{nullptr}
  {}
};

/**
   @}
   @defgroup serdpp_inserter Inserter
   @{
*/

/**
   Create an inserter that inserts statements into a model.

   @param model The model to insert received statements into.
*/
inline SinkWrapper<SerdSink>
make_inserter(Model& model)
{
  return SinkWrapper<SerdSink>{serd_inserter_new(model.cobj(), nullptr)};
}

/**
   Create an inserter that inserts statements into a specific graph in a model.

   @param model The model to insert received statements into.

   @param default_graph The default graph to set for any statements that have
   no graph.  This allows, for example, loading a Turtle document into an
   isolated graph in the model.
*/
inline SinkWrapper<SerdSink>
make_inserter(Model& model, NodeView default_graph)
{
  return SinkWrapper<SerdSink>{
    serd_inserter_new(model.cobj(), default_graph.cobj())};
}

/**
   @}
   @defgroup serdpp_validator Validator
   @{
*/

/// @copydoc SerdValidatorCheck
enum class ValidatorCheck : uint64_t {
  /// @copydoc SERD_CHECK_NOTHING
  nothing,

  /// @copydoc SERD_CHECK_ALL_VALUES_FROM
  all_values_from = SERD_CHECK_ALL_VALUES_FROM,

  /// @copydoc SERD_CHECK_ANY_URI
  any_uri = SERD_CHECK_ANY_URI,

  /// @copydoc SERD_CHECK_CARDINALITY_EQUAL
  cardinality_equal = SERD_CHECK_CARDINALITY_EQUAL,

  /// @copydoc SERD_CHECK_CARDINALITY_MAX
  cardinality_max = SERD_CHECK_CARDINALITY_MAX,

  /// @copydoc SERD_CHECK_CARDINALITY_MIN
  cardinality_min = SERD_CHECK_CARDINALITY_MIN,

  /// @copydoc SERD_CHECK_CLASS_CYCLE
  class_cycle = SERD_CHECK_CLASS_CYCLE,

  /// @copydoc SERD_CHECK_CLASS_LABEL
  class_label = SERD_CHECK_CLASS_LABEL,

  /// @copydoc SERD_CHECK_DATATYPE_PROPERTY
  datatype_property = SERD_CHECK_DATATYPE_PROPERTY,

  /// @copydoc SERD_CHECK_DATATYPE_TYPE
  datatype_type = SERD_CHECK_DATATYPE_TYPE,

  /// @copydoc SERD_CHECK_DEPRECATED_CLASS
  deprecated_class = SERD_CHECK_DEPRECATED_CLASS,

  /// @copydoc SERD_CHECK_DEPRECATED_PROPERTY
  deprecated_property = SERD_CHECK_DEPRECATED_PROPERTY,

  /// @copydoc SERD_CHECK_FUNCTIONAL_PROPERTY
  functional_property = SERD_CHECK_FUNCTIONAL_PROPERTY,

  /// @copydoc SERD_CHECK_INSTANCE_LITERAL
  instance_literal = SERD_CHECK_INSTANCE_LITERAL,

  /// @copydoc SERD_CHECK_INSTANCE_TYPE
  instance_type = SERD_CHECK_INSTANCE_TYPE,

  /// @copydoc SERD_CHECK_INVERSE_FUNCTIONAL_PROPERTY
  inverse_functional_property = SERD_CHECK_INVERSE_FUNCTIONAL_PROPERTY,

  /// @copydoc SERD_CHECK_LITERAL_INSTANCE
  literal_instance = SERD_CHECK_LITERAL_INSTANCE,

  /// @copydoc SERD_CHECK_LITERAL_MAX_EXCLUSIVE
  literal_max_exclusive = SERD_CHECK_LITERAL_MAX_EXCLUSIVE,

  /// @copydoc SERD_CHECK_LITERAL_MAX_INCLUSIVE
  literal_max_inclusive = SERD_CHECK_LITERAL_MAX_INCLUSIVE,

  /// @copydoc SERD_CHECK_LITERAL_MIN_EXCLUSIVE
  literal_min_exclusive = SERD_CHECK_LITERAL_MIN_EXCLUSIVE,

  /// @copydoc SERD_CHECK_LITERAL_MIN_INCLUSIVE
  literal_min_inclusive = SERD_CHECK_LITERAL_MIN_INCLUSIVE,

  /// @copydoc SERD_CHECK_LITERAL_PATTERN
  literal_pattern = SERD_CHECK_LITERAL_PATTERN,

  /// @copydoc SERD_CHECK_LITERAL_RESTRICTION
  literal_restriction = SERD_CHECK_LITERAL_RESTRICTION,

  /// @copydoc SERD_CHECK_LITERAL_VALUE
  literal_value = SERD_CHECK_LITERAL_VALUE,

  /// @copydoc SERD_CHECK_OBJECT_PROPERTY
  object_property = SERD_CHECK_OBJECT_PROPERTY,

  /// @copydoc SERD_CHECK_PLAIN_LITERAL_DATATYPE
  plain_literal_datatype = SERD_CHECK_PLAIN_LITERAL_DATATYPE,

  /// @copydoc SERD_CHECK_PREDICATE_TYPE
  predicate_type = SERD_CHECK_PREDICATE_TYPE,

  /// @copydoc SERD_CHECK_PROPERTY_CYCLE
  property_cycle = SERD_CHECK_PROPERTY_CYCLE,

  /// @copydoc SERD_CHECK_PROPERTY_DOMAIN
  property_domain = SERD_CHECK_PROPERTY_DOMAIN,

  /// @copydoc SERD_CHECK_PROPERTY_LABEL
  property_label = SERD_CHECK_PROPERTY_LABEL,

  /// @copydoc SERD_CHECK_PROPERTY_RANGE
  property_range = SERD_CHECK_PROPERTY_RANGE,

  /// @copydoc SERD_CHECK_SOME_VALUES_FROM
  some_values_from = SERD_CHECK_SOME_VALUES_FROM,
};

/// @copydoc SerdValidator
class Validator
  : public detail::StaticWrapper<SerdValidator, serd_validator_free>
{
public:
  /// Create a new validator with no checks enabled
  explicit Validator(World& world)
    : StaticWrapper{serd_validator_new(world.cobj())}
  {}

  /// @copydoc serd_validator_enable_checks
  Status enable_checks(StringView pattern)
  {
    return static_cast<Status>(
      serd_validator_enable_checks(cobj(), pattern.c_str()));
  }

  /// @copydoc serd_validator_disable_checks
  Status disable_checks(StringView pattern)
  {
    return static_cast<Status>(
      serd_validator_disable_checks(cobj(), pattern.c_str()));
  }

  /**
     Validate all statements in a model.

     This performs validation based on the XSD, RDF, RDFS, and OWL
     vocabularies.  All necessary data, including those vocabularies and any
     property/class definitions that use them, are assumed to be in the model.

     Validation errors are reported to the world's error sink.

     @param model The model to validate.

     @return @ref Status::success if no errors are found, or @ref
     Status::err_invalid if validation checks failed.
  */
  Status validate_model(const Model& model)
  {
    return static_cast<Status>(
      serd_validate_model(cobj(), model.cobj(), nullptr));
  }

  /**
     Validate all statements in a specific graph in a model.

     This performs the same validation as the other overload, but only
     validates statements in the given graph.  The entire model is still
     searched while running a check so that, for example, schemas that define
     classes and properties can be stored in separate graphs.

     @return @ref Status::success if no errors are found, or @ref
     Status::err_invalid if validation checks failed.
  */
  Status validate_model(const Model& model, NodeView graph)
  {
    return static_cast<Status>(
      serd_validate_model(cobj(), model.cobj(), graph.cobj()));
  }
};

/**
   @}
   @}
   @}
*/

} // namespace serd

#endif // SERD_SERD_HPP
