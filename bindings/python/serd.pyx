# cython: binding=True
# cython: language_level=3
# cython: warn.maybe_uninitialized=True
# cython: warn.multiple_declarators=True
# cython: warn.unused=True

"""A lightweight library for working with RDF data."""

import enum
import errno
import logging

import cython

from libc.stdint cimport uint32_t, int64_t

logger = logging.getLogger(__name__)

cdef extern from "stdarg.h":
    ctypedef struct va_list:
        pass

cdef extern from "serd/serd.h":
    ctypedef struct SerdWorld
    ctypedef struct SerdNodes
    ctypedef struct SerdStatement
    ctypedef struct SerdCursor
    ctypedef struct SerdEnv
    ctypedef struct SerdModel
    ctypedef struct SerdIter
    ctypedef struct SerdRange
    ctypedef struct SerdReader
    ctypedef struct SerdWriter
    ctypedef struct SerdSink

    ctypedef enum SerdStatus: pass
    ctypedef enum SerdSyntax: pass
    ctypedef enum SerdStatementFlag: pass
    ctypedef enum SerdSerialisationFlag: pass
    ctypedef enum SerdNodeType: pass
    ctypedef enum SerdField: pass
    ctypedef enum SerdStatementOrder: pass
    ctypedef enum SerdModelFlag: pass

    ctypedef uint32_t SerdStatementFlags
    ctypedef uint32_t SerdSerialisationFlags
    ctypedef uint32_t SerdModelFlags

    ctypedef struct SerdNode

    ctypedef struct SerdStringView:
        const char* buf;
        size_t      len;

    ctypedef struct SerdBuffer:
        void*  buf;
        size_t len;

    ctypedef struct SerdURIView:
        SerdStringView scheme
        SerdStringView authority
        SerdStringView path_prefix
        SerdStringView path
        SerdStringView query
        SerdStringView fragment

    ctypedef enum SerdReaderFlag : pass
    ctypedef uint32_t SerdReaderFlags

    ctypedef enum SerdWriterFlag : pass
    ctypedef uint32_t SerdWriterFlags

    void serd_free(void* ptr);

    # String Utilities
    const char* serd_strerror(SerdStatus status);

    # Base64

    size_t serd_base64_encoded_length(size_t size, bint wrap_lines);
    size_t serd_base64_decoded_size(size_t len);

    bint serd_base64_encode(char*       str,
                            const void* buf,
                            size_t      size,
                            bint        wrap_lines);

    SerdStatus serd_base64_decode(void*       buf,
                                  size_t*     size,
                                  const char* str,
                                  size_t      len);

    # Byte Source

    ctypedef struct SerdByteSource

    ctypedef int (*SerdStreamErrorFunc)(void* stream);

    ctypedef size_t (*SerdReadFunc)(void*  buf,
                                    size_t size,
                                    size_t nmemb,
                                    void*  stream);

    SerdByteSource* serd_byte_source_new_string(const char*     string,
                                                const SerdNode* name);

    SerdByteSource* serd_byte_source_new_filename(const char* path,
                                                  size_t      block_size);

    SerdByteSource* serd_byte_source_new_function(
        SerdReadFunc        read_func,
        SerdStreamErrorFunc error_func,
        void*               stream,
        const SerdNode*     name,
        size_t              block_size);

    void serd_byte_source_free(SerdByteSource* source);

    # Byte sink

    ctypedef struct SerdByteSink

    ctypedef size_t (*SerdWriteFunc)(const void* buf,
                                     size_t      size,
                                     size_t      nmemb,
                                     void*       stream);

    SerdByteSink* serd_byte_sink_new_buffer(SerdBuffer* buffer);

    SerdByteSink* serd_byte_sink_new_filename(const char* path,
                                              size_t      block_size);

    SerdByteSink* serd_byte_sink_new_function(SerdWriteFunc write_func,
                                              void*         stream,
                                              size_t        block_size);

    void serd_byte_sink_flush(SerdByteSink* sink);
    void serd_byte_sink_close(SerdByteSink* sink);
    void serd_byte_sink_free(SerdByteSink* sink);

    # Syntax Utilities

    SerdSyntax serd_syntax_by_name(const char* name);
    SerdSyntax serd_guess_syntax(const char* filename);
    bint       serd_syntax_has_graphs(SerdSyntax syntax);

    # URI

    char*      serd_parse_file_uri(const char* uri, char** hostname);
    bint       serd_uri_string_has_scheme(const char* utf8);
    SerdStatus serd_parse_uri(const char* utf8, SerdURIView* out);

    SerdURIView serd_resolve_uri(SerdURIView r, SerdURIView base);

    size_t serd_write_uri(SerdURIView uri, SerdWriteFunc sink, void* stream);

    # Node

    SerdNode* serd_node_from_syntax(const char* str, SerdSyntax syntax);
    char*     serd_node_to_syntax(const SerdNode* node, SerdSyntax syntax);

    SerdNode* serd_new_simple_node(SerdNodeType type, SerdStringView string);
    SerdNode* serd_new_string(SerdStringView string);
    SerdNode* serd_new_plain_literal(SerdStringView str, SerdStringView lang);
    SerdNode* serd_new_typed_literal(SerdStringView str, SerdStringView datatype_uri);

    SerdNode* serd_new_blank(SerdStringView string);
    SerdNode* serd_new_curie(SerdStringView string);
    SerdNode* serd_new_uri(SerdStringView string);
    SerdNode* serd_new_parsed_uri(SerdURIView uri);
    SerdNode* serd_new_file_uri(SerdStringView path, SerdStringView hostname);
    SerdNode* serd_new_boolean(bint b);
    SerdNode* serd_new_decimal(double d, const SerdNode* datatype);
    SerdNode* serd_new_double(double d);
    SerdNode* serd_new_float(float f);
    SerdNode* serd_new_integer(int64_t i, const SerdNode* datatype);
    SerdNode* serd_new_base64(const void* buf, size_t size, const SerdNode* datatype);

    bint            serd_get_boolean(const SerdNode* node);
    double          serd_get_double(const SerdNode* node);
    float           serd_get_float(const SerdNode* node);
    int64_t         serd_get_integer(const SerdNode* node);
    SerdNode*       serd_node_copy(const SerdNode* node);
    void            serd_node_free(SerdNode* node);
    SerdNodeType    serd_node_type(const SerdNode* node);
    const char*     serd_node_string(const SerdNode* node);
    size_t          serd_node_length(const SerdNode* node);
    SerdStringView  serd_node_string_view(const SerdNode* node);
    SerdURIView     serd_node_uri_view(const SerdNode* node);
    const SerdNode* serd_node_datatype(const SerdNode* node);
    const SerdNode* serd_node_language(const SerdNode* node);
    bint            serd_node_equals(const SerdNode* a, const SerdNode* b);
    int             serd_node_compare(const SerdNode* a, const SerdNode* b);

    # Event

    ctypedef enum SerdEventType: pass

    ctypedef struct SerdBaseEvent:
        SerdEventType   type;
        const SerdNode* uri;

    ctypedef struct SerdPrefixEvent:
        SerdEventType   type;
        const SerdNode* name;
        const SerdNode* uri;

    ctypedef struct SerdStatementEvent:
        SerdEventType        type;
        SerdStatementFlags   flags;
        const SerdStatement* statement;

    ctypedef struct SerdEndEvent:
        SerdEventType   type;
        const SerdNode* node;

    ctypedef union SerdEvent:
        SerdEventType      type;
        SerdBaseEvent      base;
        SerdPrefixEvent    prefix;
        SerdStatementEvent statement;
        SerdEndEvent       end;

    ctypedef SerdStatus (*SerdEventFunc)(void* handle, const SerdEvent* event);

    # World

    SerdWorld*      serd_world_new();
    void            serd_world_free(SerdWorld* world);
    SerdNodes*      serd_world_nodes(SerdWorld* world);
    const SerdNode* serd_world_get_blank(SerdWorld* world);

    # TODO: logging

    ctypedef enum SerdLogLevel: pass

    cdef struct SerdLogField:
        const char* key;
        const char* value;

    cdef struct SerdLogEntry:
        const char*         domain;
        const SerdLogField* fields;
        const char*         fmt;
        va_list*            args;
        SerdLogLevel        level;
        size_t              n_fields;

    ctypedef SerdStatus (*SerdLogFunc)(void* handle, const SerdLogEntry* entry);

    # SerdStatus serd_quiet_error_func(void* handle, const SerdLogEntry* entry);

    # const char* serd_log_entry_get_field(const SerdLogEntry* entry,
    #                                      const char*         key);

    void serd_world_set_log_func(SerdWorld*  world,
                                 SerdLogFunc log_func,
                                 void*       handle);

    # SerdStatus serd_world_vlogf(const SerdWorld*    world,
    #                             const char*         domain,
    #                             SerdLogLevel        level,
    #                             size_t              n_fields,
    #                             const SerdLogField* fields,
    #                             const char*         fmt,
    #                             va_list             args);

    SerdStatus serd_world_logf(const SerdWorld*    world,
                               const char*         domain,
                               SerdLogLevel        level,
                               size_t              n_fields,
                               const SerdLogField* fields,
                               const char*         fmt,
                               ...);

    # Environment

    SerdEnv*        serd_env_new(const SerdStringView base_uri);
    SerdEnv*        serd_env_copy(const SerdEnv* env);
    bint            serd_env_equals(const SerdEnv* a, const SerdEnv* b);
    void            serd_env_free(SerdEnv* env);
    const SerdNode* serd_env_base_uri(SerdEnv* env)
    SerdStatus      serd_env_set_base_uri(SerdEnv* env, SerdStringView uri);

    SerdStatus serd_env_set_prefix(SerdEnv*       env,
                                   SerdStringView name,
                                   SerdStringView uri);

    SerdNode* serd_env_qualify(const SerdEnv* env, const SerdNode* uri);
    SerdNode* serd_env_expand(const SerdEnv* env, const SerdNode* node);

    void serd_env_write_prefixes(const SerdEnv* env, const SerdSink* sink);

    # Inserter

    SerdSink* serd_inserter_new(SerdModel*      model,
                                const SerdNode* default_graph);

    # Statement

    SerdStatement* serd_statement_new(const SerdNode*   s,
                                      const SerdNode*   p,
                                      const SerdNode*   o,
                                      const SerdNode*   g,
                                      const SerdCursor* cursor);

    SerdStatement* serd_statement_copy(const SerdStatement* statement);
    void           serd_statement_free(SerdStatement* statement);

    const SerdNode* serd_statement_node(const SerdStatement* statement,
                                            SerdField field);

    const SerdNode* serd_statement_subject(const SerdStatement* statement);
    const SerdNode* serd_statement_predicate(const SerdStatement* statement);
    const SerdNode* serd_statement_object(const SerdStatement* statement);
    const SerdNode* serd_statement_graph(const SerdStatement* statement);

    const SerdCursor* serd_statement_cursor(const SerdStatement* statement);

    bint serd_statement_equals(const SerdStatement* a, const SerdStatement* b);

    bint serd_statement_matches(const SerdStatement* statement,
                                const SerdNode*      subject,
                                const SerdNode*      predicate,
                                const SerdNode*      object,
                                const SerdNode*      graph);

    # Iter

    SerdIter*            serd_iter_copy(const SerdIter* iter);
    const SerdStatement* serd_iter_get(const SerdIter* iter);

    bint serd_iter_next(SerdIter* iter);
    bint serd_iter_equals(const SerdIter* lhs, const SerdIter* rhs);
    void serd_iter_free(SerdIter* iter);

    # Range

    SerdRange*           serd_range_copy(const SerdRange* range);
    void                 serd_range_free(SerdRange* range);
    const SerdStatement* serd_range_front(const SerdRange* range);

    bint serd_range_equals(const SerdRange* lhs, const SerdRange* rhs);

    bint            serd_range_next(SerdRange* range);
    bint            serd_range_empty(const SerdRange* range);
    const SerdIter* serd_range_cbegin(const SerdRange* range);
    const SerdIter* serd_range_cend(const SerdRange* range);
    SerdIter*       serd_range_begin(SerdRange* range);
    SerdIter*       serd_range_end(SerdRange* range);

    SerdStatus serd_write_range(const SerdRange*       range,
                                const SerdSink*        sink,
                                SerdSerialisationFlags flags);


    # Sink

    ctypedef void (*SerdFreeFunc)(void* ptr);

    SerdSink* serd_sink_new(void*         handle,
                            SerdEventFunc event_func,
                            SerdFreeFunc  free_handle);

    void serd_sink_free(SerdSink* sink);

    SerdStatus serd_sink_set_event_func(SerdSink*     sink,
                                        SerdEventFunc event_func);

    SerdStatus serd_sink_write_event(const SerdSink*  sink,
                                     const SerdEvent* event);

    SerdStatus serd_sink_write_base(const SerdSink* sink,
                                    const SerdNode* uri);

    SerdStatus serd_sink_write_prefix(const SerdSink* sink,
                                      const SerdNode* name,
                                      const SerdNode* uri);

    SerdStatus serd_sink_write_statement(const SerdSink*      sink,
                                         SerdStatementFlags   flags,
                                         const SerdStatement* statement);

    SerdStatus serd_sink_write(const SerdSink*    sink,
                               SerdStatementFlags flags,
                               const SerdNode*    subject,
                               const SerdNode*    predicate,
                               const SerdNode*    object,
                               const SerdNode*    graph);

    SerdStatus serd_sink_write_end(const SerdSink* sink, const SerdNode* node);

    # Stream Processing

    SerdSink* serd_canon_new(const SerdSink* target);

    SerdSink* serd_filter_new(const SerdSink* target,
                              const SerdNode* subject,
                              const SerdNode* predicate,
                              const SerdNode* object,
                              const SerdNode* graph);

    # Reader

    SerdReader* serd_reader_new(SerdWorld*      world,
                                SerdSyntax      syntax,
                                SerdReaderFlags flags,
                                SerdEnv*        env,
                                const SerdSink* sink,
                                size_t          stack_size);

    void serd_reader_add_blank_prefix(SerdReader* reader, const char* prefix);

    SerdStatus serd_reader_start(SerdReader* reader, SerdByteSource* byte_source);
    SerdStatus serd_reader_read_chunk(SerdReader* reader);
    SerdStatus serd_reader_read_document(SerdReader* reader);
    SerdStatus serd_reader_finish(SerdReader* reader);

    void serd_reader_free(SerdReader* reader);

    # Writer

    SerdWriter* serd_writer_new(SerdWorld*      world,
                                SerdSyntax      syntax,
                                SerdWriterFlags flags,
                                SerdEnv*        env,
                                SerdByteSink*   byte_sink);

    void            serd_writer_free(SerdWriter* writer);
    const SerdSink* serd_writer_sink(SerdWriter* writer);

    size_t serd_buffer_sink(const void* buf,
                            size_t      size,
                            size_t      nmemb,
                            void*       stream);

    char* serd_buffer_sink_finish(SerdBuffer* stream);

    void serd_writer_chop_blank_prefix(SerdWriter* writer, const char* prefix);

    SerdStatus serd_writer_set_base_uri(SerdWriter*     writer,
                                        const SerdNode* uri);

    SerdStatus serd_writer_set_root_uri(SerdWriter*     writer,
                                        const SerdNode* uri);

    SerdStatus serd_writer_finish(SerdWriter* writer);

    # Model

    SerdModel*      serd_model_new(SerdWorld* world, SerdModelFlags flags);
    SerdModel*      serd_model_copy(const SerdModel* model);
    bint            serd_model_equals(const SerdModel* a, const SerdModel* b);
    void            serd_model_free(SerdModel* model);
    SerdWorld*      serd_model_world(SerdModel* model);
    SerdModelFlags  serd_model_flags(const SerdModel* model);
    size_t          serd_model_size(const SerdModel* model);
    bint            serd_model_empty(const SerdModel* model);
    SerdIter*       serd_model_begin(const SerdModel* model);
    const SerdIter* serd_model_end(const SerdModel* model);
    SerdRange*      serd_model_all(const SerdModel* model);

    SerdRange* serd_model_ordered(const SerdModel*         model,
                                  const SerdStatementOrder order);

    SerdIter* serd_model_find(const SerdModel* model,
                              const SerdNode*  s,
                              const SerdNode*  p,
                              const SerdNode*  o,
                              const SerdNode*  g);

    SerdRange* serd_model_range(const SerdModel* model,
                                const SerdNode*  s,
                                const SerdNode*  p,
                                const SerdNode*  o,
                                const SerdNode*  g);

    const SerdNode* serd_model_get(const SerdModel* model,
                                   const SerdNode*  s,
                                   const SerdNode*  p,
                                   const SerdNode*  o,
                                   const SerdNode*  g);

    const SerdStatement* serd_model_get_statement(const SerdModel* model,
                                                  const SerdNode*  s,
                                                  const SerdNode*  p,
                                                  const SerdNode*  o,
                                                  const SerdNode*  g);

    bint serd_model_ask(const SerdModel* model,
                        const SerdNode*  s,
                        const SerdNode*  p,
                        const SerdNode*  o,
                        const SerdNode*  g);

    size_t serd_model_count(const SerdModel* model,
                            const SerdNode*  s,
                            const SerdNode*  p,
                            const SerdNode*  o,
                            const SerdNode*  g);

    SerdStatus serd_model_add(SerdModel*      model,
                              const SerdNode* s,
                              const SerdNode* p,
                              const SerdNode* o,
                              const SerdNode* g);

    SerdStatus serd_model_insert(SerdModel*           model,
                                 const SerdStatement* statement);

    SerdStatus serd_model_add_range(SerdModel* model, SerdRange* range);
    SerdStatus serd_model_erase(SerdModel* model, SerdIter* iter);
    SerdStatus serd_model_erase_range(SerdModel* model, SerdRange* range);
    SerdStatus serd_model_clear(SerdModel* model);
    SerdStatus serd_validate(const SerdModel* model);


    # Cursor

    SerdCursor* serd_cursor_new(const SerdNode* name,
                                unsigned        line,
                                unsigned        col);

    SerdCursor* serd_cursor_copy(const SerdCursor* cursor);
    void        serd_cursor_free(SerdCursor* cursor);

    bint serd_cursor_equals(const SerdCursor* lhs, const SerdCursor* rhs);

    const SerdNode* serd_cursor_name(const SerdCursor* cursor);
    unsigned        serd_cursor_line(const SerdCursor* cursor);
    unsigned        serd_cursor_column(const SerdCursor* cursor);


