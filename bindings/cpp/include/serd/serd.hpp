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

/// @file serd.hpp C++ API for Serd, a lightweight RDF syntax library

#ifndef SERD_SERD_HPP
#define SERD_SERD_HPP

#include "serd/detail/Copyable.hpp"   // IWYU pragma: export
#include "serd/detail/Flags.hpp"      // IWYU pragma: export
#include "serd/detail/Optional.hpp"   // IWYU pragma: export
#include "serd/detail/StringView.hpp" // IWYU pragma: export
#include "serd/detail/Wrapper.hpp"    // IWYU pragma: export

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
#include <type_traits>
#include <utility>
#include <vector>

/**
   @defgroup serdxx Serdxx
   C++ bindings for Serd, a lightweight RDF syntax library.

   @ingroup serd
   @{
*/

/**
   Serd C++ API namespace.
*/
namespace serd {

template<typename T>
using Optional = detail::Optional<T>;

using StringView = detail::StringView;

using URIView = SerdURIView;

template<typename Flag>
inline constexpr
  typename std::enable_if<std::is_enum<Flag>::value, detail::Flags<Flag>>::type
  operator|(const Flag lhs, const Flag rhs) noexcept
{
  return detail::Flags<Flag>{lhs} | rhs;
}

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

/// @copydoc SerdSyntax
enum class Syntax {
  empty    = SERD_SYNTAX_EMPTY, ///< @copydoc SERD_SYNTAX_EMPTY
  Turtle   = SERD_TURTLE,       ///< @copydoc SERD_TURTLE
  NTriples = SERD_NTRIPLES,     ///< @copydoc SERD_NTRIPLES
  NQuads   = SERD_NQUADS,       ///< @copydoc SERD_NQUADS
  TriG     = SERD_TRIG          ///< @copydoc SERD_TRIG
};

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

/// Bitwise OR of #StatementFlag values
using StatementFlags = detail::Flags<StatementFlag>;

/// @copydoc SerdSerialisationFlag
enum class SerialisationFlag {
  no_inline_objects = SERD_NO_INLINE_OBJECTS ///< Disable object inlining
};

/// Bitwise OR of #SerialisationFlag values
using SerialisationFlags = detail::Flags<SerialisationFlag>;

/// @copydoc SerdNodeType
enum class NodeType {
  literal = SERD_LITERAL, ///< @copydoc SERD_LITERAL
  URI     = SERD_URI,     ///< @copydoc SERD_URI
  CURIE   = SERD_CURIE,   ///< @copydoc SERD_CURIE
  blank   = SERD_BLANK    ///< @copydoc SERD_BLANK
};

/// @copydoc SerdField
enum class Field {
  subject   = SERD_SUBJECT,   ///< @copydoc SERD_SUBJECT
  predicate = SERD_PREDICATE, ///< @copydoc SERD_PREDICATE
  object    = SERD_OBJECT,    ///< @copydoc SERD_OBJECT
  graph     = SERD_GRAPH      ///< @copydoc SERD_GRAPH
};

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

/// Bitwise OR of #ModelFlag values
using ModelFlags = detail::Flags<ModelFlag>;

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

/// @copydoc SerdReaderFlag
enum class ReaderFlag {
  lax = SERD_READ_LAX ///< @copydoc SERD_READ_LAX
};

/// @copydoc SerdReaderFlags
using ReaderFlags = detail::Flags<ReaderFlag>;

/// @copydoc SerdWriterFlag
enum class WriterFlag {
  ascii = SERD_WRITE_ASCII, ///< @copydoc SERD_WRITE_ASCII
  terse = SERD_WRITE_TERSE, ///< @copydoc SERD_WRITE_TERSE
  lax   = SERD_WRITE_LAX    ///< @copydoc SERD_WRITE_LAX
};

/// @copydoc SerdWriterFlags
using WriterFlags = detail::Flags<WriterFlag>;

/**
   @name String Utilities
   @{
*/

/// @copydoc serd_strerror
inline const char*
strerror(const Status status)
{
  return serd_strerror(static_cast<SerdStatus>(status));
}

/**
   @}
   @name Base64
   @{
*/

/**
   Encode `size` bytes of `buf` into `str`, which must be large enough

   @param buf Input binary data (vector-like container of bytes).
   @return A base64 encoded representation of the data in `buf`.
*/
template<typename Container>
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
   Decode a base64 string

