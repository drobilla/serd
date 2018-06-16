Model
=====

.. default-domain:: cpp
.. highlight:: cpp
.. namespace:: serd

A :struct:`Model` is an indexed set of statements.
A model can be used to store any set of data,
from a few statements (for example, a protocol message),
to an entire document,
to a database with millions of statements.

Constructing a model requires a world,
and :type:`flags <ModelFlags>` which can be used to configure the model:

.. literalinclude:: overview.cpp
   :start-after: begin model-new
   :end-before: end model-new
   :dedent: 2

Combinations of flags can be used to enable different indices,
or the storage of graphs and cursors.
For example, to be able to quickly search by predicate,
and store a cursor for each statement,
the flag :enumerator:`ModelFlag::store_carets` and a :enumerator:`StatementOrder::PSO` index can be added like so:

.. literalinclude:: overview.cpp
   :start-after: begin fancy-model-new
   :end-before: end fancy-model-new
   :dedent: 2

Model Operations
----------------

Models are value-like and can be copied and compared for equality:

.. literalinclude:: overview.cpp
   :start-after: begin model-copy
   :end-before: end model-copy
   :dedent: 2

The number of statements in a model can be accessed with the :func:`~Model::size` and :func:`~Model::empty` methods:

.. literalinclude:: overview.cpp
   :start-after: begin model-size
   :end-before: end model-size
   :dedent: 2

Destroying a model invalidates all nodes and statements within that model,
so care should be taken to ensure that no dangling pointers are created.

Adding Statements
-----------------

Statements can be added to the model by passing the nodes of the statement to :func:`~Model::insert`:

.. literalinclude:: overview.cpp
   :start-after: begin model-add
   :end-before: end model-add
   :dedent: 2

Alternatively, if you already have a statement (for example from another model),
the overload that takes a :type:`StatementView` can be used instead.
For example, the first statement in one model could be added to another like so:

.. literalinclude:: overview.cpp
   :start-after: begin model-insert
   :end-before: end model-insert
   :dedent: 2

An entire range of statements can be inserted at once by passing a range.
For example, all statements in one model could be copied into another like so:

.. literalinclude:: overview.cpp
   :start-after: begin model-add-range
   :end-before: end model-add-range
   :dedent: 2

Note that this overload consumes its argument,
so a copy must be made to insert a range without modifying the original.

Cursor
------

A cursor is a reference to a statement in a model,
or the end of the model.
Cursors work more or less like standard C++ iterators,
although they are smarter internally to present filtered views of a model.
The :func:`~Model::begin` method returns a cursor to the first statement in the model,
and :func:`~Model::end` returns the end sentinel (which must not be dereferenced).

.. literalinclude:: overview.cpp
   :start-after: begin model-begin-end
   :end-before: end model-begin-end
   :dedent: 2

Iterators can be advanced and compared manually:

.. literalinclude:: overview.cpp
   :start-after: begin iter-next
   :end-before: end iter-next
   :dedent: 2

For the simple case of iterating over every statement in a model,
range-based ``for`` syntax can be used to avoid using cursors directly:

.. literalinclude:: overview.cpp
   :start-after: begin model-iteration
   :end-before: end model-iteration
   :dedent: 2

Explicit use of cursors is more useful for more advanced cases.
For example, the above will scan the statements in the model's default statement order,
but it is also possible to scan the statements in an arbitrary order
(provided that the model has an appropriate index):

.. literalinclude:: overview.cpp
   :start-after: begin model-ordered
   :end-before: end model-ordered
   :dedent: 2

Pattern Matching
----------------

There are several model methods that can be used to quickly find statements in the model that match a pattern.
The simplest is :func:`~Model::ask` which checks if there is any matching statement:

.. literalinclude:: overview.cpp
   :start-after: begin model-ask
   :end-before: end model-ask
   :dedent: 2

To access the unknown fields,
an iterator to the matching statement can be found with :func:`~Model::find` instead:

.. literalinclude:: overview.cpp
   :start-after: begin model-find
   :end-before: end model-find
   :dedent: 2

Similar to :func:`~Model::ask`,
:func:`~Model::count` can be used to count the number of matching statements:

.. literalinclude:: overview.cpp
   :start-after: begin model-count
   :end-before: end model-count
   :dedent: 2

To iterate over matching statements,
:func:`~Model::find` can be used,
which returns a cursor that will visit only statements that match the pattern:

.. literalinclude:: overview.cpp
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
where a "?" represents a wildcard.

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
A more convenient way is to use the :func:`~Model::get` method.
To get a value, specify a pattern where exactly one of the subject, predicate, and object is a wildcard.
If a statement matches, then the node that "fills" the wildcard will be returned:

.. literalinclude:: overview.cpp
   :start-after: begin model-get
   :end-before: end model-get
   :dedent: 2

If multiple statements match the pattern,
then the matching node from an arbitrary statement is returned.
It is an error to specify more than one wildcard, excluding the graph.

The similar :func:`~Model::get_statement` instead returns the matching statement:

.. literalinclude:: overview.cpp
   :start-after: begin model-get-statement
   :end-before: end model-get-statement
   :dedent: 2

Erasing Statements
------------------

Individual statements can be erased with :func:`~Model::erase`,
which takes an iterator:

.. literalinclude:: overview.cpp
   :start-after: begin model-erase
   :end-before: end model-erase
   :dedent: 2

There is also an overload that takes a range and erases all statements in that range:

.. literalinclude:: overview.cpp
   :start-after: begin model-erase-range
   :end-before: end model-erase-range
   :dedent: 2

Erasing statements from a model invalidates all iterators to that model.
