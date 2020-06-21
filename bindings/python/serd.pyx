# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

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

from libc.stdint cimport int64_t, int32_t, int16_t, int8_t
from libc.stdint cimport uint64_t, uint32_t, uint16_t, uint8_t

logger = logging.getLogger(__name__)

cdef extern from "stdarg.h":
    ctypedef struct va_list:
        pass

cdef extern from "serd/serd.h":
    ctypedef struct SerdWorld
    ctypedef struct SerdNodes
    ctypedef struct SerdStatement
    ctypedef struct SerdCaret
    ctypedef struct SerdEnv
    ctypedef struct SerdModel
    ctypedef struct SerdCursor
    ctypedef struct SerdReader
    ctypedef struct SerdWriter
    ctypedef struct SerdSink

    ctypedef enum SerdStatus: pass
    ctypedef enum SerdSyntax: pass
    ctypedef enum SerdStatementFlag: pass
    ctypedef enum SerdDescribeFlag: pass
    ctypedef enum SerdNodeType: pass
    ctypedef enum SerdNodeFlags: pass
    ctypedef enum SerdValueType: pass
    ctypedef enum SerdField: pass
    ctypedef enum SerdStatementOrder: pass
    ctypedef enum SerdModelFlag: pass

    ctypedef uint32_t SerdStatementFlags
    ctypedef uint32_t SerdDescribeFlags
    ctypedef uint32_t SerdModelFlags

    ctypedef struct SerdNode

    ctypedef struct SerdAllocator

    ctypedef struct SerdStringView:
        const char* buf
        size_t      len

    ctypedef struct SerdBuffer:
        SerdAllocator* allocator
        void*          buf
        size_t         len

    ctypedef struct SerdURIView:
        SerdStringView scheme
        SerdStringView authority
        SerdStringView path_prefix
        SerdStringView path
        SerdStringView query
        SerdStringView fragment

    ctypedef union SerdValueData:
        bint     as_bool
        double   as_double
        float    as_float
        int64_t  as_long
        int32_t  as_int
        int16_t  as_short
        int8_t   as_byte
        uint64_t as_ulong
        uint32_t as_uint
        uint16_t as_ushort
        uint8_t  as_ubyte

    ctypedef struct SerdValue:
        SerdValueType type
        SerdValueData data

    ctypedef enum SerdReaderFlag : pass
    ctypedef uint32_t SerdReaderFlags

    ctypedef enum SerdWriterFlag : pass
    ctypedef uint32_t SerdWriterFlags

    void serd_free(SerdAllocator* allocator, void* ptr)

    # String Utilities

    const char* serd_strerror(SerdStatus status)

    # Base64

    size_t serd_base64_encoded_length(size_t size, bint wrap_lines)
    size_t serd_base64_decoded_size(size_t len)

    bint serd_base64_encode(char*       str,
                            const void* buf,
                            size_t      size,
                            bint        wrap_lines)

    SerdStatus serd_base64_decode(void*       buf,
                                  size_t*     size,
                                  const char* str,
                                  size_t      len)

    # Buffer

    size_t serd_buffer_write(const void* buf,
                             size_t      size,
                             size_t      nmemb,
                             void*       stream)

    int serd_buffer_error(void* const stream)
    int serd_buffer_close(void* const stream)

    # I/O Function Types

    ctypedef size_t (*SerdReadFunc)(void*  buf,
                                    size_t size,
                                    size_t nmemb,
                                    void*  stream)

    ctypedef size_t (*SerdWriteFunc)(const void* buf,
                                     size_t      size,
                                     size_t      nmemb,
                                     void*       stream)

    ctypedef int (*SerdErrorFunc)(void* stream)

    ctypedef int (*SerdCloseFunc)(void* stream)

    # Syntax Utilities

    SerdSyntax serd_syntax_by_name(const char* name)
    SerdSyntax serd_guess_syntax(const char* filename)
    bint       serd_syntax_has_graphs(SerdSyntax syntax)

    # URI

    char*      serd_parse_file_uri(const char* uri, char** hostname)
    bint       serd_uri_string_has_scheme(const char* utf8)
    SerdStatus serd_parse_uri(const char* utf8, SerdURIView* out)

    SerdURIView serd_resolve_uri(SerdURIView r, SerdURIView base)

    size_t serd_write_uri(SerdURIView uri, SerdWriteFunc sink, void* stream)

    # Node

    SerdNode* serd_node_from_syntax(SerdAllocator* allocator,
                                    const char* str,
                                    SerdSyntax  syntax,
                                    SerdEnv*    env)

    char* serd_node_to_syntax(SerdAllocator* allocator,
                              const SerdNode* node,
                              SerdSyntax      syntax,
                              const SerdEnv*  env)

    SerdNode* serd_new_token(SerdAllocator* allocator, SerdNodeType type, SerdStringView string)
    SerdNode* serd_new_string(SerdAllocator* allocator, SerdStringView string)
    SerdNode* serd_new_uri(SerdAllocator* allocator, SerdURIView uri)
    SerdNode* serd_new_file_uri(SerdAllocator* allocator, SerdStringView path, SerdStringView hostname)

    SerdNode* serd_new_literal(SerdAllocator* allocator,
                               SerdStringView string,
                               SerdNodeFlags  flags,
                               SerdStringView meta)

    SerdNode* serd_new_value(SerdAllocator* allocator, SerdValue value)
    SerdNode* serd_new_decimal(SerdAllocator* allocator, double d)
    SerdNode* serd_new_integer(SerdAllocator* allocator, int64_t i)
    SerdNode* serd_new_base64(SerdAllocator* allocator, const void* buf, size_t size)

    bint            serd_get_boolean(const SerdNode* node)
    double          serd_get_double(const SerdNode* node)
    float           serd_get_float(const SerdNode* node)
    int64_t         serd_get_integer(const SerdNode* node)
    SerdNode*       serd_node_copy(SerdAllocator* allocator, const SerdNode* node)
    void            serd_node_free(SerdAllocator* allocator, SerdNode* node)
    SerdNodeType    serd_node_type(const SerdNode* node)
    const char*     serd_node_string(const SerdNode* node)
    size_t          serd_node_length(const SerdNode* node)
    SerdStringView  serd_node_string_view(const SerdNode* node)
    SerdURIView     serd_node_uri_view(const SerdNode* node)
    const SerdNode* serd_node_datatype(const SerdNode* node)
    const SerdNode* serd_node_language(const SerdNode* node)
    bint            serd_node_equals(const SerdNode* a, const SerdNode* b)
    int             serd_node_compare(const SerdNode* a, const SerdNode* b)

    # Event

    ctypedef enum SerdEventType: pass

    ctypedef struct SerdBaseEvent:
        SerdEventType   type
        const SerdNode* uri

    ctypedef struct SerdPrefixEvent:
        SerdEventType   type
        const SerdNode* name
        const SerdNode* uri

    ctypedef struct SerdStatementEvent:
        SerdEventType        type
        SerdStatementFlags   flags
        const SerdStatement* statement

    ctypedef struct SerdEndEvent:
        SerdEventType   type
        const SerdNode* node

    ctypedef union SerdEvent:
        SerdEventType      type
        SerdBaseEvent      base
        SerdPrefixEvent    prefix
        SerdStatementEvent statement
        SerdEndEvent       end

    ctypedef SerdStatus (*SerdEventFunc)(void* handle, const SerdEvent* event)

    # World

    SerdWorld*      serd_world_new(SerdAllocator* allocator)
    void            serd_world_free(SerdWorld* world)
    SerdNodes*      serd_world_nodes(SerdWorld* world)
    const SerdNode* serd_world_get_blank(SerdWorld* world)

    SerdAllocator* serd_world_allocator(const SerdWorld* world)

    # TODO: logging

    ctypedef enum SerdLogLevel: pass

    cdef struct SerdLogField:
        const char* key
        const char* value

    cdef struct SerdLogEntry:
        const char*         domain
        const SerdLogField* fields
        const char*         fmt
        va_list*            args
        SerdLogLevel        level
        size_t              n_fields

    ctypedef SerdStatus (*SerdLogFunc)(void* handle, const SerdLogEntry* entry)

    # SerdStatus serd_quiet_error_func(void* handle, const SerdLogEntry* entry)

    # const char* serd_log_entry_get_field(const SerdLogEntry* entry,
    #                                      const char*         key)

    void serd_world_set_log_func(SerdWorld*  world,
                                 SerdLogFunc log_func,
                                 void*       handle)

    # SerdStatus serd_world_vlogf(const SerdWorld*    world,
    #                             const char*         domain,
    #                             SerdLogLevel        level,
    #                             size_t              n_fields,
    #                             const SerdLogField* fields,
    #                             const char*         fmt,
    #                             va_list             args)

    SerdStatus serd_world_logf(const SerdWorld*    world,
                               const char*         domain,
                               SerdLogLevel        level,
                               size_t              n_fields,
                               const SerdLogField* fields,
                               const char*         fmt,
                               ...)

    # Environment

    SerdEnv*        serd_env_new(const SerdWorld* world, const SerdStringView base_uri)
    SerdEnv*        serd_env_copy(SerdAllocator* allocator, const SerdEnv* env)
    bint            serd_env_equals(const SerdEnv* a, const SerdEnv* b)
    void            serd_env_free(SerdEnv* env)
    const SerdNode* serd_env_base_uri(SerdEnv* env)
    SerdStatus      serd_env_set_base_uri(SerdEnv* env, SerdStringView uri)

    SerdStatus serd_env_set_prefix(SerdEnv*       env,
                                   SerdStringView name,
                                   SerdStringView uri)

    SerdNode* serd_env_expand_node(const SerdEnv* env, const SerdNode* node)

    SerdStatus serd_env_write_prefixes(const SerdEnv* env, const SerdSink* sink)

    # Inserter

    SerdSink* serd_inserter_new(SerdModel*      model,
                                const SerdNode* default_graph)

    # Statement

    SerdStatement* serd_statement_new(SerdAllocator*   allocator,
                                      const SerdNode*  s,
                                      const SerdNode*  p,
                                      const SerdNode*  o,
                                      const SerdNode*  g,
                                      const SerdCaret* caret)

    SerdStatement* serd_statement_copy(SerdAllocator*       allocator,
                                       const SerdStatement* statement)

    void serd_statement_free(SerdAllocator* allocator,
                             SerdStatement* statement)

    const SerdNode* serd_statement_node(const SerdStatement* statement,
                                            SerdField field)

    const SerdNode* serd_statement_subject(const SerdStatement* statement)
    const SerdNode* serd_statement_predicate(const SerdStatement* statement)
    const SerdNode* serd_statement_object(const SerdStatement* statement)
    const SerdNode* serd_statement_graph(const SerdStatement* statement)

    const SerdCaret* serd_statement_caret(const SerdStatement* statement)

    bint serd_statement_equals(const SerdStatement* a, const SerdStatement* b)

    bint serd_statement_matches(const SerdStatement* statement,
                                const SerdNode*      subject,
                                const SerdNode*      predicate,
                                const SerdNode*      object,
                                const SerdNode*      graph)

    # Iter

    SerdCursor* serd_cursor_copy(SerdAllocator*    allocator,
                                 const SerdCursor* cursor)

    const SerdStatement* serd_cursor_get(const SerdCursor* cursor)

    SerdStatus serd_cursor_advance(SerdCursor* cursor)
    bint serd_cursor_is_end(const SerdCursor* lhs)
    bint serd_cursor_equals(const SerdCursor* lhs, const SerdCursor* rhs)
    void serd_cursor_free(SerdCursor* cursor)

    # Range

    SerdStatus serd_describe_range(const SerdCursor*      range,
                                   const SerdSink*        sink,
                                   SerdDescribeFlags flags)


    # Sink

    ctypedef void (*SerdFreeFunc)(void* ptr)

    SerdSink* serd_sink_new(const SerdWorld* world,
                            void*            handle,
                            SerdEventFunc    event_func,
                            SerdFreeFunc     free_handle)

    void serd_sink_free(SerdSink* sink)

    SerdStatus serd_sink_set_event_func(SerdSink*     sink,
                                        SerdEventFunc event_func)

    SerdStatus serd_sink_write_event(const SerdSink*  sink,
                                     const SerdEvent* event)

    SerdStatus serd_sink_write_base(const SerdSink* sink,
                                    const SerdNode* uri)

    SerdStatus serd_sink_write_prefix(const SerdSink* sink,
                                      const SerdNode* name,
                                      const SerdNode* uri)

    SerdStatus serd_sink_write_statement(const SerdSink*      sink,
                                         SerdStatementFlags   flags,
                                         const SerdStatement* statement)

    SerdStatus serd_sink_write(const SerdSink*    sink,
                               SerdStatementFlags flags,
                               const SerdNode*    subject,
                               const SerdNode*    predicate,
                               const SerdNode*    object,
                               const SerdNode*    graph)

    SerdStatus serd_sink_write_end(const SerdSink* sink, const SerdNode* node)

    # Stream Processing

    SerdSink* serd_canon_new(const SerdSink* target)

    SerdSink* serd_filter_new(const SerdSink* target,
                              const SerdNode* subject,
                              const SerdNode* predicate,
                              const SerdNode* object,
                              const SerdNode* graph)

    # Input Streams

    ctypedef struct SerdInputStream:
        void*         stream
        SerdReadFunc  read
        SerdErrorFunc error
        SerdCloseFunc close

    SerdInputStream serd_open_input_stream(SerdReadFunc  read_func,
                                           SerdErrorFunc error_func,
                                           SerdCloseFunc close_func,
                                           void*         stream)

    SerdInputStream serd_open_input_string(const char** position)

    SerdInputStream serd_open_input_file(const char* path)

    SerdStatus serd_close_input(SerdInputStream* input)

    # Reader

    SerdReader* serd_reader_new(SerdWorld*      world,
                                SerdSyntax      syntax,
                                SerdReaderFlags flags,
                                SerdEnv*        env,
                                const SerdSink* sink,
                                size_t          stack_size)

    SerdStatus serd_reader_start(SerdReader*      reader,
                                 SerdInputStream* input,
                                 const SerdNode*  input_name,
                                 size_t           block_size)

    SerdStatus serd_reader_read_chunk(SerdReader* reader)
    SerdStatus serd_reader_read_document(SerdReader* reader)
    SerdStatus serd_reader_finish(SerdReader* reader)

    void serd_reader_free(SerdReader* reader)

    # Output Streams

    ctypedef struct SerdOutputStream:
        void*         stream
        SerdWriteFunc write
        SerdErrorFunc error
        SerdCloseFunc close

    SerdOutputStream serd_open_output_stream(SerdWriteFunc write_func,
                                             SerdCloseFunc close_func,
                                             void*         stream)

    SerdOutputStream serd_open_output_buffer(SerdBuffer* buffer)

    SerdOutputStream serd_open_output_file(const char* path)

    SerdStatus serd_close_output(SerdOutputStream* output)

    # Writer

    SerdWriter* serd_writer_new(SerdWorld*        world,
                                SerdSyntax        syntax,
                                SerdWriterFlags   flags,
                                SerdEnv*          env,
                                SerdOutputStream* output,
                                size_t            block_size)

    void            serd_writer_free(SerdWriter* writer)
    const SerdSink* serd_writer_sink(SerdWriter* writer)

    SerdStatus serd_writer_set_base_uri(SerdWriter*     writer,
                                        const SerdNode* uri)

    SerdStatus serd_writer_set_root_uri(SerdWriter*     writer,
                                        SerdStringView uri)

    SerdStatus serd_writer_finish(SerdWriter* writer)

    # Model

    SerdModel* serd_model_new(SerdWorld*         world,
                              SerdStatementOrder default_order,
                              SerdModelFlags     flags)

    SerdModel*        serd_model_copy(SerdAllocator* allocator, const SerdModel* model)
    bint              serd_model_equals(const SerdModel* a, const SerdModel* b)
    void              serd_model_free(SerdModel* model)
    SerdWorld*        serd_model_world(SerdModel* model)
    SerdStatementOrder serd_model_default_order(const SerdModel* model)
    SerdModelFlags    serd_model_flags(const SerdModel* model)
    size_t            serd_model_size(const SerdModel* model)
    bint              serd_model_empty(const SerdModel* model)
    SerdCursor*       serd_model_begin(const SerdModel* model)
    const SerdCursor* serd_model_end(const SerdModel* model)
    SerdCursor*       serd_model_begin_ordered(const SerdModel* model,
                                               SerdStatementOrder order)

    SerdStatus serd_model_add_index(SerdModel* model, SerdStatementOrder order)

    SerdStatus serd_model_drop_index(SerdModel* model, SerdStatementOrder order)

    SerdCursor* serd_model_ordered(const SerdModel*         model,
                                   const SerdStatementOrder order)

    SerdCursor* serd_model_find(const SerdModel* model,
                                const SerdNode*  s,
                                const SerdNode*  p,
                                const SerdNode*  o,
                                const SerdNode*  g)

    const SerdNode* serd_model_get(const SerdModel* model,
                                   const SerdNode*  s,
                                   const SerdNode*  p,
                                   const SerdNode*  o,
                                   const SerdNode*  g)

    const SerdStatement* serd_model_get_statement(const SerdModel* model,
                                                  const SerdNode*  s,
                                                  const SerdNode*  p,
                                                  const SerdNode*  o,
                                                  const SerdNode*  g)

    bint serd_model_ask(const SerdModel* model,
                        const SerdNode*  s,
                        const SerdNode*  p,
                        const SerdNode*  o,
                        const SerdNode*  g)

    size_t serd_model_count(const SerdModel* model,
                            const SerdNode*  s,
                            const SerdNode*  p,
                            const SerdNode*  o,
                            const SerdNode*  g)

    SerdStatus serd_model_add(SerdModel*      model,
                              const SerdNode* s,
                              const SerdNode* p,
                              const SerdNode* o,
                              const SerdNode* g)

    SerdStatus serd_model_insert(SerdModel*           model,
                                 const SerdStatement* statement)

    SerdStatus serd_model_insert_statements(SerdModel* model, SerdCursor* range)

    SerdStatus serd_model_insert_statements(SerdModel* model, SerdCursor* cursor)
    SerdStatus serd_model_erase(SerdModel* model, SerdCursor* cursor)
    SerdStatus serd_model_erase_statements(SerdModel* model, SerdCursor* cursor)
    SerdStatus serd_model_clear(SerdModel* model)
    SerdStatus serd_validate(const SerdModel* model)


    # Caret

    SerdCaret* serd_caret_new(SerdAllocator*  allocator,
                              const SerdNode* name,
                              unsigned        line,
                              unsigned        col)

    SerdCaret* serd_caret_copy(SerdAllocator* allocator,
                               const SerdCaret* caret)

    void serd_caret_free(SerdAllocator* allocator, SerdCaret* caret)

    bint serd_caret_equals(const SerdCaret* lhs, const SerdCaret* rhs)

    const SerdNode* serd_caret_name(const SerdCaret* caret)
    unsigned        serd_caret_line(const SerdCaret* caret)
    unsigned        serd_caret_column(const SerdCaret* caret)


