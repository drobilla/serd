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

A set of nodes is stored in a :struct:`SerdNodes`:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-new
   :end-before: end nodes-new
   :dedent: 2

Nodes are identified by a non-zero :type:`SerdNodeID`, and can be retrieved with :func:`serd_nodes_id`, which will add it to the set if necessary: 

.. literalinclude:: overview_code.c
   :start-after: begin nodes-id
   :end-before: end nodes-id
   :dedent: 2

Alternatively, :func:`serd_nodes_find` can be used to retrieve an ID only if the node is already present:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-find
   :end-before: end nodes-find
   :dedent: 2

To allow for more efficient storage, access to node strings is managed via a separate :struct:`SerdStrings` associated with the node set:

.. literalinclude:: overview_code.c
   :start-after: begin strings-new
   :end-before: end strings-new
   :dedent: 2

This allows retrieving a view of the node contents given its ID:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-get-token
   :end-before: end nodes-get-token
   :dedent: 2

Similarly, a more complex view that includes flags and optionally a datatype URI or language tag can be retrieved with :func:`serd_strings_object`:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-get-object
   :end-before: end nodes-get-object
   :dedent: 2

The returned views point to memory in the :struct:`SerdStrings` or :struct:`SerdNodes` and so are invalidated when these are destroyed:

.. literalinclude:: overview_code.c
   :start-after: begin nodes-free
   :end-before: end nodes-free
   :dedent: 2
