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
Serd doesn't contain any static mutable data,
allowing it to be used concurrently in several parts of a program,
for example in plugins.

If multiple worlds *are* used in a single program,
they must never be mixed:
objects "inside" one world cann't be used with objects inside another.

Note that the world isn't a database,
it only manages a small amount of library state for things like configuration and logging.