cdef SerdValue _value(v):
    cdef SerdValue value

    if isinstance(v, bool):
        value.type = <SerdValueType>1 # SERD_BOOL
        value.data.as_bool = <bint>v
        return value

    if isinstance(v, float):
        value.type = <SerdValueType>2 # SERD_DOUBLE
        value.data.as_double = v
        return value

    if isinstance(v, int):
        if v < -9223372036854775808 or v > 9223372036854775807:
            raise ValueError("Integer out of range for xsd:long: %s" % v)

        value.type = <SerdValueType>4 # SERD_LONG
        value.data.as_long = v
        return value

    raise ValueError("Unsupported value type %s" % type(v))


cdef SerdStringView _empty_string = SerdStringView("", 0)


class _WrapSentinel:
    pass


class Status(enum.IntEnum):
    """Return status code."""

    SUCCESS = 0,       # Success
    FAILURE = 1,       # Non-fatal failure
    UNKNOWN_ERROR = 2, # Unknown error
    NO_DATA = 3,       # Missing input
    OVERFLOW = 4,      # Insufficient space

    BAD_ALLOC = 5,     # Memory allocation failed
    BAD_ARG = 6,       # Invalid argument
    BAD_CALL = 7,      # Invalid call
    BAD_CURIE = 8,     # Invalid CURIE or unknown namespace prefix
    BAD_CURSOR = 9,    # Use of invalidated cursor
    BAD_EVENT = 10,    # Invalid event in stream
    BAD_INDEX = 11,    # No optimal model index available
    BAD_LABEL = 12,    # Encountered clashing blank node label
    BAD_LITERAL = 13,  # Invalid literal
    BAD_PATTERN = 14,  # Invalid statement pattern
    BAD_READ = 15,     # Error reading from file
    BAD_STACK = 16,    # Stack overflow
    BAD_SYNTAX = 17,   # Invalid syntax
    BAD_TEXT = 18,     # Invalid text encoding
    BAD_URI = 19,      # Invalid or unresolved URI
    BAD_WRITE = 20,    # Error writing to file
    BAD_DATA = 21,     # Invalid data


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


