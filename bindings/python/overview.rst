.. testsetup:: *

   import serd

========
Overview
========

Serd is a lightweight C library for working with RDF data.  This is the
documentation for its Python bindings, which also serves as a gentle
introduction to the basics of RDF.

Serd is designed for high-performance or resource-constrained applications, and
makes it possible to work with very large documents quickly and/or using
minimal memory.  In particular, it is dramatically faster than `rdflib
<https://rdflib.readthedocs.io/en/stable/>`_, though it is less fully-featured
and not pure Python.

Nodes
=====

Nodes are the basic building blocks of data.  Nodes are essentially strings:

>>> print(serd.uri("http://example.org/something"))
http://example.org/something

>>> print(serd.string("hello"))
hello

>>> print(serd.decimal(1234))
1234.0

>>> len(serd.string("hello"))
5

However, nodes also have a :meth:`~serd.Node.type`, and optionally either a
:meth:`~serd.Node.datatype` or :meth:`~serd.Node.language`.

Representation
--------------

The string content of a node as shown above can be ambiguous.  For example, it
is impossible to tell a URI from a string literal using only their string
contents.  The :meth:`~serd.Node.to_syntax` method returns a complete
representation of a node, in the `Turtle <https://www.w3.org/TR/turtle/>`_
syntax by default:

>>> print(serd.uri("http://example.org/something").to_syntax())
<http://example.org/something>

>>> print(serd.string("hello").to_syntax())
"hello"

>>> print(serd.decimal(1234).to_syntax())
1234.0

Note that the representation of a node in some syntax *may* be the same as the
``str()`` contents which are printed, but this is usually not the case.  For
example, as shown above, URIs and strings are quoted differently in Turtle.

A different syntax can be used by specifying one explicitly:

>>> print(serd.decimal(1234).to_syntax(serd.Syntax.NTRIPLES))
"1234.0"^^<http://www.w3.org/2001/XMLSchema#decimal>

An identical node can be recreated from such a string using the
:meth:`~serd.Node.from_syntax` method:

>>> node = serd.decimal(1234)
>>> copy = serd.Node.from_syntax(node.to_syntax()) # Don't actually do this
>>> print(copy)
1234.0

Alternatively, the ``repr()`` builtin will return the Python construction
representation:

>>> repr(serd.decimal(1234))
'serd.typed_literal("1234.0", "http://www.w3.org/2001/XMLSchema#decimal")'

Any node can be round-tripped to and from a string using these methods.  That
is, for any node `n`, both::

    serd.Node.from_syntax(n.to_syntax())

and::

    eval(repr(n))

produce an equivalent node.  Using the `to_syntax()` method is generally
recommended, since it uses standard syntax.

Primitives
----------

For convenience, nodes can be constructed from Python primitives by simply
passing a value to the constructor:

>>> repr(serd.Node(True))
'serd.boolean(True)'
>>> repr(serd.Node("hello"))
'serd.string("hello")'
>>> repr(serd.Node(1234))
'serd.typed_literal("1234", "http://www.w3.org/2001/XMLSchema#integer")'
>>> repr(serd.Node(12.34))
'serd.typed_literal("1.234E1", "http://www.w3.org/2001/XMLSchema#double")'

Note that it is not possible to construct every type of node this way, and care
should be taken to not accidentally construct a string literal where a URI is
desired.

Fundamental Constructors
------------------------

As the above examples suggest, several node constructors are just convenience
wrappers for more fundamental ones.  All node constructors reduce to one of the
following:

:func:`serd.plain_literal`
   A string with optional language, like ``"hallo"@de`` in Turtle.

:func:`serd.typed_literal`
   A string with optional datatype, like ``"1.2E9"^^xsd:float`` in Turtle.

:func:`serd.blank`
   A blank node ID, like "b42", or ``_:b42`` in Turtle.

:func:`serd.curie`
   A compact URI, like ``eg:name`` in Turtle.

:func:`serd.uri`
   A URI, like "http://example.org", or ``<http://example.org>`` in Turtle.

Convenience Constructors
------------------------

:func:`serd.string`
   A string literal with no language or datatype.

:func:`serd.decimal`
   An `xsd:decimal <https://www.w3.org/TR/xmlschema-2/#decimal>`_,
   like "123.45".

:func:`serd.double`
   An `xsd:double <https://www.w3.org/TR/xmlschema-2/#double>`_,
   like "1.2345E2".

:func:`serd.float`
   An `xsd:float <https://www.w3.org/TR/xmlschema-2/#float>`_,
   like "1.2345E2".

:func:`serd.integer`
   An `xsd:integer <https://www.w3.org/TR/xmlschema-2/#integer>`_,
   like "1234567".

:func:`serd.boolean`
   An `xsd:boolean <https://www.w3.org/TR/xmlschema-2/#boolean>`_,
   like "true" or "false".

