String Views
============

.. default-domain:: c
.. highlight:: c

For performance reasons,
many functions in serd that take a string take a ``ZixStringView``,
rather than a bare pointer.
This forces code to be explicit about string measurement,
which discourages common patterns of repeated measurement of the same string.

Since a string view interface is a useful abstraction to share across several libraries,
Serd uses the string view interface of its dependency,
`Zix <https://gitlab.com/drobilla/zix>`_.
For convenience, several constructors are provided:

``zix_empty_string``

   Constructs a view of an empty string, for example:

   .. literalinclude:: overview_code.c
      :start-after: begin make-empty-string
      :end-before: end make-empty-string
      :dedent: 2

``zix_string``

   Constructs a view of an arbitrary string, for example:

   .. literalinclude:: overview_code.c
      :start-after: begin measure-string
      :end-before: end measure-string
      :dedent: 2

   This calls ``strlen`` to measure the string.
   Modern compilers should optimize this away if the parameter is a literal.

``zix_substring``

   Constructs a view of a slice of a string with an explicit length,
   for example:

   .. literalinclude:: overview_code.c
      :start-after: begin make-string-view
      :end-before: end make-string-view
      :dedent: 2

   This can also be used to create a view of a pre-measured string.
   If the length a dynamic string is already known,
   this is faster than ``serd_string``,
   since it avoids redundant measurement.

These constructors can be used inline when passing parameters,
but if the same dynamic string is used several times,
it is better to make a string view variable to avoid redundant measurement.