class Status(enum.IntEnum):
    """Return status code."""
    SUCCESS = 0  # No error
    FAILURE = 1  # Non-fatal failure
    ERR_UNKNOWN = 2  # Unknown error
    ERR_BAD_SYNTAX = 3  # Invalid syntax
    ERR_BAD_ARG = 4  # Invalid argument
    ERR_BAD_ITER = 5  # Use of invalidated iterator
    ERR_NOT_FOUND = 6  # Not found
    ERR_ID_CLASH = 7  # Encountered clashing blank node IDs
    ERR_BAD_CURIE = 8  # Invalid CURIE (e.g. prefix does not exist)
    ERR_INTERNAL = 9  # Unexpected internal error (should not happen)
    ERR_OVERFLOW = 10  # Stack overflow
    ERR_INVALID = 11  # Invalid data
    ERR_NO_DATA = 12  # Unexpected end of input
    ERR_BAD_WRITE = 13  # Error writing to file/stream
    ERR_BAD_CALL = 14  # Invalid call


class Syntax(enum.IntEnum):
    """RDF syntax type."""
    EMPTY = 0  # Empty syntax (suppress input or output)
    TURTLE = 1  # Terse triples http://www.w3.org/TR/turtle
    NTRIPLES = 2  # Flat triples http://www.w3.org/TR/n-triples/
    NQUADS = 3  # Flat quads http://www.w3.org/TR/n-quads/
    TRIG = 4  # Terse quads http://www.w3.org/TR/trig/


