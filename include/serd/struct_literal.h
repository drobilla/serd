// Copyright 2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRUCT_LITERAL_H
#define SERD_STRUCT_LITERAL_H

/**
   @defgroup serd_struct_literal Struct Literals
   @ingroup serd_utilities
   @{
*/

/**
   Macro for writing portable struct literals.

   This is used to construct literals in public convenience functions, to work
   around inconsistent C99 and C++11 syntax.
*/
#ifdef __cplusplus
#  define SERD_STRUCT_LITERAL(T, ...) (T{__VA_ARGS__})
#else
#  define SERD_STRUCT_LITERAL(T, ...) ((T){__VA_ARGS__})
#endif

/**
   @}
*/

#endif // SERD_STRUCT_LITERAL_H
