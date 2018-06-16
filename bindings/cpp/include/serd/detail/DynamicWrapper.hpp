// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_DETAIL_DYNAMICWRAPPER_HPP
#define SERD_DETAIL_DYNAMICWRAPPER_HPP

// IWYU pragma: no_include "serd/serd.h"

#include "serd/detail/Wrapper.hpp"

#include <type_traits>

namespace serd {
namespace detail {

/**
   Ownership for `DynamicDeleter`.

   @ingroup serdpp_detail
*/
enum class Ownership {
  owned, ///< This pointer owns the data and must delete it
  view,  ///< This pointer is just a view and must not delete the data
};

/**
   Deleter for a C object that can handle dynamic ownership.

   Unlike StaticDeleter, this can be used to handle non-owned references to
   mutable objects, at the cost of an extra word for tracking the ownership
   (since constness in the type can't convey this information).

   @ingroup serdpp_detail
*/
template<class T, void Free(Mutable<T>*)>
struct DynamicDeleter {
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  DynamicDeleter(const Ownership ownership)
    : _ownership{ownership}
  {}

  template<class = std::enable_if<!std::is_const<T>::value>>
  void operator()(Mutable<T>* const ptr)
  {
    if (_ownership == Ownership::owned) {
      Free(ptr);
    }
  }

  template<class = std::enable_if<std::is_const<T>::value>>
  void operator()(const T*)
  {}

private:
  Ownership _ownership;
};

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_DYNAMICWRAPPER_HPP