class StatementFlags(enum.IntFlag):
    """Flags indicating inline abbreviation information for a statement."""
    EMPTY_S = 1 << 0  # Empty blank node subject
    ANON_S = 1 << 1  # Start of anonymous subject
    ANON_O = 1 << 2  # Start of anonymous object
    LIST_S = 1 << 3  # Start of list subject
    LIST_O = 1 << 4  # Start of list object
    TERSE_S = 1 << 5  # Terse serialisation of new subject
    TERSE_O = 1 << 6  # Terse serialisation of new object


class SerialisationFlags(enum.IntFlag):
    """Flags that control the style of a model serialisation."""
    NO_INLINE_OBJECTS = 1 << 0  # Disable object inlining


class NodeType(enum.IntEnum):
    """Type of a node

    An RDF node, in the abstract sense, can be either a resource, literal, or a
    blank.  This type is more precise, because syntactically there are two ways
    to refer to a resource (by URI or CURIE).  Serd also has support for
    variable nodes to support some features, which are not RDF nodes.

    There are also two ways to refer to a blank node in syntax (by ID or
    anonymously), but this is handled by statement flags rather than distinct
    node types.
    """
    LITERAL = 1  # Literal value
    LONG_LITERAL = 2  # Long (triple-quoted) literal value
    URI = 3  # URI (absolute or relative)
    CURIE = 4  # CURIE (shortened URI)
    BLANK = 5  # Blank node
    VARIABLE = 6  # Variable node


class Field(enum.IntEnum):
    """Index of a statement in a field."""
    SUBJECT = 0  # Subject
    PREDICATE = 1  # Predicate ("key")
    OBJECT = 2  # Object ("value")
    GRAPH = 3  # Graph ("context")

class StatementOrder(enum.IntEnum):
    """Statement ordering."""
    SPO = 0   #         Subject,   Predicate, Object
    SOP = 1   #         Subject,   Object,    Predicate
    OPS = 2   #         Object,    Predicate, Subject
    OSP = 3   #         Object,    Subject,   Predicate
    PSO = 4   #         Predicate, Subject,   Object
    POS = 5   #         Predicate, Object,    Subject
    GSPO = 6  # Graph,  Subject,   Predicate, Object
    GSOP = 7  # Graph,  Subject,   Object,    Predicate
    GOPS = 8  # Graph,  Object,    Predicate, Subject
    GOSP = 9  # Graph,  Object,    Subject,   Predicate
    GPSO = 10 # Graph,  Predicate, Subject,   Object
    GPOS = 11 # Graph,  Predicate, Object,    Subject

class ModelFlags(enum.IntFlag):
    """Flags that control model storage and indexing."""
    INDEX_SPO = 1 << 0  # Subject,   Predicate, Object
    INDEX_SOP = 1 << 1  # Subject,   Object,    Predicate
    INDEX_OPS = 1 << 2  # Object,    Predicate, Subject
    INDEX_OSP = 1 << 3  # Object,    Subject,   Predicate
    INDEX_PSO = 1 << 4  # Predicate, Subject,   Object
    INDEX_POS = 1 << 5  # Predicate, Object,    Subject
    INDEX_GRAPHS = 1 << 6  # Support multiple graphs in model
    STORE_CURSORS = 1 << 7  # Store original cursor of statements

# TODO: URI


class ReaderFlags(enum.IntFlag):
    """Reader support options."""
    READ_LAX = 1 << 0  # Tolerate invalid input where possible
    READ_VARIABLES = 1 << 1  # Support variable nodes


class WriterFlags(enum.IntFlag):
    """Writer style options.

    These flags allow more precise control of writer output style.  Note that
    some options are only supported for some syntaxes, for example, NTriples
    does not support abbreviation and is always ASCII.
    """
    WRITE_ASCII = 1 << 0  # Escape all non-ASCII characters
    WRITE_TERSE = 1 << 1  # Write terser output without newlines
    WRITE_LAX = 1 << 2  # Tolerate lossy output


class EventType(enum.IntEnum):
    """The type of a :class:`serd.Event`."""
    BASE = 1
    PREFIX = 2
    STATEMENT = 3
    END = 4


# Private Python Bindings Utilities

cdef SerdNode* _unwrap_node(node: Node):
    if node is None:
        return NULL
    elif type(node) == Node:
        return (<Node>node)._ptr

    raise TypeError("Expected Node, got %s" % type(node))


def _uri_from_param(param) -> Node:
    if isinstance(param, type("")):
        return uri(param)
    elif isinstance(param, Node) and (<Node>param).type() == NodeType.URI:
        return param

    raise TypeError("Expected string or URI node, got %s" % type(param))


def _blank_from_param(param) -> Node:
    if isinstance(param, type("")):
        return blank(param)
    elif isinstance(param, Node) and (<Node>param).type() == NodeType.BLANK:
        return param

    raise TypeError("Expected string or blank node, got %s" % type(param))


def _tocstr(s: str):
    return s.encode('utf-8')


def _string_view(s: str):
    encoded = s.encode('utf-8')
    return SerdStringView(encoded, len(encoded)) # FIXME: len?


def _fromcstr(const char* s):
    return s.decode('utf-8')


# Public Python API Utilities

class Namespace:
    """Namespace prefix.

    Use attribute syntax to easily create URIs within this namespace, for
    example::

       >>> world = lilv.World()
       >>> ns = Namespace(world, "http://example.org/")
       >>> print(ns.foo)
       http://example.org/foo
    """

    def __init__(self, prefix):
        self.prefix = str(_uri_from_param(prefix))

    def __add__(self, suffix: str):
        return uri(self.prefix + suffix)

    def __eq__(self, other):
        if type(other) == Namespace:
            return self.prefix == other.prefix
        elif type(other) == str:
            return self.prefix == other
        elif type(other) == Node:
            return other.type() == NodeType.URI and other == self.prefix

    def __str__(self):
        return self.prefix

    def __getattr__(self, suffix: str):
        return uri(self.prefix + suffix)

    def __getitem__(self, suffix: str):
        return uri(self.prefix + suffix)

    def name(self, uri):
        uri = _uri_from_param(uri)
        if uri is not None and str(uri).startswith(self.prefix):
            return str(uri)[len(self.prefix):]

        return None


# String Utilities


def strerror(status: Status) -> str:
    """Return a string describing a status code."""
    return _fromcstr(serd_strerror(status))


# Base64


# def base64_encode(const unsigned char[:] data, wrap_lines=False) -> str:
#     """Encode `data` to base64.

#     Args:
#         data: Array of arbitrary bytes to encode.
#         wrap_lines: Wrap lines at 76 characters to conform to RFC 2045.

#     Returns:
#         A string encoded in base64 format.
#     """
#     size = len(data)
#     length = serd_base64_encoded_length(size, wrap_lines)
#     result = bytes(length)
#     serd_base64_encode(result, &data[0], size, wrap_lines)

#     return result.decode("utf-8")


