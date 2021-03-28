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

Construction
------------

Nodes can be allocated and constructed in several ways.
To accommodate many functions that access or create arbitrary nodes,
the arguments to specify a node are passed in a :c:struct:`SerdNodeArgs`.
This is a tagged union of values and/or views that define a node.

Convenience constructors are provided,
which can be used to specify arguments for any node:

- :func:`serd_a_token`

- :func:`serd_a_file_uri`
- :func:`serd_a_parsed_uri`
- :func:`serd_a_uri_string`
- :func:`serd_a_uri`

- :func:`serd_a_blank`

- :func:`serd_a_base64`
- :func:`serd_a_decimal`
- :func:`serd_a_hex`
- :func:`serd_a_integer`
- :func:`serd_a_literal`
- :func:`serd_a_plain_literal`
- :func:`serd_a_primitive`
- :func:`serd_a_string_view`
- :func:`serd_a_string`
- :func:`serd_a_typed_literal`

Note that most of these are simple wrappers for more fundamental constructors;
there are only three "kinds" of RDF nodes: URIs, blank nodes, and literals.

Nodes can be constructed in a user-provided buffer with :func:`serd_node_construct`.
This is useful for applications with custom memory management schemes,
such as allocating memory in a preexisting buffer.

Typical higher-level applications without such needs can use :func:`serd_node_new`,
which dynamically allocates a new node using the given allocator (the system's, by default).
The application must eventually call :func:`serd_node_free` to free the node.

The memory management hassle can be avoided by using :c:struct:`SerdNodes`.
A node can be created or retrieved using :func:`serd_nodes_get`,
and it will be freed when the whole set of nodes is destroyed with :func:`serd_nodes_free`.

Accessors
---------

The basic attributes of a node can be accessed with :func:`serd_node_type`,
:func:`serd_node_string`,
and :func:`serd_node_length`.

A measured view of the string can be accessed with :func:`serd_node_string_view`.
This can be passed to functions that take a string view,
to avoid redundant measurement of the node string.

The datatype or language of a literal can be retrieved with :func:`serd_node_datatype` or :func:`serd_node_language`, respectively.
Note that literals may have a datatype or a language,
but never both at once.
