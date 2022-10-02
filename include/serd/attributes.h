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

// SERD_API must be used to decorate things in the public API
#ifndef SERD_API
#  if defined(_WIN32) && !defined(SERD_STATIC) && defined(SERD_INTERNAL)
#    define SERD_API __declspec(dllexport)
#  elif defined(_WIN32) && !defined(SERD_STATIC)
#    define SERD_API __declspec(dllimport)
#  elif defined(__GNUC__)
#    define SERD_API __attribute__((visibility("default")))
#  else
#    define SERD_API
#  endif
#endif

// GCC function attributes
#ifdef __GNUC__
#  define SERD_ALWAYS_INLINE_FUNC __attribute__((always_inline))
#  define SERD_CONST_FUNC __attribute__((const))
#  define SERD_LOG_FUNC(fmt, arg1) __attribute__((format(printf, fmt, arg1)))
#  define SERD_MALLOC_FUNC __attribute__((malloc))
#  define SERD_NODISCARD __attribute__((warn_unused_result))
#  define SERD_PURE_FUNC __attribute__((pure))
#else
#  define SERD_ALWAYS_INLINE_FUNC  ///< Should absolutely always be inlined
#  define SERD_CONST_FUNC          ///< Only reads its parameters
#  define SERD_LOG_FUNC(fmt, arg1) ///< Has printf-like parameters
#  define SERD_MALLOC_FUNC         ///< Allocates memory
#  define SERD_NODISCARD           ///< Returns a value that must be used
#  define SERD_PURE_FUNC           ///< Only reads memory
#endif

// Clang nullability annotations
#if defined(__clang__) && __clang_major__ >= 7
#  define SERD_NONNULL _Nonnull
#  define SERD_NULLABLE _Nullable
#  define SERD_ALLOCATED _Null_unspecified
#  define SERD_UNSPECIFIED _Null_unspecified
#else
#  define SERD_NONNULL     ///< A non-null pointer
#  define SERD_NULLABLE    ///< A nullable pointer
#  define SERD_ALLOCATED   ///< An allocated (possibly null) pointer
#  define SERD_UNSPECIFIED ///< A pointer with unspecified nullability
#endif

/// A pure function in the public API that only reads memory
#define SERD_PURE_API SERD_API SERD_PURE_FUNC

/// A const function in the public API that is pure and only reads parameters
#define SERD_CONST_API SERD_API SERD_CONST_FUNC

/**
   @}
*/

#endif // SERD_ATTRIBUTES_H
