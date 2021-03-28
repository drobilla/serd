##########
Using Serd
##########

.. default-domain:: c
.. highlight:: c

The serd API is declared in ``serd.h``:

.. code-block:: c

   #include <serd/serd.h>

Several types of object are available that can be used together in various ways.

The :doc:`api/serd_world` represents an instance of serd,
and is used to manage "global" facilities like logging.

A :doc:`api/serd_node` is the fundamental unit of data,
and 3 or 4 nodes make a :doc:`api/serd_statement`.

Reading and writing data is performed using a :doc:`api/serd_reader`,
which reads text and fires callbacks,
and a :doc:`api/serd_writer`,
which writes text when driven by corresponding functions.
Both work in a streaming fashion so that large documents can be pretty-printed,
translated,
or otherwise processed quickly using only a small amount of memory.

An :doc:`api/serd_env` represents a syntactic context,
that is, the base URI and set of namespace prefixes that are active at a particular point in a stream or document.
This is used to expand relative URIs and abbreviated nodes.

A :doc:`api/serd_model` represents an in-memory set of statements.
A model can be configured with various indices to provide good performance for various kinds of queries.
An :doc:`api/serd_iterator` points to a statement in a model,
and a :doc:`api/serd_range` points to a range of statements in a model.

A :doc:`api/serd_sink` is an interface that can receive a stream of data.
Several objects act as a sink,
for example,
a :doc:`api/serd_writer` is a sink that writes text in some syntax,
and an :doc:`api/serd_inserter` is a sink that inserts statements into a model.

Some objects serve as stream processors by acting as a sink and forwarding modified or filtered data to another sink.
A :doc:`api/serd_canon` converts literals to canonical form,
and a :doc:`api/serd_filter` filters statements that match (or do not match) some pattern.
For example, by piping a reader into a canon into a model,
a document can be loaded into a model with canonical literals.

Serd uses a "push" model, that is,
data is pushed to a sink explicitly and there is no "source" interface.
For example, instead of using a reader as above,
it is possible to generate data by creating nodes from strings and pushing statements built from them to some sink.

String Views
============

For performance reasons,
most functions in serd that take a string take a :struct:`SerdStringView`,
rather than a bare pointer.
This forces code to be explicit about string measurement,
which discourages common patterns of repeated measurement of the same string.
For convenience, several macros are provided for constructing string views:

:macro:`SERD_EMPTY_STRING`

   Constructs a view of an empty string, for example:

   .. literalinclude:: overview.c
      :start-after: begin make-empty-string
      :end-before: end make-empty-string
      :dedent: 2

:macro:`SERD_STATIC_STRING`

   Constructs a view of a string literal, for example:

   .. literalinclude:: overview.c
      :start-after: begin make-static-string
      :end-before: end make-static-string
      :dedent: 2

   Note that this measures its argument with ``sizeof``,
   so care must be taken to only use it with string literals,
   or the length may be incorrect.

:macro:`SERD_MEASURE_STRING`

   Constructs a view of a string by measuring it with ``strlen``,
   for example:

   .. literalinclude:: overview.c
      :start-after: begin measure-string
      :end-before: end measure-string
      :dedent: 2

   This can be used to make a view of any string.

:macro:`SERD_STRING_VIEW`

   Constructs a view of a slice of a string with an explicit length,
   for example:

   .. literalinclude:: overview.c
      :start-after: begin make-string-view
      :end-before: end make-string-view
      :dedent: 2

Typically,
these macros are used inline when passing parameters,
and so can be thought of as syntax for the different string types.

Nodes
=====

Nodes are the basic building blocks of data.
Nodes are essentially strings,
but also have a :enum:`SerdNodeType`,
and optionally either a datatype or a language.

In RDF, a node is either a literal, URI, or blank.
Serd can also represent "CURIE" nodes,
or shortened URIs,
which represent prefixed names often written in documents.

Fundamental Constructors
------------------------

There are five fundamental node constructors,
which can be used to create any node:

:func:`serd_new_plain_literal`
   Creates a new string literal with an optional language tag.

:func:`serd_new_typed_literal`
   Creates a new string literal with a datatype URI.

:func:`serd_new_blank`
   Creates a new blank node ID.

:func:`serd_new_curie`
   Creates a new shortened URI.

:func:`serd_new_uri`
   Creates a new URI.

Convenience Constructors
------------------------

For convenience,
many other constructors are also provided to make common types of nodes:

:func:`serd_new_simple_node`
   Creates a new simple blank, CURIE, or URI node.

:func:`serd_new_string`
   Creates a new string literal (with no datatype or language).

:func:`serd_new_parsed_uri`
   Creates a new URI from a parsed URI view.

:func:`serd_new_file_uri`
   Creates a new file URI from a path.

:func:`serd_new_boolean`
   Creates a new boolean literal.

:func:`serd_new_decimal`
   Creates a new decimal literal.

:func:`serd_new_double`
   Creates a new double literal.

:func:`serd_new_float`
   Creates a new float literal.

:func:`serd_new_integer`
   Creates a new integer literal.

:func:`serd_new_base64`
   Creates a new binary blob literal using xsd:base64Binary encoding.

The datatype or language, if present, can be retrieved with :func:`serd_node_datatype` or :func:`serd_node_language`, respectively.
Note that no node has both a datatype and a language.

Statements
==========

A :struct:`SerdStatement` is a tuple of either 3 or 4 nodes:
the `subject`, `predicate`, `object`, and optional `graph`.
Statements declare that a subject has some property.
The predicate identifies the property,
and the object is its value.

A statement is a bit like a very simple machine-readable sentence.
The subject and object are as in natural language,
and the predicate is like the verb, but more general.
For example, we could make a statement in English about your intrepid author:

   drobilla has the first name "David"

We can break this statement into 3 pieces like so:

.. list-table::
   :header-rows: 1

   * - Subject
     - Predicate
     - Object
   * - drobilla
     - has the first name
     - "David"

To make a :struct:`SerdStatement` out of this, we need to define some URIs.
In RDF, the subject and predicate must be *resources* with an identifier
(for example, neither can be a string).
Conventionally, predicate names do not start with "has" or similar words,
since that would be redundant in this context.
So, we assume that ``http://example.org/drobilla`` is the URI for drobilla,
and that ``http://example.org/firstName`` has been defined somewhere to be
a property with the appropriate meaning,
and can make an equivalent :struct:`SerdStatement`:

.. literalinclude:: overview.c
   :start-after: begin statement-new
   :end-before: end statement-new
   :dedent: 2

The last two fields are the graph and the cursor.
The graph is another node that can be used to group statements,
for example by the URI of the document they were loaded from.
The cursor represents the location in a document where the statement was loaded from, if applicable.

Accessing Fields
----------------

Statement fields can be accessed with
:func:`serd_statement_node`, for example:

.. literalinclude:: overview.c
   :start-after: begin get-subject
   :end-before: end get-subject
   :dedent: 2

Alternatively, an accessor function is provided for each field:

.. literalinclude:: overview.c
   :start-after: begin get-pog
   :end-before: end get-pog
   :dedent: 2

Every statement has a subject, predicate, and object,
but the graph may be null.
The cursor may also be null (as it would be in this case),
but if available it can be accessed with :func:`serd_statement_cursor`:

.. literalinclude:: overview.c
   :start-after: begin get-cursor
   :end-before: end get-cursor
   :dedent: 2

Comparison
----------

Two statements can be compared with :func:`serd_statement_equals`:

.. literalinclude:: overview.c
   :start-after: begin statement-equals
   :end-before: end statement-equals
   :dedent: 2

Statements are equal if all four corresponding pairs of nodes are equal.
The cursor is considered metadata, and is ignored for comparison.

It is also possible to match statements against a pattern using ``NULL`` as a wildcard,
with :func:`serd_statement_matches`:

.. literalinclude:: overview.c
   :start-after: begin statement-matches
   :end-before: end statement-matches
   :dedent: 2