class DescribeFlags(enum.IntFlag):
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
    URI = 2  # URI (absolute or relative)
    BLANK = 3  # Blank node
    VARIABLE = 4  # Variable node


class NodeFlag(enum.IntEnum):
    """Flags that describe the details of a node."""

    IS_LONG      = 1u << 0u # Literal node should be triple-quoted
    HAS_DATATYPE = 1u << 1u # Literal node has datatype
    HAS_LANGUAGE = 1u << 2u # Literal node has language


class Field(enum.IntEnum):
    """Index of a statement in a field."""

    SUBJECT = 0  # Subject
    PREDICATE = 1  # Predicate ("key")
    OBJECT = 2  # Object ("value")
    GRAPH = 3  # Graph ("context")


class StatementOrder(enum.IntEnum):
    """Statement ordering."""

    SPO = 0  #         Subject,   Predicate, Object
    SOP = 1  #         Subject,   Object,    Predicate
    OPS = 2  #         Object,    Predicate, Subject
    OSP = 3  #         Object,    Subject,   Predicate
    PSO = 4  #         Predicate, Subject,   Object
    POS = 5  #         Predicate, Object,    Subject
    GSPO = 6  # Graph,  Subject,   Predicate, Object
    GSOP = 7  # Graph,  Subject,   Object,    Predicate
    GOPS = 8  # Graph,  Object,    Predicate, Subject
    GOSP = 9  # Graph,  Object,    Subject,   Predicate
    GPSO = 10  # Graph,  Predicate, Subject,   Object
    GPOS = 11  # Graph,  Predicate, Object,    Subject


class ModelFlags(enum.IntFlag):
    """Flags that control model storage and indexing."""

    STORE_GRAPHS = 1 << 0  # Support multiple graphs in model
    STORE_CARETS = 1 << 1  # Store original caret of statements

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
    elif isinstance(param, Namespace):
        return uri(param.prefix)

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


class Namespaces:
    def __init__(self, **kwargs):
        self._namespaces = {}
        for key, value in kwargs.items():
            self._namespaces[key] = Namespace(value)

    def __getattr__(self, name: str) -> Namespace:
        return self._namespaces[name]


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


