String Views
============

.. default-domain:: c
.. highlight:: c

For performance reasons,
most functions in serd that take a string take a :struct:`SerdStringView`,
rather than a bare pointer.
This forces code to be explicit about string measurement,
which discourages common patterns of repeated measurement of the same string.
For convenience, several macros are provided for constructing string views:

:macro:`SERD_EMPTY_STRING`

   Constructs a view of an empty string, for example:

   .. literalinclude:: overview_code.c
      :start-after: begin make-empty-string
      :end-before: end make-empty-string
      :dedent: 2

:macro:`SERD_STRING`

   Constructs a view of an entire string or string literal, for example:

   .. literalinclude:: overview_code.c
      :start-after: begin make-static-string
      :end-before: end make-static-string
      :dedent: 2

   or:

   .. literalinclude:: overview_code.c
      :start-after: begin measure-string
      :end-before: end measure-string
      :dedent: 2

   This macro calls ``strlen`` to measure the string.
   Modern compilers will optimise this away if the parameter is a string literal.

:macro:`SERD_SUBSTRING`

   Constructs a view of a slice of a string with an explicit length,
   for example:

   .. literalinclude:: overview_code.c
      :start-after: begin make-string-view
      :end-before: end make-string-view
      :dedent: 2

   This macro can also be used to create a view of a pre-measured string.
   If the length a dynamic string is already known,
   it is faster to use this than :macro:`SERD_STRING`.

These macros can be used inline when passing parameters,
but if the same dynamic string is used several times,
it is better to make a string view variable to avoid redundant measurement.
