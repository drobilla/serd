// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_DETAIL_COPYABLE_HPP
#define SERD_DETAIL_COPYABLE_HPP

// IWYU pragma: no_include "serd/serd.h"

#include "serd/detail/Wrapper.hpp"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace serd {
namespace detail {

/**
   @addtogroup serdpp_detail
   @{
*/

/// Copy function for an allocator-managed C object
template<class T>
using CopyFunc = Mutable<T>* (*)(ZixAllocator*, const T*);

/// Equality comparison function for C objects
template<class T>
using EqualsFunc = bool (*)(const T*, const T*);

template<class T, CopyFunc<T> copy_func>
typename std::enable_if_t<std::is_const<T>::value, T>*
copy_cobj(const T* ptr)
{
  return ptr; // Constant wrapper, do not copy
}

template<class T, CopyFunc<T> copy_func>
typename std::enable_if_t<!std::is_const<T>::value, T>*
copy_cobj(const T* ptr)
{
  return ptr ? copy_func(nullptr, ptr) : nullptr; // Mutable wrapper, copy
}

/**
   Generic wrapper for a "basic" copyable object.

   This wraps objects with simple ownership semantics where a const pointer is
   never owned, and a mutable pointer is owned.  This has no space overhead
   compared to a raw pointer since the ownership is encoded in the type.
*/
template<class T, class Deleter, CopyFunc<T> copy, EqualsFunc<T> equals>
class Copyable : public Wrapper<T, Deleter>
{
public:
  using Base = Wrapper<T, Deleter>;

  explicit Copyable(T* ptr)
    : Base{ptr}
  {}

  Copyable(const Copyable& wrapper)
    : Base(copy_cobj<T, copy>(wrapper.cobj()))
  {}

  template<class U, class UDeleter>
  explicit Copyable(const Copyable<U, UDeleter, copy, equals>& wrapper)
    : Base(copy_cobj<T, copy>(wrapper.cobj()))
  {}

  Copyable(Copyable&&) noexcept            = default;
  Copyable& operator=(Copyable&&) noexcept = default;

  ~Copyable() noexcept = default;

  Copyable& operator=(const Copyable& wrapper)
  {
    if (&wrapper != this) {
      this->_ptr =
        std::unique_ptr<T, Deleter>(copy_cobj<T, copy>(wrapper.cobj()));
    }
    return *this;
  }

  template<class U, class UDeleter, Mutable<T>* UCopy(ZixAllocator*, const T*)>
  bool operator==(const Copyable<U, UDeleter, UCopy, equals>& wrapper) const
  {
    return equals(this->cobj(), wrapper.cobj());
  }

  template<class U, class UDeleter, Mutable<T>* UCopy(ZixAllocator*, const T*)>
  bool operator!=(const Copyable<U, UDeleter, UCopy, equals>& wrapper) const
  {
    return !operator==(wrapper);
  }
};

/**
   @}
*/

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_COPYABLE_HPP
