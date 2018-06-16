// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_OPTIONAL_HPP
#define SERD_OPTIONAL_HPP

#include <cassert>
#include <type_traits>
#include <utility>

namespace serd {

/**
   @defgroup serdpp_optional Optional
   @ingroup serdpp
   @{
*/

struct ConstructNullOptional {};

/// Special tag for constructing an unset Optional
struct Nullopt {
  enum class Construct { internal };

  explicit constexpr Nullopt(Construct) {}
};

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
template<class T>
class Optional
{
public:
  /// The type of the underlying C object
  using CType = typename T::CType;

  /// Constructs an empty optional
  Optional() = default;

  /// Constructs an empty optional
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Optional(Nullopt) {}

  /// Constructs an optional that contains the given value
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Optional(T value)
    : _value{std::move(value)}
  {}

  /// Constructs an optional that contains a converted value
  template<
    typename U,
    typename = typename std::enable_if<std::is_convertible<U, T>::value>::type>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Optional(U&& value)
    : _value{std::forward<U>(value)}
  {}

  /// Destroys any contained value
  void reset() { _value = T{nullptr}; }

  /// Accesses the contained value
  const T& operator*() const
  {
    assert(_value.cobj());
    return _value;
  }

  /// Accesses the contained value
  T& operator*()
  {
    assert(_value.cobj());
    return _value;
  }

  /// Accesses the contained value
  const T* operator->() const
  {
    assert(_value.cobj());
    return &_value;
  }

  /// Accesses the contained value
  T* operator->()
  {
    assert(_value.cobj());
    return &_value;
  }

  /// Tests if optional objects are equal
  bool operator==(const Optional& optional)
  {
    return (!*this && !optional) ||
           (*this && optional && _value == optional._value);
  }

  /// Tests if optional objects are not equal
  bool operator!=(const Optional& optional) { return !operator==(optional); }

  /// Returns true if this optional contains a value
  explicit operator bool() const { return _value.cobj(); }

  /// Returns true if this optional does not contain a value
  bool operator!() const { return !_value.cobj(); }

  /// Return a pointer to the underlying C object, or null
  CType* cobj() { return _value.cobj(); }

  /// Return a pointer to the underlying C object, or null
  const CType* cobj() const { return _value.cobj(); }

private:
  T _value{nullptr};
};

/// Creates an optional object from `value`
template<class T>
constexpr Optional<std::decay_t<T>>
make_optional(T&& value)
{
  return Optional<T>{std::forward<T>(value)};
}

/// Creates an optional object with a value constructed in-place from `args`
template<class T, class... Args>
constexpr Optional<T>
make_optional(Args&&... args)
{
  return Optional<T>{std::forward<Args>(args)...};
}

/// Constant that represents an empty optional
static constexpr Nullopt nullopt{Nullopt::Construct::internal};

/**
   @}
*/

} // namespace serd

#endif // SERD_OPTIONAL_HPP