Lifetime
--------

A statement only contains const references to nodes,
it does not own nodes or manage their lifetimes internally.
The cursor, however, is owned by the statement.
A statement can be copied with :func:`serd_statement_copy`:

.. literalinclude:: overview.c
   :start-after: begin statement-copy
   :end-before: end statement-copy
   :dedent: 2

The copied statement will refer to exactly the same nodes,
though the cursor will be deep copied.

In most cases, statements actually come from a reader or model,
and are managed by them,
but a statement owned by the application must be freed with :func:`serd_statement_free`:

.. literalinclude:: overview.c
   :start-after: begin statement-free
   :end-before: end statement-free
   :dedent: 2

World
=====

So far, we have only used nodes and statements,
which are simple independent objects.
Higher-level facilities in Serd require a :struct:`SerdWorld`,
which represents the global library state.

A program typically uses just one world,
which can be constructed using :func:`serd_world_new`:

.. literalinclude:: overview.c
   :start-after: begin world-new
   :end-before: end world-new
   :dedent: 2

All "global" library state is handled explicitly via the world.
Serd does not contain any static mutable data,
allowing it to be used concurrently in several parts of a program,
for example in plugins.

If multiple worlds *are* used in a single program,
they must never be mixed:
objects "inside" one world can not be used with objects inside another.

Note that the world is not a database,
it only manages a small amount of library state for things like configuration and logging.

Generating Blanks
-----------------

Blank nodes, or simply "blanks",
are used for resources that do not have URIs.
Unlike URIs, they are not global identifiers,
and only have meaning within their local context (for example, a document).
The world provides a method for automatically generating unique blank identifiers:

.. literalinclude:: overview.c
   :start-after: begin get-blank
   :end-before: end get-blank
   :dedent: 2

Note that the returned pointer is to a node that will be updated on the next call to :func:`serd_world_get_blank`,
so it is usually best to copy the node,
like in the example above.

Model
=====

A :struct:`SerdModel` is an indexed set of statements.
A model can be used to store any set of data,
from a few statements (for example, a protocol message),
to an entire document,
to a database with millions of statements.

A new model can be created with :func:`serd_model_new`:

.. literalinclude:: overview.c
   :start-after: begin model-new
   :end-before: end model-new
   :dedent: 2

Combinations of flags can be used to enable different indices,
or the storage of graphs and cursors.
For example, to be able to quickly search by predicate,
and store a cursor for each statement,
the flags :enumerator:`SERD_INDEX_PSO` and :enumerator:`SERD_STORE_CURSORS` could be added like so:

.. literalinclude:: overview.c
   :start-after: begin fancy-model-new
   :end-before: end fancy-model-new
   :dedent: 2

Model Operations
----------------

Models are value-like and can be copied with :func:`serd_model_copy` and compared with :func:`serd_model_equals`:

.. literalinclude:: overview.c
   :start-after: begin model-copy
   :end-before: end model-copy
   :dedent: 2

The number of statements in a model can be accessed with :func:`serd_model_size` and :func:`serd_model_empty`:

.. literalinclude:: overview.c
   :start-after: begin model-size
   :end-before: end model-size
   :dedent: 2

When a model is no longer needed, it can be destroyed with :func:`serd_model_free`:

.. literalinclude:: overview.c
   :start-after: begin model-free
   :end-before: end model-free
   :dedent: 2

Destroying a model invalidates all nodes and statements within that model,
so care should be taken to ensure that no dangling pointers are created.

Adding Statements
-----------------

Statements can be added to the model with :func:`serd_model_add`:

.. literalinclude:: overview.c
   :start-after: begin model-add
   :end-before: end model-add
   :dedent: 2

Alternatively, if you already have a statement (for example from another model),
:func:`serd_model_insert` can be used instead.
For example, the first statement in one model could be added to another like so:

.. literalinclude:: overview.c
   :start-after: begin model-insert
   :end-before: end model-insert
   :dedent: 2

