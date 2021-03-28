World
=====

.. default-domain:: c
.. highlight:: c

Configuration and state used throughout the library are stored in a :struct:`SerdWorld`.
A program typically uses just one world,
which can be constructed using :func:`serd_world_new`:

.. literalinclude:: overview_code.c
   :start-after: begin world-new
   :end-before: end world-new
   :dedent: 2

All "global" library state is handled explicitly via the world.
Serd doesn't contain any static mutable data,
allowing it to be used concurrently in several parts of a program,
for example in plugins.
If multiple worlds *are* used in a single program,
they shouldn't be mixed:
objects "inside" one world cann't be used with objects inside another.

Note that the world isn't a database,
it only manages a small amount of library state.
