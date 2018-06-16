Stream Processing
=================

.. default-domain:: cpp
.. highlight:: cpp
.. namespace:: serd

The above examples show how a document can be either written to a file or loaded into a model,
simply by changing the sink that the data is written to.
There are also sinks that filter or transform the data before passing it on to another sink,
which can be used to build more advanced pipelines with several processing stages.

Canonical Literals
------------------

A `canon` is a stream processor that converts literals with supported XSD datatypes into canonical form.
For example, this will rewrite an xsd:decimal literal like ".10" as "0.1".
A canon can be constructed by passing the "target" sink that the transformed statements should be written to,
for example:

.. literalinclude:: overview.cpp
   :start-after: begin canon-new
   :end-before: end canon-new
   :dedent: 2

The last argument is a bitwise ``OR`` of :enum:`CanonFlag` flags.
For example, :enumerator:`CanonFlag::lax` will tolerate and pass through invalid literals,
which can be useful for cleaning up questionabe data as much as possible without losing any information.

Filtering Statements
--------------------

A `filter` is a stream processor that filters statements based on a pattern.
It can be configured in either inclusive or exclusive mode,
which passes through only statements that match or don't match the pattern,
respectively.
A filter can be constructed by passing the target sink,
the statement pattern as individual nodes,
and an inclusive flag.
For example, all statements with predicate ``rdf:type`` could be filtered out when loading a model:

.. literalinclude:: overview.cpp
   :start-after: begin filter-new
   :end-before: end filter-new
   :dedent: 2

If ``false`` is passed for the last parameter instead,
then the filter operates in exclusive mode and will instead insert only statements with predicate ``rdf:type``.
