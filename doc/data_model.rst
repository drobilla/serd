##########
Data Model
##########

*********
Structure
*********

Serd is based on RDF, a model for Linked Data.
A deep understanding of what this means isn't necessary,
but it is important to have a basic understanding of how this data is structured.

The basic building block of data is the *node*,
which is essentially a string with some extra type information.
A *statement* is a tuple of 3 or 4 nodes.
All information is represented by a set of statements,
which makes this model structurally very simple:
any document or database is essentially a single table with 3 or 4 columns.
This is easiest to see in NTriples or NQuads documents,
which are simple flat files with a single statement per line.

There are, however, some restrictions.
Each node in a statement has a specific role:
subject, predicate, object, and (optionally) graph, in that order.
A statement declares that a subject has some property.
The predicate identifies the property,
and the object is its value.

A statement is a bit like a very simple machine-readable sentence.
The "subject" and "object" are as in natural language,
and the predicate is something like a verb (but much more general).
For example, we could make a statement in English
about your intrepid author:

   drobilla has the first name David

We can break this statement into 3 pieces like so:

.. list-table::
   :header-rows: 1

   * - Subject
     - Predicate
     - Object
   * - drobilla
     - has the first name
     - David

The subject and predicate must be *resources* with an identifier,
so we will need to define some URIs to represent this statement.
Conventionally, predicate names do not start with "has" or similar words,
since that would be redundant in this context.
So,
we assume that ``http://example.org/drobilla`` is the URI for drobilla,
and that ``http://example.org/firstName`` has been defined as the appropriate property ("has the first name"),
and can represent the statement in a machine-readable way:

.. list-table::
   :header-rows: 1

   * - Subject
     - Predicate
     - Object
   * - ``http://example.org/drobilla``
     - ``http://example.org/firstName``
     - David

Which can be written in NTriples like so::

  <http://example.org/drobilla> <http://example.org/firstName> "David" .

*****************
Working with Data
*****************

The power of this data model lies in its uniform "physical" structure,
and the use of URIs as a decentralized namespace mechanism.
In particular, it makes filtering, merging, and otherwise "mixing" data from various sources easy.

For example, we could add some statements to the above example to better describe the same subject::

  <http://example.org/drobilla> <http://example.org/firstName> "David" .
  <http://example.org/drobilla> <http://example.org/lastName> "Robillard" .

We could also add information about other subjects::

  <http://drobilla.net/sw/serd> <http://example.org/programmingLanguage> "C" .

Including statements that relate them to each other::

  <http://example.org/drobilla> <http://example.org/wrote> <http://drobilla.net/sw/serd> .

Note that there is no "physical" tree structure here,
which is an important distinction from structured document formats like XML or JSON.
Since all information is just a set of statements,
the information in two documents,
for example,
can be combined by simply concatenating the documents.
Similarly,
any arbitrary subset of statements in a document can be separated into a new document.
The use of URIs enables such things even with data from many independent sources,
without any need to agree on a common schema.

In practice, sharing URI "vocabulary" is encouraged since this is how different parties can have a shared understanding of what data *means*.
That, however, is a higher-level application concern.
Only the "physical" structure of data described here is important for understanding how Serd works,
and what its tools and APIs can do.