# FIXME
cdef class World:
    """Global library state."""

    cdef SerdWorld* _ptr

    def __cinit__(self):
        self._ptr = serd_world_new(NULL)

    def __dealloc__(self):
        serd_world_free(self._ptr)
        self._ptr = NULL

    def get_blank(self) -> Node:
        """Return a unique blank node."""
        return Node._wrap(serd_world_get_blank(self._ptr))

    def read_file(self,
                  path: str,
                  sink,
                  syntax: Syntax = None,
                  reader_flags: ReaderFlags = ReaderFlags(0),
                  env: Env = None,
                  stack_size: int = 4096):
        """Read a file by streaming events to a sink."""

        if syntax is None:
            syntax = guess_syntax(path)

        if env is None:
            env = Env(self)

        reader = Reader(self, syntax, env, sink)

        with FileInput(path) as in_stream:
            with reader.open(in_stream) as context:
                context.read_document()

    def read_string(self,
                    s: str,
                    sink,
                    syntax: Syntax = Syntax.TRIG,
                    reader_flags: ReaderFlags = ReaderFlags(0),
                    env: Env = None,
                    stack_size: int = 4096):
        """Read a string by streaming events to a sink."""

        if env is None:
            env = Env(self)

        reader = Reader(self, syntax, env, sink)

        with StringInput(s) as in_stream:
            with reader.open(in_stream) as context:
                context.read_document()

    def file_sink(self,
                  path: str,
                  env: Env = None,
                  syntax: Syntax = None,
                  flags: WriterFlags = WriterFlags(0),
                  block_size: int = 1):
        """Return a scoped context manager for writing to a file.

        This is for use with 'with' statements, for example::

            with world.file_sink("output.ttl", serd.Syntax.TURTLE) as sink:
                sink.write_base("http://example.org/")
        """

        if env is None:
            env = Env(self)

        if syntax is None:
            syntax = guess_syntax(path)
            if syntax is None:
                raise ValueError("Unable to determine syntax for %s" % path)

        out_file = FileOutput(path)
        return _WriteContext(Writer(self, syntax, env, out_file), out_file)

    def load(
        self,
        path: str,
        syntax: Syntax = None,
        reader_flags: ReaderFlags = ReaderFlags(0),
        default_order: StatementOrder = None,
        model_flags: ModelFlags = None,
        default_graph: Node = None,
        stack_size: int = 4096,
    ) -> Model:
        """Load a model from a file and return it."""

        if syntax is None:
            syntax = guess_syntax(path)

        has_graphs = (
            (syntax in [Syntax.NQUADS, syntax.TRIG])
            or (default_order is not None
                and default_order >= StatementOrder.GSPO)
            or (model_flags is not None
                and (model_flags & ModelFlags.STORE_GRAPHS))
            or (default_graph is not None)
        )

        # Use reasonable defaults if model settings aren't given
        if has_graphs:
            if default_order is None:
                default_order = StatementOrder.GSPO

            if model_flags is None:
                model_flags = ModelFlags.STORE_GRAPHS
        else:
            if default_order is None:
                default_order = StatementOrder.SPO

            if model_flags is None:
                model_flags = ModelFlags(0)

        base_uri = file_uri(path)
        env = Env(self, base_uri)
        model = Model(self, default_order, model_flags)
        inserter = model.inserter(env, default_graph)
        input_stream = FileInput(path)
        reader = Reader(self,
                        syntax,
                        env,
                        inserter,
                        flags=reader_flags,
                        stack_size=stack_size)

        st = reader.start(input_stream, base_uri)
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
        model_flags: ModelFlags = ModelFlags(0),
        default_graph: Node = None,
        stack_size: int = 4096,
    ) -> Model:
        """Load a model from a string and return it."""
        env = Env(self, base_uri)
        model = Model(self, StatementOrder.SPO, model_flags)
        inserter = model.inserter(env, default_graph)
        input_stream = StringInput(s)
        reader = Reader(self,
                        syntax,
                        env,
                        inserter,
                        flags=reader_flags,
                        stack_size=stack_size)

        st = reader.start(input_stream)
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
        describe_flags: DescribeFlags = DescribeFlags(0),
        env: Env = None,
    ) -> None:
        """Write a model to a file."""

        if env is None:
            env = Env(self, file_uri(path))

        output_stream = FileOutput(filename=path)
        writer = Writer(self, syntax, env, output_stream, flags=writer_flags)

        st = env.write_prefixes(writer.sink())
        if st == Status.SUCCESS:
            st = model.all().write(writer.sink(), describe_flags)

        writer.finish()
        output_stream.close()
        _ensure_success(st, "Failed to dump model to file")

    def dumps(
        self,
        model: Model,
        syntax: Syntax = Syntax.TURTLE,
        writer_flags: WriterFlags = WriterFlags(0),
        describe_flags: DescribeFlags = DescribeFlags(0),
        env: Env = None,
    ) -> str:
        """Write a model to a string and return it."""

        if env is None:
            env = Env(self)

        output_stream = StringOutput()
        writer = Writer(self, syntax, env, output_stream, flags=writer_flags)
        st = model.all().write(writer.sink(), describe_flags)
        writer.finish()

        _ensure_success(st, "Failed to dump model to string")

        output_stream.close()
        return output_stream.output()


cdef class Node:
    """An RDF node."""

    cdef SerdNode* _ptr

    @staticmethod
    cdef Node _manage(SerdNode* ptr):
        if ptr is NULL:
            return None

        cdef Node wrapper = Node.__new__(Node, _WrapSentinel())
        wrapper._ptr = ptr
        return wrapper

    @staticmethod
    cdef Node _wrap(const SerdNode* ptr):
        if ptr is NULL:
            return None

        cdef SerdNode* copy = serd_node_copy(NULL, ptr)
        if not copy:
            return None

        cdef Node wrapper = Node.__new__(Node, _WrapSentinel())
        wrapper._ptr = copy
        return wrapper

    @staticmethod
    def from_syntax(string: str,
                    syntax: Syntax = Syntax.TURTLE,
                    env: Env = None):
        """Return a new node created from a string.

        The string must be a single node in the given syntax, as returned by
        :meth:`serd.Node.to_syntax`.
        """

        cenv = env._ptr if env is not None else NULL
        return Node._manage(serd_node_from_syntax(NULL,
                                                  _tocstr(string),
                                                  Syntax.TURTLE,
                                                  cenv))

    def __cinit__(self, v):
        if isinstance(v, _WrapSentinel):
            self._ptr = NULL
            return # Call from _wrap or _manage via __new__

        if isinstance(v, str):
            value_view = _string_view(v)
            self._ptr = serd_new_string(NULL, value_view)
        elif isinstance(v, bool):
            self._ptr = serd_new_value(NULL, _value(v))
        elif isinstance(v, int):
            if v < -9223372036854775808 or v > 9223372036854775807:
                int_string = str(v)
                self._ptr = serd_new_literal(NULL,
                                             _string_view(int_string),
                                             <SerdNodeFlags>2, # SERD_HAS_DATATYPE
                                             _string_view("http://www.w3.org/2001/XMLSchema#integer"))
            else:
                self._ptr = serd_new_integer(NULL, v)
        else:
            self._ptr = serd_new_value(NULL, _value(v))

        assert self._ptr

    def __dealloc__(self):
        if self._ptr is not NULL:
            serd_node_free(NULL, self._ptr)
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
        if self.type() == NodeType.BLANK:
            return 'serd.blank("{}")'.format(self)
        if self.type() == NodeType.VARIABLE:
            return 'serd.variable("{}")'.format(self)

        raise NotImplementedError("Unknown node type {}".format(self.type()))

    def __len__(self):
        if self._ptr:
            return serd_node_length(self._ptr)

        return 0

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
        assert self._ptr
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

    def to_syntax(self,
                  syntax: Syntax = Syntax.TURTLE,
                  env: Env = None) -> str:
        """Return a string representation of this node in a syntax.

        The returned string represents that node as if written as an object in
        the given syntax, without any extra quoting or punctuation.  The syntax
        should be either TURTLE or NTRIPLES (the others are redundant).  Note
        that namespaced (CURIE) nodes and relative URIs can not be expressed in
        NTriples.

        Passing the returned string to Node.from_syntax() will produce a node
        equivalent to this one.
        """

        cenv = env._ptr if env is not None else NULL
        cstr = serd_node_to_syntax(NULL, self._ptr, syntax, cenv)

        result = _fromcstr(cstr)
        serd_free(NULL, cstr)
        return result


