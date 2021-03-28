Reading and Writing
===================

.. default-domain:: c
.. highlight:: c

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

.. literalinclude:: overview_code.c
   :start-after: begin env-new
   :end-before: end env-new
   :dedent: 2

Namespace prefixes can also be defined for any vocabularies used:

.. literalinclude:: overview_code.c
   :start-after: begin env-set-prefix
   :end-before: end env-set-prefix
   :dedent: 2

We now have an environment set up for our document,
but still need to specify where to write it.
This is done by creating a :struct:`SerdOutputStream`,
which is a generic interface that can be set up to write to a file,
a buffer in memory,
or a custom function that can be used to write output anywhere.
In this case, we will write to the file we set up as the base URI:

.. literalinclude:: overview_code.c
   :start-after: begin byte-sink-new
   :end-before: end byte-sink-new
   :dedent: 2

The second argument is the page size in bytes,
so I/O will be performed in chunks for better performance.
The value used here, 4096, is a typical filesystem block size that should perform well on most machines.

With an environment and byte sink ready,
the writer can now be created:

.. literalinclude:: overview_code.c
   :start-after: begin writer-new
   :end-before: end writer-new
   :dedent: 2

Output is written by feeding statements and other events to the sink returned by :func:`serd_writer_sink`.
:struct:`SerdSink` is the generic interface for anything that can consume data streams.
Many objects provide the same interface to do various things with the data,
but in this case we will send data directly to the writer:

.. literalinclude:: overview_code.c
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

.. literalinclude:: overview_code.c
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

.. literalinclude:: overview_code.c
   :start-after: begin reader-writer-free
   :end-before: end reader-writer-free
   :dedent: 2

Note that it is important to free the reader first in this case,
since finishing the read may push events to the writer.
Finally, closing the output with :func:`serd_close_output` will flush and close the output file,
so it is ready to be read again later.

.. literalinclude:: overview_code.c
   :start-after: begin byte-sink-free
   :end-before: end byte-sink-free
   :dedent: 2

Reading into a Model
--------------------

A document can be loaded into a model by setting up a reader that pushes data to a model "inserter" rather than a writer:

.. literalinclude:: overview_code.c
   :start-after: begin inserter-new
   :end-before: end inserter-new
   :dedent: 2

The process of reading the document is the same as above,
only the sink is different:

.. literalinclude:: overview_code.c
   :start-after: begin model-reader-new
   :end-before: end model-reader-new
   :dedent: 2

Writing a Model
---------------

A model, or parts of a model, can be written by writing the desired range with :func:`serd_describe_range`:

.. literalinclude:: overview_code.c
   :start-after: begin write-range
   :end-before: end write-range
   :dedent: 2

By default,
this writes the range in chunks suited to pretty-printing with anonymous blank nodes (like "[ ... ]" in Turtle or TriG).
Any rdf:type properties (written "a" in Turtle or TriG) will be written before any other properties of their subject.
This can be disabled by passing the flag :enumerator:`SERD_NO_TYPE_FIRST`.
