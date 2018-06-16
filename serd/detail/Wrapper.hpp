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

#ifndef SERD_DETAIL_WRAPPER_HPP
#define SERD_DETAIL_WRAPPER_HPP

#include <cstddef>
#include <memory>

namespace serd {
namespace detail {

template <typename T>
class Optional;

/// Free function for a C object
template <typename T>
using FreeFunc = void (*)(T*);

template <class T>
using Mutable = typename std::remove_const<T>::type;

/**
   Simple overhead-free deleter for a C object.

   Can be used with const or mutable pointers, but only mutable pointers will
   be freed.  This makes it simple to wrap APIs where constness conveys
   ownership, but can not handle unowned mutable pointers.
*/
template <typename T, void Free(Mutable<T>*)>
struct BasicDeleter
{
	template <typename = std::enable_if<!std::is_const<T>::value>>
	void operator()(typename std::remove_const<T>::type* ptr)
	{
		Free(ptr);
	}

	template <typename = std::enable_if<std::is_const<T>::value>>
	void operator()(const T*)
	{
	}
};

/// Ownership for `DynamicDeleter`
enum class Ownership { owned, view };

/**
   Deleter for a C object that can handle dynamic ownership.

   Unlike `BasicDeleter`, this can be used to handle non-owned references to
   mutable objects, at the cost of an extra word for tracking the ownership
   (since constness in the type can't convey this information).
*/
template <typename T, void Free(Mutable<T>*)>
struct DynamicDeleter : BasicDeleter<T, Free>
{
	DynamicDeleter(Ownership ownership) : _ownership{ownership} {}

	void operator()(T* ptr)
	{
		if (_ownership == Ownership::owned) {
			BasicDeleter<T, Free>::operator()(ptr);
		}
	}

private:
	Ownership _ownership;
};

/// Generic C++ wrapper for a C object
template <typename T, class Deleter>
class Wrapper
{
public:
	using CType = T;
	using UPtr  = std::unique_ptr<T, Deleter>;

	explicit Wrapper(T* ptr) : _ptr(ptr, Deleter{}) {}
	Wrapper(T* ptr, Deleter deleter) : _ptr(ptr, std::move(deleter)) {}
	Wrapper(UPtr ptr) : _ptr(std::move(ptr)) {}

	Wrapper(Wrapper&&) noexcept = default;
	Wrapper& operator=(Wrapper&&) noexcept = default;

	Wrapper(const Wrapper&) = delete;
	Wrapper& operator=(const Wrapper&) = delete;

	~Wrapper() = default;

	T*       cobj() { return _ptr.get(); }
	const T* cobj() const { return _ptr.get(); }

protected:
	friend class detail::Optional<T>;

	explicit Wrapper(std::nullptr_t) : _ptr(nullptr) {}

	void reset() { _ptr.reset(); }

	std::unique_ptr<T, Deleter> _ptr;
};

template <typename T, void Free(Mutable<T>*)>
class BasicWrapper : public Wrapper<T, BasicDeleter<T, Free>>
{
public:
	explicit BasicWrapper(T* ptr) : Wrapper<T, BasicDeleter<T, Free>>{ptr} {}
};

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_WRAPPER_HPP