An entire range of statements can be inserted at once with :func:`serd_model_add_range`.
For example, all statements in one model could be copied into another like so:

.. literalinclude:: overview.c
   :start-after: begin model-add-range
   :end-before: end model-add-range
   :dedent: 2

Iteration
---------

An iterator is a reference to a particular statement in a model.
:func:`serd_model_begin` returns an iterator to the first statement in the model,
and :func:`serd_model_end` returns a sentinel that is one past the last statement in the model:

.. literalinclude:: overview.c
   :start-after: begin model-begin-end
   :end-before: end model-begin-end
   :dedent: 2

An iterator can be advanced to the next statement with :func:`serd_iter_next`,
which returns true if the iterator has reached the end:

.. literalinclude:: overview.c
   :start-after: begin iter-next
   :end-before: end iter-next
   :dedent: 2

Iterators are dynamically allocated,
and must eventually be destroyed with :func:`serd_iter_free`:

.. literalinclude:: overview.c
   :start-after: begin iter-free
   :end-before: end iter-free
   :dedent: 2

Ranges
------

It is often more convenient to work with ranges of statements,
rather than iterators to individual statements.

The simplest range,
the range of all statements in the model,
is returned by :func:`serd_model_all`:

.. literalinclude:: overview.c
   :start-after: begin model-all
   :end-before: end model-all
   :dedent: 2

The order argument can be used to specify a particular order for statements,
which can be useful for optimizing certain algorithms.
In most cases, this function is simply used to scan the entire model,
so the default SPO (subject, predicate, object) order is appropriate,
and is always available.

It is possible to iterate over a range by advancing the begin iterator,
in much the same way as advancing an iterator:

.. literalinclude:: overview.c
   :start-after: begin range-next
   :end-before: end range-next
   :dedent: 2

Pattern Matching
----------------

There are several functions that can be used to quickly find statements in the model that match a pattern.
The simplest is :func:`serd_model_ask` which checks if there is any matching statement:

.. literalinclude:: overview.c
   :start-after: begin model-ask
   :end-before: end model-ask
   :dedent: 2

To access the unknown fields,
an iterator to the matching statement can be found with :func:`serd_model_find` instead:

.. literalinclude:: overview.c
   :start-after: begin model-find
   :end-before: end model-find
   :dedent: 2

Similar to :func:`serd_model_ask`,
:func:`serd_model_count` can be used to count the number of matching statements:

.. literalinclude:: overview.c
   :start-after: begin model-count
   :end-before: end model-count
   :dedent: 2

To iterate over the matching statements,
:func:`serd_model_range` can be used,
which returns a range that includes only statements that match the pattern:

.. literalinclude:: overview.c
   :start-after: begin model-range
   :end-before: end model-range
   :dedent: 2

Indexing
--------

A model can contain several indices that use different orderings to support different kinds of queries.
For good performance,
there should be an index where the least significant fields in the ordering correspond to wildcards in the pattern
(or, in other words, one where the most significant fields in the ordering correspond to nodes given in the pattern).
The table below lists the indices that best support a kind of pattern,
where a "?" represents a wildcard in the pattern.

+---------+--------------+
| Pattern | Good Indices |
+=========+==============+
| s p o   | Any          |
+---------+--------------+
| s p ?   | SPO, PSO     |
+---------+--------------+
| s ? o   | SOP, OSP     |
+---------+--------------+
| s ? ?   | SPO, SOP     |
+---------+--------------+
| ? p o   | POS, OPS     |
+---------+--------------+
| ? p ?   | POS, PSO     |
+---------+--------------+
| ? ? o   | OSP, OPS     |
+---------+--------------+
| ? ? ?   | Any          |
+---------+--------------+

If graphs are enabled,
then statements are indexed both with and without the graph fields,
so queries with and without a graph wildcard will have similar performance.

Since indices take up space and slow down insertion,
it is best to enable the fewest indices possible that cover the queries that will be performed.
For example,
an applications might enable just SPO and OPS order,
because they always search for specific subjects or objects,
but never for just a predicate without specifying any other field.