:func:`serd.base64`
   An `xsd:base64Binary <https://www.w3.org/TR/xmlschema-2/#base64Binary>`_,
   like "aGVsbG8=".

:func:`serd.file_uri`
   A file URI, like "file:///doc.ttl".

Namespaces
==========

It is common to use many URIs that share a common prefix.  The
:class:`~serd.Namespace` utility class can be used to make code more readable
and make mistakes less likely:

>>> eg = serd.Namespace("http://example.org/")
>>> print(eg.thing)
http://example.org/thing

.. testsetup:: *

   eg = serd.Namespace("http://example.org/")

Dictionary syntax can also be used:

>>> print(eg["thing"])
http://example.org/thing

For convenience, namespaces also act like strings in many cases:

>>> print(eg)
http://example.org/
>>> print(eg + "stringeyName")
http://example.org/stringeyName

Note that this class is just a simple syntactic convenience, it does not
"remember" names and there is no corresponding C API.

Statements
==========

A :class:`~serd.Statement` is a tuple of either 3 or 4 nodes: the subject,
predicate, object, and optional graph.  Statements declare that a subject has
some property.  The predicate identifies the property, and the object is its
value.

A statement is a bit like a very simple machine-readable sentence.  The
"subject" and "object" are as in natural language, and the predicate is like
the verb, but more general.  For example, we could make a statement in English
about your intrepid author:

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

To make a :class:`~serd.Statement` out of this, we need to define some URIs.  In
RDF, the subject and predicate must be *resources* with an identifier (for
example, neither can be a string).  Conventionally, predicate names do not
start with "has" or similar words, since that would be redundant in this
context.  So, we assume that ``http://example.org/drobilla`` is the URI for
drobilla, and ``http://example.org/firstName`` has been defined somewhere to be
a property with the appropriate meaning, and can make an equivalent
:class:`~serd.Statement`:

>>> print(serd.Statement(eg.drobilla, eg.firstName, serd.string("David")))
<http://example.org/drobilla> <http://example.org/firstName> "David"

If you find this terminology confusing, it may help to think in terms of
dictionaries instead.  For example, the above can be thought of as equivalent
to::

    drobilla[firstName] = "David"

or::

    drobilla.firstName = "David"

Accessing Fields
----------------

Statement fields can be accessed via named methods or array indexing:

>>> statement = serd.Statement(eg.s, eg.p, eg.o, eg.g)
>>> print(statement.subject())
http://example.org/s
>>> print(statement[serd.Field.SUBJECT])
http://example.org/s
>>> print(statement[0])
http://example.org/s

Graph
-----

The graph field can be used as a context to distinguish otherwise identical
statements.  For example, it is often set to the URI of the document that the
statement was loaded from:

>>> print(serd.Statement(eg.s, eg.p, eg.o, serd.uri("file:///doc.ttl")))
<http://example.org/s> <http://example.org/p> <http://example.org/o> <file:///doc.ttl>

The graph field is always accessible, but may be ``None``:

    >>> triple = serd.Statement(eg.s, eg.p, eg.o)
    >>> print(triple.graph())
    None
    >>> quad = serd.Statement(eg.s, eg.p, eg.o, eg.g)
    >>> print(quad.graph())
    http://example.org/g

World
=====

So far, we have only used nodes and statements, which are simple independent
objects.  Higher-level facilities in serd require a :class:`~serd.World` which
represents the global library state.

A program typically uses just one world, which can be constructed with no
arguments::

    world = serd.World()

.. testsetup:: *

    world = serd.World()

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

Blank nodes, or simply "blanks", are used for resources that do not have URIs.
Unlike URIs, they are not global identifiers, and only have meaning within
their local context (for example, a document).  The world provides a method for
automatically generating unique blank identifiers:

>>> print(repr(world.get_blank()))
serd.blank("b1")
>>> print(repr(world.get_blank()))
serd.blank("b2")

Model
=====

A :class:`~serd.Model` is an indexed set of statements.  A model can be used to
store any set of data, from a few statements (for example, a protocol message),
to an entire document, to a database with millions of statements.

A model can be constructed and statements inserted manually using the
:meth:`~serd.Model.insert` method.  Tuple syntax is supported as a shorthand
for creating statements:

>>> model = serd.Model(world)
>>> model.insert((eg.s, eg.p, eg.o1))
>>> model.insert((eg.s, eg.p, eg.o2))
>>> model.insert((eg.t, eg.p, eg.o3))

.. testsetup:: model_manual

   import serd
   eg = serd.Namespace("http://example.org/")
   world = serd.World()
   model = serd.Model(world)
   model.insert((eg.s, eg.p, eg.o1))
   model.insert((eg.s, eg.p, eg.o2))
   model.insert((eg.t, eg.p, eg.o3))