# Node constructors


def string(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_string(NULL, s_view))


def plain_literal(s: str, lang: str = None) -> Node:
    s_view = _string_view(s)
    if lang is not None:
        s_view = _string_view(s)
        lang_view = _string_view(lang)
        return Node._manage(serd_new_literal(NULL, s_view, NodeFlag.HAS_LANGUAGE, lang_view))
    else:
        s_view = _string_view(s)
        return Node._manage(serd_new_string(NULL, s_view))


def typed_literal(s: str, datatype) -> Node:
    s_view = _string_view(s)
    datatype_node = _uri_from_param(datatype)
    if type(datatype_node) == Node:
        datatype_uri_view = datatype_node.string_view()
        return Node._manage(serd_new_literal(NULL, s_view, NodeFlag.HAS_DATATYPE, datatype_uri_view))

    return None


def blank(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_token(NULL, NodeType.BLANK, s_view))


def uri(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_token(NULL, NodeType.URI, s_view))


def file_uri(path: str, hostname: str = "") -> Node:
    path_view = _string_view(path)
    hostname_view = _string_view(hostname)
    return Node._manage(serd_new_file_uri(NULL, path_view, hostname_view))


def decimal(
    d: float,
) -> Node:
    return Node._manage(serd_new_decimal(NULL, d))


def double(d: float) -> Node:
    return Node._manage(serd_new_value(NULL, _value(d)))


def integer(i: int) -> Node:
    return Node._manage(serd_new_integer(NULL, i))


def boolean(b: bool) -> Node:
    return Node._manage(serd_new_value(NULL, _value(b)))


def base64(const unsigned char[:] buf) -> Node:
    return Node._manage(serd_new_base64(NULL, &buf[0], len(buf)))


def variable(s: str) -> Node:
    s_view = _string_view(s)
    return Node._manage(serd_new_token(NULL, NodeType.VARIABLE, s_view))


cdef class Env:

    """Lexical environment for abbreviating and expanding URIs."""

    cdef SerdEnv* _ptr
    cdef World _world

    def __init__(self, world: World, arg=None):
        assert world is not None
        assert type(world) == World

        self._world = world

        if arg is None:
            self._ptr = serd_env_new(world._ptr, _empty_string)
        elif type(arg) == Env:
            self._ptr = serd_env_copy(serd_world_allocator(world._ptr),
                                      (<Env>arg)._ptr)
        elif type(arg) == Node:
            arg_view = arg.string_view()
            self._ptr = serd_env_new(world._ptr, arg_view)
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

    def expand(self, node: Node) -> Node:
        """Expand `node`, transforming CURIEs into URIs

        If `node` is a relative URI reference, it is expanded to a full URI if
        possible.  If `node` is a literal, its datatype is expanded if
        necessary.  If `node` is a CURIE, it is expanded to a full URI if
        possible.

        Returns None if `node` can not be expanded.
        """
        return Node._manage(serd_env_expand_node(self._ptr, node._ptr))

    def write_prefixes(self, sink: SinkBase) -> Status:
        """Write all prefixes to `sink`."""
        return Status(serd_env_write_prefixes(self._ptr, sink._cptr))


class ReadContext(object):
    """Context manager for a scoped read."""

    def __init__(self, reader, source):
        self.reader = reader
        self.source = source

    def __enter__(self):
        _ensure_success(self.reader.start(self.source),
                        "Failed to start reading")
        return self

    def __exit__(self, exc_type, exc_value, exc_tb) -> None:
        _ensure_success(self.reader.finish(), "Failed to finish reading")

    def read_chunk(self) -> None:
        """Read a single "chunk" of data during an incremental read.

        This function will read a single top level description, and return.
        This may be a directive, statement, or several statements; essentially
        it reads until a '.' is encountered.  This is particularly useful for
        reading directly from a pipe or socket.
        """

        _ensure_success(self.reader.read_chunk(), "Failed to read chunk")

    def read_document(self) -> None:
        """Read a complete document from the source.

        This function will continue pulling from the source until a complete
        document has been read.  Note that this may block when used with
        streams, for incremental reading use serd_reader_read_chunk().
        """

        _ensure_success(self.reader.read_document(), "Failed to read document")


cdef class Reader:
    """Streaming parser that reads a text stream and writes to a sink.

    .. py:function:: serd.Reader(world: serd.World, syntax: serd.Syntax, env: serd.Env, sink, flags: serd.ReaderFlags = serd.ReaderFlags(0), stack_size: int = 4096)

       Construct a new reader.

       The `sink` can be either a :class:`serd.Sink`, a built-in sink (for
       example, from :meth:`serd.Writer.sink()` or :meth:`serd.Model.inserter`),
       or a function that takes a :class:`serd.Event` and returns a
       :class:`serd.Status`.
    """

    cdef SerdReader* _ptr
    cdef SinkBase    _sink
    cdef object      _callback

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
                 env: Env,
                 sink,
                 flags: ReaderFlags = ReaderFlags(0),
                 stack_size: int = 4096):
        if isinstance(sink, SinkBase):
            self._sink = sink
        else:
            self._callback = sink
            self._sink = Sink(world, func=self._callback)

        assert isinstance(self._sink, SinkBase)
        assert self._sink._cptr is not NULL

        self._ptr = serd_reader_new(world._ptr,
                                    syntax,
                                    flags,
                                    env._ptr,
                                    (<SinkBase>self._sink)._cptr,
                                    stack_size)

    def __dealloc__(self):
        serd_reader_free(self._ptr)
        self._ptr = NULL

    def start(self,
              input_stream: InputStream,
              input_name: Node = None,
              block_size: int = 1) -> Status:
        """Prepare to read from an input stream."""
        return Status(serd_reader_start(
            self._ptr,
            &input_stream._stream,
            input_name._ptr if input_name is not None else NULL,
            block_size))

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

    def open(self, input_stream) -> ReadContext:
        """Return a scoped read context."""

        return ReadContext(self, input_stream)


cdef class InputStream:
    """A source for bytes that provides text input.

    This is only a base class, use StringInput or FileInput instead.
    """

    cdef SerdInputStream _stream

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb) -> None:
        self.close()

    def __dealloc__(self):
        self.close()
        self._stream.stream = NULL

    def close(self):
        if self._stream.stream:
            serd_close_input(&self._stream)
            self._stream.stream = NULL


cdef class StringInput(InputStream):
    """A byte source for text input that reads from a string."""

    cdef const char* _position
    cdef bytes       _bytes

    def __init__(self, string: str):
        super().__init__()

        self._bytes    = string.encode('utf-8')
        self._position = self._bytes
        self._stream   = serd_open_input_string(&self._position)


cdef class FileInput(InputStream):
    """A byte source for text input that reads from a file."""

    def __init__(self, filename: str):
        super().__init__()
        self._stream = serd_open_input_file(_tocstr(filename))


# cdef class ByteSink:
#     """A sink for bytes that receives text output."""
#     cdef SerdByteSink* _ptr

#     def __dealloc__(self):
#         serd_byte_sink_free(self._ptr)
#         self._ptr = NULL

#     def flush(self) -> None:
#         """Flush any pending output to the underlying stream."""
#         serd_byte_sink_flush(self._ptr)

