Statements
==========

.. default-domain:: cpp
.. highlight:: cpp
.. namespace:: serd

A :struct:`Statement` is a tuple of either 3 or 4 nodes:
the `subject`, `predicate`, `object`, and optional `graph`.
Statements declare that a subject has some property.
The predicate identifies the property,
and the object is its value on the subject.

A statement can be thought of as a very simple machine-readable sentence.
The subject and object are as in natural language,
and the predicate is something like a verb, but more general.
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

To make a :class:`Statement` out of this, we need to define some URIs.
In RDF, the subject and predicate must be *resources* with an identifier
(for example, neither can be a string).
Conventionally, predicate names do not start with "has" or similar words,
since that would be redundant in this context.
So, we assume that ``http://example.org/drobilla`` is the URI for drobilla,
and that ``http://example.org/firstName`` has been defined somewhere to be
a property with the appropriate meaning,
and can make an equivalent :class:`Statement`:

.. literalinclude:: overview.cpp
   :start-after: begin statement-new
   :end-before: end statement-new
   :dedent: 2

Statements also have an additional field, the graph,
which is used to group statements together.
For example, this can be used to store the document where statements originated,
or to keep schema data separate from application data.
A statement with a graph can be constructed by passing the graph as the fourth parameter:

.. literalinclude:: overview.cpp
   :start-after: begin statement-new-graph
   :end-before: end statement-new-graph
   :dedent: 2

Finally, a :class:`Caret` may also be passed which records a position in the file that the statement was loaded from.
This is typically used for printing useful error messages.
The cursor is considered metadata and not part of the statement itself,
for example,
it is not considered in equality comparison.
Typically, the cursor will be automatically set by a reader,
but a statement with a cursor can be constructed manually by passing the cursor as the last parameter:

.. literalinclude:: overview.cpp
   :start-after: begin statement-new-cursor
   :end-before: end statement-new-cursor
   :dedent: 2

.. literalinclude:: overview.cpp
   :start-after: begin statement-new-graph-cursor
   :end-before: end statement-new-graph-cursor
   :dedent: 2


Accessing Fields
----------------

Statement fields can be accessed with the :func:`~StatementWrapper::node` method, for example:

.. literalinclude:: overview.cpp
   :start-after: begin get-subject
   :end-before: end get-subject
   :dedent: 2

Alternatively, an accessor function is provided for each field:

.. literalinclude:: overview.cpp
   :start-after: begin get-pog
   :end-before: end get-pog
   :dedent: 2

Every statement has a subject, predicate, and object,
but the graph is optional.
The caret is also optional,
and can be accessed with the :func:`~StatementWrapper::caret` method:

.. literalinclude:: overview.cpp
   :start-after: begin get-caret
   :end-before: end get-caret
   :dedent: 2

Comparison
----------

Two statements can be compared with the equals operator:

.. literalinclude:: overview.cpp
   :start-after: begin statement-equals
   :end-before: end statement-equals
   :dedent: 2

Statements are equal if all four corresponding pairs of nodes are equal.
The cursor is considered metadata, and is ignored for comparison.

It is also possible to match statements against a pattern with the :func:`~StatementWrapper::matches` method,
where empty parameters act as wildcards:

.. literalinclude:: overview.cpp
   :start-after: begin statement-matches
   :end-before: end statement-matches
   :dedent: 2