Iterating over the model yields every statement:

>>> for s in model: print(s)
<http://example.org/s> <http://example.org/p> <http://example.org/o1>
<http://example.org/s> <http://example.org/p> <http://example.org/o2>
<http://example.org/t> <http://example.org/p> <http://example.org/o3>

Familiar Pythonic collection operations work as you would expect:

>>> print(len(model))
3
>>> print((eg.s, eg.p, eg.o4) in model)
False
>>> model += (eg.s, eg.p, eg.o4)
>>> print((eg.s, eg.p, eg.o4) in model)
True

Pattern Matching
----------------

The :meth:`~serd.Model.ask` method can be used to check if a statement is in a
model:

>>> print(model.ask(eg.s, eg.p, eg.o1))
True
>>> print(model.ask(eg.s, eg.p, eg.s))
False

This method is more powerful than the ``in`` statement because it also does
pattern matching.  To check for a pattern, use `None` as a wildcard:

>>> print(model.ask(eg.s, None, None))
True
>>> print(model.ask(eg.unknown, None, None))
False

The :meth:`~serd.Model.count` method works similarly, but instead returns the
number of statements that match the pattern:

>>> print(model.count(eg.s, None, None))
3
>>> print(model.count(eg.unknown, None, None))
0

Getting Values
--------------

Sometimes you are only interested in a single node, and it is cumbersome to
first search for a statement and then get the node from it.  The
:meth:`~serd.Model.get` method provides a more convenient way to do this.  To
get a value, specify a triple pattern where exactly one field is ``None``.  If
a statement matches, then the node that "fills" the wildcard will be returned:

>>> print(model.get(eg.t, eg.p, None))
http://example.org/o3

If multiple statements match the pattern, then the matching node from an
arbitrary statement is returned.  It is an error to specify more than one
wildcard, excluding the graph.

Erasing Statements
------------------

>>> model2 = model.copy()
>>> for s in model2: print(s)
<http://example.org/s> <http://example.org/p> <http://example.org/o1>
<http://example.org/s> <http://example.org/p> <http://example.org/o2>
<http://example.org/s> <http://example.org/p> <http://example.org/o4>
<http://example.org/t> <http://example.org/p> <http://example.org/o3>

Individual statements can be erased by value, again with tuple syntax supported
for convenience:

>>> model2.erase((eg.s, eg.p, eg.o1))
>>> for s in model2: print(s)
<http://example.org/s> <http://example.org/p> <http://example.org/o2>
<http://example.org/s> <http://example.org/p> <http://example.org/o4>
<http://example.org/t> <http://example.org/p> <http://example.org/o3>

Many statements can be erased at once by erasing a range:

>>> model2.erase(model2.range((eg.s, None, None)))
>>> for s in model2: print(s)
<http://example.org/t> <http://example.org/p> <http://example.org/o3>

Saving Documents
----------------

Serd provides simple methods to save an entire model to a file or string, which
are similar to functions in the standard Python ``json`` module.

A model can be saved to a file with the :meth:`~serd.World.dump` method:

.. doctest::
   :options: +NORMALIZE_WHITESPACE

   >>> world.dump(model, "out.ttl")
   >>> print(open("out.ttl", "r").read())
   <http://example.org/s>
    <http://example.org/p> <http://example.org/o1> ,
        <http://example.org/o2> ,
        <http://example.org/o4> .
   <BLANKLINE>
   <http://example.org/t>
    <http://example.org/p> <http://example.org/o3> .
   <BLANKLINE>

Similarly, a model can be written as a string with the :meth:`serd.World.dumps`
method:

.. doctest::
   :options: +ELLIPSIS

   >>> print(world.dumps(model))
   <http://example.org/s>
   ...

Loading Documents
-----------------

There are also simple methods to load an entire model, again loosely following
the standard Python ``json`` module.

A model can be loaded from a file with the :meth:`~serd.World.load` method:

>>> model3 = world.load("out.ttl")
>>> print(model3 == model)
True

By default, the syntax type is determined by the file extension, and only
:attr:`serd.ModelFlags.INDEX_SPO` will be set, so only ``(s p ?)`` and ``(s ?
?)`` queries will be fast.  See the method documentation for how to control
things more precisely.

Similarly, a model can be loaded from a string with the
:meth:`~serd.World.loads` method:

>>> ttl = "<{}> <{}> <{}> .".format(eg.s, eg.p, eg.o)
>>> model4 = world.loads(ttl)
>>> for s in model4: print(s)
<http://example.org/s> <http://example.org/p> <http://example.org/o>

File Cursor
-----------

When data is loaded from a file into a model with the flag
:data:`~serd.ModelFlags.STORE_CURSORS`, each statement will have a *cursor*
which describes the file name, line, and column where the statement originated.
The cursor points to the start of the object node in the statement:

>>> model5 = world.load("out.ttl", model_flags=serd.ModelFlags.STORE_CURSORS)
>>> for s in model5: print(s.cursor())
out.ttl:2:24
out.ttl:3:2
out.ttl:4:2
out.ttl:7:24

Streaming Data
==============

More advanced input and output can be performed by using the
:class:`~serd.Reader` and :class:`~serd.Writer` classes directly.  The Reader
produces an :class:`~serd.Event` stream which describes the content of the
file, and the Writer consumes such a stream and writes syntax.

Reading Files
-------------

The reader reads from a source, which should be a :class:`~serd.FileSource`
to read from a file.  Parsed input is sent to a sink, which is
called for each event:

.. testcode::

   def sink(event):
       print(event)

   env = serd.Env()
   reader = serd.Reader(world, serd.Syntax.TURTLE, 0, env, sink, 4096)
   with reader.open(serd.FileSource("out.ttl")) as context:
       context.read_document()

.. testoutput::
   :options: +ELLIPSIS

   serd.Event.statement(serd.Statement(serd.uri("http://example.org/s"), serd.uri("http://example.org/p"), serd.uri("http://example.org/o1"), serd.Cursor(serd.uri("out.ttl"), 2, 24)))
   ...

For more advanced use cases that keep track of state, the sink can be a custom
:class:`~serd.Sink` with a call operator:

.. testcode::

   class MySink(serd.Sink):
       def __init__(self):
           super().__init__()
           self.events = []

       def __call__(self, event: serd.Event) -> serd.Status:
           self.events += [event]
           return serd.Status.SUCCESS

   env = serd.Env()
   sink = MySink()
   reader = serd.Reader(world, serd.Syntax.TURTLE, 0, env, sink, 4096)
   with reader.open(serd.FileSource("out.ttl")) as context:
       context.read_document()

   print(sink.events[0])

.. testoutput::

   serd.Event.statement(serd.Statement(serd.uri("http://example.org/s"), serd.uri("http://example.org/p"), serd.uri("http://example.org/o1"), serd.Cursor(serd.uri("out.ttl"), 2, 24)))

Reading Strings
---------------

To read from a string, use a :class:`~serd.StringSource` with the reader:

.. testcode::

   ttl = """
   @base <http://drobilla.net/> .
   @prefix eg: <http://example.org/> .
   <sw/serd> eg:name "Serd" .
   """

   def sink(event):
       print(event)

   env = serd.Env()
   reader = serd.Reader(world, serd.Syntax.TURTLE, 0, env, sink, 4096)
   with reader.open(serd.StringSource(ttl)) as context:
       context.read_document()

.. testoutput::

    serd.Event.base("http://drobilla.net/")
    serd.Event.prefix("eg", "http://example.org/")
    serd.Event.statement(serd.Statement(serd.uri("http://drobilla.net/sw/serd"), serd.uri("http://example.org/name"), serd.string("Serd"), serd.Cursor(serd.string("string"), 4, 19)))

Reading into a Model
--------------------

To read new data into an existing model,
send it to the sink returned by :meth:`~serd.Model.inserter`:

.. testcode::

   ttl = """
   @prefix eg: <http://example.org/> .
   eg:newSubject eg:p eg:o .
   """

   env = serd.Env()
   sink = model.inserter(env)
   reader = serd.Reader(world, serd.Syntax.TURTLE, 0, env, sink, 4096)
   with reader.open(serd.StringSource(ttl)) as context:
       context.read_document()

   for s in model: print(s)

.. testoutput::

   <http://example.org/newSubject> <http://example.org/p> <http://example.org/o>
   <http://example.org/s> <http://example.org/p> <http://example.org/o1>
   <http://example.org/s> <http://example.org/p> <http://example.org/o2>
   <http://example.org/s> <http://example.org/p> <http://example.org/o4>
   <http://example.org/t> <http://example.org/p> <http://example.org/o3>

Writing Files
-------------

.. testcode::

   env = serd.Env()
   byte_sink = serd.FileSink("written.ttl")
   writer = serd.Writer(world, serd.Syntax.TURTLE, 0, env, byte_sink)
   st = model.all().write(writer.sink(), 0)
   writer.finish()
   byte_sink.close()
   print(open("written.ttl", "r").read())

.. testoutput::
   :options: +NORMALIZE_WHITESPACE

   <http://example.org/newSubject>
   	<http://example.org/p> <http://example.org/o> .

   <http://example.org/s>
   	<http://example.org/p> <http://example.org/o1> ,
   		<http://example.org/o2> ,
   		<http://example.org/o4> .

   <http://example.org/t>
   	<http://example.org/p> <http://example.org/o3> .