# def base64_decode(string: str) -> bytes:
#     """Decode `string` from base64."""
#     length = len(string)
#     size = serd_base64_decoded_size(length)
#     result = cython.view.array(shape=(size,), itemsize=1, format="c")
#     actual_size = <size_t>0

#     cdef unsigned char[::1] result_view = result

#     serd_base64_decode(&result_view[0], &actual_size, _tocstr(string), length)
#     assert actual_size <= size

#     return bytes(result[0 : actual_size])


# Syntax Utilities


def syntax_by_name(name: str) -> Syntax:
    """Get a syntax by name.

    Case-insensitive, supports "Turtle", "NTriples", "NQuads", and "TriG".

    Returns:
        A syntax, or Syntax.EMPTY if the name is not recognized.
    """
    return Syntax(serd_syntax_by_name(_tocstr(name)))


def guess_syntax(filename: str) -> Syntax:
    """Guess a syntax from a filename.

    This uses the file extension to guess the syntax of a file.

    Returns:
        A syntax, or Syntax.EMPTY if the name is not recognized.
    """
    return Syntax(serd_guess_syntax(_tocstr(filename)))


def syntax_has_graphs(syntax: Syntax) -> bool:
    """Return whether a syntax can represent multiple graphs.

    Returns:
        True for Syntax.NQUADS and Syntax.TRIG, False otherwise.
    """
    return serd_syntax_has_graphs(syntax)


# World


@cython.no_gc
cdef class World:
    """Global library state."""

    cdef SerdWorld* _ptr

    def __cinit__(self):
        self._ptr = serd_world_new()

    def __dealloc__(self):
        serd_world_free(self._ptr)
        self._ptr = NULL

    def get_blank(self) -> Node:
        return Node._wrap(serd_world_get_blank(self._ptr))

    def load(
        self,
        path: str,
        syntax: Syntax = Syntax.TURTLE,
        reader_flags: ReaderFlags = ReaderFlags(0),
        model_flags: ModelFlags = ModelFlags.INDEX_SPO,
        stack_size: int = 4096,
    ) -> Model:
        """Load a model from a file and return it."""
        base_uri = file_uri(path)
        env = Env(base_uri)
        model = Model(self, model_flags)
        inserter = model.inserter(env)
        byte_source = FileSource(path)
        reader = Reader(self, syntax, reader_flags, env, inserter, stack_size)

        st = reader.start(byte_source)
        _ensure_success(st, "Failed to open file {}".format(path))

        st = reader.read_document()
        _ensure_success(st, "Failed to read file {}".format(path))

        st = reader.finish()
        _ensure_success(st, "Failed to finish reading file {}".format(path))

        return model

    def loads(
        self,
        s: str,
        base_uri: Node = None,
        syntax: Syntax = Syntax.TURTLE,
        reader_flags: ReaderFlags = ReaderFlags(0),
        model_flags: ModelFlags = ModelFlags.INDEX_SPO,
        stack_size: int = 4096,
    ) -> Model:
        """Load a model from a string and return it."""
        env = Env(base_uri)
        model = Model(self, model_flags)
        inserter = model.inserter(env)
        byte_source = StringSource(s)
        reader = Reader(self, syntax, reader_flags, env, inserter, stack_size)

        st = reader.start(byte_source)
        _ensure_success(st, "Failed to start reading string")

        st = reader.read_document()
        _ensure_success(st, "Failed to read string")

        st = reader.finish()
        _ensure_success(st, "Failed to finish reading string")

        return model

    def dump(
        self,
        model: Model,
        path: str,
        syntax: Syntax = Syntax.TURTLE,
        writer_flags: WriterFlags = WriterFlags(0),
        serialisation_flags: SerialisationFlags = SerialisationFlags(0),
        env: Env = None,
    ) -> None:
        """Write a model to a file."""
        byte_sink = FileSink(filename=path)
        writer = Writer(self, syntax, writer_flags, env, byte_sink)
        st = model.all().write(writer.sink(), serialisation_flags)
        writer.finish()
        byte_sink.flush()
        _ensure_success(st)

    def dumps(
        self,
        model: Model,
        syntax: Syntax = Syntax.TURTLE,
        writer_flags: WriterFlags = WriterFlags(0),
        serialisation_flags: SerialisationFlags = SerialisationFlags(0),
        env: Env = None,
    ) -> str:
        """Write a model to a string and return it."""
        byte_sink = StringSink()
        writer = Writer(self, syntax, writer_flags, env, byte_sink)
        st = model.all().write(writer.sink(), serialisation_flags)
        writer.finish()

        _ensure_success(st)

        return byte_sink.output()


cdef class Node:
    """An RDF node."""

    cdef SerdNode* _ptr

    @staticmethod
    cdef Node _manage(SerdNode* ptr):
        if ptr is NULL:
            return None

        cdef Node wrapper = Node.__new__(Node)
        wrapper._ptr = ptr
        return wrapper

    @staticmethod
    cdef Node _wrap(const SerdNode* ptr):
        if ptr is NULL:
            return None

        cdef Node wrapper = Node.__new__(Node)
        wrapper._ptr = serd_node_copy(ptr)
        return wrapper

    @staticmethod
    def from_syntax(string: str, syntax: Syntax = Syntax.TURTLE):
        """Return a new node created from a string.

        The string must be a single node in the given syntax, as returned by
        :meth:`serd.Node.to_syntax`.
        """
        return Node._manage(serd_node_from_syntax(_tocstr(string),
                                                  Syntax.TURTLE))

    def __init__(self, value):
        if isinstance(value, str):
            value_view = _string_view(value)
            self._ptr = serd_new_string(value_view)
        elif isinstance(value, type(True)):
            self._ptr = serd_new_boolean(value)
        elif isinstance(value, type(1)):
            if value < -9223372036854775808 or value > 9223372036854775807:
                raise ValueError("Integer out of range for xsd:long: %s" % value)

            self._ptr = serd_new_integer(value, NULL)
        elif isinstance(value, type(1.0)):
            self._ptr = serd_new_double(value)
        else:
            raise TypeError("Bad argument type for Node(): %s" % type(value))

    def __dealloc__(self):
        if self._ptr is not NULL:
            serd_node_free(self._ptr)
            self._ptr = NULL

    def __hash__(self):
        return (hash(self.type()) ^
                hash(str(self)) ^
                hash(self.datatype()) ^
                hash(self.language()))

    def __str__(self):
        return _fromcstr(serd_node_string(self._ptr))

    def __repr__(self):
        assert self._ptr is not NULL
        if self.type() == NodeType.LITERAL:
            datatype = self.datatype()
            language = self.language()
            if datatype is None and language is None:
                return 'serd.string("{}")'.format(self)
            elif language is not None:
                return 'serd.plain_literal("{}", "{}")'.format(
                    self, self.language())
            elif datatype == "http://www.w3.org/2001/XMLSchema#boolean":
                return 'serd.boolean({})'.format(
                    "True" if self == "true" else "False")

            return 'serd.typed_literal("{}", "{}")'.format(
                self, self.datatype())
        if self.type() == NodeType.URI:
            return 'serd.uri("{}")'.format(self)
        if self.type() == NodeType.CURIE:
            return 'serd.curie("{}")'.format(self)
        if self.type() == NodeType.BLANK:
            return 'serd.blank("{}")'.format(self)
        if self.type() == NodeType.VARIABLE:
            return 'serd.variable("{}")'.format(self)

        raise NotImplementedError("Unknown node type {}".format(self.type()))

    def __len__(self):
        return serd_node_length(self._ptr)

    def __eq__(self, rhs):
        if rhs is None:
            return False
        elif type(rhs) == Node:
            return serd_node_equals(self._ptr, (<Node>rhs)._ptr)
        else:
            return str(self) == rhs

    def __lt__(self, rhs: Node):
        return serd_node_compare(self._ptr, rhs._ptr) < 0

    def __le__(self, rhs: Node):
        return serd_node_compare(self._ptr, rhs._ptr) <= 0

    def string_view(self):
        return serd_node_string_view(self._ptr)

    def type(self) -> NodeType:
        """Return the type of this node.

        This returns the fundamental "kind" of the node, for example
        NodeType.URI or NodeType.LITERAL.  Note that this is different than the
        optional datatype URI of a literal node, which, for example, states
        that a literal is an integer or a double.
        """
        return NodeType(serd_node_type(self._ptr))

    def datatype(self) -> Node:
        """Return the datatype of this literal, or None.

        The returned node is always a URI, typically something like
        `serd.uri("http://www.w3.org/2001/XMLSchema#decimal")`.
        """
        return Node._wrap(serd_node_datatype(self._ptr))

    def language(self) -> Node:
        """Return the language of this literal, or None.

        The returned node is always a string, typically something like
        `serd.string("en")`.
        """
        return Node._wrap(serd_node_language(self._ptr))

    def to_syntax(self, syntax: Syntax = Syntax.TURTLE) -> str:
        """Return a string representation of this node in a syntax.

        The returned string represents that node as if written as an object in
        the given syntax, without any extra quoting or punctuation.  The syntax
        should be either TURTLE or NTRIPLES (the others are redundant).  Note
        that namespaced (CURIE) nodes and relative URIs can not be expressed in
        NTriples.

        Passing the returned string to Node.from_syntax() will produce a node
        equivalent to this one.
        """
        cstr = serd_node_to_syntax(self._ptr, syntax)
        result = _fromcstr(cstr)
        serd_free(cstr)
        return result


