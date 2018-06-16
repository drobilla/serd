// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_DETAIL_WRAPPER_HPP
#define SERD_DETAIL_WRAPPER_HPP

// IWYU pragma: no_include "serd/serd.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace serd {

/// Utility template for a mutable type which removes const if necessary
template<class T>
class Optional;

/// Detail namespace
namespace detail {

/**
   @defgroup serdpp_detail Serd C++ API details
   Internal C++ wrapper details that should not be used directly by clients.
   @ingroup serdpp
   @{
*/

/// Utility template for a mutable type which removes const if necessary
template<class T>
using Mutable = typename std::remove_const_t<T>;

/// Generic C++ wrapper for a C object
template<class T, class Deleter>
class Wrapper
{
public:
  using CType = T;

  explicit Wrapper(T* ptr)
    : _ptr{ptr, Deleter{}}
  {}

  Wrapper(T* ptr, Deleter deleter)
    : _ptr{ptr, std::move(deleter)}
  {}

  explicit Wrapper(std::unique_ptr<T, Deleter> ptr)
    : _ptr{std::move(ptr)}
  {}

  Wrapper(Wrapper&&) noexcept = default;
  Wrapper& operator=(Wrapper&&) noexcept = default;

  Wrapper(const Wrapper&) = delete;
  Wrapper& operator=(const Wrapper&) = delete;

  ~Wrapper() = default;

  /// Return a pointer to the underlying C object
  T* cobj() { return _ptr.get(); }

  /// Return a pointer to the underlying C object
  const T* cobj() const { return _ptr.get(); }

protected:
  friend class Optional<T>;

  explicit Wrapper(std::nullptr_t)
    : _ptr{nullptr}
  {}

  void reset() { _ptr.reset(); }

  std::unique_ptr<T, Deleter> _ptr;
};

/**
   @}
*/

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_WRAPPER_HPP
