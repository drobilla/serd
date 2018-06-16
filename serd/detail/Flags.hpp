/*
  Copyright 2019 David Robillard <http://drobilla.net>

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

#ifndef SERD_DETAIL_FLAGS_HPP
#define SERD_DETAIL_FLAGS_HPP

#include <type_traits>

namespace serd {
namespace detail {

/**
   Type-safe bit flags

  This is a minimal interface for a type-safe bit flags field, which only
  allows values from the given enum to be set.

  @tparam Flag Enum class of flag values.
*/
template <typename Flag>
class Flags
{
public:
	static_assert(std::is_enum<Flag>::value, "");

	using FlagUnderlyingType = typename std::underlying_type<Flag>::type;
	using Value = typename std::make_unsigned<FlagUnderlyingType>::type;

	constexpr Flags() noexcept : _value(0) {}
	constexpr explicit Flags(const Value value) noexcept : _value{value} {}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	constexpr Flags(const Flag f) noexcept
	    : _value(static_cast<Value>(f))
	{
	}

	constexpr Flags operator|(const Flag rhs) const noexcept
	{
		return Flags{_value | static_cast<Value>(rhs)};
	}

	constexpr Flags operator|(const Flags rhs) const noexcept
	{
		return Flags{_value | rhs._value};
	}

	constexpr bool operator==(const Flag rhs) const noexcept
	{
		return _value == static_cast<Value>(rhs);
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	constexpr operator Value() const noexcept { return _value; }

private:
	Value _value{};
};

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_FLAGS_HPP
