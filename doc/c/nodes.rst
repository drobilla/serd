Nodes
=====

.. default-domain:: c
.. highlight:: c

Nodes are the basic building blocks of data.
Nodes are essentially strings,
but also have a :enum:`type <SerdNodeType>`,
and optionally either a datatype or a language.

In the abstract, a node is either a literal, a URI, or blank.
Literals are essentially strings,
but may have a datatype or a language tag.
URIs are used to identify resources,
as are blank nodes,
except blank nodes only have labels with a limited scope and may be written anonymously.

Serd also has a type for variable nodes,
which are used for some features but not present in RDF data.

Fundamental Constructors
------------------------

To allow the application to manage node memory,
node constructors are provided that construct nodes in existing memory buffers.
The universal constructor :func:`serd_node_construct` can construct any type of node,
but is somewhat verbose and tricky to use.

Several constructors for more specific types of node are also available:

- :func:`serd_node_construct_token`
- :func:`serd_node_construct_uri`
- :func:`serd_node_construct_file_uri`
- :func:`serd_node_construct_literal`
- :func:`serd_node_construct_boolean`
- :func:`serd_node_construct_decimal`
- :func:`serd_node_construct_double`
- :func:`serd_node_construct_float`
- :func:`serd_node_construct_integer`
- :func:`serd_node_construct_base64`

If explicit memory management is not required,
high-level constructors that allocate nodes on the heap can be used instead:

- :func:`serd_new_token`
- :func:`serd_new_uri`
- :func:`serd_new_file_uri`
- :func:`serd_new_literal`
- :func:`serd_new_boolean`
- :func:`serd_new_decimal`
- :func:`serd_new_double`
- :func:`serd_new_float`
- :func:`serd_new_integer`
- :func:`serd_new_base64`

Accessors
---------

The basic attributes of a node can be accessed with :func:`serd_node_type`,
:func:`serd_node_string`,
and :func:`serd_node_length`.

A measured view of the string can be accessed with :func:`serd_node_string_view`.
This can be passed to functions that take a string view,
to avoid redundant measurement of the node string.

The datatype or language can be retrieved with :func:`serd_node_datatype` or :func:`serd_node_language`, respectively.
Note that only literals can have a datatype or language,
but never both at once.
