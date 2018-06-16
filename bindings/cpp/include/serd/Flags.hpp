// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_FLAGS_HPP
#define SERD_FLAGS_HPP

#include <type_traits>

namespace serd {

/**
   @defgroup serdpp_flags Flags
   @ingroup serdpp
   @{
*/

/**
   Type-safe bit flags

   This is a minimal interface for type-safe bit flags, which only allows
   values from the corresponding flags enum to be set.  It functions like a
   normal unsigned integer bit field, but attempting to get or set a flag with
   the incorrect type will fail to compile.

   @tparam Flag Strong enumeration type for flag values.
*/
template<class Flag>
class Flags
{
public:
  static_assert(std::is_enum<Flag>::value, "");

  using Value = std::make_unsigned_t<std::underlying_type_t<Flag>>;

  /// Construct with no flags set
  constexpr Flags() noexcept = default;

  /// Construct from a raw bit field value
  constexpr explicit Flags(const Value value) noexcept
    : _value{value}
  {}

  /// Construct from a single flag
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  constexpr Flags(const Flag f) noexcept
    : _value{static_cast<Value>(f)}
  {}

  /// Set a flag
  constexpr Flags operator|(const Flag rhs) const noexcept
  {
    return Flags{_value | static_cast<Value>(rhs)};
  }

  /// Set all the flags from another set of flags
  constexpr Flags operator|(const Flags rhs) const noexcept
  {
    return Flags{_value | rhs._value};
  }

  /// Return true if only the given flag is set
  constexpr bool operator==(const Flag rhs) const noexcept
  {
    return _value == static_cast<Value>(rhs);
  }

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  constexpr operator Value() const noexcept { return _value; }

private:
  Value _value{};
};

/// Make a new Flags by combining two individual flags
template<class Flag>
inline constexpr Flags<Flag>
operator|(const Flag lhs, const Flag rhs) noexcept
{
  return Flags<Flag>{lhs} | rhs;
}

/**
   @}
*/

} // namespace serd

#endif // SERD_FLAGS_HPP
