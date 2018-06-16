Reading and Writing
===================

.. default-domain:: cpp
.. highlight:: cpp
.. namespace:: serd

Reading and writing documents in a textual syntax is handled by the :struct:`Reader` and :struct:`Writer`, respectively.
Serd is designed around a concept of event streams,
so the reader or writer can be at the beginning or end of a "pipeline" of stream processors.
This allows large documents to be processed quickly in an "online" fashion,
while requiring only a small constant amount of memory.
If you are familiar with XML,
this is roughly analogous to SAX.

A common setup is to simply connect a reader directly to a writer.
This can be used for things like pretty-printing,
or converting a document from one syntax to another.
This can be done by passing the sink returned by the writer's :func:`~Writer::sink` method to the :class:`~Reader` constructor.

First though,
an environment needs to be set up in order to write a document.
This defines the base URI and any namespace prefixes,
which are used to resolve any relative URIs or prefixed names by the reader,
and to abbreviate the output by the writer.
In most cases, the base URI should simply be the URI of the file being written.
For example:

.. literalinclude:: overview.cpp
   :start-after: begin env-new
   :end-before: end env-new
   :dedent: 2

Namespace prefixes can also be defined for any vocabularies used:

.. literalinclude:: overview.cpp
   :start-after: begin env-set-prefix
   :end-before: end env-set-prefix
   :dedent: 2

The reader will set any additional prefixes from the document as they are encountered.

We now have an environment set up for the contents of our document,
but still need to specify where to write it.
This is done by creating an :struct:`OutputStream`,
which is a generic interface that can be set up to write to a file,
a buffer in memory,
or a custom function that can be used to write output anywhere.
In this case, we will write to the file we set up as the base URI:

.. literalinclude:: overview.cpp
   :start-after: begin byte-sink-new
   :end-before: end byte-sink-new
   :dedent: 2

The second argument is the page size in bytes,
so I/O will be performed in chunks for better performance.
The value used here, 4096, is a typical filesystem block size that should perform well on most machines.

With an environment and byte sink ready,
the writer can now be created:

.. literalinclude:: overview.cpp
   :start-after: begin writer-new
   :end-before: end writer-new
   :dedent: 2

Output is written by feeding statements and other events to the sink returned by the writer's :func:`~Writer::sink` method.
:struct:`Sink` is the generic interface for anything that can consume data streams.
Many objects provide the same interface to do various things with the data,
but in this case we will send data directly to the writer:

.. literalinclude:: overview.cpp
   :start-after: begin reader-new
   :end-before: end reader-new
   :dedent: 2

The third argument of the reader constructor takes a bitwise ``OR`` of :enum:`ReaderFlag` flags that can be used to configure the reader.
In this case no flags are given,
but for example,
passing ``ReaderFlag::lax | ReaderFlag::relative`` would enable lax mode and preserve relative URIs in the input.

Now that we have a reader that is set up to directly push its output to a writer,
we can finally process the document:

.. literalinclude:: overview.cpp
   :start-after: begin read-document
   :end-before: end read-document
   :dedent: 2

Alternatively, one "chunk" of input can be read at a time with :func:`~Reader::read_chunk`.
A "chunk" is generally one top-level description of a resource,
including any anonymous blank nodes in its description,
but this depends on the syntax and the structure of the document being read.

The reader pushes events to its sink as input is read,
so in this scenario the data should now have been re-written by the writer
(assuming no error occurred).
To finish and ensure that a complete document has been read and written,
:func:`~Reader::finish` can be called followed by :func:`~Writer::finish`.
However these will be automatically called on destruction if necessary,
so if the reader and writer are no longer required they can simply be destroyed.

Finally, closing the byte sink will flush and close the output file,
so it is ready to be read again later.
Similar to the reader and writer,
this can be done explicitly by calling its :func:`~OutputStream::close` method,
or implicitly by destroying the byte sink if it is no longer needed:

.. literalinclude:: overview.cpp
   :start-after: begin byte-sink-close
   :end-before: end byte-sink-close
   :dedent: 2

Reading into a Model
--------------------

A document can be loaded into a model by setting up a reader that pushes data to a model `inserter` rather than a writer:

.. literalinclude:: overview.cpp
   :start-after: begin inserter-new
   :end-before: end inserter-new
   :dedent: 2

The process of reading the document is the same as above,
only the sink is different:

.. literalinclude:: overview.cpp
   :start-after: begin model-reader-new
   :end-before: end model-reader-new
   :dedent: 2

..
   Writing a Model
   ---------------

   A model, or parts of a model, can be written by writing the desired range using its :func:`Range::write` method:

   .. literalinclude:: overview.cpp
      :start-after: begin write-range
      :end-before: end write-range
      :dedent: 2

   By default,
   this writes the range in chunks suited to pretty-printing with anonymous blank nodes (like "[ ... ]" in Turtle or TriG).
   The flag :enumerator:`SerialisationFlag::no_inline_objects` can be given to instead write the range in a simple SPO order,
   which can be useful in other situations because it is faster and emits statements in strictly increasing order.