# Node constructors


def string(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_string(s_view))


def plain_literal(s: str, lang: str = None) -> Node:
    s_view = _string_view(s)
    if lang is not None:
        s_view = _string_view(s)
        lang_view = _string_view(lang)
        return Node._manage(serd_new_plain_literal(s_view, lang_view))
    else:
        s_view = _string_view(s)
        return Node._manage(serd_new_string(s_view))


def typed_literal(s: str, datatype) -> Node:
    s_view = _string_view(s)
    datatype_node = _uri_from_param(datatype)
    if type(datatype_node) == Node:
        datatype_uri_view = datatype_node.string_view()
        return Node._manage(serd_new_typed_literal(s_view, datatype_uri_view))

    return None


def blank(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_blank(s_view))


def curie(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_curie(s_view))


def uri(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_uri(s_view))


def file_uri(path: str, hostname: str = "") -> Node:
    path_view = _string_view(path)
    hostname_view = _string_view(hostname)
    return Node._manage(serd_new_file_uri(path_view, hostname_view))


def decimal(
    d: float,
    datatype: Node = None,
) -> Node:
    return Node._manage(
        serd_new_decimal(
            d,
            datatype._ptr if datatype else NULL,
        )
    )


def double(d: float) -> Node:
    return Node._manage(serd_new_double(d))


def float(f: float) -> Node:
    return Node._manage(serd_new_float(<float>f) )


def integer(i: int, datatype: Node = None) -> Node:
    return Node._manage(
        serd_new_integer(i, datatype._ptr if datatype else NULL)
    )


def boolean(b: bool) -> Node:
    return Node._manage(serd_new_boolean(b))


def base64(const unsigned char[:] buf,
         datatype: Node = None) -> Node:
    assert datatype is None or type(datatype) == Node
    return Node._manage(
        serd_new_base64(
            &buf[0], len(buf), datatype._ptr if datatype else NULL
        )
    )


def variable(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_simple_node(NodeType.VARIABLE, s_view))


cdef SerdStringView _empty_string = SerdStringView("", 0)


cdef class Env:

    """Lexical environment for abbreviating and expanding URIs."""

    cdef SerdEnv* _ptr

    @staticmethod
    cdef Env _manage(SerdEnv* ptr):
        if ptr is NULL:
            return None

        cdef Env wrapper = Env.__new__(Node)
        wrapper._ptr = ptr
        return wrapper

    def __init__(self, arg=None):
        if arg is None:
            self._ptr = serd_env_new(_empty_string)
        elif type(arg) == Env:
            self._ptr = serd_env_copy((<Env>arg)._ptr)
        elif type(arg) == Node:
            arg_view = arg.string_view()
            self._ptr = serd_env_new(arg_view)
        else:
            raise TypeError("Bad argument type for Env(): %s" % type(arg))

    def __dealloc__(self):
        serd_env_free(self._ptr)
        self._ptr = NULL

    def __eq__(self, rhs):
        return type(rhs) == Env and serd_env_equals(self._ptr, (<Env>rhs)._ptr)

    def base_uri(self) -> Node:
        """Return the current base URI."""
        return Node._wrap(serd_env_base_uri(self._ptr))

    def set_base_uri(self, uri) -> Status:
        """Set the current base URI."""
        if uri is None:
            return Status(serd_env_set_base_uri(self._ptr, _empty_string))

        uri_view = _uri_from_param(uri).string_view()
        return Status(serd_env_set_base_uri(self._ptr, uri_view))

    def set_prefix(self, name, uri: Node) -> Status:
        """Set a namespace prefix.

        A namespace prefix is used to expand CURIE nodes, for example, with the
        prefix "xsd" set to "http://www.w3.org/2001/XMLSchema#", "xsd:decimal"
        will expand to "http://www.w3.org/2001/XMLSchema#decimal".
        """
        name_node = string(name) if type(name) == str else name
        name_view = name_node.string_view()
        uri_view = uri.string_view()
        assert type(name_node) == Node
        return Status(serd_env_set_prefix(self._ptr, name_view, uri_view))

    def qualify(self, uri: Node) -> Node:
        """Qualify `uri` into a CURIE if possible.

        Returns null if `uri` can not be qualified (usually because no
        corresponding prefix is defined).
        """
        return Node._manage(serd_env_qualify(self._ptr, uri._ptr))

    def expand(self, node: Node) -> Node:
        """Expand `node`, transforming CURIEs into URIs

        If `node` is a relative URI reference, it is expanded to a full URI if
        possible.  If `node` is a literal, its datatype is expanded if
        necessary.  If `node` is a CURIE, it is expanded to a full URI if
        possible.

        Returns None if `node` can not be expanded.
        """
        return Node._manage(serd_env_expand(self._ptr, node._ptr))


class ReadContext(object):
    def __init__(self, reader, source):
        self.reader = reader
        self.source = source

    def __enter__(self):
        _ensure_success(self.reader.start(self.source),
                        "Failed to start reading")
        return self

    def __exit__(self, type, value, traceback) -> None:
        _ensure_success(self.reader.finish(), "Failed to finish reading")

    def read_chunk(self) -> None:
        _ensure_success(self.reader.read_chunk(), "Failed to read chunk")

    def read_document(self) -> None:
        _ensure_success(self.reader.read_document(), "Failed to read document")


cdef class Reader:
    """Streaming parser that reads a text stream and writes to a sink.

    .. py:function:: serd.Reader(world: serd.World, syntax: serd.Syntax, flags: serd.ReaderFlags, env: serd.Env, sink, stack_size: int = 4096)

       Construct a new reader.

       The `sink` can be either a :class:`serd.Sink`, a built-in sink (for
       example, from :meth:`serd.Writer.sink()` or :meth:`serd.Model.inserter`),
       or a function that takes a :class:`serd.Event` and returns a
       :class:`serd.Status`.
    """

    cdef SerdReader*   _ptr
    cdef __ByteSource  _byte_source
    cdef _SinkBase     _sink
    cdef object        _callback

    @staticmethod
    cdef Reader _manage(SerdReader* ptr):
        if ptr is NULL:
            return None

        cdef Reader wrapper = Reader.__new__(Reader)
        wrapper._ptr = ptr
        return wrapper

    def __init__(self,
                 world: World,
                 syntax: Syntax,
                 flags: ReaderFlags,
                 env: Env,
                 sink,
                 stack_size: int = 4096):
        if isinstance(sink, _SinkBase):
            self._sink = sink
        else:
            self._callback = sink
            self._sink = Sink(func=self._callback)

        assert isinstance(self._sink, _SinkBase)
        assert self._sink._cptr is not NULL

        self._ptr = serd_reader_new(world._ptr,
                                    syntax,
                                    flags,
                                    env._ptr,
                                    (<_SinkBase>self._sink)._cptr,
                                    stack_size)

    def __dealloc__(self):
        serd_reader_free(self._ptr)
        self._ptr = NULL

    def add_blank_prefix(self, prefix: str) -> None:
        """Set a prefix to be added to all blank node identifiers.

        This is useful when multiple files are to be parsed into the same
        output (a model or a file).  Since Serd preserves blank node IDs, this
        could cause conflicts where two non-equivalent blank nodes are merged,
        resulting in corrupt data.  By setting a unique blank node prefix for
        each parsed file, this can be avoided, while preserving blank node
        names.
        """
        serd_reader_add_blank_prefix(self._ptr, prefix)

    def start(self, byte_source: __ByteSource) -> Status:
        """Prepare to read from a byte source."""
        return Status(serd_reader_start(self._ptr, byte_source._ptr))

    def read_chunk(self) -> Status:
        """Read a single "chunk" of data during an incremental read.

        This function will read a single top level description, and return.
        This may be a directive, statement, or several statements; essentially
        it reads until a '.' is encountered.  This is particularly useful for
        reading directly from a pipe or socket.
        """
        return Status(serd_reader_read_chunk(self._ptr))

    def read_document(self) -> Status:
        """Read a complete document from the source.

        This function will continue pulling from the source until a complete
        document has been read.  Note that this may block when used with
        streams, for incremental reading use serd_reader_read_chunk().
        """
        return Status(serd_reader_read_document(self._ptr))

    def finish(self) -> Status:
        """Finish reading from the source.

        This should be called before starting to read from another source.
        Finish reading from the source.
        """
        return Status(serd_reader_finish(self._ptr))

    def open(self, source) -> ReadContext:
        return ReadContext(self, source)


cdef class __ByteSource:
    """A source for bytes that provides text input."""
    cdef SerdByteSource* _ptr

    def __dealloc__(self):
        serd_byte_source_free(self._ptr)
        self._ptr = NULL


