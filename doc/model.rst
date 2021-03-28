Model
=====

.. default-domain:: c
.. highlight:: c

A :struct:`SerdModel` is an indexed set of statements.
A model can be used to store any data set,
from a few statements (for example, a protocol message),
to an entire document,
to a database with millions of statements.

A new model can be created with :func:`serd_model_new`:

.. literalinclude:: overview_code.c
   :start-after: begin model-new
   :end-before: end model-new
   :dedent: 2

The information to store for each statement can be controlled by passing flags.
Additional indices can also be enabled with :func:`serd_model_add_index`.
For example, to be able to quickly search by predicate,
and store a cursor for each statement,
the model can be constructed with the :enumerator:`SERD_STORE_CARETS` flag,
and an additional :enumerator:`SERD_ORDER_PSO` index can be added like so:

.. literalinclude:: overview_code.c
   :start-after: begin fancy-model-new
   :end-before: end fancy-model-new
   :dedent: 2

Accessors
---------

The flags set for a model can be accessed with :func:`serd_model_flags`.

The number of statements can be accessed with :func:`serd_model_size` and :func:`serd_model_empty`:

.. literalinclude:: overview_code.c
   :start-after: begin model-size
   :end-before: end model-size
   :dedent: 2

Adding Statements
-----------------

Statements can be added to a model with :func:`serd_model_add`:

.. literalinclude:: overview_code.c
   :start-after: begin model-add
   :end-before: end model-add
   :dedent: 2

Alternatively, :func:`serd_model_insert` can be used if you already have a statement.
For example, the first statement in one model could be added to another like so:

.. literalinclude:: overview_code.c
   :start-after: begin model-insert
   :end-before: end model-insert
   :dedent: 2

An entire range of statements can be inserted at once with :func:`serd_model_insert_statements`.
For example, all statements in one model could be copied into another like so:

.. literalinclude:: overview_code.c
   :start-after: begin model-add-range
   :end-before: end model-add-range
   :dedent: 2

Iteration
---------

An iterator is a reference to a particular statement in a model.
:func:`serd_model_begin` returns an iterator to the first statement in the model,
and :func:`serd_model_end` returns a sentinel that is one past the last statement in the model:

.. literalinclude:: overview_code.c
   :start-after: begin model-begin-end
   :end-before: end model-begin-end
   :dedent: 2

A cursor can be advanced to the next statement with :func:`serd_cursor_advance`,
which returns :enumerator:`SERD_FAILURE` if the iterator reached the end:

.. literalinclude:: overview_code.c
   :start-after: begin iter-next
   :end-before: end iter-next
   :dedent: 2

Iterators are dynamically allocated,
and must eventually be destroyed with :func:`serd_cursor_free`:

.. literalinclude:: overview_code.c
   :start-after: begin iter-free
   :end-before: end iter-free
   :dedent: 2

Pattern Matching
----------------

There are several functions that can be used to quickly find statements in the model that match a pattern.
The simplest is :func:`serd_model_ask` which checks if there is any matching statement:

.. literalinclude:: overview_code.c
   :start-after: begin model-ask
   :end-before: end model-ask
   :dedent: 2

To access the unknown fields,
an iterator to the matching statement can be found with :func:`serd_model_find` instead:

.. literalinclude:: overview_code.c
   :start-after: begin model-find
   :end-before: end model-find
   :dedent: 2

To iterate over the matching statements,
the iterator returned by :func:`serd_model_find` can be advanced.
It will reach its end when it reaches the last matching statement:

.. literalinclude:: overview_code.c
   :start-after: begin model-range
   :end-before: end model-range
   :dedent: 2


Similar to :func:`serd_model_ask`,
:func:`serd_model_count` can be used to count the number of matching statements:

.. literalinclude:: overview_code.c
   :start-after: begin model-count
   :end-before: end model-count
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

.. literalinclude:: overview_code.c
   :start-after: begin model-get
   :end-before: end model-get
   :dedent: 2

If multiple statements match the pattern,
then the matching node from an arbitrary statement is returned.
It is an error to specify more than one wildcard, excluding the graph.

The similar :func:`serd_model_get_statement` instead returns the matching statement:

.. literalinclude:: overview_code.c
   :start-after: begin model-get-statement
   :end-before: end model-get-statement
   :dedent: 2

Erasing Statements
------------------

Individual statements can be erased with :func:`serd_model_erase`,
which takes a cursor:

.. literalinclude:: overview_code.c
   :start-after: begin model-erase
   :end-before: end model-erase
   :dedent: 2

The similar :func:`serd_model_erase_statements` will erase all statements in the cursor's range:

.. literalinclude:: overview_code.c
   :start-after: begin model-erase-range
   :end-before: end model-erase-range
   :dedent: 2

Lifetime
--------

Models are value-like and can be copied with :func:`serd_model_copy` and compared with :func:`serd_model_equals`:

.. literalinclude:: overview_code.c
   :start-after: begin model-copy
   :end-before: end model-copy
   :dedent: 2

When a model is no longer needed, it can be destroyed with :func:`serd_model_free`:

.. literalinclude:: overview_code.c
   :start-after: begin model-free
   :end-before: end model-free
   :dedent: 2

Destroying a model invalidates all nodes and statements within that model,
so care should be taken to ensure that no dangling pointers are created.
