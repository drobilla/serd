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

#ifndef SERD_DETAIL_COPYABLE_HPP
#define SERD_DETAIL_COPYABLE_HPP

#include "serd/detail/Wrapper.hpp"

#include <cstddef>
#include <memory>
#include <type_traits>

namespace serd {
namespace detail {

/// Copy function for a C object
template <class T>
using CopyFunc = T* (*)(const T*);

template <class T, Mutable<T>* Copy(const T*)>
typename std::enable_if<std::is_const<T>::value, T>::type*
copy(const T* ptr)
{
	return ptr; // Making a view (const reference), do not copy
}

template <class T, Mutable<T>* Copy(const T*)>
typename std::enable_if<!std::is_const<T>::value, T>::type*
copy(const T* ptr)
{
	return Copy(ptr); // Making a mutable wrapper, copy
}

template <class T,
          Mutable<T>* Copy(const T*),
          bool        Equals(const T*, const T*),
          void        Free(Mutable<T>*)>
class BasicCopyable : public Wrapper<T, BasicDeleter<T, Free>>
{
public:
	using Deleter = BasicDeleter<T, Free>;
	using Base    = Wrapper<T, Deleter>;

	explicit BasicCopyable(T* ptr) : Base{ptr} {}

	BasicCopyable(const BasicCopyable& wrapper)
	    : Base(copy<T, Copy>(wrapper.cobj()))
	{
	}

	template <class U, void UFree(Mutable<U>*)>
	explicit BasicCopyable(const BasicCopyable<U, Copy, Equals, UFree>& wrapper)
	    : Base(copy<T, Copy>(wrapper.cobj()))
	{
	}

	BasicCopyable(BasicCopyable&&) noexcept = default;
	~BasicCopyable() noexcept               = default;

	BasicCopyable& operator=(BasicCopyable&&) noexcept = default;

	BasicCopyable& operator=(const BasicCopyable& wrapper)
	{
		this->_ptr = std::unique_ptr<T, Deleter>(copy<T, Copy>(wrapper.cobj()));
		return *this;
	}

	template <class U>
	bool operator==(const BasicCopyable<U, Copy, Equals, Free>& wrapper) const
	{
		return Equals(this->cobj(), wrapper.cobj());
	}

	template <class U>
	bool operator!=(const BasicCopyable<U, Copy, Equals, Free>& wrapper) const
	{
		return !operator==(wrapper);
	}
};

/// Generic C++ wrapper for a copyable C object
template <class T,
          Mutable<T>* Copy(const T*),
          bool        Equals(const T*, const T*),
          void        Free(Mutable<T>*)>
class DynamicCopyable : public Wrapper<T, DynamicDeleter<T, Free>>
{
public:
	using Deleter = DynamicDeleter<T, Free>;
	using Base    = Wrapper<T, Deleter>;
	using UPtr    = typename Base::UPtr;

	explicit DynamicCopyable(UPtr ptr) : Base(std::move(ptr)) {}

	DynamicCopyable(const DynamicCopyable& wrapper)
	    : Base(Copy(wrapper.cobj()), Ownership::owned)
	{
	}

	DynamicCopyable(DynamicCopyable&&) noexcept = default;
	DynamicCopyable& operator=(DynamicCopyable&&) noexcept = default;

	~DynamicCopyable() noexcept = default;

	DynamicCopyable& operator=(const DynamicCopyable& wrapper)
	{
		this->_ptr = std::unique_ptr<T, Deleter>(Copy(wrapper.cobj()),
		                                         Ownership::owned);
		return *this;
	}

	template <class U>
	bool operator==(const DynamicCopyable<U, Copy, Equals, Free>& wrapper) const
	{
		return Equals(this->cobj(), wrapper.cobj());
	}

	template <class U>
	bool operator!=(const DynamicCopyable<U, Copy, Equals, Free>& wrapper) const
	{
		return !operator==(wrapper);
	}

protected:
	explicit DynamicCopyable(std::nullptr_t) : Base(nullptr) {}
};

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_COPYABLE_HPP
