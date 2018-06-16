// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SERD_HPP
#define SERD_SERD_HPP

#include "serd/Flags.hpp"           // IWYU pragma: export
#include "serd/Optional.hpp"        // IWYU pragma: export
#include "serd/StringView.hpp"      // IWYU pragma: export
#include "serd/detail/Copyable.hpp" // IWYU pragma: export
#include "serd/detail/Wrapper.hpp"  // IWYU pragma: export
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
  success       = SERD_SUCCESS,       ///< @copydoc SERD_SUCCESS
  failure       = SERD_FAILURE,       ///< @copydoc SERD_FAILURE
  unknown_error = SERD_UNKNOWN_ERROR, ///< @copydoc SERD_UNKNOWN_ERROR
  no_data       = SERD_NO_DATA,       ///< @copydoc SERD_NO_DATA
  overflow      = SERD_OVERFLOW,      ///< @copydoc SERD_OVERFLOW

  bad_alloc   = SERD_BAD_ALLOC,   ///< @copydoc SERD_BAD_ALLOC
  bad_arg     = SERD_BAD_ARG,     ///< @copydoc SERD_BAD_ARG
  bad_call    = SERD_BAD_CALL,    ///< @copydoc SERD_BAD_CALL
  bad_curie   = SERD_BAD_CURIE,   ///< @copydoc SERD_BAD_CURIE
  bad_cursor  = SERD_BAD_CURSOR,  ///< @copydoc SERD_BAD_CURSOR
  bad_event   = SERD_BAD_EVENT,   ///< @copydoc SERD_BAD_EVENT
  bad_index   = SERD_BAD_INDEX,   ///< @copydoc SERD_BAD_INDEX
  bad_label   = SERD_BAD_LABEL,   ///< @copydoc SERD_BAD_LABEL
  bad_literal = SERD_BAD_LITERAL, ///< @copydoc SERD_BAD_LITERAL
  bad_pattern = SERD_BAD_PATTERN, ///< @copydoc SERD_BAD_PATTERN
  bad_read    = SERD_BAD_READ,    ///< @copydoc SERD_BAD_READ
  bad_stack   = SERD_BAD_STACK,   ///< @copydoc SERD_BAD_STACK
  bad_syntax  = SERD_BAD_SYNTAX,  ///< @copydoc SERD_BAD_SYNTAX
  bad_text    = SERD_BAD_TEXT,    ///< @copydoc SERD_BAD_TEXT
  bad_uri     = SERD_BAD_URI,     ///< @copydoc SERD_BAD_URI
  bad_write   = SERD_BAD_WRITE,   ///< @copydoc SERD_BAD_WRITE
  bad_data    = SERD_BAD_DATA,    ///< @copydoc SERD_BAD_DATA
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

