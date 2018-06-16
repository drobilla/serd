/*
  Copyright 2019-2020 David Robillard <http://drobilla.net>

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

#ifndef SERD_DETAIL_OPTIONAL_HPP
#define SERD_DETAIL_OPTIONAL_HPP

#include <cassert>
#include <type_traits>
#include <utility>

namespace serd {
namespace detail {

struct ConstructNullOptional {};

/**
   A simple optional wrapper around a wrapped type with a pointer-like API

   This works like a typical optional type, but only works with Wrapper types,
   and exploits the fact that these are interally just pointers to avoid adding
   space overhead for an "is_set" flag, like a generic optional class would.

   Types must explicitly opt-in to being optional by providing a constructor
   that takes a single ContructNullOptional argument.  This constructor should
   only be used by the Optional implementation, which guarantees that such an
   object will not be used except by calling its cobj() method.
*/
template<typename T>
class Optional
{
public:
  using CType = typename T::CType;

  Optional()
    : _value(nullptr)
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Optional(T value)
    : _value(std::move(value))
  {}

  template<
    typename U,
    typename = typename std::enable_if<std::is_convertible<U, T>::value>::type>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Optional(U&& value)
    : _value(std::forward<U>(value))
  {}

  void reset() { _value = T{nullptr}; }

  const T& operator*() const
  {
    assert(_value.cobj());
    return _value;
  }

  T& operator*()
  {
    assert(_value.cobj());
    return _value;
  }

  const T* operator->() const
  {
    assert(_value.cobj());
    return &_value;
  }

  T* operator->()
  {
    assert(_value.cobj());
    return &_value;
  }

  bool operator==(const Optional& optional)
  {
    return (!*this && !optional) ||
           (*this && optional && _value == optional._value);
  }

  bool operator!=(const Optional& optional) { return !operator==(optional); }

  explicit operator bool() const { return _value.cobj(); }
  bool     operator!() const { return !_value.cobj(); }

  inline CType*       cobj() { return _value.cobj(); }
  inline const CType* cobj() const { return _value.cobj(); }

private:
  T _value;
};

} // namespace detail

template<class T>
constexpr detail::Optional<T>
make_optional(T&& value)
{
  return detail::Optional<T>{std::forward<T>(value)};
}

template<class T, class... Args>
constexpr detail::Optional<T>
make_optional(Args&&... args)
{
  return detail::Optional<T>{std::forward<Args>(args)...};
}

} // namespace serd

#endif // SERD_DETAIL_OPTIONAL_HPP
