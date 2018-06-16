C++ Facilities
==============

.. default-domain:: cpp
.. highlight:: cpp
.. namespace:: serd

String Views
------------

For performance reasons,
most functions that take a string take a :type:`StringView`.
This allows many types of string to be passed as an argument,
and redundant string measurement to be avoided.

:type:`StringView` works similarly to ``std::string_view`` (and will likely be removed when C++17 support is more widespread).
A :type:`StringView` parameter will accept a string literal,
dynamic C string,
or a ``std::string`` as an argument.
Note, however, that the constructor that takes only a ``const char*`` calls ``strlen`` to measure the string,
so care should be taken to avoid this in performance-critical code if the string length is already known.

Optionals
---------

Several places in the C API take or return a pointer that may be null.
This is wrapped more safely in the C++ API as an :class:`Optional`.

From a user perspective, :class:`Optional` works similarly to ``std::optional``,
with pointer-like access operators and explicit boolean conversion enabling code like:

.. code-block:: cpp

   if (optional_value) {
     use_value(*optional_value);
   }

or:

.. code-block:: cpp

   if (optional_object) {
     optional_object->do_something();
   }

The :class:`Optional` implementation is serd-specific,
and takes advantage of the fact that the contained object is really just a "fancy pointer".
This means that null can be used to represent an unset value,
avoiding the space overhead of more general types like ``std::optional``.

A pointer to the underlying C object can be retrieved with the :func:`~Optional::cobj` method,
which will return null if the optional is unset.

