Nodes
=====

.. default-domain:: cpp
.. highlight:: cpp
.. namespace:: serd

Nodes are the basic building blocks of data.
Nodes are essentially strings,
but also have a :enum:`type <NodeType>`,
and optionally either a datatype or a language.

In RDF, a node is either a literal, URI, or blank.
Serd can also represent "CURIE" nodes,
or shortened URIs,
which represent prefixed names often written in documents.

Fundamental Constructors
------------------------

There are five fundamental node constructors which can be used to create any node:

:func:`make_plain_literal`
   Creates a new string literal with an optional language tag.

:func:`make_typed_literal`
   Creates a new string literal with a datatype URI.

:func:`make_blank`
   Creates a new blank node ID.

:func:`make_curie`
   Creates a new shortened URI.

:func:`make_uri`
   Creates a new URI.

Convenience Constructors
------------------------

For convenience,
several other constructors are provided to make common types of nodes:

:func:`make_string`
   Creates a new string literal (with no datatype or language).

:func:`make_file_uri`
   Creates a new file URI from a path.

:func:`make_boolean`
   Creates a new boolean literal.

:func:`make_decimal`
   Creates a new decimal literal.

:func:`make_double`
   Creates a new double literal.

:func:`make_float`
   Creates a new float literal.

:func:`make_integer`
   Creates a new integer literal.

:func:`make_base64`
   Creates a new binary blob literal using xsd:base64Binary encoding.

The datatype or language, if present, can be retrieved with the :func:`~NodeWrapper::datatype` or :func:`~NodeWrapper::language` method, respectively.
Note that no node has both a datatype and a language.