cdef class FileSource(__ByteSource):
    """A byte source for text input that reads from a file."""
    def __init__(self, filename: str, block_size: int = 4096):
        super().__init__()
        self._ptr = serd_byte_source_new_filename(_tocstr(filename), block_size)


cdef class StringSource(__ByteSource):
    cdef bytes _bytes

    """A byte source for text input that reads from a string."""
    def __init__(self, string: str, name: Node = None):
        super().__init__()
        self._bytes = _tocstr(string)
        self._ptr = serd_byte_source_new_string(self._bytes,
                                                _unwrap_node(name))


cdef class ByteSink:
    """A sink for bytes that receives text output."""
    cdef SerdByteSink* _ptr

    def __dealloc__(self):
        serd_byte_sink_free(self._ptr)
        self._ptr = NULL

    def flush(self) -> None:
        """Flush any pending output to the underlying stream."""
        serd_byte_sink_flush(self._ptr)

    def close(self) -> None:
        """Close sink, including the underlying file if necessary."""
        serd_byte_sink_close(self._ptr)


cdef class FileSink(ByteSink):
    """A sink for bytes that writes text output to a file."""

    def __init__(self,
                 filename: str,
                 block_size: int = 4096):
        super().__init__()

        self._ptr = serd_byte_sink_new_filename(_tocstr(filename),
                                                block_size)

        if self._ptr is NULL:
            raise OSError(errno, strerror(errno), filename)


cdef class StringSink(ByteSink):
    cdef SerdBuffer _buffer

    def __dealloc__(self):
        serd_free(self._buffer.buf)
        self._buffer.buf = NULL
        self._buffer.len = 0;
        # super().__dealloc__(self)

    def __init__(self):
        super().__init__()

        self._buffer.buf = NULL
        self._buffer.len = 0;
        self._ptr = serd_byte_sink_new_buffer(&self._buffer)

    def output(self) -> str:
        self.flush()
        return _fromcstr(serd_buffer_sink_finish(&self._buffer))


cdef class Writer:
    """Streaming writer that emits text as it receives events.
    """

    cdef SerdWriter* _ptr

    def __init__(self,
                 world: World,
                 syntax: Syntax,
                 flags: WriterFlags,
                 env: Env,
                 byte_sink: ByteSink):
        self._ptr = serd_writer_new(
            world._ptr,
            syntax,
            flags,
            env._ptr if env is not None else NULL,
            byte_sink._ptr,
        )

    def __dealloc__(self):
        serd_writer_free(self._ptr)
        self._ptr = NULL

    def sink(self) -> SinkView:
        """Return a sink interface that emits statements via this writer."""
        return SinkView._wrap(serd_writer_sink(self._ptr))

    def chop_blank_prefix(self, prefix: str):
        """Set a prefix to be removed from matching blank node identifiers.

        This is the counterpart to :meth:`serd.Reader.add_blank_prefix()` and
        can be used to "undo" added prefixes.
        """
        serd_writer_chop_blank_prefix(self._ptr, prefix._ptr)

    def set_base_uri(self, uri: Node) -> Status:
        """Set the current output base URI, and emit a directive if applicable.
        """
        return Status(serd_writer_set_base_uri(self._ptr, uri._ptr))

    def set_root_uri(self, uri: Node) -> Status:
        """Set the current root URI.

        The root URI should be a prefix of the base URI.  The path of the root
        URI is the highest path any relative up-reference can refer to.  For
        example, with root <file:///foo/root> and base <file:///foo/root/base>,
        <file:///foo/root> will be written as <../>, but <file:///foo> will be
        written non-relatively as <file:///foo>.  If the root is not explicitly
        set, it defaults to the base URI, so no up-references will be created
        at all.
        """
        return Status(serd_writer_set_root_uri(self._ptr, uri._ptr))

    def finish(self) -> Status:
        """Finish a write.

        This flushes any pending output, for example terminating punctuation,
        so that the output is a complete document.
        """
        return Status(serd_writer_finish(self._ptr))


class SerdError(RuntimeError):
    def __init__(self, status: Status, message: str = ""):
        if message:
            super().__init__("%s (%s)" % (message, strerror(status)))
        else:
            super().__init__(strerror(status))

        self.status = status


def _ensure_success(status: Status, message: str = ""):
    if status != Status.SUCCESS:
        raise SerdError(status, message)


@cython.no_gc
cdef class Model:
    """An indexed set of statements."""
    cdef SerdModel* _ptr
    cdef World _world

    def __cinit__(self,
                  world: World,
                  flags: ModelFlags = ModelFlags.INDEX_SPO,
                  model: Model = None):
        if model is not None:
            self._world = world
            self._ptr = serd_model_copy(model._ptr)
        elif type(flags) == ModelFlags:
            self._world = world
            self._ptr = serd_model_new(world._ptr, flags)
        else:
            raise TypeError("Bad arguments for Model()")

    def __dealloc__(self):
        serd_model_free(self._ptr)
        self._world = None
        self._ptr = NULL

    def __eq__(self, rhs):
        return (type(rhs) == Model and
                serd_model_equals(self._ptr, (<Model>rhs)._ptr))

    def __len__(self):
        return self.size()

    def __iter__(self):
        if self.size() == 0:
            return _Iter._end()

        return _Iter._manage(serd_model_begin(self._ptr))

    def __contains__(self, statement):
        return self._find(Statement._from_param(statement)) != self._end()

    def __delitem__(self, statement):
        i = self._find(statement)
        if i is not None:
            self.erase(i)

    def __add__(self: Model, statement_param):
        statement = Statement._from_param(statement_param)
        status = serd_model_insert(self._ptr, (<Statement>statement)._ptr)
        _ensure_success(status)

        return self

    def world(self) -> World:
        return self._world

    def clear(self) -> None:
        return serd_model_clear(self._ptr);

    def copy(self) -> Model:
        return Model(self._world, self.flags(), self)

    def flags(self) -> ModelFlags:
        """Get the flags enabled on this model."""
        return ModelFlags(serd_model_flags(self._ptr))

    def size(self) -> int:
        """Return the number of statements stored in this model."""
        return serd_model_size(self._ptr)

    def empty(self) -> bool:
        """Return true iff there are no statements in this model."""
        return serd_model_empty(self._ptr)

    def inserter(self, env: Env, default_graph: Node = None) -> Sink:
        """Return a sink that will insert into this model when written to."""
        return Sink._manage(serd_inserter_new(
            self._ptr, _unwrap_node(default_graph)
        ))

    def insert(self, arg) -> None:
        """Insert a Statement or Range into this model."""
        if type(arg) == Range:
            return Status(serd_model_add_range(self._ptr, (<Range>arg)._ptr))

        statement = Statement._from_param(arg)
        st = serd_model_insert(self._ptr, (<Statement>statement)._ptr)
        _ensure_success(st)

    def erase(self, arg) -> Status:
        """Erase a statement or range from the model.

        The argument can be a range, iterator, statement, or tuple of nodes.
        """

        if type(arg) == Range:
            _ensure_success(
                serd_model_erase_range(self._ptr, (<Range>arg)._ptr),
                "Failed to erase range")
        elif type(arg) == _Iter:
            # FIXME: remove?
            _ensure_success(
                serd_model_erase(self._ptr, (<_Iter>arg)._ptr),
                "Failed to erase iterator")
        elif type(arg) == Statement:
            i = self._find(arg)
            if i == self._end():
                raise ValueError("serd.Model.erase(): statement not in model")

            self.erase(i)
        elif type(arg) == tuple:
            self.erase(Statement._from_param(arg))
        else:
            raise TypeError("Bad argument type for Model.erase: %s" % type(arg))

    # def begin(self) -> _Iter:
    #     return _Iter._manage(serd_model_begin(self._ptr))

    def _end(self) -> _Iter:
        return _Iter._wrap(serd_model_end(self._ptr))

    def all(self) -> Range:
        """Return a range of all statements in the model in SPO order."""
        return Range._manage(serd_model_all(self._ptr))

    def ordered(self, order: StatementOrder) -> Range:
        """Return a range of all statements in the model in a given order."""
        return Range._manage(serd_model_ordered(self._ptr, order))

    def _find(self, statement) -> _Iter:
        statement = Statement._from_param(statement)
        s = statement.subject()
        p = statement.predicate()
        o = statement.object()
        g = statement.graph()

        c_iter = serd_model_find(
            self._ptr,
            _unwrap_node(s),
            _unwrap_node(p),
            _unwrap_node(o),
            _unwrap_node(g)
        )

        return _Iter._manage(c_iter) if c_iter else self._end()

    def range(self, pattern) -> Range:
        """Return a range of statements that match a pattern."""
        assert type(pattern) == tuple
        assert len(pattern) == 3 or len(pattern) == 4

        s = pattern[0]
        p = pattern[1]
        o = pattern[2]
        g = pattern[3] if len(pattern) == 4 else None

        return Range._manage(
            serd_model_range(
                self._ptr,
                _unwrap_node(s),
                _unwrap_node(p),
                _unwrap_node(o),
                _unwrap_node(g)
            )
        )

    def get(self,
            subject: Node = None,
            predicate: Node = None,
            object: Node = None,
            graph: Node = None) -> Node:
        return Node._wrap(
            serd_model_get(
                self._ptr,
                _unwrap_node(subject),
                _unwrap_node(predicate),
                _unwrap_node(object),
                _unwrap_node(graph)
            )
        )

    def ask(self, s: Node, p: Node, o: Node, g: Node = None) -> bool:
        """Return true iff the model contains a statement matching a pattern.

        None can be used as a wildcard which matches any node.
        """
        return serd_model_ask(
            self._ptr,
            _unwrap_node(s),
            _unwrap_node(p),
            _unwrap_node(o),
            _unwrap_node(g)
        )

    def count(self, s: Node, p: Node, o: Node, g: Node = None) -> int:
        """Return the number of statements in the model that match a pattern.

        None can be used as a wildcard which matches any node.
        """
        return serd_model_count(
            self._ptr,
            _unwrap_node(s),
            _unwrap_node(p),
            _unwrap_node(o),
            _unwrap_node(g)
        )