// FIXME: grouping?
static inline size_t
stream_write(const void* buf, size_t size, size_t nmemb, void* sink) noexcept
{
  (void)size;
  assert(size == 1);

  std::ostream& os = *static_cast<std::ostream*>(sink);

  try {
    os.write(static_cast<const char*>(buf),
             static_cast<std::streamsize>(nmemb));
    return os.good() ? nmemb : 0U;
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
  return static_cast<Syntax>(serd_syntax_by_name(name.c_str()));
}

/// @copydoc serd_guess_syntax
inline Syntax
guess_syntax(StringView filename)
{
  return static_cast<Syntax>(serd_guess_syntax(filename.c_str()));
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
   @param hostname If non-null, set to the hostname, if present.
   @return A filesystem path.
*/
inline std::string
parse_file_uri(StringView uri, std::string* hostname = nullptr)
{
  char* c_hostname = nullptr;
  char* c_path     = serd_parse_file_uri(nullptr, uri.data(), &c_hostname);
  if (hostname && c_hostname) {
    *hostname = c_hostname;
  }

  std::string path{c_path};
  zix_free(nullptr, c_hostname);
  zix_free(nullptr, c_path);
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
  static Component make_component(const ZixStringView slice)
  {
    return slice.data ? Component{slice.data, slice.length} : Component{};
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

/// @copydoc SerdValue
using Value = SerdValue;

template<class T>
inline SerdValue
value(T value);

template<>
inline SerdValue
value(const bool value)
{
  return serd_bool(value);
}

template<>
inline SerdValue
value(const double value)
{
  return serd_double(value);
}

template<>
inline SerdValue
value(const float value)
{
  return serd_float(value);
}

template<>
inline SerdValue
value(const int64_t value)
{
  return serd_long(value);
}

template<>
inline SerdValue
value(const int32_t value)
{
  return serd_int(value);
}

template<>
inline SerdValue
value(const int16_t value)
{
  return serd_short(value);
}

template<>
inline SerdValue
value(const int8_t value)
{
  return serd_byte(value);
}

template<>
inline SerdValue
value(const uint64_t value)
{
  return serd_ulong(value);
}

template<>
inline SerdValue
value(const uint32_t value)
{
  return serd_uint(value);
}

template<>
inline SerdValue
value(const uint16_t value)
{
  return serd_ushort(value);
}

template<>
inline SerdValue
value(const uint8_t value)
{
  return serd_ubyte(value);
}

/// @copydoc SerdNodeType
enum class NodeType {
  literal  = SERD_LITERAL,  ///< @copydoc SERD_LITERAL
  URI      = SERD_URI,      ///< @copydoc SERD_URI
  blank    = SERD_BLANK,    ///< @copydoc SERD_BLANK
  variable = SERD_VARIABLE, ///< @copydoc SERD_VARIABLE
};

/// @copydoc SerdNodeFlag
enum class NodeFlag {
  is_long      = SERD_IS_LONG,      ///< @copydoc SERD_IS_LONG
  has_datatype = SERD_HAS_DATATYPE, ///< @copydoc SERD_HAS_DATATYPE
  has_language = SERD_HAS_LANGUAGE, ///< @copydoc SERD_HAS_LANGUAGE
};

/// Bitwise OR of NodeFlag values
using NodeFlags = Flags<NodeFlag>;

template<typename CPtr>
using NodeDeleter = detail::AllocatedDeleter<CPtr, serd_node_free>;

template<typename CPtr>
using NodeHandle =
  detail::Copyable<CPtr, NodeDeleter<CPtr>, serd_node_copy, serd_node_equals>;

class NodeView;

/// Common base class for any wrapped node
template<typename CPtr>
class NodeWrapper : public NodeHandle<CPtr>
{
public:
  template<class C>
  explicit NodeWrapper(const NodeWrapper<C>& node)
    : NodeHandle<CPtr>{node}
  {}

  /// @copydoc serd_node_type
  NodeType type() const
  {
    return static_cast<NodeType>(serd_node_type(this->cobj()));
  }

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

  /// @copydoc serd_node_decoded_size
  size_t decoded_size() const { return serd_node_decoded_size(this->cobj()); }

  /// @copydoc serd_node_decode
  SerdWriteResult decode(const size_t buf_size, void* const buf) const
  {
    return serd_node_decode(this->cobj(), buf_size, buf);
  }

  /// Returns a newly allocated copy of the node's string
  explicit operator std::string() const
  {
    return std::string{c_str(), length()};
  }

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  explicit operator StringView() const { return StringView(c_str(), length()); }

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  explicit operator ZixStringView() const
  {
    return ZixStringView{c_str(), length()};
  }

  /// Return a pointer to the first character in the node's string
  const char* begin() const { return c_str(); }

  /// Return a pointer to the null terminator at the end of the node's string
  const char* end() const { return c_str() + length(); }

  /// Return true if the node's string is empty
  bool empty() const { return length() == 0; }

protected:
  explicit NodeWrapper(CPtr* const ptr)
    : NodeHandle<CPtr>{ptr}
  {}
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

/// @copydoc serd_node_datatype
template<typename CPtr>
inline Optional<NodeView>
NodeWrapper<CPtr>::datatype() const
{
  return NodeView{serd_node_datatype(this->cobj())};
}

/// @copydoc serd_node_language
template<typename CPtr>
inline Optional<NodeView>
NodeWrapper<CPtr>::language() const
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
  explicit Node(const NodeView& node)
    : NodeWrapper<SerdNode>{node}
  {}

  explicit Node(const Value& value)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(value))}
  {}

  /// Create an xsd:boolean node from a ``bool``
  explicit Node(const bool b)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_bool(b)))}
  {}

  /// Create an xsd:double node from a ``double``
  explicit Node(const double d)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_double(d)))}
  {}

  /// Create an xsd:float node from a ``float``
  explicit Node(const float f)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_float(f)))}
  {}

  /// Create an xsd:long node from a ``int64_t``
  explicit Node(const int64_t i)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_long(i)))}
  {}

  /// Create an xsd:int node from a ``int32_t``
  explicit Node(const int32_t i)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_int(i)))}
  {}

  /// Create an xsd:short node from a ``int16_t``
  explicit Node(const int16_t i)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_short(i)))}
  {}

  /// Create an xsd:byte node from a ``int8_t``
  explicit Node(const int8_t i)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_byte(i)))}
  {}

  /// Create an xsd:unsignedLong node from a ``int64_t``
  explicit Node(const uint64_t i)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_ulong(i)))}
  {}

  /// Create an xsd:unsignedInt node from a ``int32_t``
  explicit Node(const uint32_t i)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_uint(i)))}
  {}

  /// Create an xsd:unsignedShort node from a ``int16_t``
  explicit Node(const uint16_t i)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_ushort(i)))}
  {}

  /// Create an xsd:unsignedByte node from a ``int8_t``
  explicit Node(const uint8_t i)
    : NodeWrapper{serd_node_new(nullptr, serd_a_primitive(serd_ubyte(i)))}
  {}

  Node(const Node& node)            = default;
  Node& operator=(const Node& node) = default;

  Node(Node&& node)            = default;
  Node& operator=(Node&& node) = default;

  ~Node() = default;

private:
  friend class Optional<Node>;
  friend class Caret;

  explicit Node(std::nullptr_t)
    : NodeWrapper{nullptr}
  {}
};

inline std::ostream&
operator<<(std::ostream& os, const NodeView& node)
{
  return os << node.c_str();
}

/// Create a new simple "token" node
inline Node
make_token(const NodeType type, StringView str)
{
  return Node{
    serd_node_new(nullptr, serd_a_token(static_cast<SerdNodeType>(type), str))};
}

/// Create a new plain literal node with no language from `str`
inline Node
make_string(StringView str)
{
  return Node{serd_node_new(nullptr, serd_a_string_view(str))};
}

/// @copydoc serd_a_uri
inline Node
make_uri(StringView uri)
{
  return Node{serd_node_new(nullptr, serd_a_uri(uri))};
}

/// @copydoc serd_a_parsed_uri
inline Node
make_uri(SerdURIView uri)
{
  return Node{serd_node_new(nullptr, serd_a_parsed_uri(uri))};
}

/// @copydoc serd_a_parsed_uri
inline Node
make_uri(URI uri)
{
  return Node{serd_node_new(nullptr, serd_a_parsed_uri(*uri.cobj()))};
}

/// Create a new file URI node from a local filesystem path
inline Node
make_file_uri(StringView path)
{
  return Node{
    serd_node_new(nullptr, serd_a_file_uri(path, zix_empty_string()))};
}

/// Create a new file URI node from a filesystem path on some host
inline Node
make_file_uri(StringView path, StringView hostname)
{
  return Node{serd_node_new(nullptr, serd_a_file_uri(path, hostname))};
}

/// @copydoc serd_a_literal
inline Node
make_literal(StringView string, NodeFlags flags, StringView meta)
{
  return Node{serd_node_new(
    nullptr, serd_a_literal(string, static_cast<SerdNodeFlags>(flags), meta))};
}

