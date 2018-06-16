World
=====

.. default-domain:: cpp
.. highlight:: cpp
.. namespace:: serd

So far, we have only used nodes and statements,
which are simple independent objects.
Higher-level facilities in Serd require a :struct:`World`,
which represents the global library state.

A program typically uses just one world,
which can be constructed with no arguments:

.. literalinclude:: overview.cpp
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

.. literalinclude:: overview.cpp
   :start-after: begin get-blank
   :end-before: end get-blank
   :dedent: 2
