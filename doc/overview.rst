..
   Copyright 2020-2021 David Robillard <d@drobilla.net>
   SPDX-License-Identifier: ISC

########
Overview
########

.. default-domain:: c
.. highlight:: c

.. |api_dir| replace:: replacement *text*

..
   The API revolves around two main types: :struct:`SerdReader`,
   which reads text and fires callbacks,
   and :struct:`SerdWriter`,
   which writes text when driven by corresponding functions.
   Both work in a streaming fashion but still support pretty-printing,
   so the pair can be used to pretty-print, translate,
   or otherwise process arbitrarily large documents very quickly.
   The context of a stream is tracked by the :struct:`SerdEnv`,
   which stores the current base URI and set of namespace prefixes.

The complete API is declared in ``serd.h``:

.. code-block:: c

   #include <serd/serd.h>

