// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ATTRIBUTES_H
#define SERD_ATTRIBUTES_H

#include "zix/attributes.h"

/**
   @defgroup serd_attributes Attributes
   @ingroup serd_library
   @{
*/

#ifdef __cplusplus
#  ifdef __GNUC__

#    define SERD_BEGIN_DECLS                                                \
      _Pragma("GCC diagnostic push")                                        \
      _Pragma("GCC diagnostic ignored \"-Wzero-as-null-pointer-constant\"") \
      extern "C" {

#    define SERD_END_DECLS \
      }                    \
      _Pragma("GCC diagnostic pop")

#  else
#    define SERD_BEGIN_DECLS extern "C" {
#    define SERD_END_DECLS }
#  endif
#else
#  define SERD_BEGIN_DECLS
#  define SERD_END_DECLS
#endif

// Symbols in the public API
#if defined(_WIN32) && !defined(SERD_STATIC) && defined(SERD_INTERNAL)
#  define SERD_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(SERD_STATIC)
#  define SERD_API __declspec(dllimport)
#elif defined(__GNUC__)
#  define SERD_API __attribute__((visibility("default")))
#else
#  define SERD_API
#endif

/// A function that returns a value which shouldn't be ignored
#ifdef __GNUC__
#  define SERD_NODISCARD __attribute__((warn_unused_result))
#else
#  define SERD_NODISCARD
#endif

/// A function that takes a printf-style format string and arguments
#if defined(__MINGW32__)
#  define SERD_LOG_FUNC(fmt, a) __attribute__((format(gnu_printf, fmt, a)))
#elif defined(__GNUC__)
#  define SERD_LOG_FUNC(fmt, a) __attribute__((format(printf, fmt, a)))
#else
#  define SERD_LOG_FUNC(fmt, a)
#endif

/// A pure function in the public API that only reads memory
#define SERD_PURE_API \
  SERD_API            \
  ZIX_PURE_FUNC

/// A const function in the public API that is pure and only reads parameters
#define SERD_CONST_API \
  SERD_API             \
  ZIX_CONST_FUNC

/// A malloc function in the public API that returns allocated memory
#define SERD_MALLOC_API \
  SERD_API              \
  ZIX_MALLOC_FUNC

/**
   @}
*/

#endif // SERD_ATTRIBUTES_H
