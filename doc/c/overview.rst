########
Overview
########

.. default-domain:: c
.. highlight:: c

The serd API is declared in ``serd.h``:

.. code-block:: c

   #include <serd/serd.h>

An application using serd first creates a :doc:`api/serd_world`,
which represents an instance of serd and is used to manage "global" facilities like logging.

The rest of the API declares objects that can be used together in different ways.
They can be broadly placed into four categories:

Data
   A :doc:`api/serd_node` is the basic building block of data,
   3 or 4 nodes together make a :doc:`api/serd_statement`.
   All data is expressed in this form.

Streams
   Objects stream data to each other via :doc:`api/serd_sink`,
   which is an abstract interface that receives :doc:`api/serd_event`.
   An event is essentially a statement,
   but there are a few additional event types that reflect context changes and support pretty-printing.

   Some objects both act as a sink and send data to another sink,
   which allow them to be inserted in a data `pipeline` to process the data as it streams through.
   For example,
   a :doc:`api/serd_canon` converts literals to canonical form,
   and a :doc:`api/serd_filter` filters statements that match (or do not match) some pattern.

   The syntactic context at a particular point is represented by an :doc:`api/serd_env`.
   This stores the base URI and set of namespace prefixes,
   which are used to expand relative and abbreviated URIs.

Reading and Writing
   Reading and writing data is performed using a :doc:`api/serd_reader`,
   which reads text and emits data to a sink,
   and a :doc:`api/serd_writer`,
   which is a sink that writes the incoming data as text.
   Both work in a streaming fashion so that large documents can be pretty-printed,
   translated,
   or otherwise processed quickly using only a small amount of memory.

Storage
   A set of statements can be stored in memory as a :doc:`api/serd_model`.
   There are several methods that can be used to search for statements quickly,
   provided an appropriate index is enabled.

   Data can be loaded into a model via an :doc:`api/serd_inserter`,
   which is a sink that inserts incoming statements into a model.
   Data in a model can be written out by calling :func:`serd_write_range` on the desired range of statements.

The sink interface acts as a generic connection which can be used to build custom data processing pipelines.
For example,
a simple pipeline to read a document, filter out some statements, and write the result to a new file,
would look something like:

.. image:: ../_static/writer_pipeline.svg

Here, event streams are shown as a dashed arrow,
and a solid arrow represents explicit use of an object.
In other words, dashed arrows represent connections via the abstract :doc:`api/serd_sink` interface.
In this case both reader and writer are using the same environment,
so the output document will have the same abbreviations as the input.
It is also possible to use different environments,
for example to set additional namespace prefixes to further abbreviate the document.

Similarly, a document could be loaded into a model with canonical literals using a pipeline like:

.. image:: ../_static/model_pipeline.svg

Many other useful pipelines can be built from the objects included in serd,
and applications can implement custom sinks if those are not sufficient.

The remainder of this overview gives a bottom-up introduction to the API,
with links to the complete reference where further detail can be found.
