Nodes
=====

.. default-domain:: c
.. highlight:: c

Nodes are the basic building blocks of data.
Nodes are essentially strings,
but also have a :enum:`type <SerdNodeType>`,
and, for literals, optionally either a datatype or a language.

In the abstract, a node is either a literal, a URI, or blank.
A literal is a string that may have an associated datatype URI or language tag.
URIs are used to identify resources,
as are blank nodes,
except blank nodes only have labels with a limited scope and may be written anonymously.

Serd also has a type for “variable” nodes,
which are used in the interface of some pattern-matching features,
but not present in RDF data.

Arguments
---------

Node descriptions are used in many places: function parameters, event payloads, and so on.
Since there are many different kinds of node,
the arguments to specify one are encapsulated in a :struct:`SerdNodeArgs`,
a shallow view type that doesn't own any memory.
Several convenience constructors are provided,
which can be used inline to pass a view of any node to functions that expect one.

Simple nodes that are just a string and a type are called “tokens”,
and more complex literals that may have a datatype URI or language tag are called “objects”.
Any node can be described with a general string-based constructor: :func:`serd_a_token`, :func:`serd_a_object`, or a view-accepting variant :func:`serd_a_token_view` or :func:`serd_a_object_view`.

Many other constructors are also provided:

- IDs

  - :func:`serd_a_null`
  - :func:`serd_a_node_id`

- URIs

  - :func:`serd_a_path`
  - :func:`serd_a_prefixed_name`
  - :func:`serd_a_joined_uri`

- Tokens

  - :func:`serd_a_string`
  - :func:`serd_a_blank`
  - :func:`serd_a_uri`
  - :func:`serd_a_curie`

- Literals

  - :func:`serd_a_literal`
  - :func:`serd_a_typed_literal`
  - :func:`serd_a_plain_literal`

  - Numbers

    - :func:`serd_a_value`
    - :func:`serd_a_decimal`
    - :func:`serd_a_integer`

  - Binary

    - :func:`serd_a_hex`
    - :func:`serd_a_base64`

Storage
-------

Nodes are stored collectively in a :struct:`SerdNodes` and identified by a non-zero :type:`SerdNodeID`.

.. literalinclude:: overview_code.c
   :start-after: begin nodes-new
   :end-before: end nodes-new
   :dedent: 2

A node can be retrieved with :func:`serd_nodes_id`, which will add it to the set if necessary: 

.. literalinclude:: overview_code.c
   :start-after: begin nodes-id
   :end-before: end nodes-id
   :dedent: 2

Alternatively, :func:`serd_nodes_existing_id` can be used to retrieve an ID only if the node is already present:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-existing-id
   :end-before: end nodes-existing-id
   :dedent: 2

A view of a node's string and its type can be retrived with :func:`serd_nodes_get_token`:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-get-token
   :end-before: end nodes-get-token
   :dedent: 2

Similarly :func:`serd_nodes_get_object` can retrieve a more complex view that includes flags, a datatype URI, or a language tag, if applicable:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-get-object
   :end-before: end nodes-get-object
   :dedent: 2

The returned views point to memory stored in the set.
When the set is destroyed, this memory is released, and all previously returned views become invalid:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-free
   :end-before: end nodes-free
   :dedent: 2