# cdef class Inserter:
#     """A statement sink that inserts into a model."""
#     cdef SerdInserter* _ptr

#     @staticmethod
#     cdef Inserter _manage(SerdInserter* ptr):
#         if ptr is NULL:
#             return None

#         cdef Inserter wrapper = Inserter.__new__(Inserter)
#         wrapper._ptr = ptr
#         return wrapper

#     def __init__(self, model: Model, env: Env, default_graph: Node = None):
#         self._ptr = serd_inserter_new(
#             model._ptr, env._ptr, _unwrap_node(default_graph)
#         )

#     def __dealloc__(self):
#         serd_inserter_free(self._ptr)
#         self._ptr = NULL

#     def sink(self) -> SinkView:
#         return SinkView._wrap(serd_inserter_get_sink(self._ptr))


cdef class Statement:
    """An RDF statement.

    .. py:function:: serd.serd.Statement(subject: serd.Node, predicate: serd.Node, object: serd.Node, graph: serd.Node = None, cursor: serd.Cursor = None)

       Construct a new statement.
    """

    cdef SerdStatement* _ptr
    cdef Node           _subject
    cdef Node           _predicate
    cdef Node           _object
    cdef Node           _graph
    cdef Cursor         _cursor

    @staticmethod
    cdef Statement _manage(SerdStatement* ptr):
        if ptr is NULL:
            return None

        cdef Statement wrapper = Statement.__new__(Statement)
        wrapper._ptr = ptr
        return wrapper

    @staticmethod
    cdef Statement _wrap(const SerdStatement* ptr):
        if ptr is NULL:
            return None

        cdef Statement wrapper = Statement.__new__(Statement)
        wrapper._subject = Node._wrap(serd_statement_subject(ptr))
        wrapper._predicate = Node._wrap(serd_statement_predicate(ptr))
        wrapper._object = Node._wrap(serd_statement_object(ptr))
        wrapper._graph = Node._wrap(serd_statement_graph(ptr))
        wrapper._cursor = Cursor._wrap(serd_statement_cursor(ptr))
        wrapper._ptr = serd_statement_new(
            _unwrap_node(wrapper._subject),
            _unwrap_node(wrapper._predicate),
            _unwrap_node(wrapper._object),
            _unwrap_node(wrapper._graph),
            (<Cursor>wrapper._cursor)._ptr if wrapper._cursor is not None else NULL)

        return wrapper

    @staticmethod
    def _from_param(obj):
        if type(obj) == Statement:
            return obj

        if type(obj) == tuple:
            if len(obj) != 3 and len(obj) != 4:
                raise ValueError("Bad number of statement fields")

            for i in range(len(obj)):
                if type(obj[i]) != Node:
                    raise TypeError("Bad type for statement field " + i)

            g = obj[3] if len(obj) == 4 else None
            return Statement(obj[0], obj[1], obj[2], g)

        raise TypeError("Bad argument type for Statement: %s" % type(obj))

    def __init__(
            self,
            subject: Node,
            predicate: Node,
            object: Node,
            graph: Node = None,
            cursor: Cursor = None,
    ):
        self._subject = <Node>subject
        self._predicate = <Node>predicate
        self._object = <Node>object
        self._graph = <Node>graph
        self._cursor = <Cursor>cursor

        self._ptr = serd_statement_new(
            _unwrap_node(self._subject),
            _unwrap_node(self._predicate),
            _unwrap_node(self._object),
            _unwrap_node(self._graph),
            (<Cursor>self._cursor)._ptr if self._cursor is not None else NULL,
        )

    def __dealloc__(self):
        serd_statement_free(self._ptr)
        self._ptr = NULL

    def __getitem__(self, field):
        if field < 0 or field > 3 or (field == 3 and self.graph() is None):
            raise IndexError(field)

        return self.node(field)

    def __eq__(self, rhs):
        return type(rhs) == Statement and serd_statement_equals(
            self._ptr, (<Statement>rhs)._ptr
        )

    def __str__(self):
        result = " ".join(
            [
                self.subject().to_syntax(),
                self.predicate().to_syntax(),
                self.object().to_syntax(),
            ]
        )

        if serd_statement_graph(self._ptr) is not NULL:
            result += " " + self.graph().to_syntax()

        return result

    def __repr__(self):
        args = [repr(self.subject()),
                repr(self.predicate()),
                repr(self.object())]

        if self.graph() is not None:
            args += [repr(self.graph())]

        if self.cursor() is not None:
            args += [repr(self.cursor())]

        return "serd.Statement({})".format(", ".join(args))

    def matches(self, s: Node, p: Node, o: Node, g: Node = None):
        """Return true iff this statement matches the given pattern.

        Nodes match if they are equivalent, or if one of them is NULL.  The
        statement matches if every node matches.
        """
        return serd_statement_matches(
            self._ptr,
            s._ptr if s is not None else NULL,
            p._ptr if p is not None else NULL,
            o._ptr if o is not None else NULL,
            g._ptr if g is not None else NULL,
        )

    def node(self, field: Field) -> Node:
        """Return a node in this statement."""
        assert field >= Field.SUBJECT and field <= Field.GRAPH
        return Node._wrap(serd_statement_node(self._ptr, field))

    def subject(self) -> Node:
        """Return the subject node of this statement."""
        return Node._wrap(serd_statement_subject(self._ptr))

    def predicate(self) -> Node:
        """Return the predicate node of this statement."""
        return Node._wrap(serd_statement_predicate(self._ptr))

    def object(self) -> Node:
        """Return the object node in this statement."""
        return Node._wrap(serd_statement_object(self._ptr))

    def graph(self) -> Node:
        """Return the graph node in this statement."""
        return Node._wrap(serd_statement_graph(self._ptr))

    def cursor(self) -> Cursor:
        """Return the file location this statement came from, or None."""
        return Cursor._wrap(serd_statement_cursor(self._ptr))


cdef class _Iter:
    """An iterator that points to a statement in a model."""
    cdef SerdIter* _ptr
    cdef bint      _is_end

    @staticmethod
    cdef _end():
        cdef _Iter wrapper = _Iter.__new__(_Iter)

        wrapper._ptr = NULL
        wrapper._is_end = True

        return wrapper

    @staticmethod
    cdef _manage(SerdIter* ptr):
        cdef _Iter wrapper = _Iter.__new__(_Iter)

        if ptr is NULL:
            wrapper._ptr = NULL
            wrapper._is_end = True
        else:
            wrapper._ptr = ptr
            wrapper._is_end = False

        return wrapper

    @staticmethod
    cdef _wrap(const SerdIter* ptr):
        return _Iter._manage(serd_iter_copy(ptr))

    def __init__(self, iter: _Iter):
        assert type(iter) == _Iter

        self._is_end = False
        self._ptr = serd_iter_copy(iter._ptr)
        self._is_end = iter._is_end

    def __dealloc__(self):
        serd_iter_free(self._ptr)
        self._ptr = NULL

    def __eq__(self, rhs):
        return type(rhs) == _Iter and serd_iter_equals(self._ptr, (<_Iter>rhs)._ptr)

    def __next__(self):
        """Move to and return the next item."""
        if self._is_end:
            raise StopIteration

        item = serd_iter_get(self._ptr)
        self._is_end = serd_iter_next(self._ptr)

        return Statement._wrap(item)

    def get(self) -> Statement:
        """Get the current item."""
        return Statement._wrap(serd_iter_get(self._ptr))