   Container must be a vector-like container of bytes.

   @param str Base64 string to decode.
   @return The decoded data represented in `str`.
*/
template<typename Container = std::vector<uint8_t>>
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

/**
   @}
   @name Byte Source
   @{
*/

class ByteSource
  : public detail::BasicWrapper<SerdByteSource, serd_byte_source_free>
{
public:
  explicit ByteSource(std::istream& stream)
    : BasicWrapper(serd_byte_source_new_function(s_read,
                                                 s_error,
                                                 nullptr,
                                                 this,
                                                 nullptr,
                                                 1))
    , _stream(&stream)
  {}

  explicit ByteSource(const std::string& string)
    : BasicWrapper(serd_byte_source_new_string(string.c_str(), nullptr))
    , _stream(nullptr)
  {}

  ByteSource(const serd::ByteSource&) = delete;
  ByteSource& operator=(const serd::ByteSource&) = delete;

  ByteSource(serd::ByteSource&&) = delete;
  ByteSource& operator=(serd::ByteSource&&) = delete;

  ~ByteSource() = default;

  static inline size_t s_read(void*  buf,
                              size_t size,
                              size_t nmemb,
                              void*  source) noexcept
  {
    assert(size == 1);
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
   @name Byte Sink
   @{
*/

/**
   Sink function for string output

   Similar semantics to `SerdWriteFunc` (and in turn `fwrite`), but takes char*
   for convenience and may set errno for more informative error reporting than
   supported by `SerdStreamErrorFunc`.

   @return Number of elements (bytes) written, which is short on error.
*/
using WriteFunc = std::function<size_t(const char*, size_t)>;

class ByteSink : public detail::BasicWrapper<SerdByteSink, serd_byte_sink_free>
{
public:
  ByteSink(WriteFunc write_func, const size_t block_size)
    : BasicWrapper(serd_byte_sink_new_function(s_write, this, block_size))
    , _write_func(std::move(write_func))
  {}

  explicit ByteSink(WriteFunc write_func)
    : ByteSink(write_func, 1)
  {}

  explicit ByteSink(std::ostream& stream)
    : BasicWrapper(serd_byte_sink_new_function(s_write, this, 1))
    , _write_func([&](const char* str, size_t len) {
      stream.write(str, std::streamsize(len));
      return stream.good() ? len : size_t(0);
    })
  {}

  static inline size_t s_write(const void* buf,
                               size_t      size,
                               size_t      nmemb,
                               void*       sink) noexcept
  {
    assert(size == 1);
    auto* self = static_cast<ByteSink*>(sink);

    try {
      return self->_write_func(static_cast<const char*>(buf), nmemb);
    } catch (...) {
    }
    return 0;
  }

private:
  WriteFunc _write_func;
};

/**
   @}
   @name Syntax Utilities
   @{
*/

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

/// @copydoc serd_syntax_has_graphs
inline bool
syntax_has_graphs(const Syntax syntax)
{
  return serd_syntax_has_graphs(static_cast<SerdSyntax>(syntax));
}

/**
   @}
   @name Node
   @{
*/

template<typename CObj>
using NodeHandle =
  detail::BasicCopyable<CObj, serd_node_copy, serd_node_equals, serd_node_free>;

template<typename CObj>
class NodeWrapper;

using Node     = NodeWrapper<SerdNode>;
using NodeView = NodeWrapper<const SerdNode>;

template<typename CObj>
class NodeWrapper : public NodeHandle<CObj>
{
public:
  using Base = NodeHandle<CObj>;
  using Base::cobj;

  explicit NodeWrapper(CObj* ptr)
    : Base(ptr)
  {}

  template<typename C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  NodeWrapper(const NodeWrapper<C>& node)
    : Base{node}
  {}

  explicit NodeWrapper(const bool b)
    : NodeWrapper{serd_new_boolean(b)}
  {}

  explicit NodeWrapper(const double d)
    : NodeWrapper{serd_new_double(d)}
  {}

  explicit NodeWrapper(const float f)
    : NodeWrapper{serd_new_float(f)}
  {}

  explicit NodeWrapper(const int64_t i)
    : NodeWrapper{serd_new_integer(i, nullptr)}
  {}

  NodeType    type() const { return NodeType(serd_node_type(cobj())); }
  const char* c_str() const { return serd_node_string(cobj()); }
  StringView  str() const { return StringView{c_str(), length()}; }
  size_t      size() const { return serd_node_length(cobj()); }
  size_t      length() const { return serd_node_length(cobj()); }

  Optional<NodeView> datatype() const;
  Optional<NodeView> language() const;

  SerdURIView uri_view() const { return serd_parse_uri(c_str()); }

  Node resolve(NodeView base) const
  {
    return Node(
      serd_new_parsed_uri(serd_resolve_uri(uri_view(), base.uri_view())));
  }

  bool operator<(NodeView node) const
  {
    return serd_node_compare(cobj(), node.cobj()) < 0;
  }

  explicit operator std::string() const
  {
    return std::string(c_str(), length());
  }

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  operator StringView() const { return StringView(c_str(), length()); }

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  operator SerdStringView() const { return SerdStringView{c_str(), length()}; }

  const char* begin() const { return c_str(); }
  const char* end() const { return c_str() + length(); }
  bool        empty() const { return length() == 0; }
};

template<typename CObj>
Optional<NodeView>
NodeWrapper<CObj>::datatype() const
{
  return NodeView(serd_node_datatype(cobj()));
}

template<typename CObj>
Optional<NodeView>
NodeWrapper<CObj>::language() const
{
  return NodeView(serd_node_language(cobj()));
}

inline std::ostream&
operator<<(std::ostream& os, const NodeView& node)
{
  return os << node.c_str();
}

/// Create a new plain literal node with no language from `str`
inline Node
make_string(StringView str)
{
  return Node(serd_new_string(str));
}

/// @copydoc serd_new_plain_literal
inline Node
make_plain_literal(StringView str, StringView lang)
{
  return Node(serd_new_plain_literal(str, lang));
}

/// Create a new typed literal node from `str`
inline Node
make_typed_literal(StringView str, const StringView datatype)
{
  return Node(serd_new_typed_literal(str, datatype));
}

/// @copydoc serd_new_blank
inline Node
make_blank(StringView str)
{
  return Node(serd_new_blank(str));
}

/// @copydoc serd_new_curie
inline Node
make_curie(StringView str)
{
  return Node(serd_new_curie(str));
}

/// @copydoc serd_new_uri
inline Node
make_uri(StringView str)
{
  return Node(serd_new_uri(str));
}

/// @copydoc serd_new_parsed_uri
inline Node
make_uri(URIView uri)
{
  return Node(serd_new_parsed_uri(uri));
}

/// Create a new file URI node from a local filesystem path
inline Node
make_file_uri(StringView path)
{
  return Node(serd_new_file_uri(path, SERD_EMPTY_STRING()));
}

/// @copydoc serd_new_file_uri
inline Node
make_file_uri(StringView path, StringView hostname)
{
  return Node(serd_new_file_uri(path, hostname));
}

/// @copydoc serd_new_boolean
inline Node
make_boolean(const bool b)
{
  return Node(serd_new_boolean(b));
}

/// @copydoc serd_new_decimal
inline Node
make_decimal(double d, Optional<NodeView> datatype = {})
{
  return Node(serd_new_decimal(d, datatype.cobj()));
}

/// @copydoc serd_new_double
inline Node
make_double(double d)
{
  return Node(serd_new_double(d));
}

/// @copydoc serd_new_float
inline Node
make_float(float f)
{
  return Node(serd_new_float(f));
}

/// @copydoc serd_new_integer
inline Node
make_integer(int64_t i, Optional<NodeView> datatype = {})
{
  return Node(serd_new_integer(i, datatype.cobj()));
}

/// @copydoc serd_new_base64
inline Node
make_base64(const void* buf, size_t size, NodeView datatype)
{
  return Node(serd_new_base64(buf, size, datatype.cobj()));
}

/// @copydoc serd_new_base64
inline Node
make_base64(const void* buf, size_t size)
{
  return Node(serd_new_base64(buf, size, nullptr));
}

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
   @name URI
   @{
*/

inline std::string
parse_file_uri(StringView uri, std::string* hostname = nullptr)
{
  char* c_hostname = nullptr;
  char* c_path     = serd_parse_file_uri(uri.data(), &c_hostname);
  if (hostname && c_hostname) {
    *hostname = c_hostname;
  }

  std::string path(c_path);
  serd_free(c_hostname);
  serd_free(c_path);
  return path;
}

inline bool
uri_string_has_scheme(StringView uri)
{
  return serd_uri_string_has_scheme(uri.c_str());
}

/**
   A parsed URI

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

  explicit URI(StringView str)
    : _uri{serd_parse_uri(str.data())}
  {}

  explicit URI(const SerdURIView& uri)
    : _uri(uri)
  {}

  Component scheme() const { return make_component(_uri.scheme); }
  Component authority() const { return make_component(_uri.authority); }
  Component path_prefix() const { return make_component(_uri.path_prefix); }
  Component path() const { return make_component(_uri.path); }
  Component query() const { return make_component(_uri.query); }
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
    ByteSink           byte_sink{ss};

    serd_write_uri(_uri, ByteSink::s_write, &byte_sink);
    return ss.str();
  }

  /// Return URI as a string relative to `base`
  std::string relative_string(const URI& base) const
  {
    std::ostringstream ss;
    ByteSink           byte_sink{ss};

    const SerdURIView rel = serd_relative_uri(_uri, base._uri);

    serd_write_uri(rel, ByteSink::s_write, &byte_sink);
    return ss.str();
  }

  /**
     Return URI as a string relative to `base` but constrained to `root`

     The returned URI string is relative iff this URI is a child of `base`
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
  ByteSink byte_sink{os};
  serd_write_uri(*uri.cobj(), ByteSink::s_write, &byte_sink);
  return os;
}

/**
   @}
   @name Cursor
   @{
*/

template<typename CObj>
using CursorHandle = detail::
  BasicCopyable<CObj, serd_cursor_copy, serd_cursor_equals, serd_cursor_free>;

template<typename CObj>
class CursorWrapper : public CursorHandle<CObj>
{
public:
  using Base = CursorHandle<CObj>;
  using Base::cobj;

  explicit CursorWrapper(CObj* cursor)
    : Base(cursor)
  {}

  template<typename C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  CursorWrapper(const CursorWrapper<C>& cursor)
    : Base{cursor.cobj()}
  {}

  NodeView name() const { return NodeView(serd_cursor_name(cobj())); }
  unsigned line() const { return serd_cursor_line(cobj()); }
  unsigned column() const { return serd_cursor_column(cobj()); }
};

using CursorView = CursorWrapper<const SerdCursor>;

/// Extra data managed by mutable (user created) Cursor
struct CursorData {
  Node name_node;
};

class Cursor
  : private CursorData
  , public CursorWrapper<SerdCursor>
{
public:
  Cursor(NodeView name, const unsigned line, const unsigned col)
    : CursorData{Node{name}}
    , CursorWrapper{serd_cursor_new(name_node.cobj(), line, col)}
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Cursor(const CursorView& cursor)
    : Cursor(cursor.name(), cursor.line(), cursor.column())
  {}

private:
  friend class detail::Optional<Cursor>;
  friend class Statement;

  explicit Cursor(std::nullptr_t)
    : CursorData{Node{nullptr}}
    , CursorWrapper{nullptr}
  {}
};

/**
   @}
   @name Logging
   @{
*/

/**
   @}
   @name World
   @{
*/

using LogFields = std::map<StringView, StringView>;

using LogFunc = std::function<Status(LogLevel, LogFields, std::string)>;

class World : public detail::BasicWrapper<SerdWorld, serd_world_free>
{
public:
  World()
    : BasicWrapper(serd_world_new())
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
   @name Statement
   @{
*/

template<typename CObj>
using StatementHandle = detail::BasicCopyable<CObj,
                                              serd_statement_copy,
                                              serd_statement_equals,
                                              serd_statement_free>;

template<typename CObj>
class StatementWrapper;

using StatementView = StatementWrapper<const SerdStatement>;

/// Extra data managed by mutable (user created) Statement
struct StatementData {
  Node             _subject;
  Node             _predicate;
  Node             _object;
  Optional<Node>   _graph;
  Optional<Cursor> _cursor;
};

template<typename CObj>
class StatementWrapper : public StatementHandle<CObj>
{
public:
  using Base = StatementHandle<CObj>;
  using Base::cobj;

  explicit StatementWrapper(CObj* statement)
    : Base{statement}
  {}

  template<typename C>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  StatementWrapper(const StatementWrapper<C>& statement)
    : Base{statement}
  {}

  /// @copydoc serd_statement_node
  NodeView node(Field field) const
  {
    return NodeView(serd_statement_node(cobj(), static_cast<SerdField>(field)));
  }

  /// @copydoc serd_statement_subject
  NodeView subject() const { return NodeView(serd_statement_subject(cobj())); }

  /// @copydoc serd_statement_predicate
  NodeView predicate() const
  {
    return NodeView(serd_statement_predicate(cobj()));
  }

  /// @copydoc serd_statement_object
  NodeView object() const { return NodeView(serd_statement_object(cobj())); }

  /// @copydoc serd_statement_graph
  Optional<NodeView> graph() const
  {
    return NodeView{serd_statement_graph(cobj())};
  }

  /// @copydoc serd_statement_cursor
  Optional<CursorView> cursor() const
  {
    return CursorView(serd_statement_cursor(cobj()));
  }

private:
  template<typename CIter>
  friend class IterWrapper;

  StatementWrapper()
    : Base{nullptr}
  {}
};

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
   @name Sink
   @{
*/

/// @copydoc SerdBaseFunc
using BaseFunc = std::function<Status(NodeView)>;

/// @copydoc SerdPrefixFunc
using PrefixFunc = std::function<Status(NodeView name, NodeView uri)>;

/// @copydoc SerdStatementFunc
using StatementFunc = std::function<Status(StatementFlags, StatementView)>;

/// @copydoc SerdEndFunc
using EndFunc = std::function<Status(NodeView)>;

template<typename CSink>
class SinkWrapper
  : public detail::Wrapper<CSink, detail::BasicDeleter<CSink, serd_sink_free>>
{
public:
  explicit SinkWrapper(CSink* ptr)
    : detail::Wrapper<CSink, detail::BasicDeleter<CSink, serd_sink_free>>(ptr)
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

class SinkView : public SinkWrapper<const SerdSink>
{
public:
  explicit SinkView(const SerdSink* ptr)
    : SinkWrapper<const SerdSink>(ptr)
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  SinkView(const SinkWrapper<SerdSink>& sink)
    : SinkWrapper<const SerdSink>(sink.cobj())
  {}
};

class Sink : public SinkWrapper<SerdSink>
{
public:
  Sink()
    : SinkWrapper(serd_sink_new(this, s_event, nullptr))
  {}

  // EnvView env() const { return serd_sink_get_env(cobj()); }

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
   @name Environment
   @{
*/

template<typename CObj>
using EnvHandle =
  detail::DynamicCopyable<CObj, serd_env_copy, serd_env_equals, serd_env_free>;

template<typename CObj>
class EnvWrapper : public EnvHandle<CObj>
{
public:
  using Base = EnvHandle<CObj>;
  using UPtr = typename Base::UPtr;

  using Base::cobj;

  explicit EnvWrapper(UPtr ptr)
    : Base(std::move(ptr))
  {}

  /// Return the base URI
  NodeView base_uri() const { return NodeView(serd_env_base_uri(cobj())); }

  /// Set the base URI
  Status set_base_uri(const StringView& uri)
  {
    return Status(serd_env_set_base_uri(cobj(), uri));
  }

  /// Set a namespace prefix
  Status set_prefix(StringView name, StringView uri)
  {
    return Status(serd_env_set_prefix(cobj(), name, uri));
  }

  /// Qualify `uri` into a CURIE if possible
  Optional<Node> qualify(const NodeView& uri) const
  {
    return Node(serd_env_qualify(cobj(), uri.cobj()));
  }

  /// Expand `node` into an absolute URI if possible
  Optional<Node> expand(const NodeView& node) const
  {
    return Node(serd_env_expand(cobj(), node.cobj()));
  }

  /// Send all prefixes to `sink`
  void write_prefixes(SinkView sink) const
  {
    serd_env_write_prefixes(cobj(), sink.cobj());
  }
};

using EnvView = EnvWrapper<const SerdEnv>;

class Env : public EnvWrapper<SerdEnv>
{
public:
  Env()
    : EnvWrapper(EnvWrapper::UPtr{serd_env_new(SERD_EMPTY_STRING()),
                                  detail::Ownership::owned})
  {}

  explicit Env(const NodeView& base)
    : EnvWrapper(EnvWrapper::UPtr{serd_env_new(base), detail::Ownership::owned})
  {}
};

/**
   @}
   @name Reader
   @{
*/

class Reader : public detail::BasicWrapper<SerdReader, serd_reader_free>
{
public:
  Reader(World&            world,
         const Syntax      syntax,
         const ReaderFlags flags,
         Env&              env,
         SinkView          sink,
         size_t            stack_size)
    : BasicWrapper(serd_reader_new(world.cobj(),
                                   SerdSyntax(syntax),
                                   flags,
                                   env.cobj(),
                                   sink.cobj(),
                                   stack_size))
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
   @name Byte Streams
   @{
*/

/**
   @}
   @name Writer
   @{
*/

struct WriterData {
  ByteSink _byte_sink;
};

class Writer
  : public WriterData
  , public detail::BasicWrapper<SerdWriter, serd_writer_free>
{
public:
  Writer(World&            world,
         const Syntax      syntax,
         const WriterFlags flags,
         Env&              env,
         WriteFunc         sink)
    : WriterData{ByteSink{sink}}
    , BasicWrapper(serd_writer_new(world.cobj(),
                                   SerdSyntax(syntax),
                                   flags,
                                   env.cobj(),
                                   _byte_sink.cobj()))
  {}

  Writer(World&            world,
         const Syntax      syntax,
         const WriterFlags flags,
         Env&              env,
         std::ostream&     stream)
    : WriterData{ByteSink{stream}}
    , BasicWrapper(serd_writer_new(world.cobj(),
                                   SerdSyntax(syntax),
                                   flags,
                                   env.cobj(),
                                   _byte_sink.cobj()))
  {}

  SinkView sink() { return SinkView{serd_writer_sink(cobj())}; }

  Status set_root_uri(const NodeView& uri)
  {
    return Status(serd_writer_set_root_uri(cobj(), uri.cobj()));
  }

  Status finish() { return Status(serd_writer_finish(cobj())); }
};

/**
   @}
*/

template<typename CObj>
using IterHandle = detail::
  DynamicCopyable<CObj, serd_iter_copy, serd_iter_equals, serd_iter_free>;

template<typename CObj>
class IterWrapper : public IterHandle<CObj>
{
public:
  using Base = IterHandle<CObj>;
  using UPtr = typename Base::UPtr;
  using Base::cobj;

  explicit IterWrapper(CObj* ptr, detail::Ownership ownership)
    : Base(UPtr{ptr, ownership})
  {}

  IterWrapper(IterWrapper&&) noexcept = default;
  IterWrapper(const IterWrapper&)     = default;

  IterWrapper& operator=(IterWrapper&&) noexcept = default;
  IterWrapper& operator=(const IterWrapper&) = default;

  ~IterWrapper() = default;

  const StatementView& operator*() const
  {
    _statement = StatementView{serd_iter_get(cobj())};
    return _statement;
  }

  const StatementView* operator->() const
  {
    _statement = StatementView{serd_iter_get(cobj())};
    return &_statement;
  }

protected:
  mutable StatementView _statement{};
};

using IterView = IterWrapper<const SerdIter>;

class Iter : public IterWrapper<SerdIter>
{
public:
  explicit Iter(SerdIter* ptr, detail::Ownership ownership)
    : IterWrapper(ptr, ownership)
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

class Range
  : public detail::BasicCopyable<SerdRange,
                                 serd_range_copy,
                                 serd_range_equals,
                                 serd_range_free>
{
public:
  using Base = detail::BasicCopyable<SerdRange,
                                     serd_range_copy,
                                     serd_range_equals,
                                     serd_range_free>;

  explicit Range(SerdRange* r)
    : BasicCopyable(r)
    , _begin{serd_range_begin(r), detail::Ownership::view}
    , _end{serd_range_end(r), detail::Ownership::view}
  {}

  const Iter& begin() const { return _begin; }
  Iter&       begin() { return _begin; }
  const Iter& end() const { return _end; }

  Status write(SinkView sink, SerialisationFlags flags = {})
  {
    return Status(serd_write_range(cobj(), sink.cobj(), flags));
  }

private:
  Iter _begin;
  Iter _end;
};

using ModelHandle = detail::
  BasicCopyable<SerdModel, serd_model_copy, serd_model_equals, serd_model_free>;

class Model : public ModelHandle
{
public:
  using Base           = ModelHandle;
  using value_type     = Statement;
  using iterator       = Iter;
  using const_iterator = Iter;

  Model(World& world, ModelFlags flags)
    : Base(serd_model_new(world.cobj(), flags))
  {}

  Model(World& world, ModelFlag flag)
    : Base(serd_model_new(world.cobj(), ModelFlags(flag)))
  {}

  size_t size() const { return serd_model_size(cobj()); }

  bool empty() const { return serd_model_empty(cobj()); }

  void insert(StatementView s) { serd_model_insert(cobj(), s.cobj()); }

  void insert(const NodeView&    s,
              const NodeView&    p,
              const NodeView&    o,
              Optional<NodeView> g = {})
  {
    serd_model_add(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj());
  }

  Iter find(Optional<NodeView> s,
            Optional<NodeView> p,
            Optional<NodeView> o,
            Optional<NodeView> g = {}) const
  {
    return Iter(serd_model_find(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()),
                detail::Ownership::owned);
  }

  Range range(Optional<NodeView> s,
              Optional<NodeView> p,
              Optional<NodeView> o,
              Optional<NodeView> g = {}) const
  {
    return Range(
      serd_model_range(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
  }

  Optional<NodeView> get(Optional<NodeView> s,
                         Optional<NodeView> p,
                         Optional<NodeView> o,
                         Optional<NodeView> g = {}) const
  {
    return NodeView(
      serd_model_get(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
  }

  Optional<StatementView> get_statement(Optional<NodeView> s,
                                        Optional<NodeView> p,
                                        Optional<NodeView> o,
                                        Optional<NodeView> g = {}) const
  {
    return StatementView(
      serd_model_get_statement(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
  }

  bool ask(Optional<NodeView> s,
           Optional<NodeView> p,
           Optional<NodeView> o,
           Optional<NodeView> g = {}) const
  {
    return serd_model_ask(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj());
  }

  size_t count(Optional<NodeView> s,
               Optional<NodeView> p,
               Optional<NodeView> o,
               Optional<NodeView> g = {}) const
  {
    return serd_model_count(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj());
  }

  Range all() const { return Range(serd_model_all(cobj())); }

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
    return iterator(serd_iter_copy(serd_model_end(cobj())),
                    detail::Ownership::owned);
  }

private:
  friend class detail::Optional<Model>;
  explicit Model(std::nullptr_t)
    : BasicCopyable(nullptr)
  {}
};

class Inserter : public SinkWrapper<SerdSink>
{
public:
  explicit Inserter(Model& model)
    : SinkWrapper(serd_inserter_new(model.cobj(), nullptr))
  {}

  Inserter(Model& model, Optional<NodeView> default_graph)
    : SinkWrapper(serd_inserter_new(model.cobj(), default_graph.cobj()))
  {}
};

} // namespace serd

/**
   @}
*/

#endif // SERD_SERD_HPP