/// Create a new blank node from a local name
inline Node
make_blank(StringView str)
{
  return Node{serd_node_new(nullptr, serd_a_blank(str))};
}

/// Create a new plain literal with an optional language tag
inline Node
make_plain_literal(StringView str, StringView lang)
{
  return Node{serd_node_new(nullptr, serd_a_plain_literal(str, lang))};
}

/// Create a new typed literal node from `str`
inline Node
make_typed_literal(StringView str, const StringView datatype)
{
  return Node{serd_node_new(nullptr, serd_a_typed_literal(str, datatype))};
}

/**
   Create a new literal from a number.

   This supports `bool`, `float`, `double`, and both signed and unsigned
   integers from 8 to 64 bits wide.  The returned node will have the
   corresponding xsd datatype, for example, `uint16_t` will produce an
   `xsd:unsignedShort` literal.
*/
template<class T>
inline Node
make(const T v)
{
  return Node{value(v)};
}

/// @copydoc serd_a_decimal
inline Node
make_decimal(double d)
{
  return Node{serd_node_new(nullptr, serd_a_decimal(d))};
}

/// @copydoc serd_a_integer
inline Node
make_integer(int64_t i)
{
  return Node{serd_node_new(nullptr, serd_a_integer(i))};
}

/**
   Create a new canonical xsd:base64Binary literal.

   This function can be used to make a node out of arbitrary binary data, which
   can be decoded using Node::decode().

   @param buf Raw binary data to encode in node.
   @param size Size of `buf` in bytes.
*/
inline Node
make_base64(const void* buf, size_t size)
{
  return Node{serd_node_new(nullptr, serd_a_base64(size, buf))};
}

/// Prototype for Node get() templates
template<class T>
inline T
get(NodeView node);

/// Return the value of `node` coerced to a boolean
template<>
inline bool
get<bool>(NodeView node)
{
  return serd_node_value_as(node.cobj(), SERD_BOOL, true).data.as_bool;
}

/// Return the value of `node` coerced to a double
template<>
inline double
get<double>(NodeView node)
{
  return serd_node_value_as(node.cobj(), SERD_DOUBLE, true).data.as_double;
}

/// Return the value of `node` coerced to a float
template<>
inline float
get<float>(NodeView node)
{
  return serd_node_value_as(node.cobj(), SERD_FLOAT, true).data.as_float;
}

/// Return the value of `node` coerced to a int64_t
template<>
inline int64_t
get<int64_t>(NodeView node)
{
  return serd_node_value_as(node.cobj(), SERD_LONG, true).data.as_long;
}

/// Return the value of `node` coerced to a uint64_t
template<>
inline uint64_t
get<uint64_t>(NodeView node)
{
  return serd_node_value_as(node.cobj(), SERD_ULONG, true).data.as_ulong;
}

/**
   @}
   @defgroup serdpp_nodes Nodes
   @{
*/

// TODO

/**
   @}
   @defgroup serdpp_caret Caret
   @{
*/

/// Deleter for a Caret wrapper
template<typename CPtr>
using CaretDeleter = detail::AllocatedDeleter<CPtr, serd_caret_free>;

/// Caret handle
template<typename CPtr>
using CaretHandle = detail::
  Copyable<CPtr, CaretDeleter<CPtr>, serd_caret_copy, serd_caret_equals>;

/// Caret wrapper
template<typename CPtr>
class CaretWrapper : public CaretHandle<CPtr>
{
public:
  explicit CaretWrapper(CPtr* caret)
    : CaretHandle<CPtr>{caret}
  {}

  template<class C>
  explicit CaretWrapper(const CaretWrapper<C>& caret)
    : CaretHandle<CPtr>{caret.cobj()}
  {}

  /// @copydoc serd_caret_document
  NodeView document() const
  {
    return NodeView(serd_caret_document(this->cobj()));
  }

  /// @copydoc serd_caret_line
  unsigned line() const { return serd_caret_line(this->cobj()); }

  /// @copydoc serd_caret_column
  unsigned column() const { return serd_caret_column(this->cobj()); }
};

/// A non-owning constant view of a caret
class CaretView : public CaretWrapper<const SerdCaret>
{
public:
  /// Create a view of a C caret pointer
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  CaretView(const SerdCaret* const ptr)
    : CaretWrapper{ptr}
  {}

  /// Create a view of some other caret
  template<class C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  CaretView(const CaretWrapper<C>& caret)
    : CaretWrapper{caret}
  {}
};

/// Extra data managed by mutable (user created) Caret
struct CaretData {
  Node name_node;
};

