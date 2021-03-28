Statements
==========

.. default-domain:: c
.. highlight:: c

A :struct:`SerdStatement` is a tuple of either 3 or 4 nodes:
the `subject`, `predicate`, `object`, and optional `graph`.
Statements declare that a subject has some property.
The predicate identifies the property,
and the object is its value.

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

To make a :struct:`SerdStatement` out of this, we need to define some URIs.
In RDF, the subject and predicate must be *resources* with an identifier
(for example, neither can be a string).
Conventionally, predicate names do not start with "has" or similar words,
since that would be redundant in this context.
So, we assume that ``http://example.org/drobilla`` is the URI for drobilla,
and that ``http://example.org/firstName`` has been defined somewhere to be
a property with the appropriate meaning,
and can make an equivalent :struct:`SerdStatement`:

.. literalinclude:: overview_code.c
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

.. literalinclude:: overview_code.c
   :start-after: begin get-subject
   :end-before: end get-subject
   :dedent: 2

Alternatively, an accessor function is provided for each field:

.. literalinclude:: overview_code.c
   :start-after: begin get-pog
   :end-before: end get-pog
   :dedent: 2

Every statement has a subject, predicate, and object,
but the graph may be null.
The cursor may also be null (as it would be in this case),
but if available it can be accessed with :func:`serd_statement_caret`:

.. literalinclude:: overview_code.c
   :start-after: begin get-caret
   :end-before: end get-caret
   :dedent: 2

Comparison
----------

Two statements can be compared with :func:`serd_statement_equals`:

.. literalinclude:: overview_code.c
   :start-after: begin statement-equals
   :end-before: end statement-equals
   :dedent: 2

Statements are equal if all four corresponding pairs of nodes are equal.
The cursor is considered metadata, and is ignored for comparison.

It is also possible to match statements against a pattern using ``NULL`` as a wildcard,
with :func:`serd_statement_matches`:

.. literalinclude:: overview_code.c
   :start-after: begin statement-matches
   :end-before: end statement-matches
   :dedent: 2

Lifetime
--------

A statement only contains const references to nodes,
it does not own nodes or manage their lifetimes internally.
The cursor, however, is owned by the statement.
A statement can be copied with :func:`serd_statement_copy`:

.. literalinclude:: overview_code.c
   :start-after: begin statement-copy
   :end-before: end statement-copy
   :dedent: 2

The copied statement will refer to exactly the same nodes,
though the cursor will be deep copied.

In most cases, statements come from a reader or model which manages them internally,
but a statement owned by the application must be freed with :func:`serd_statement_free`:

.. literalinclude:: overview_code.c
   :start-after: begin statement-free
   :end-before: end statement-free
   :dedent: 2
