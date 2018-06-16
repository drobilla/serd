Nodes
=====

.. default-domain:: cpp
.. highlight:: cpp
.. namespace:: serd

Nodes are the basic building blocks of data.
Nodes are essentially strings,
but also have a :enum:`type <NodeType>`,
and optionally either a datatype or a language.

In the abstract, a node is either a literal, a URI, or blank.
Serd also has a type for variable nodes,
which are used for some features but not present in RDF data.

Construction
------------

Several convenient constructors are provided that build nodes:

- :func:`make_token`
- :func:`make_uri`
- :func:`make_file_uri`
- :func:`make_literal`
- :func:`make_decimal`
- :func:`make_integer`
- :func:`make_base64`

Literal nodes for number types (`bool`, `double`, `int32_t`, and so on) can be constructed with the generic :func:`make` template.


Accessors
---------

The datatype or language of a node can be retrieved with :func:`~NodeWrapper::datatype` or :func:`~NodeWrapper::language`, respectively.
Note that only literals can have a datatype or language,
but never both at once.
