// Copyright 2019-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STRINGVIEW_HPP
#define SERD_STRINGVIEW_HPP

#include "serd/serd.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <string>

namespace serd {

/**
   @defgroup serdpp_string_view String View
   @ingroup serdpp
   @{
*/

/**
   Immutable slice of a string.

   This is a minimal implementation that is compatible with std::string_view
   and std::string for most basic use cases.  This could be replaced with
   std::string_view once C++17 support can be relied on.
*/
class StringView
{
public:
  using char_type       = char;
  using size_type       = size_t;
  using traits_type     = std::char_traits<char>;
  using value_type      = char;
  using pointer         = value_type*;
  using const_pointer   = const value_type*;
  using reference       = value_type&;
  using const_reference = const value_type&;
  using iterator        = const char*;
  using const_iterator  = const char*;

  static constexpr size_type npos = static_cast<size_t>(-1);

  constexpr StringView() noexcept = default;

  constexpr StringView(const char* const str, const size_t len) noexcept
    : _str{str}
    , _len{len}
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  StringView(const char* const str) noexcept
    : _str{str}
    , _len{str ? strlen(str) : 0}
  {}

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  StringView(const std::string& str) noexcept
    : _str{str.c_str()}
    , _len{str.length()}
  {}

  constexpr size_t      size() const { return _len; }
  constexpr size_t      length() const { return _len; }
  constexpr bool        empty() const { return _len == 0; }
  constexpr const char* c_str() const { return _str; }
  constexpr const char* data() const { return _str; }
  constexpr const char& front() const { return _str[0]; }
  constexpr const char& back() const { return _str[_len - 1]; }

  constexpr const_iterator begin() const { return _str; }
  constexpr const_iterator end() const { return _str + _len; }
  constexpr const_iterator cbegin() const { return begin(); }
  constexpr const_iterator cend() const { return end(); }

  constexpr const char& operator[](const size_t pos) const { return _str[pos]; }

  const char& at(const size_t pos) const
  {
    if (pos >= size()) {
      throw std::out_of_range("serd::StringView::at pos");
    }

    return _str[pos];
  }

  StringView substr(const size_t pos) const
  {
    if (pos > size()) {
      throw std::out_of_range("serd::StringView::substr pos");
    }

    return StringView{data() + pos, size() - pos};
  }

  StringView substr(const size_t pos, const size_t n) const
  {
    if (pos > size()) {
      throw std::out_of_range("serd::StringView::substr pos");
    }

    return StringView{data() + pos, std::min(size() - pos, n)};
  }

  int compare(StringView rhs) const noexcept
  {
    if (!data() && !rhs.data()) {
      return 0;
    }

    if (!data()) {
      return -1;
    }

    if (!rhs.data()) {
      return 1;
    }

    const size_type len = std::min(size(), rhs.size());
    const int       cmp = strncmp(data(), rhs.data(), len);

    if (cmp) {
      return cmp;
    }

    if (size() == rhs.size()) {
      return 0;
    }

    if (size() < rhs.size()) {
      return -1;
    }

    return 1;
  }

  template<class Alloc = std::allocator<char>>
  std::basic_string<char, traits_type, Alloc> str() const
  {
    return std::basic_string<char, traits_type, Alloc>(data(), size(), Alloc{});
  }

  template<class Alloc = std::allocator<char>>
  std::basic_string<char, traits_type, Alloc> str(const Alloc& alloc) const
  {
    return std::basic_string<char, traits_type, Alloc>(data(), size(), alloc);
  }

  explicit operator std::string() const { return str(); }

  explicit operator const char*() const { return _str; }

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  operator ZixStringView() const { return ZixStringView{_str, _len}; }

private:
  const char* const _str{};
  const size_t      _len{};
};

inline bool
operator==(const StringView& lhs, const StringView& rhs)
{
  return !lhs.compare(rhs);
}

inline bool
operator==(const StringView& lhs, const std::string& rhs)
{
  return lhs.length() == rhs.length() &&
         !strncmp(lhs.c_str(), rhs.c_str(), lhs.length());
}

inline bool
operator==(const StringView& lhs, const char* rhs)
{
  return !strncmp(lhs.c_str(), rhs, lhs.length());
}

inline bool
operator!=(const StringView& lhs, const StringView& rhs)
{
  return lhs.compare(rhs);
}

inline bool
operator!=(const StringView& lhs, const std::string& rhs)
{
  return lhs.length() != rhs.length() ||
         !!strncmp(lhs.c_str(), rhs.c_str(), lhs.length());
}

inline bool
operator!=(const StringView& lhs, const char* rhs)
{
  return !!strncmp(lhs.c_str(), rhs, lhs.length());
}

inline bool
operator<(const StringView& lhs, const StringView& rhs)
{
  return lhs.compare(rhs) < 0;
}

inline bool
operator<(const StringView& lhs, const std::string& rhs)
{
  return lhs.c_str() < StringView(rhs);
}

inline bool
operator<(const StringView& lhs, const char* rhs)
{
  return strncmp(lhs.c_str(), rhs, lhs.length()) < 0;
}

inline std::ostream&
operator<<(std::ostream& os, const StringView& str)
{
  os.write(str.data(), static_cast<std::streamsize>(str.size()));
  return os;
}

/**
   @}
*/

} // namespace serd

#endif // SERD_STRINGVIEW_HPP