#     def close(self) -> None:
#         """Close sink, including the underlying file if necessary."""
#         serd_byte_sink_close(self._ptr)


# cdef class FileSink(ByteSink):
#     """A sink for bytes that writes text output to a file."""

#     def __init__(self,
#                  filename: str,
#                  block_size: int = 4096):
#         super().__init__()

#         self._ptr = serd_byte_sink_new_filename(_tocstr(filename),
#                                                 block_size)

#         if self._ptr is NULL:
#             raise OSError(errno, strerror(errno), filename)


# cdef class StringSink(ByteSink):
#     cdef SerdBuffer _buffer

#     def __dealloc__(self):
#         serd_free(NULL, self._buffer.buf)
#         self._buffer.buf = NULL
#         self._buffer.len = 0
#         # super().__dealloc__(self)

#     def __init__(self):
#         super().__init__()

#         self._buffer.buf = NULL
#         self._buffer.len = 0
#         self._ptr = serd_byte_sink_new_buffer(&self._buffer)

#     def output(self) -> str:
#         """Finish writing to this string sink and return the output."""
#         self.flush()
#         self.close()
#         return _fromcstr(<char*>self._buffer.buf)


cdef class OutputStream:
    """An output stream that receives bytes.

    This is only a base class, use StringOutput or FileOutput instead.
    """

    cdef SerdOutputStream _stream

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb) -> None:
        self.close()

    def __dealloc__(self):
        self.close()

    def close(self):
        if self._stream.stream:
            serd_close_output(&self._stream)
            self._stream.stream = NULL


cdef class StringOutput(OutputStream):
    cdef SerdBuffer _buffer
    cdef str        _output

    """An output stream that writes to a string."""
    def __init__(self):
        super().__init__()

        self._stream = serd_open_output_buffer(&self._buffer)
        self._output = None

    def __cinit__(self):
        self._buffer.allocator = NULL
        self._buffer.buf = NULL
        self._buffer.len = 0

    def close(self):
        if self._output is None:
            super().close()
            self._output = _fromcstr(<const char*>self._buffer.buf)
            serd_free(self._buffer.allocator, self._buffer.buf)
            self._buffer.buf = NULL
            self._buffer.len = 0

    def output(self) -> str:
        return self._output


cdef class FileOutput(OutputStream):
    """An output stream that writes to a file."""
    def __init__(self, filename: str):
        super().__init__()
        self._stream = serd_open_output_file(_tocstr(filename))


class _WriteContext(object):
    """Context manager for a scoped write."""

    def __init__(self, writer, stream):
        self.writer = writer
        self.stream = stream

    def __enter__(self):
        self.stream.__enter__()
        return self.writer.sink()

    def __exit__(self, exc_type, exc_value, exc_tb) -> None:
        _ensure_success(self.writer.finish(), "Failed to finish write")
        self.stream.__exit__(exc_type, exc_value, exc_tb)


cdef class Writer:
    """Streaming writer that emits text as it receives events."""

    cdef SerdWriter* _ptr
    cdef World _world

    def __init__(self,
                 world: World,
                 syntax: Syntax,
                 env: Env,
                 output_stream: OutputStream,
                 flags: WriterFlags = WriterFlags(0),
                 block_size: int = 1):
        self._world = world
        self._ptr = serd_writer_new(
            world._ptr,
            syntax,
            flags,
            env._ptr if env is not None else NULL,
            &output_stream._stream,
            block_size
        )

    def __dealloc__(self):
        serd_writer_free(self._ptr)
        self._ptr = NULL

    def sink(self) -> SinkView:
        """Return a sink interface that emits statements via this writer."""
        return SinkView._wrap(serd_writer_sink(self._ptr))

    def set_base_uri(self, uri: Node) -> Status:
        """Set the current output base URI, and emit a directive if applicable.
        """
        return Status(serd_writer_set_base_uri(self._ptr, uri._ptr))

    def set_root_uri(self, uri: str) -> Status:
        """Set the current root URI.

        The root URI should be a prefix of the base URI.  The path of the root
        URI is the highest path any relative up-reference can refer to.  For
        example, with root <file:///foo/root> and base <file:///foo/root/base>,
        <file:///foo/root> will be written as <../>, but <file:///foo> will be
        written non-relatively as <file:///foo>.  If the root is not explicitly
        set, it defaults to the base URI, so no up-references will be created
        at all.
        """
        return Status(serd_writer_set_root_uri(self._ptr, _string_view(uri)))

    def finish(self) -> Status:
        """Finish a write.

        This flushes any pending output, for example terminating punctuation,
        so that the output is a complete document.
        """
        return Status(serd_writer_finish(self._ptr))


class SerdError(RuntimeError):
    """An exception thrown by serd."""

    def __init__(self, status: Status, message: str):
        super().__init__("%s (%s)" % (message, strerror(status)))

        self.status = status


def _ensure_success(status: Status, message: str):
    if status != Status.SUCCESS:
        raise SerdError(status, message)


