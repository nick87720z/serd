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
#include "serd/serd.h"

#include <cstddef>
#include <memory>
#include <type_traits>

namespace serd {
namespace detail {

/// Copy function for a C object
template <class T>
using CopyFunc = T* (*)(const T*);

template <class T, Mutable<T>* Copy(const T*), void Free(Mutable<T>*)>
typename std::enable_if<std::is_const<T>::value, T>::type*
copy(const T* ptr)
{
	return ptr; // Making a view (const reference), do not copy
}

template <class T,
          Mutable<T>* Copy(const T*),
          void        Free(typename std::remove_const<T>::type*)>
typename std::enable_if<!std::is_const<T>::value, T>::type*
copy(const T* ptr)
{
	return Copy(ptr); // Making a mutable wrapper, copy
}

/// Generic C++ wrapper for a copyable C object
template <class T,
          Mutable<T>* Copy(const T*),
          bool        Equals(const T*, const T*),
          void        Free(Mutable<T>*)>
class Copyable : public Wrapper<T, Free>
{
public:
	explicit Copyable(T* ptr) : Wrapper<T, Free>(ptr) {}

	Copyable(const Copyable& wrapper)
	    : Wrapper<T, Free>(copy<T, Copy, Free>(wrapper.cobj()))
	{
	}

	template <class U, void UFree(Mutable<U>*)>
	explicit Copyable(const Copyable<U, Copy, Equals, UFree>& wrapper)
	    : Wrapper<T, Free>(copy<T, Copy, Free>(wrapper.cobj()))
	{
	}

	Copyable(Copyable&&) noexcept = default;
	Copyable& operator=(Copyable&&) noexcept = default;
	~Copyable() noexcept                     = default;

	Copyable& operator=(const Copyable& wrapper)
	{
		this->_ptr = std::unique_ptr<T, Deleter<T, Free>>(
		        copy<T, Copy, Free>(wrapper.cobj()));
		return *this;
	}

	template <class U, void UFree(Mutable<U>*)>
	bool operator==(const Copyable<U, Copy, Equals, UFree>& wrapper) const
	{
		return Equals(this->cobj(), wrapper.cobj());
	}

	template <class U, void UFree(Mutable<U>*)>
	bool operator!=(const Copyable<U, Copy, Equals, UFree>& wrapper) const
	{
		return !operator==(wrapper);
	}

protected:
	explicit Copyable(std::nullptr_t) : Wrapper<T, Free>(nullptr) {}
};

} // namespace detail
} // namespace serd

#endif // SERD_DETAIL_COPYABLE_HPP