/// @copydoc SerdCaret
class Caret
  : private CaretData
  , public CaretWrapper<SerdCaret>
{
public:
  /**
     Create a new caret.

     @param name The name of the document or stream (usually a file URI)
     @param line The line number in the document (1-based)
     @param col The column number in the document (1-based)
  */
  Caret(const NodeView& name, const unsigned line, const unsigned col)
    : CaretData{Node{name}}
    , CaretWrapper{serd_caret_new(nullptr, name_node.cobj(), line, col)}
  {}

  explicit Caret(const CaretView& caret)
    : Caret(caret.document(), caret.line(), caret.column())
  {}

private:
  friend class Optional<Caret>;
  friend class Statement;

  explicit Caret(std::nullptr_t)
    : CaretData{Node{nullptr}}
    , CaretWrapper{nullptr}
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

/// Deleter for a Statement wrapper
template<typename CPtr>
using StatementDeleter = detail::AllocatedDeleter<CPtr, serd_statement_free>;

template<typename CPtr>
using StatementHandle = detail::Copyable<CPtr,
                                         StatementDeleter<CPtr>,
                                         serd_statement_copy,
                                         serd_statement_equals>;

template<typename CPtr>
class StatementWrapper;

/// Extra data managed by mutable (user created) Statement
struct StatementData {
  Node            _subject;
  Node            _predicate;
  Node            _object;
  Optional<Node>  _graph;
  Optional<Caret> _caret;
};

/// Statement wrapper
template<typename CPtr>
class StatementWrapper : public StatementHandle<CPtr>
{
public:
  explicit StatementWrapper(CPtr* statement)
    : StatementHandle<CPtr>{statement}
  {}

  template<class C>
  explicit StatementWrapper(const StatementWrapper<C>& statement)
    : StatementHandle<CPtr>{statement}
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

  /// @copydoc serd_statement_caret
  Optional<CaretView> caret() const
  {
    return CaretView{serd_statement_caret(this->cobj())};
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
  friend class Cursor;

  StatementWrapper()
    : StatementHandle<CPtr>{nullptr}
  {}
};

template<typename CPtr>
class CursorWrapper;

/// A non-owning constant view of a statement
class StatementView final : public StatementWrapper<const SerdStatement>
{
public:
  /// Create a view of a C statement
  explicit StatementView(const SerdStatement* const ptr)
    : StatementWrapper<const SerdStatement>{ptr}
  {}

  /// Create a view of some other statement
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  StatementView(const StatementWrapper<SerdStatement>& statement)
    : StatementWrapper<const SerdStatement>{statement.cobj()}
  {}

  // FIXME
  // private:
  //   friend class CursorWrapper<const SerdStatement>;
  //   friend class CursorWrapper<SerdStatement>;

  // StatementView(std::nullptr_t)
  //   : StatementWrapper<const SerdStatement>{nullptr}
  // {}
};

/// @copydoc SerdStatement
class Statement
  : public StatementData
  , public StatementWrapper<SerdStatement>
{
public:
  Statement(const NodeView& s,
            const NodeView& p,
            const NodeView& o,
            const NodeView& g)
    : StatementData{Node{s}, Node{p}, Node{o}, Node{g}, {}}
    , StatementWrapper{serd_statement_new(nullptr,
                                          _subject.cobj(),
                                          _predicate.cobj(),
                                          _object.cobj(),
                                          _graph.cobj(),
                                          nullptr)}
  {}

  Statement(const NodeView&  s,
            const NodeView&  p,
            const NodeView&  o,
            const NodeView&  g,
            const CaretView& caret)
    : StatementData{Node{s}, Node{p}, Node{o}, Node{g}, Caret{caret}}
    , StatementWrapper{serd_statement_new(nullptr,
                                          _subject.cobj(),
                                          _predicate.cobj(),
                                          _object.cobj(),
                                          _graph.cobj(),
                                          _caret.cobj())}
  {}

  Statement(const NodeView& s, const NodeView& p, const NodeView& o)
    : StatementData{Node{s}, Node{p}, Node{o}, {}, {}}
    , StatementWrapper{serd_statement_new(nullptr,
                                          _subject.cobj(),
                                          _predicate.cobj(),
                                          _object.cobj(),
                                          nullptr,
                                          nullptr)}
  {}

  Statement(const NodeView&  s,
            const NodeView&  p,
            const NodeView&  o,
            const CaretView& caret)
    : StatementData{Node{s}, Node{p}, Node{o}, {}, Caret{caret}}
    , StatementWrapper{serd_statement_new(nullptr,
                                          _subject.cobj(),
                                          _predicate.cobj(),
                                          _object.cobj(),
                                          nullptr,
                                          _caret.cobj())}
  {}

  explicit Statement(const StatementView& statement)
    : StatementData{Node{statement.subject()},
                    Node{statement.predicate()},
                    Node{statement.object()},
                    statement.graph() ? Node{*statement.graph()}
                                      : Optional<Node>{},
                    statement.caret() ? Caret{*statement.caret()}
                                      : Optional<Caret>{}}
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

/// Deleter for a World wrapper
using WorldDeleter = detail::StandaloneDeleter<SerdWorld, serd_world_free>;

using WorldHandle = detail::Wrapper<SerdWorld, WorldDeleter>;

/// @copydoc SerdWorld
class World : public WorldHandle
{
public:
  World()
    : WorldHandle{serd_world_new(nullptr)}
  {}

  NodeView get_blank()
  {
    return static_cast<NodeView>(serd_world_get_blank(cobj()));
  }

  void set_message_func(LogFunc log_func)
  {
    _log_func = std::move(log_func);
    serd_set_log_func(cobj(), s_log_func, this);
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

    const SerdStatus st = serd_vxlogf(cobj(),
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

    const auto n_bytes = vsnprintf(nullptr, 0, fmt, args_copy);

    va_end(args_copy);

    const auto buffer_size = static_cast<size_t>(n_bytes) + 1U;

#if __cplusplus >= 201703L
    std::string result(static_cast<size_t>(n_bytes), '\0');
    if (vsnprintf(result.data(), buffer_size, fmt, args) != n_bytes) {
      return "";
    }
#else
    std::vector<char> str(buffer_size, '\0');
    if (vsnprintf(str.data(), buffer_size, fmt, args) != n_bytes) {
      return "";
    }

    std::string result(str.data(), static_cast<size_t>(n_bytes));
#endif
    return result;
  }

  static SerdStatus s_log_func(void*                     handle,
                               const SerdLogLevel        level,
                               const size_t              n_fields,
                               const SerdLogField* const fields,
                               const ZixStringView       message) noexcept
  {
    const auto* const self = static_cast<const World*>(handle);
    try {
      LogFields cpp_fields;
      for (size_t i = 0; i < n_fields; ++i) {
        cpp_fields.emplace(fields[i].key, fields[i].value);
      }

      return static_cast<SerdStatus>(
        self->_log_func(static_cast<LogLevel>(level),
                        cpp_fields,
                        std::string(message.data, message.length)));
    } catch (...) {
      return SERD_UNKNOWN_ERROR;
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

/// @copydoc SerdEventType
enum class EventType {
  base      = SERD_BASE,      ///< @copydoc SERD_BASE
  prefix    = SERD_PREFIX,    ///< @copydoc SERD_PREFIX
  statement = SERD_STATEMENT, ///< @copydoc SERD_STATEMENT
  end       = SERD_END        ///< @copydoc SERD_END
};

struct BaseEvent {
  NodeView uri; ///< Base URI
};

struct PrefixEvent {
  NodeView name; ///< Prefix name
  NodeView uri;  ///< Namespace URI
};

struct StatementEvent {
  StatementFlags flags;     ///< Flags for pretty-printing
  StatementView  statement; ///< Statement
};

struct EndEvent {
  NodeView node; ///< Anonymous node that is finished
};

class Event
{
public:
  explicit Event(const SerdEvent* const e)
    : _event{*e}
  {}

  EventType type() const { return static_cast<EventType>(_event.type); }

  BaseEvent base() const
  {
    assert(_event.type == SERD_BASE);
    return {NodeView{_event.base.uri}};
  }

  PrefixEvent prefix() const
  {
    assert(_event.type == SERD_PREFIX);
    return {NodeView{_event.prefix.name}, NodeView{_event.prefix.uri}};
  }

  StatementEvent statement() const
  {
    assert(_event.type == SERD_STATEMENT);
    return {StatementFlags{_event.statement.flags},
            StatementView{_event.statement.statement}};
  }

  EndEvent end() const
  {
    assert(_event.type == SERD_END);
    return {NodeView{_event.end.node}};
  }

private:
  SerdEvent _event;

  // union {
  //   BaseEvent      base;
  //   PrefixEvent    prefix;
  //   StatementEvent statement;
  //   EndEvent       end;
  // } event;
};

/**
   @}
   @defgroup serdpp_sink Sink
   @{
*/

/// A function called when the base URI changes
using BaseFunc = std::function<Status(NodeView)>;

/// A function called when a namespace prefix is defined
using PrefixFunc = std::function<Status(NodeView name, NodeView uri)>;

/// A function called when a statement is emitted
using StatementFunc = std::function<Status(StatementFlags, StatementView)>;

/// A function called at the end of anonymous node descriptions
using EndFunc = std::function<Status(NodeView)>;

/// Deleter for a Sink wrapper
template<class CPtr>
using SinkDeleter = detail::StandaloneDeleter<CPtr, serd_sink_free>;

template<class CPtr>
using SinkHandle = detail::Wrapper<CPtr, SinkDeleter<CPtr>>;

/// Common base class for any wrapped sink
template<class CPtr>
class SinkWrapper : public SinkHandle<CPtr>
{
public:
  /// @copydoc serd_sink_write_base
  Status base(const NodeView& uri) const
  {
    return static_cast<Status>(serd_sink_write_base(this->cobj(), uri.cobj()));
  }

  /// @copydoc serd_sink_write_prefix
  Status prefix(NodeView name, const NodeView& uri) const
  {
    return static_cast<Status>(
      serd_sink_write_prefix(this->cobj(), name.cobj(), uri.cobj()));
  }

  /// @copydoc serd_sink_write_statement
  Status statement(StatementFlags flags, StatementView statement) const
  {
    return static_cast<Status>(
      serd_sink_write_statement(this->cobj(), flags, statement.cobj()));
  }

  /// @copydoc serd_sink_write
  Status write(StatementFlags     flags,
               const NodeView&    subject,
               const NodeView&    predicate,
               const NodeView&    object,
               Optional<NodeView> graph = {}) const
  {
    return static_cast<Status>(serd_sink_write(this->cobj(),
                                               flags,
                                               subject.cobj(),
                                               predicate.cobj(),
                                               object.cobj(),
                                               graph.cobj()));
  }

  /// @copydoc serd_sink_write_end
  Status end(const NodeView& node) const
  {
    return static_cast<Status>(serd_sink_write_end(this->cobj(), node.cobj()));
  }

protected:
  explicit SinkWrapper(CPtr* const ptr)
    : SinkHandle<CPtr>{ptr}
  {}
};

/// A non-owning constant view of some other sink
class SinkView final : public SinkWrapper<const SerdSink>
{
public:
  /// Create a view of a C sink
  explicit SinkView(const SerdSink* const ptr)
    : SinkWrapper<const SerdSink>{ptr}
  {}

  /// Create a view of some other sink
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  SinkView(const SinkWrapper<SerdSink>& sink)
    : SinkWrapper<const SerdSink>{sink.cobj()}
  {}
};

/// @copydoc SerdSink
class Sink final : public SinkWrapper<SerdSink>
{
public:
  explicit Sink(const World& world)
    : SinkWrapper{serd_sink_new(serd_world_allocator(world.cobj()),
                                this,
                                s_event,
                                nullptr)}
  {}

  explicit Sink(SerdSink* const ptr)
    : SinkWrapper{ptr}
  {}

  /// Set a function to be called when the base URI changes
  void set_base_func(BaseFunc base_func) { _base_func = std::move(base_func); }

  /// Set a function to be called when a namespace prefix changes
  void set_prefix_func(PrefixFunc prefix_func)
  {
    _prefix_func = std::move(prefix_func);
  }

  /// Set a function to be called for every statement
  void set_statement_func(StatementFunc statement_func)
  {
    _statement_func = std::move(statement_func);
  }

  /// Set a function to be called at the end of an anonymous node
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
inline Sink
make_canon(const World& world, SinkView target, const CanonFlags flags)
{
  return Sink{serd_canon_new(world.cobj(), target.cobj(), flags)};
}

/**
   @}
   @defgroup serdpp_filter Filter
   @{
*/

/// @copydoc serd_filter_new
inline Sink
make_filter(const World&       world,
            SinkView           target,
            Optional<NodeView> subject,
            Optional<NodeView> predicate,
            Optional<NodeView> object,
            Optional<NodeView> graph,
            const bool         inclusive)
{
  return Sink{serd_filter_new(world.cobj(),
                              target.cobj(),
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

/// Deleter for an Env wrapper
template<typename CPtr>
using EnvDeleter = detail::StandaloneDeleter<CPtr, serd_env_free>;

template<typename CPtr>
using EnvHandle =
  detail::Copyable<CPtr, EnvDeleter<CPtr>, serd_env_copy, serd_env_equals>;

/// Env wrapper
template<typename CPtr>
class EnvWrapper : public EnvHandle<CPtr>
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
    return static_cast<Status>(serd_env_set_base_uri(this->cobj(), uri));
  }

  /// Set a namespace prefix
  Status set_prefix(StringView name, StringView uri)
  {
    return static_cast<Status>(serd_env_set_prefix(this->cobj(), name, uri));
  }

  /// Send all prefixes to `sink`
  void describe(SinkView sink) const
  {
    serd_env_describe(this->cobj(), sink.cobj());
  }

protected:
  explicit EnvWrapper(std::unique_ptr<CPtr> ptr)
    : EnvHandle<CPtr>{std::move(ptr)}
  {}

  explicit EnvWrapper(CPtr* const ptr)
    : EnvHandle<CPtr>{ptr}
  {}
};

/// EnvView
using EnvView = EnvWrapper<const SerdEnv>;

/// @copydoc SerdEnv
class Env : public EnvWrapper<SerdEnv>
{
public:
  explicit Env(World& world)
    : EnvWrapper{
        serd_env_new(serd_world_allocator(world.cobj()), zix_empty_string())}
  {}

  explicit Env(World& world, const NodeView& base)
    : EnvWrapper{
        serd_env_new(serd_world_allocator(world.cobj()), base.string_view())}
  {}
};

/**
   @}
   @defgroup serdpp_syntax_io Reading and Writing
   @{
   @defgroup serdpp_byte_source Byte Source
   @{
*/

/// @copydoc SerdInputStream
class InputStream : public SerdInputStream
{
public:
  explicit InputStream(SerdInputStream is)
    : SerdInputStream{is}
  {}

  InputStream(const InputStream&)            = delete;
  InputStream& operator=(const InputStream&) = delete;

  InputStream(InputStream&&)            = default;
  InputStream& operator=(InputStream&&) = default;

  ~InputStream() { serd_close_input(this); }
};

static inline size_t
istream_read(void* const  buf,
             const size_t size,
             const size_t nmemb,
             void* const  stream) noexcept
{
  std::istream& is  = *static_cast<std::istream*>(stream);
  const size_t  len = size * nmemb;

  try {
    is.read(static_cast<char*>(buf), static_cast<std::streamsize>(len));
  } catch (...) {
    return 0U;
  }

  return is.fail() ? 0U : len;
}

static inline int
istream_error(void* const stream)
{
  std::istream& is = *static_cast<std::istream*>(stream);

  return !is.good();
}

inline InputStream
open_input_stream(std::istream& is)
{
  return InputStream{
    serd_open_input_stream(istream_read, istream_error, nullptr, &is)};
}

// InputStream
// open_input_string(StringView string)
// {
//   return InputStream{
//     serd_open_input_string(stream(istream_read, istream_error, nullptr,
//     &is)};
// }

/**
   @}
   @defgroup serdpp_reader Reader
   @{
*/

/// @copydoc SerdReaderFlag
enum class ReaderFlag {
  lax       = SERD_READ_LAX,       ///< @copydoc SERD_READ_LAX
  variables = SERD_READ_VARIABLES, ///< @copydoc SERD_READ_VARIABLES
  relative  = SERD_READ_RELATIVE,  ///< @copydoc SERD_READ_RELATIVE
  global    = SERD_READ_GLOBAL,    ///< @copydoc SERD_READ_GLOBAL
};

/// @copydoc SerdReaderFlags
using ReaderFlags = Flags<ReaderFlag>;

/// Deleter for a Reader wrapper
using ReaderDeleter = detail::StandaloneDeleter<SerdReader, serd_reader_free>;

using ReaderHandle = detail::Wrapper<SerdReader, ReaderDeleter>;

/// @copydoc SerdReader
class Reader : public ReaderHandle
{
public:
  Reader(World&            world,
         const Syntax      syntax,
         const ReaderFlags flags,
         Env&              env,
         SinkView          sink)
    : ReaderHandle{serd_reader_new(world.cobj(),
                                   static_cast<SerdSyntax>(syntax),
                                   flags,
                                   env.cobj(),
                                   sink.cobj())}
  {}

  Status start(SerdInputStream& in,
               const NodeView&  input_name,
               const size_t     block_size)
  {
    return static_cast<Status>(
      serd_reader_start(cobj(), &in, input_name.cobj(), block_size));
  }

  /// @copydoc serd_reader_read_chunk
  Status read_chunk()
  {
    return static_cast<Status>(serd_reader_read_chunk(cobj()));
  }

  /// @copydoc serd_reader_read_document
  Status read_document()
  {
    return static_cast<Status>(serd_reader_read_document(cobj()));
  }

  /// @copydoc serd_reader_finish
  Status finish() { return static_cast<Status>(serd_reader_finish(cobj())); }

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
      s->read(static_cast<char*>(buf), static_cast<std::streamsize>(nmemb));
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

/// @copydoc SerdOutputStream
class OutputStream : public SerdOutputStream
{
public:
  explicit OutputStream(SerdOutputStream os)
    : SerdOutputStream{os}
  {}

  OutputStream(const OutputStream&)            = delete;
  OutputStream& operator=(const OutputStream&) = delete;

  OutputStream(OutputStream&&)            = default;
  OutputStream& operator=(OutputStream&&) = default;

  ~OutputStream() { close(); }

  /// @copydoc serd_close_output
  Status close() { return static_cast<Status>(serd_close_output(this)); }
};

static inline size_t
ostream_write(const void* const buf,
              const size_t      size,
              const size_t      nmemb,
              void* const       stream) noexcept
{
  std::ostream& os  = *static_cast<std::ostream*>(stream);
  const size_t  len = size * nmemb;

  try {
    os.write(static_cast<const char*>(buf), static_cast<std::streamsize>(len));
  } catch (...) {
    return 0U;
  }

  return os.fail() ? 0U : len;
}

inline OutputStream
open_output_stream(std::ostream& os)
{
  return OutputStream{
    serd_open_output_stream(ostream_write, nullptr, nullptr, &os)};
}

inline OutputStream
open_output_file(const StringView path)
{
  return OutputStream{serd_open_output_file(path.c_str())};
}

/**
   @}
   @defgroup serdpp_writer Writer
   @{
*/

/// @copydoc SerdWriterFlag
enum class WriterFlag {
  ascii      = SERD_WRITE_ASCII,      ///< @copydoc SERD_WRITE_ASCII
  expanded   = SERD_WRITE_EXPANDED,   ///< @copydoc SERD_WRITE_EXPANDED
  verbatim   = SERD_WRITE_VERBATIM,   ///< @copydoc SERD_WRITE_VERBATIM
  terse      = SERD_WRITE_TERSE,      ///< @copydoc SERD_WRITE_TERSE
  lax        = SERD_WRITE_LAX,        ///< @copydoc SERD_WRITE_LAX
  longhand   = SERD_WRITE_LONGHAND,   ///< @copydoc SERD_WRITE_LONGHAND
  contextual = SERD_WRITE_CONTEXTUAL, ///< @copydoc SERD_WRITE_CONTEXTUAL
  escapes    = SERD_WRITE_ESCAPES,    ///< @copydoc SERD_WRITE_ESCAPES
};

/// @copydoc SerdWriterFlags
using WriterFlags = Flags<WriterFlag>;

/// Deleter for a Writer wrapper
using WriterDeleter = detail::StandaloneDeleter<SerdWriter, serd_writer_free>;

using WriterHandle = detail::Wrapper<SerdWriter, WriterDeleter>;

/// @copydoc SerdWriter
class Writer : public WriterHandle
{
public:
  /**
     Create a writer that writes syntax to the given byte sink.

     @param world The world that this writer is a part of.

     @param syntax Syntax to write.

     @param flags Flags to control writer behaviour.

     @param env Environment used for expansion and abbreviation.  The writer
     uses a reference to this, so the environment must outlive the writer.

     @param out Stream where output is written.  The writer uses a reference to
     this, so the stream must outlive the writer.

     @param block_size Number of bytes to write to the output stream at once.
  */
  Writer(World&            world,
         const Syntax      syntax,
         const WriterFlags flags,
         Env&              env,
         OutputStream&     out,
         const size_t      block_size = 1U)
    : WriterHandle{serd_writer_new(world.cobj(),
                                   static_cast<SerdSyntax>(syntax),
                                   flags,
                                   env.cobj(),
                                   &out,
                                   block_size)}
  {}

  /// Return a sink that can be used to write data
  SinkView sink() { return SinkView{serd_writer_sink(cobj())}; }

  /// @copydoc serd_writer_set_root_uri
  Status set_root_uri(const StringView uri)
  {
    return static_cast<Status>(serd_writer_set_root_uri(cobj(), uri));
  }

  /// @copydoc serd_writer_finish
  Status finish() { return static_cast<Status>(serd_writer_finish(cobj())); }
};

/**
   @}
   @}
   @defgroup serdpp_storage Storage
   @{
*/

/**
   @defgroup serdpp_cursor Cursor
   @{
*/

/// Deleter for a CursorHandle
template<typename CPtr>
using CursorDeleter = detail::AllocatedDeleter<CPtr, serd_cursor_free>;

/// Owning handle to a cursor (const or mutable)
template<typename CPtr>
using CursorHandle = detail::
  Copyable<CPtr, CursorDeleter<CPtr>, serd_cursor_copy, serd_cursor_equals>;

/// A read-only view of a cursor
class CursorView;

/// @copydoc SerdCursor
template<typename CPtr>
class CursorWrapper : public CursorHandle<CPtr>
{
public:
  template<class C>
  explicit CursorWrapper(const CursorWrapper<C>& cursor)
    : CursorHandle<CPtr>{cursor}
  {}

  const StatementView& operator*() const
  {
    _statement = StatementView{serd_cursor_get(this->cobj())};
    return _statement;
  }

  const StatementView* operator->() const
  {
    _statement = StatementView{serd_cursor_get(this->cobj())};
    return &_statement;
  }

protected:
  explicit CursorWrapper(CPtr* const ptr)
    : CursorHandle<CPtr>{ptr}
  {}

private:
  mutable StatementView _statement{nullptr};
};

/// A non-owning constant view of a cursor
class CursorView : public CursorWrapper<const SerdCursor>
{
public:
  /// Create a view of a C cursor pointer
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  CursorView(const SerdCursor* const ptr)
    : CursorWrapper<const SerdCursor>{ptr}
  {}

  /// Create a view of some other cursor
  template<class C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  CursorView(const CursorWrapper<C>& cursor)
    : CursorWrapper<const SerdCursor>{cursor}
  {}
};

/// An owning handle to a cursor
class Cursor : public CursorWrapper<SerdCursor>
{
public:
  /// Create a cursor by taking ownership of a C cursor
  explicit Cursor(SerdCursor* const ptr)
    : CursorWrapper<SerdCursor>{ptr}
  {}

  /// Create a cursor by copying another cursor
  explicit Cursor(const CursorView& ptr)
    : CursorWrapper<SerdCursor>{ptr}
  {}

  Cursor(const Cursor&)            = default;
  Cursor& operator=(const Cursor&) = default;

  Cursor(Cursor&&)            = default;
  Cursor& operator=(Cursor&&) = default;

  ~Cursor() = default;

  Cursor& operator++()
  {
    serd_cursor_advance(this->cobj());
    return *this;
  }
};

/**
   @}
   @defgroup serdpp_range Range
   @{
*/

/// @copydoc SerdDescribeFlag
enum class DescribeFlag {
  no_type_first = SERD_NO_TYPE_FIRST ///< @copydoc SERD_NO_TYPE_FIRST
};

/// Bitwise OR of DescribeFlag values
using DescribeFlags = Flags<DescribeFlag>;

/**
   @}
   @defgroup serdpp_model Model
   @{
*/

/// @copydoc SerdModelFlag
enum class ModelFlag {
  store_graphs = SERD_STORE_GRAPHS, ///< @copydoc SERD_STORE_GRAPHS
  store_carets = SERD_STORE_CARETS  ///< @copydoc SERD_STORE_CARETS
};

/// Bitwise OR of ModelFlag values
using ModelFlags = Flags<ModelFlag>;

/// @copydoc SerdStatementOrder
enum class StatementOrder {
  SPO,  ///< @copydoc SERD_ORDER_SPO
  SOP,  ///< @copydoc SERD_ORDER_SOP
  OPS,  ///< @copydoc SERD_ORDER_OPS
  OSP,  ///< @copydoc SERD_ORDER_OSP
  PSO,  ///< @copydoc SERD_ORDER_PSO
  POS,  ///< @copydoc SERD_ORDER_POS
  GSPO, ///< @copydoc SERD_ORDER_GSPO
  GSOP, ///< @copydoc SERD_ORDER_GSOP
  GOPS, ///< @copydoc SERD_ORDER_GOPS
  GOSP, ///< @copydoc SERD_ORDER_GOSP
  GPSO, ///< @copydoc SERD_ORDER_GPSO
  GPOS  ///< @copydoc SERD_ORDER_GPOS
};

/// A wrapper for a cursor that acts as a collection
class ModelRange
{
public:
  ModelRange(Cursor begin, Cursor end)
    : _begin(std::move(begin))
    , _end(std::move(end))
  {}

  const Cursor& begin() const { return _begin; }
  Cursor&       begin() { return _begin; }
  const Cursor& end() const { return _end; }

private:
  Cursor _begin;
  Cursor _end;
};

/// Deleter for a Model wrapper
using ModelDeleter = detail::StandaloneDeleter<SerdModel, serd_model_free>;

using ModelHandle =
  detail::Copyable<SerdModel, ModelDeleter, serd_model_copy, serd_model_equals>;

/// @copydoc SerdModel
class Model : public ModelHandle
{
public:
  using Range = ModelRange;

  using value_type     = Statement; ///< Element value type (ala std)
  using iterator       = Cursor;    ///< Iterator type (ala std)
  using const_iterator = Cursor;    ///< Const iterator type (ala std)

  Model(World&               world,
        const StatementOrder default_order,
        const ModelFlags     flags)
    : ModelHandle{serd_model_new(world.cobj(),
                                 static_cast<SerdStatementOrder>(default_order),
                                 flags)}
    , _end{serd_model_end(cobj())}
  {}

  /// @copydoc serd_model_size
  size_t size() const { return serd_model_size(cobj()); }

  /// @copydoc serd_model_empty
  bool empty() const { return serd_model_empty(cobj()); }

  /// @copydoc serd_model_add_index
  Status add_index(const StatementOrder order)
  {
    return static_cast<Status>(
      serd_model_add_index(cobj(), static_cast<SerdStatementOrder>(order)));
  }

  /// @copydoc serd_model_drop_index
  Status drop_index(const StatementOrder order)
  {
    return static_cast<Status>(
      serd_model_drop_index(cobj(), static_cast<SerdStatementOrder>(order)));
  }

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

  /// @copydoc serd_model_insert_statements
  Status insert_statements(Cursor&& range)
  {
    return static_cast<Status>(
      serd_model_insert_statements(cobj(), range.cobj()));
  }

  /**
     Remove a statement from a model via an iterator.

     Calling this function invalidates all iterators on `model` except `iter`.

     @param iter Iterator to the element to erase.

     @returns An iterator to the statement following the erased statement,
     or the end iterator if the statement was the last or an error occurred.
  */
  Cursor erase(Cursor iter)
  {
    if (!serd_model_erase(cobj(), iter.cobj())) {
      return iter;
    }

    return iter;
  }

  /**
     Remove a range from a model.

     Calling this function invalidates all iterators on `model` except `iter`.

     @param range Range to erase.
  */
  Status erase_statements(Cursor range)
  {
    return static_cast<Status>(
      serd_model_erase_statements(cobj(), range.cobj()));
  }

  /// Search for statements that match a pattern
  ModelRange find(Optional<NodeView> s,
                  Optional<NodeView> p,
                  Optional<NodeView> o,
                  Optional<NodeView> g = {}) const
  {
    return ModelRange{
      Cursor{serd_model_find(
        nullptr, cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj())},
      end()};
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

  /// @copydoc serd_model_begin_ordered
  Cursor begin_ordered(StatementOrder order) const
  {
    return Cursor{serd_model_begin_ordered(
      nullptr, cobj(), static_cast<SerdStatementOrder>(order))};
  }

  /// @copydoc serd_model_begin
  Cursor begin() const { return Cursor{serd_model_begin(nullptr, cobj())}; }

  /// @copydoc serd_model_end
  const Cursor& end() const { return _end; }

private:
  friend class Optional<Model>;

  explicit Model(std::nullptr_t)
    : ModelHandle{nullptr}
    , _end{nullptr}
  {}

  Cursor _end;
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
inline Sink
make_inserter(Model& model)
{
  return Sink{serd_inserter_new(model.cobj(), nullptr)};
}

/**
   Create an inserter that inserts statements into a specific graph in a model.

   @param model The model to insert received statements into.

   @param default_graph The default graph to set for any statements that have
   no graph.  This allows, for example, loading a Turtle document into an
   isolated graph in the model.
*/
inline Sink
make_inserter(Model& model, NodeView default_graph)
{
  return Sink{serd_inserter_new(model.cobj(), default_graph.cobj())};
}

/**
   @}
   @}
   @}
*/

} // namespace serd

#endif // SERD_SERD_HPP