Getting Values
--------------

Sometimes you are only interested in a single node,
and it is cumbersome to first search for a statement and then get the node from it.
A more convenient way is to use :func:`serd_model_get`.
To get a value, specify a triple pattern where exactly one of the subject, predicate, and object is a wildcard.
If a statement matches, then the node that "fills" the wildcard will be returned:

.. literalinclude:: overview.c
   :start-after: begin model-get
   :end-before: end model-get
   :dedent: 2

If multiple statements match the pattern,
then the matching node from an arbitrary statement is returned.
It is an error to specify more than one wildcard, excluding the graph.

The similar :func:`serd_model_get_statement` instead returns the matching statement:

.. literalinclude:: overview.c
   :start-after: begin model-get-statement
   :end-before: end model-get-statement
   :dedent: 2

Erasing Statements
------------------

Individual statements can be erased with :func:`serd_model_erase`,
which takes an iterator:

.. literalinclude:: overview.c
   :start-after: begin model-erase
   :end-before: end model-erase
   :dedent: 2

The similar :func:`serd_model_erase_range` takes a range and erases all statements in the range:

.. literalinclude:: overview.c
   :start-after: begin model-erase-range
   :end-before: end model-erase-range
   :dedent: 2

Reading and Writing
===================

Reading and writing documents in a textual syntax is handled by the :struct:`SerdReader` and :struct:`SerdWriter`, respectively.
Serd is designed around a concept of event streams,
so the reader or writer can be at the beginning or end of a "pipeline" of stream processors.
This allows large documents to be processed quickly in an "online" fashion,
while requiring only a small constant amount of memory.
If you are familiar with XML,
this is roughly analogous to SAX.

A common simple setup is to simply connect a reader directly to a writer.
This can be used for things like pretty-printing,
or converting a document from one syntax to another.
This can be done by passing the sink returned by :func:`serd_writer_sink` to the reader constructor, :func:`serd_reader_new`.

First,
in order to write a document,
an environment needs to be created.
This defines the base URI and any namespace prefixes,
which is used to resolve any relative URIs or prefixed names,
and may be used to abbreviate the output.
In most cases, the base URI should simply be the URI of the file being written.
For example:

.. literalinclude:: overview.c
   :start-after: begin env-new
   :end-before: end env-new
   :dedent: 2

Namespace prefixes can also be defined for any vocabularies used:

.. literalinclude:: overview.c
   :start-after: begin env-set-prefix
   :end-before: end env-set-prefix
   :dedent: 2

We now have an environment set up for our document,
but still need to specify where to write it.
This is done by creating a :struct:`SerdByteSink`,
which is a generic interface that can be set up to write to a file,
a buffer in memory,
or a custom function that can be used to write output anywhere.
In this case, we will write to the file we set up as the base URI:

.. literalinclude:: overview.c
   :start-after: begin byte-sink-new
   :end-before: end byte-sink-new
   :dedent: 2

The second argument is the page size in bytes,
so I/O will be performed in chunks for better performance.
The value used here, 4096, is a typical filesystem block size that should perform well on most machines.

With an environment and byte sink ready,
the writer can now be created:

.. literalinclude:: overview.c
   :start-after: begin writer-new
   :end-before: end writer-new
   :dedent: 2

Output is written by feeding statements and other events to the sink returned by :func:`serd_writer_sink`.
:struct:`SerdSink` is the generic interface for anything that can consume data streams.
Many objects provide the same interface to do various things with the data,
but in this case we will send data directly to the writer:

.. literalinclude:: overview.c
   :start-after: begin reader-new
   :end-before: end reader-new
   :dedent: 2

The third argument of :func:`serd_reader_new` takes a bitwise ``OR`` of :enum:`SerdReaderFlag` flags that can be used to configure the reader.
In this case only :enumerator:`SERD_READ_LAX` is given,
which tolerates some invalid input without halting on an error,
but others can be included.
For example, passing ``SERD_READ_LAX | SERD_READ_RELATIVE`` would enable lax mode and preserve relative URIs in the input.

