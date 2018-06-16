/*
  Copyright 2019-2021 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef SERD_DETAIL_COPYABLE_HPP
#define SERD_DETAIL_COPYABLE_HPP

// IWYU pragma: no_include "serd/serd.h"

#include "serd/detail/DynamicWrapper.hpp"
#include "serd/detail/StaticWrapper.hpp"
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

/// Copy function for a C object
template<class T>
using CopyFunc = T* (*)(const T*);

template<class T, std::remove_const_t<T>* Copy(SerdAllocator*, const T*)>
typename std::enable_if_t<std::is_const<T>::value, T>*
copy(const T* ptr)
{
  return ptr; // Making a view (const reference), do not copy
}

template<class T, std::remove_const_t<T>* Copy(SerdAllocator*, const T*)>
typename std::enable_if_t<!std::is_const<T>::value, T>*
copy(const T* ptr)
{
  return ptr ? Copy(nullptr, ptr) : nullptr; // Making a mutable wrapper, copy
}

/**
   Generic wrapper for a "basic" copyable object.

   This wraps objects with simple ownership semantics where a const pointer is
   never owned, and a mutable pointer is owned.  This has no space overhead
   compared to a raw pointer since the ownership is encoded in the type.
*/
template<class T,
         Mutable<T>* Copy(SerdAllocator*, const T*),
         bool        Equals(const T*, const T*),
         void        Free(Mutable<T>*)>
class StaticCopyable : public StaticWrapper<T, Free>
{
public:
  using Deleter = StaticDeleter<T, Free>;
  using Base    = StaticWrapper<T, Free>;

  explicit StaticCopyable(T* ptr)
    : Base{ptr}
  {}

  StaticCopyable(const StaticCopyable& wrapper)
    : Base(copy<T, Copy>(wrapper.cobj()))
  {}

  template<class U, void UFree(Mutable<U>*)>
  explicit StaticCopyable(const StaticCopyable<U, Copy, Equals, UFree>& wrapper)
    : Base(copy<T, Copy>(wrapper.cobj()))
  {}

  StaticCopyable(StaticCopyable&&) noexcept = default;
  ~StaticCopyable() noexcept                = default;

  StaticCopyable& operator=(StaticCopyable&&) noexcept = default;

  StaticCopyable& operator=(const StaticCopyable& wrapper)
  {
    if (&wrapper != this) {
      this->_ptr = std::unique_ptr<T, Deleter>(copy<T, Copy>(wrapper.cobj()));
    }
    return *this;
  }

  template<class U>
  bool operator==(const StaticCopyable<U, Copy, Equals, Free>& wrapper) const
  {
    return Equals(this->cobj(), wrapper.cobj());
  }

  template<class U>
  bool operator!=(const StaticCopyable<U, Copy, Equals, Free>& wrapper) const
  {
    return !operator==(wrapper);
  }
};

/**
   Generic wrapper for a "basic" copyable object.

   This wraps objects with simple ownership semantics where a const pointer is
   never owned, and a mutable pointer is owned.  This has no space overhead
   compared to a raw pointer since the ownership is encoded in the type.
*/
template<class T,
         Mutable<T>* Copy(SerdAllocator*, const T*),
         bool        Equals(const T*, const T*),
         void        Free(SerdAllocator*, Mutable<T>*)>
class StaticAllocatedCopyable : public StaticAllocatedWrapper<T, Free>
{
public:
  using Deleter = StaticAllocatedDeleter<T, Free>;
  using Base    = StaticAllocatedWrapper<T, Free>;

  explicit StaticAllocatedCopyable(T* ptr)
    : Base{ptr}
  {}

  StaticAllocatedCopyable(const StaticAllocatedCopyable& wrapper)
    : Base(copy<T, Copy>(wrapper.cobj()))
  {}

  template<class U, void UFree(SerdAllocator*, Mutable<U>*)>
  explicit StaticAllocatedCopyable(
    const StaticAllocatedCopyable<U, Copy, Equals, UFree>& wrapper)
    : Base(copy<T, Copy>(wrapper.cobj()))
  {}

  StaticAllocatedCopyable(StaticAllocatedCopyable&&) noexcept = default;
  ~StaticAllocatedCopyable() noexcept                         = default;

  StaticAllocatedCopyable& operator=(StaticAllocatedCopyable&&) noexcept =
    default;

  StaticAllocatedCopyable& operator=(const StaticAllocatedCopyable& wrapper)
  {
    if (&wrapper != this) {
      this->_ptr = std::unique_ptr<T, Deleter>(copy<T, Copy>(wrapper.cobj()));
    }
    return *this;
  }

  template<class U>
  bool operator==(
    const StaticAllocatedCopyable<U, Copy, Equals, Free>& wrapper) const
  {
    return Equals(this->cobj(), wrapper.cobj());
  }

  template<class U>
  bool operator!=(
    const StaticAllocatedCopyable<U, Copy, Equals, Free>& wrapper) const
  {
    return !operator==(wrapper);
  }
};

/**
   Wrapper for a "dynamic" copyable C object.

   This wraps objects that require dynamic tracking of the ownership.
*/
template<class T,
         Mutable<T>* Copy(SerdAllocator*, const T*),
         bool        Equals(const T*, const T*),
         void        Free(Mutable<T>*)>
class DynamicCopyable : public Wrapper<T, DynamicDeleter<T, Free>>
{
public:
  using Deleter = DynamicDeleter<T, Free>;
  using Base    = Wrapper<T, Deleter>;

  explicit DynamicCopyable(std::unique_ptr<T, Deleter> ptr)
    : Base{std::move(ptr)}
  {}

  DynamicCopyable(const DynamicCopyable& wrapper)
    : Base{Copy(nullptr /* FIXME */, wrapper.cobj()), Ownership::owned}
  {}

  DynamicCopyable(DynamicCopyable&&) noexcept = default;
  DynamicCopyable& operator=(DynamicCopyable&&) noexcept = default;

  ~DynamicCopyable() noexcept = default;

  DynamicCopyable& operator=(const DynamicCopyable& wrapper)
  {
    if (&wrapper != this) {
      this->_ptr = std::unique_ptr<T, Deleter>(
        Copy(nullptr /* FIXME */, wrapper.cobj()), Ownership::owned);
    }

    return *this;
  }

  template<class U>
  bool operator==(const DynamicCopyable<U, Copy, Equals, Free>& wrapper) const
  {
    return Equals(this->cobj(), wrapper.cobj());
  }

  template<class U>
  bool operator!=(const DynamicCopyable<U, Copy, Equals, Free>& wrapper) const
  {
    return !operator==(wrapper);
  }

protected:
  explicit DynamicCopyable(std::nullptr_t)
    : Base(nullptr)
  {}
};

/**
   @}
*/

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_COPYABLE_HPP
