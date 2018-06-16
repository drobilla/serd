// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_DETAIL_STATICWRAPPER_HPP
#define SERD_DETAIL_STATICWRAPPER_HPP

// IWYU pragma: no_include "serd/serd.h"

#include "serd/detail/Wrapper.hpp"

#include <type_traits>

namespace serd {
namespace detail {

/**
   Simple overhead-free deleter for a C object.

   Can be used with const or mutable pointers, but only mutable pointers will
   be freed.  In other words, mutability implies ownership, and this can not
   handle unowned mutable pointers.

   @ingroup serdpp_detail
*/
template<class T, void Free(Mutable<T>*)>
struct StaticDeleter {
  template<class = std::enable_if<!std::is_const<T>::value>>
  void operator()(Mutable<T>* const ptr)
  {
    Free(ptr);
  }

  template<class = std::enable_if<std::is_const<T>::value>>
  void operator()(const T*)
  {}
};

/**
   Simple overhead-free wrapper for a C object that can free itself.

   @ingroup serdpp_detail
*/
template<class T, void Free(std::remove_const_t<T>*)>
class StaticWrapper : public Wrapper<T, StaticDeleter<T, Free>>
{
public:
  explicit StaticWrapper(T* const ptr)
    : Wrapper<T, StaticDeleter<T, Free>>{ptr}
  {}
};

/**
   Simple overhead-free deleter for a C object.

   Can be used with const or mutable pointers, but only mutable pointers will
   be freed.  In other words, mutability implies ownership, and this can not
   handle unowned mutable pointers.

   @ingroup serdpp_detail
*/
template<class T, void Free(SerdAllocator*, Mutable<T>*)>
struct StaticAllocatedDeleter {
  template<class = std::enable_if<!std::is_const<T>::value>>
  void operator()(Mutable<T>* const ptr)
  {
    Free(nullptr, ptr);
  }

  template<class = std::enable_if<std::is_const<T>::value>>
  void operator()(const T*)
  {}
};

/**
   Simple overhead-free wrapper for a C object that uses an allocator.

   @ingroup serdpp_detail
*/
template<class T, void Free(SerdAllocator*, std::remove_const_t<T>*)>
class StaticAllocatedWrapper
  : public Wrapper<T, StaticAllocatedDeleter<T, Free>>
{
public:
  explicit StaticAllocatedWrapper(T* const ptr)
    : Wrapper<T, StaticAllocatedDeleter<T, Free>>{ptr}
  {}
};

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_STATICWRAPPER_HPP