Now that we have a reader that is set up to directly push its output to a writer,
we can finally process the document:

.. literalinclude:: overview.c
   :start-after: begin read-document
   :end-before: end read-document
   :dedent: 2

Alternatively, one "chunk" of input can be read at a time with :func:`serd_reader_read_chunk`.
A "chunk" is generally one top-level description of a resource,
including any anonymous blank nodes in its description,
but this depends on the syntax and the structure of the document being read.

The reader pushes events to its sink as input is read,
so in this scenario the data should now have been re-written by the writer
(assuming no error occurred).
To finish and ensure that a complete document has been read and written,
:func:`serd_reader_finish` can be called followed by :func:`serd_writer_finish`.
However these will be automatically called on destruction if necessary,
so if the reader and writer are no longer required they can simply be destroyed:

.. literalinclude:: overview.c
   :start-after: begin reader-writer-free
   :end-before: end reader-writer-free
   :dedent: 2

Note that it is important to free the reader first in this case,
since finishing the read may push events to the writer.
Finally, closing the byte sink will flush and close the output file,
so it is ready to be read again later.
Similar to the reader and writer,
this can be done explicitly with :func:`serd_byte_sink_close`,
or implicitly with :func:`serd_byte_sink_free` if the byte sink is no longer needed:

.. literalinclude:: overview.c
   :start-after: begin byte-sink-free
   :end-before: end byte-sink-free
   :dedent: 2

Reading into a Model
--------------------

A document can be loaded into a model by setting up a reader that pushes data to a model "inserter" rather than a writer:

.. literalinclude:: overview.c
   :start-after: begin inserter-new
   :end-before: end inserter-new
   :dedent: 2

The process of reading the document is the same as above,
only the sink is different:

.. literalinclude:: overview.c
   :start-after: begin model-reader-new
   :end-before: end model-reader-new
   :dedent: 2

Writing a Model
---------------

A model, or parts of a model, can be written by writing the desired range with :func:`serd_write_range`:

.. literalinclude:: overview.c
   :start-after: begin write-range
   :end-before: end write-range
   :dedent: 2

By default,
this writes the range in chunks suited to pretty-printing with anonymous blank nodes (like "[ ... ]" in Turtle or TriG).
The flag :enumerator:`SERD_NO_INLINE_OBJECTS` can be given to instead write the range in a simple SPO order,
which can be useful in other situations because it is faster and emits statements in strictly increasing order.

Stream Processing
=================

The above examples show how a document can be either written to a file or loaded into a model,
simply by changing the sink that the data is written to.
There are also sinks that filter or transform the data before passing it on to another sink,
which can be used to build more advanced pipelines with several processing stages.

Canonical Literals
------------------

The "canon" is a stream processor that converts literals with supported XSD datatypes into canonical form.
For example, this will rewrite an xsd:decimal literal like ".10" as "0.1".
A canon is created with :func:`serd_canon_new`,
which needs to be passed the "target" sink that the transformed statements should be written to,
for example:

.. literalinclude:: overview.c
   :start-after: begin canon-new
   :end-before: end canon-new
   :dedent: 2

The last argument is a bitwise ``OR`` of :enum:`SerdCanonFlag` flags.
For example, :enumerator:`SERD_CANON_LAX` will tolerate and pass through invalid literals,
which can be useful for cleaning up questionabe data as much as possible without losing any information.

Filtering Statements
--------------------

The "filter" is a stream processor that filters statements based on a pattern.
It can be configured in either inclusive or exclusive mode,
which passes through only statements that match or don't match the pattern,
respectively.
A filter is created with :func:`serd_filter_new`,
which takes a target, pattern, and inclusive flag.
For example, all statements with predicate ``rdf:type`` could be filtered out when loading a model:

.. literalinclude:: overview.c
   :start-after: begin filter-new
   :end-before: end filter-new
   :dedent: 2

If ``false`` is passed for the last parameter instead,
then the filter operates in exclusive mode and will instead insert only statements with predicate ``rdf:type``.
