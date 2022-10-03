// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_ATTRIBUTES_H
#define SERD_ATTRIBUTES_H

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

#if defined(_WIN32) && !defined(SERD_STATIC) && defined(SERD_INTERNAL)
#  define SERD_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(SERD_STATIC)
#  define SERD_API __declspec(dllimport)
#elif defined(__GNUC__)
#  define SERD_API __attribute__((visibility("default")))
#else
#  define SERD_API
#endif

#ifdef __GNUC__
#  define SERD_ALWAYS_INLINE_FUNC __attribute__((always_inline))
#  define SERD_CONST_FUNC __attribute__((const))
#  define SERD_MALLOC_FUNC __attribute__((malloc))
#  define SERD_PURE_FUNC __attribute__((pure))
#  define SERD_NODISCARD __attribute__((warn_unused_result))
#else
#  define SERD_ALWAYS_INLINE_FUNC
#  define SERD_CONST_FUNC
#  define SERD_MALLOC_FUNC
#  define SERD_PURE_FUNC
#  define SERD_NODISCARD
#endif

#if defined(__MINGW32__)
#  define SERD_LOG_FUNC(fmt, a) __attribute__((format(gnu_printf, fmt, a)))
#elif defined(__GNUC__)
#  define SERD_LOG_FUNC(fmt, a) __attribute__((format(printf, fmt, a)))
#else
#  define SERD_LOG_FUNC(fmt, a)
#endif

#if defined(__clang__) && __clang_major__ >= 7
#  define SERD_NONNULL _Nonnull
#  define SERD_NULLABLE _Nullable
#  define SERD_ALLOCATED _Null_unspecified
#else
#  define SERD_NONNULL
#  define SERD_NULLABLE
#  define SERD_ALLOCATED
#endif

/// A pure function in the public API that only reads memory
#define SERD_PURE_API \
  SERD_API            \
  SERD_PURE_FUNC

/// A const function in the public API that is pure and only reads parameters
#define SERD_CONST_API \
  SERD_API             \
  SERD_CONST_FUNC

/// A malloc function in the public API that returns allocated memory
#define SERD_MALLOC_API \
  SERD_API              \
  SERD_MALLOC_FUNC

/**
   @}
*/

#endif // SERD_ATTRIBUTES_H