cdef class Range:
    """A range of statements in a model.

    This class is iterable so it can be used like a collection.  For example,
    :meth:`serd.Model.all()` returns a range, so all the statements in a model
    can be printed like so::

        for statement in model.all():
            print(statement)

    A range is "truthy" if it is non-empty.
    """
    cdef SerdRange* _ptr

    @staticmethod
    cdef _manage(SerdRange* ptr):
        if ptr is NULL:
            return None

        cdef Range wrapper = Range.__new__(Range)
        wrapper._ptr = ptr
        return wrapper

    @staticmethod
    cdef _wrap(const SerdRange* ptr):
        return Range._manage(serd_range_copy(ptr))

    def __init__(self, range: Range):
        assert type(range) == Range
        self._ptr = serd_range_copy((<Range>range)._ptr)

    def __dealloc__(self):
        serd_range_free(self._ptr)
        self._ptr = NULL

    def __bool__(self):
        return not self.empty()

    def __eq__(self, rhs):
        return type(rhs) == Range and serd_range_equals(self._ptr, (<Range>rhs)._ptr)

    def __iter__(self):
        if self.empty():
            return _Iter._end()

        return _Iter._wrap(serd_range_begin(self._ptr))

    def front(self) -> Statement:
        """Return the first statement in this range, or None."""
        return Statement._wrap(serd_range_front(self._ptr))

    def empty(self) -> bool:
        """Return true iff there are no statements in this range."""
        return serd_range_empty(self._ptr)

    def write(self, sink: _SinkBase, flags: SerialisationFlags) -> Status:
        """Write this range to `sink`.

        The serialisation style can be controlled with `flags`.  The default is
        to write statements in an order suited for pretty-printing with Turtle
        or TriG with as many objects written inline as possible.  If
        `SerialisationFlags.NO_INLINE_OBJECTS` is given, a simple sorted stream
        is written instead, which is significantly faster since no searching is
        required, but can result in ugly output for Turtle or Trig.
        """
        return Status(serd_write_range(self._ptr, sink._cptr, flags))


cdef class Cursor:
    """The origin of a statement in a document."""
    cdef SerdCursor* _ptr
    cdef Node        _name_node

    @staticmethod
    cdef Cursor _manage(SerdCursor* ptr):
        if ptr is NULL:
            return None

        cdef Cursor wrapper = Cursor.__new__(Cursor)
        wrapper._ptr = ptr
        return wrapper

    @staticmethod
    cdef Cursor _wrap(const SerdCursor* ptr):
        return Cursor._manage(serd_cursor_copy(ptr))

    def __init__(self, name, line: uint = 1, col: uint = 0):
        if type(name) == Node:
            self._name_node = <Node>name
            self._ptr = serd_cursor_new((<Node>self._name_node)._ptr, line, col)
        elif type(name) == str:
            self._name_node = string(name)
            self._ptr = serd_cursor_new((<Node>self._name_node)._ptr, line, col)
        else:
            raise TypeError("Bad name argument type for Cursor(): %s" % type(name))


    def __dealloc__(self):
        serd_cursor_free(self._ptr)
        self._ptr = NULL

    def __eq__(self, rhs):
        return (type(rhs) == Cursor and
                serd_cursor_equals(self._ptr, (<Cursor>rhs)._ptr))

    def __str__(self):
        return "{}:{}:{}".format(self.name(), self.line(), self.column())

    def __repr__(self):
        return "serd.Cursor({})".format(
            ", ".join([repr(self.name()), str(self.line()), str(self.column())]))

    def name(self) -> Node:
        """Return the document name.

        This is typically a file URI, but may be a descriptive string node for
        statements that originate from strings or streams.
        """

        return Node._wrap(serd_cursor_name(self._ptr))

    def line(self) -> int:
        """Return the one-relative line number in the document."""
        return serd_cursor_line(self._ptr)

    def column(self) -> int:
        """Return the zero-relative column number in the line."""
        return serd_cursor_column(self._ptr)


cdef class Event:
    """An event in a data stream.

    Streams of data are represented as a series of events.  Events represent
    everything that can occur in an RDF document, and are used to plumb
    together different components.  For example, when parsing a document, a
    reader emits a stream of events which can be sent to a writer to rewrite
    the document, or to an inserter to build a model in memory.
    """

    cdef SerdEvent*         _ptr
    cdef SerdEventType      _type
    cdef Node               _name
    cdef Node               _uri
    cdef SerdStatementFlags _flags
    cdef Statement          _statement
    cdef Node               _node

    @staticmethod
    cdef Event _wrap(const SerdEvent* ptr):
        event = Event()
        event._type = ptr.type

        if event._type == EventType.BASE:
            base_event = <const SerdBaseEvent*>ptr
            event._uri = Node._wrap(base_event.uri)
        elif event._type == EventType.PREFIX:
            prefix_event = <const SerdPrefixEvent*>ptr
            event._name = Node._wrap(prefix_event.name)
            event._uri = Node._wrap(prefix_event.uri)
        elif event._type == EventType.STATEMENT:
            statement_event = <const SerdStatementEvent*>ptr
            event._flags = StatementFlags(statement_event.flags)
            event._statement = Statement._wrap(statement_event.statement)
        elif event._type == EventType.END:
            end_event = <const SerdEndEvent*>ptr
            event._node = Node._wrap(end_event.node)
        else:
            return None

        return event

    @staticmethod
    def base(base_uri):
        """Return an event that sets the base URI."""
        event = Event()
        event._type = EventType.BASE
        event._uri = _uri_from_param(base_uri)
        return event

    @staticmethod
    def prefix(name, namespace_uri):
        """Return an event that sets a namespace prefix."""
        event = Event()
        event._type = EventType.PREFIX
        event._name = string(name)
        event._uri = _uri_from_param(namespace_uri)
        return event

    @staticmethod
    def statement(statement, flags: StatementFlags = StatementFlags(0)):
        """Return an event that represents a statement."""
        assert type(statement) == Statement

        event = Event()
        event._type = EventType.STATEMENT
        event._statement = Statement._from_param(statement)
        event._flags = StatementFlags(flags)
        return event

    @staticmethod
    def end(node):
        """Return an event that ends an anonymous node description."""
        event = Event()
        event._type = EventType.END
        event._node = _blank_from_param(node)
        return event

    def __eq__(self, rhs):
        if type(rhs) != Event:
            return False

        other = <Event>rhs
        if self._type != other._type:
            return False
        elif self._type == EventType.BASE:
            return self._uri == other._uri
        elif self._type == EventType.PREFIX:
            return (self._name, self._uri) == (other._name, other._uri)
        elif self._type == EventType.STATEMENT:
            return (self._statement, self._flags) == (other._statement, other._flags)
        elif self._type == EventType.END:
            return self._node == other._node

        return False

    def __repr__(self):
        def flags_repr(flags):
            active = []
            for f in [StatementFlags.EMPTY_S,
                      StatementFlags.ANON_S,
                      StatementFlags.ANON_O,
                      StatementFlags.LIST_S,
                      StatementFlags.LIST_O,
                      StatementFlags.TERSE_S,
                      StatementFlags.TERSE_O]:
                if flags & f:
                    active += ['serd.' + str(f)]

            return ' | '.join(active)

        if self._type == EventType.BASE:
            return 'serd.Event.base("%s")' % self._uri
        elif self._type == EventType.PREFIX:
            return 'serd.Event.prefix("%s", "%s")' % (self._name, self._uri)
        elif self._type == EventType.STATEMENT:
            result = 'serd.Event.statement(%s' % repr(self._statement)
            if self._flags:
                result += ', %s' % flags_repr(self._flags)

            return result + ')'
        elif self._type == EventType.END:
            return 'serd.Event.end(%s)' % repr(self._node)

        return "None"


cdef class _SinkBase:
    cdef const SerdSink* _cptr


cdef class SinkView(_SinkBase):
    @staticmethod
    cdef SinkView _wrap(const SerdSink* cptr):
        if cptr is NULL:
            return None

        cdef SinkView wrapper = SinkView.__new__(SinkView)
        wrapper._cptr = cptr
        return wrapper


cdef class Sink(_SinkBase):
    cdef SerdSink* _ptr
    cdef Env       _env
    cdef object    _func

    @staticmethod
    cdef Sink _manage(SerdSink* ptr):
        if ptr is NULL:
            return None

        cdef Sink wrapper = Sink.__new__(Sink)
        wrapper._cptr = ptr
        wrapper._ptr = ptr
        return wrapper

    def __init__(self: Sink, func: callable = None):
        if func is not None:
            self._env = Env()
            self._func = func
            self._ptr = serd_sink_new(<void*>self, Sink._c_on_event, NULL)
            self._cptr = self._ptr
            # TODO: get_env?
        else:
            self._env = Env()
            self._func = None
            self._ptr = serd_sink_new(<void*>self, Sink._c_on_event, NULL)
            self._cptr = self._ptr
            # TODO: get_env?

    def __dealloc__(self):
        serd_sink_free(self._ptr)
        self._ptr = NULL
        self._cptr = NULL

    def on_event(self, event: Event) -> Status:
        return Status.SUCCESS

    def __call__(self, event: Event) -> Status:
        return self._func(event) if self._func is not None else Status.SUCCESS

    @staticmethod
    cdef SerdStatus _c_on_event(void* handle, const SerdEvent* event):
        self = <Sink>handle
        result = self.__call__(Event._wrap(event))
        assert result is None or type(result) == Status
        return result if result is not None else Status.SUCCESS