@cython.no_gc
cdef class Model:
    """An indexed set of statements."""
    cdef SerdModel* _ptr
    cdef World _world

    def __cinit__(self,
                  world: World,
                  default_order: StatementOrder = StatementOrder.SPO,
                  flags: ModelFlags = ModelFlags(0),
                  model: Model = None):
        if model is not None:
            self._world = world
            self._ptr = serd_model_copy(NULL, model._ptr)
        else:
            assert type(default_order) == StatementOrder
            assert type(flags) == ModelFlags or type(flags) == int
            self._world = world
            self._ptr = serd_model_new(world._ptr, default_order, flags)
            # FIXME: ?
            serd_model_add_index(self._ptr, StatementOrder.OPS)
            serd_model_add_index(self._ptr, StatementOrder.GSPO)
            serd_model_add_index(self._ptr, StatementOrder.GOPS)
        # elif type(flags) == ModelFlags:
        #     self._world = world
        #     self._ptr = serd_model_new(world._ptr, default_order, flags)
        # else:
        #     raise TypeError("Bad arguments for Model()")

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
            return self._end()

        return Cursor._manage(serd_model_begin(self._ptr))

    def __contains__(self, statement):
        return self._find(Statement._from_param(statement)) != self._end()

    def __delitem__(self, statement):
        i = self._find(statement)
        if i is not None:
            self.erase(i)

    def __add__(self: Model, statement_param):
        statement = Statement._from_param(statement_param)
        status = serd_model_insert(self._ptr, (<Statement>statement)._ptr)
        _ensure_success(status, "Failed to insert statement")
        return self

    def world(self) -> World:
        """Get the world associated with this model."""
        return self._world

    def clear(self) -> None:
        """Remove everything from this model."""
        return serd_model_clear(self._ptr)

    def copy(self) -> Model:
        """Return a deep copy of this model."""
        return Model(self._world, self.default_order(), self.flags(), self)

    def default_order(self) -> StatementOrder:
        """Get the default statement order of this model."""
        return StatementOrder(serd_model_default_order(self._ptr))

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
        sink = Sink._manage(serd_inserter_new(
            self._ptr, _unwrap_node(default_graph)
        ))

        sink._parent = self
        return sink

    def insert(self, arg) -> None:
        """Insert a statement into this model."""
        if type(arg) == Cursor:
            return Status(serd_model_insert(
                self._ptr, serd_cursor_get((<Cursor>arg)._ptr)))

        statement = Statement._from_param(arg)
        st = serd_model_insert(self._ptr, (<Statement>statement)._ptr)
        _ensure_success(st, "Failed to insert statement")

    def insert_statements(self, range: Cursor) -> None:
        """Insert a range of statements into this model."""
        st = serd_model_insert_statements(self._ptr, range._ptr)
        if st != Status.SUCCESS and st != Status.FAILURE:
            raise SerdError(st, "Failed to insert statement")

    def erase(self, arg) -> Status:
        """Erase a statement from the model.

        The argument can be a statement, tuple of nodes, or a cursor.
        """

        if type(arg) == Cursor:
            # TODO: Check for end
            _ensure_success(
                serd_model_erase(self._ptr, (<Cursor>arg)._ptr),
                "Failed to erase range")
        elif type(arg) == Statement:
            i = self._find(arg)
            if i == self._end():
                raise ValueError("serd.Model.erase(): statement not in model")

            self.erase(i)
        elif type(arg) == tuple:
            self.erase(Statement._from_param(arg))
        else:
            raise TypeError("Bad argument type for Model.erase: %s" % type(arg))

    def erase_statements(self, cursor: Cursor) -> Status:
        """Erase a range of statements from the model."""
        _ensure_success(
            serd_model_erase_statements(self._ptr, cursor._ptr),
            "Failed to erase range")

    # def begin(self) -> _Iter:
    #     return _Iter._manage(serd_model_begin(self._ptr))

    def _end(self) -> Cursor:
        return Cursor._wrap(serd_model_end(self._ptr))

    def all(self) -> Cursor:
        """Return a range of all statements in the model in SPO order."""
        return Cursor._manage(serd_model_begin(self._ptr))

    def ordered(self, order: StatementOrder) -> Cursor:
        """Return a range of all statements in the model in a given order."""
        return Cursor._manage(serd_model_begin_ordered(self._ptr, order))

    # FIXME: ?
    def _find(self, statement) -> Cursor:
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

        return Cursor._manage(c_iter) if c_iter else self._end()

    def find(self,
             subject: Node = None,
             predicate: Node = None,
             object: Node = None,
             graph: Node = None) -> Cursor:
        """Search for statements that match a pattern.

        Returns a cursor that points to the first match, or the end if no
        matches were found.
        """

        return Cursor._manage(
            serd_model_find(
                self._ptr,
                _unwrap_node(subject),
                _unwrap_node(predicate),
                _unwrap_node(object),
                _unwrap_node(graph)
            )
        )

    def get(self,
            subject: Node = None,
            predicate: Node = None,
            object: Node = None,
            graph: Node = None) -> Node:
        """Search for a single node that matches a pattern.

        Exactly one of ``subject``, ``predicate``, or ``object`` must be
        ``None``.  This function is mainly useful for predicates that only have
        one value.

        Returns the first matching node, or ``None`` if no matches are found.
        """

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

    .. py:function:: serd.serd.Statement(subject: serd.Node, predicate: serd.Node, object: serd.Node, graph: serd.Node = None, caret: serd.Caret = None)

       Construct a new statement.
    """

    cdef SerdStatement* _ptr
    cdef Node           _subject
    cdef Node           _predicate
    cdef Node           _object
    cdef Node           _graph
    cdef Caret         _caret

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
        wrapper._caret = Caret._wrap(serd_statement_caret(ptr))
        wrapper._ptr = serd_statement_new(
            NULL,
            _unwrap_node(wrapper._subject),
            _unwrap_node(wrapper._predicate),
            _unwrap_node(wrapper._object),
            _unwrap_node(wrapper._graph),
            (<Caret>wrapper._caret)._ptr if wrapper._caret is not None else NULL)

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
                    raise TypeError("Bad type for statement field %d" % i)

            g = obj[3] if len(obj) == 4 else None
            return Statement(obj[0], obj[1], obj[2], g)

        raise TypeError("Bad argument type for Statement: %s" % type(obj))

    def __init__(
            self,
            subject: Node,
            predicate: Node,
            object: Node,
            graph: Node = None,
            caret: Caret = None,
    ):
        self._subject = <Node>subject
        self._predicate = <Node>predicate
        self._object = <Node>object
        self._graph = <Node>graph
        self._caret = <Caret>caret

        self._ptr = serd_statement_new(
            NULL,
            _unwrap_node(self._subject),
            _unwrap_node(self._predicate),
            _unwrap_node(self._object),
            _unwrap_node(self._graph),
            (<Caret>self._caret)._ptr if self._caret is not None else NULL,
        )

    def __dealloc__(self):
        serd_statement_free(NULL, self._ptr)
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

        if serd_statement_caret(self._ptr):
            args += [repr(self.caret())]

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

    def caret(self) -> Caret:
        """Return the file location this statement came from, or None."""
        return Caret._wrap(serd_statement_caret(self._ptr))


cdef class Cursor:
    """A range of statements in a model.

    This class is iterable so it can be used like a collection.  For example,
    :meth:`serd.Model.all()` returns a range, so all the statements in a model
    can be printed like so::

        for statement in model.all():
            print(statement)

    A range is "truthy" if it is non-empty.
    """
    cdef SerdCursor* _ptr

    @staticmethod
    cdef _manage(SerdCursor* ptr):
        if ptr is NULL:
            return None

        cdef Cursor wrapper = Cursor.__new__(Cursor)
        wrapper._ptr = ptr
        return wrapper

    @staticmethod
    cdef _wrap(const SerdCursor* ptr):
        return Cursor._manage(serd_cursor_copy(NULL, ptr))

    def __init__(self, range: Cursor):
        assert type(range) == Cursor
        self._ptr = serd_cursor_copy(NULL, (<Cursor>range)._ptr)

    def __dealloc__(self):
        serd_cursor_free(self._ptr)
        self._ptr = NULL

    def __bool__(self):
        return not self.empty()

    def __eq__(self, rhs):
        return type(rhs) == Cursor and serd_cursor_equals(self._ptr, (<Cursor>rhs)._ptr)

    def __iter__(self):
        return self
        # if self.empty():
        #     return Cursor._end()

        # return _Iter._wrap(serd_cursor_begin(self._ptr))

    def __next__(self):
        """Move to and return the next item."""

        if serd_cursor_is_end(self._ptr):
            raise StopIteration

        item = serd_cursor_get(self._ptr)

        # status = serd_cursor_advance(self._ptr)
        # if status > 1:
        #     raise StopIteration

        # # if status != 0:
        # #     print("STOP ITERATION: {}".format(status))
        # #     raise StopIteration
        # # else:
        # #     print("GOOD ITERATION")

        # item = serd_cursor_get(self._ptr)
        serd_cursor_advance(self._ptr)

        return Statement._wrap(item)

    # def front(self) -> Statement:
    #     """Return the first statement in this range, or None."""
    #     return Statement._wrap(serd_cursor_front(self._ptr))

    def empty(self) -> bool:
        """Return true iff there are no statements in this range."""
        return serd_cursor_is_end(self._ptr)

    def write(self,
              sink: SinkBase,
              flags: DescribeFlags = DescribeFlags(0)) -> Status:
        """Write this range to `sink`.

        The serialisation style can be controlled with `flags`.  The default is
        to write statements in an order suited for pretty-printing with Turtle
        or TriG with as many objects written inline as possible.  If
        `DescribeFlags.NO_INLINE_OBJECTS` is given, a simple sorted stream is
        written instead, which is significantly faster since no searching is
        required, but can result in ugly output for Turtle or Trig.
        """
        return Status(serd_describe_range(self._ptr, sink._cptr, flags))


cdef class Caret:
    """The origin of a statement in a document."""
    cdef SerdCaret* _ptr
    cdef Node       _name_node

    @staticmethod
    cdef Caret _wrap(const SerdCaret* ptr):
        if ptr is NULL:
            return None

        name_node = Node._wrap(serd_caret_name(ptr))

        cdef SerdCaret* copy = serd_caret_new(NULL,
                                              name_node._ptr,
                                              serd_caret_line(ptr),
                                              serd_caret_column(ptr))

        cdef Caret wrapper = Caret.__new__(Caret)
        wrapper._ptr = copy
        wrapper._name_node = name_node
        return wrapper

    def __init__(self, name, line: uint = 1, col: uint = 0):
        if type(name) == Node:
            self._name_node = name
            self._ptr = serd_caret_new(NULL, self._name_node._ptr, line, col)
        elif type(name) == str:
            self._name_node = string(name)
            self._ptr = serd_caret_new(NULL, self._name_node._ptr, line, col)
        else:
            raise TypeError("Bad name argument type for Caret(): %s" % type(name))

    def __dealloc__(self):
        serd_caret_free(NULL, self._ptr)
        self._ptr = NULL
        self._name_node = None

    def __eq__(self, rhs):
        return (type(rhs) == Caret and
                serd_caret_equals(self._ptr, (<Caret>rhs)._ptr))

    def __str__(self):
        return "{}:{}:{}".format(self._name_node, self.line(), self.column())

    def __repr__(self):
        return "serd.Caret({})".format(
            ", ".join([repr(self._name_node), str(self.line()), str(self.column())]))

    def name(self) -> Node:
        """Return the document name.

        This is typically a file URI, but may be a descriptive string node for
        statements that originate from strings or streams.
        """

        return self._name_node

    def line(self) -> int:
        """Return the one-relative line number in the document."""
        return serd_caret_line(self._ptr)

    def column(self) -> int:
        """Return the zero-relative column number in the line."""
        return serd_caret_column(self._ptr)


cdef class Event:
    """An event in a data stream.

    Streams of data are represented as a series of events.  Events represent
    everything that can occur in an RDF document, and are used to plumb
    together different components.  For example, when parsing a document, a
    reader emits a stream of events which can be sent to a writer to rewrite
    the document, or to an inserter to build a model in memory.
    """

    cdef SerdEvent _event
    cdef Node      _name
    cdef Node      _uri
    cdef Statement _statement
    cdef Node      _node

    @staticmethod
    cdef Event _wrap(const SerdEvent* ptr):
        event = Event()

        if ptr.type == EventType.BASE:
            base_event = <const SerdBaseEvent*>ptr
            event._uri = Node._wrap(base_event.uri)
            event._event.base.type = EventType.BASE
            event._event.base.uri = event._uri._ptr
        elif ptr.type == EventType.PREFIX:
            prefix_event = <const SerdPrefixEvent*>ptr
            event._name = Node._wrap(prefix_event.name)
            event._uri = Node._wrap(prefix_event.uri)
            event._event.prefix.type = EventType.PREFIX
            event._event.prefix.name = event._name._ptr
            event._event.prefix.uri = event._uri._ptr
        elif ptr.type == EventType.STATEMENT:
            statement_event = <const SerdStatementEvent*>ptr
            event._statement = Statement._wrap(statement_event.statement)
            event._event.statement.type = EventType.STATEMENT
            event._event.statement.flags = statement_event.flags
            event._event.statement.statement = event._statement._ptr
        elif ptr.type == EventType.END:
            end_event = <const SerdEndEvent*>ptr
            event._node = Node._wrap(end_event.node)
            event._event.end.type = EventType.END
            event._event.end.node = event._node._ptr
        else:
            return None

        return event

    def __eq__(self, rhs):
        if type(rhs) != Event:
            return False

        other = <Event>rhs
        if self._event.type != other._event.type:
            return False
        elif self._event.type == EventType.BASE:
            return self._uri == other._uri
        elif self._event.type == EventType.PREFIX:
            return (self._name, self._uri) == (other._name, other._uri)
        elif self._event.type == EventType.STATEMENT:
            return ((self._statement, self._event.statement.flags)
                    == (other._statement, other._event.statement.flags))
        elif self._event.type == EventType.END:
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

        if self._event.type == EventType.BASE:
            return 'serd.base_event("%s")' % self._uri
        elif self._event.type == EventType.PREFIX:
            return 'serd.prefix_event("%s", "%s")' % (self._name, self._uri)
        elif self._event.type == EventType.STATEMENT:
            result = 'serd.statement_event(%s' % repr(self._statement)
            if self._event.statement.flags:
                result += ', %s' % flags_repr(self._event.statement.flags)

            return result + ')'
        elif self._event.type == EventType.END:
            return 'serd.end_event(%s)' % repr(self._node)

        return "None"

    def type(self) -> EventType:
        return EventType(self._event.type)

    def name(self) -> Node:
        assert self.type() == EventType.PREFIX
        return self._name

    def uri(self) -> Node:
        assert self.type() in [EventType.BASE, EventType.PREFIX]
        return self._uri

    def flags(self) -> StatementFlags:
        assert self.type() == EventType.STATEMENT
        return StatementFlags(self._event.statement.flags)

    def statement(self) -> StatementFlags:
        assert self.type() == EventType.STATEMENT
        return self._statement

    def node(self) -> Node:
        assert self.type() == EventType.END
        return self._node


def base_event(base_uri):
    """Return an event that sets the base URI."""
    event = Event()
    event._uri = _uri_from_param(base_uri)
    event._event.base.type = EventType.BASE
    event._event.base.uri = event._uri._ptr
    return event


def prefix_event(name, namespace_uri):
    """Return an event that sets a namespace prefix."""
    event = Event()
    event._name = string(name)
    event._uri = _uri_from_param(namespace_uri)
    event._event.prefix.type = EventType.PREFIX
    event._event.prefix.name = event._name._ptr
    event._event.prefix.uri = event._uri._ptr
    return event


def statement_event(statement, flags: StatementFlags = StatementFlags(0)):
    """Return an event that represents a statement."""
    event = Event()
    event._statement = Statement._from_param(statement)
    event._event.statement.type = EventType.STATEMENT
    event._event.statement.flags = flags
    event._event.statement.statement = event._statement._ptr
    return event


def end_event(node):
    """Return an event that ends an anonymous node description."""
    event = Event()
    event._node = _blank_from_param(node)
    event._event.end.type = EventType.END
    event._event.end.node = event._node._ptr
    return event


cdef class SinkBase:
    """Base class for any Sink (not for direct use)."""

    cdef const SerdSink* _cptr


cdef class SinkView(SinkBase):
    @staticmethod
    cdef SinkView _wrap(const SerdSink* cptr):
        if cptr is NULL:
            return None

        cdef SinkView wrapper = SinkView.__new__(SinkView)
        wrapper._cptr = cptr
        return wrapper

    def write_event(self, event: Event) -> Status:
        """Send an event to the sink."""
        return Status(serd_sink_write_event(self._cptr, &event._event))

    def write_base(self, uri) -> Status:
        """Set the base URI."""
        uri_node = <Node>_uri_from_param(uri)
        return Status(serd_sink_write_base(self._cptr, uri_node._ptr))

    def write_prefix(self, name, uri) -> Status:
        """Set a namespace prefix."""
        if type(name) == str:
            name_node = string(name)
        elif type(name) == Node:
            name_node = name
        else:
            raise TypeError("Bad name type for write_prefix(): %s" % type(name))

        uri_node = <Node>_uri_from_param(uri)
        return Status(serd_sink_write_prefix(self._cptr,
                                             (<Node>name_node)._ptr,
                                             uri_node._ptr))

    def write_statement(self, statement, flags: StatementFlags=StatementFlags(0)) -> Status:
        """Write a statement."""
        s = <Statement>Statement._from_param(statement)
        return Status(serd_sink_write_statement(self._cptr,
                                                <SerdStatementFlags>flags,
                                                s._ptr))


cdef class Sink(SinkBase):
    cdef SerdSink* _ptr
    cdef object    _parent
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

    def __init__(self: Sink, world: World, func: callable = None):
        if func is not None:
            self._parent = world
            self._env = Env(world)
            self._func = func
            self._ptr = serd_sink_new(world._ptr, <void*>self, Sink._c_on_event, NULL)
            self._cptr = self._ptr
            # TODO: get_env?
        else:
            self._parent = world
            self._env = Env(world)
            self._func = None
            self._ptr = serd_sink_new(world._ptr, <void*>self, Sink._c_on_event, NULL)
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
