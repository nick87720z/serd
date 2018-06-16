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

#include "serd/serd.h"

#include <cstddef>
#include <memory>

namespace serd {
namespace detail {

template <typename T>
class Optional;

// Free function for a C object
template <typename T>
using FreeFunc = void (*)(T*);

template <class T>
using Mutable = typename std::remove_const<T>::type;

/// Callable deleter for a C object, noopt for const pointers
template <typename T, void Free(Mutable<T>*)>
struct Deleter
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

/// Generic C++ wrapper for a C object
template <typename T, void FreeFunc(Mutable<T>*)>
class Wrapper
{
public:
	using CType = T;

	explicit Wrapper(T* ptr) : _ptr(ptr) {}

	Wrapper(Wrapper&& wrapper) noexcept = default;
	Wrapper& operator=(Wrapper&& wrapper) noexcept = default;

	Wrapper(const Wrapper&) = delete;
	Wrapper& operator=(const Wrapper&) = delete;

	~Wrapper() = default;

	T*       cobj() { return _ptr.get(); }
	const T* cobj() const { return _ptr.get(); }

protected:
	friend class detail::Optional<T>;

	explicit Wrapper(std::nullptr_t) : _ptr(nullptr) {}

	void reset() { _ptr.reset(); }

	std::unique_ptr<T, Deleter<T, FreeFunc>> _ptr;
};

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_WRAPPER_HPP
