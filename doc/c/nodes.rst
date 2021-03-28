Nodes
=====

.. default-domain:: c
.. highlight:: c

Nodes are the basic building blocks of data.
Nodes are essentially strings,
but also have a :enum:`type <SerdNodeType>`,
and optionally either a datatype or a language.

In RDF, a node is either a literal, URI, or blank.
Serd can also represent "CURIE" nodes,
or shortened URIs,
which represent prefixed names often written in documents.

Fundamental Constructors
------------------------

There are five fundamental node constructors which can be used to create any node:

:func:`serd_new_plain_literal`
   Creates a new string literal with an optional language tag.

:func:`serd_new_typed_literal`
   Creates a new string literal with a datatype URI.

:func:`serd_new_blank`
   Creates a new blank node ID.

:func:`serd_new_curie`
   Creates a new shortened URI.

:func:`serd_new_uri`
   Creates a new URI.

Convenience Constructors
------------------------

For convenience,
several other constructors are provided to make common types of nodes:

:func:`serd_new_simple_node`
   Creates a new simple blank, CURIE, or URI node.

:func:`serd_new_string`
   Creates a new string literal (with no datatype or language).

:func:`serd_new_parsed_uri`
   Creates a new URI from a parsed URI view.

:func:`serd_new_file_uri`
   Creates a new file URI from a path.

:func:`serd_new_boolean`
   Creates a new boolean literal.

:func:`serd_new_decimal`
   Creates a new decimal literal.

:func:`serd_new_double`
   Creates a new double literal.

:func:`serd_new_float`
   Creates a new float literal.

:func:`serd_new_integer`
   Creates a new integer literal.

:func:`serd_new_base64`
   Creates a new binary blob literal using xsd:base64Binary encoding.

Accessors
---------

The basic attributes of a node can be accessed with :func:`serd_node_type`,
:func:`serd_node_string`,
and :func:`serd_node_length`.

A measured view of the string can be accessed with :func:`serd_node_string_view`.
This can be passed to functions that take a string view,
to avoid redundant measurement of the node string.

The datatype or language can be retrieved with :func:`serd_node_datatype` or :func:`serd_node_language`, respectively.
Note that a node may have either a datatype or a language,
but never both.
