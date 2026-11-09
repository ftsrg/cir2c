/*
 * Copyright 2025 Budapest University of Technology and Economics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Part of the cir2c operational models of libc++. See OM_DESIGN.md.
// std::vector as a heap array grown with malloc, with raw pointers as iterators.
// Shadows libc++'s internal header, so <vector> and everything that includes the
// class directly (vector<bool> and comparisons stay libc++'s) get this one.
#ifndef _OM_VECTOR_H
#define _OM_VECTOR_H

#include <__config>
#include <__fwd/vector.h>
#include <__iterator/iterator_traits.h>
#include <__iterator/reverse_iterator.h>
#include <__type_traits/is_allocator.h>
#include <__memory/allocator.h>
#include <__type_traits/enable_if.h>
#include <__type_traits/is_integral.h>
#include <__utility/forward.h>
#include <__utility/move.h>
#include <cstddef>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <cstdlib>

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _Tp, class _Allocator>
class vector {
  _Tp* __begin_;
  size_t __size_;
  size_t __cap_;

  static _Tp* __alloc(size_t __n) {
    if (__n == 0) return nullptr;
    _Tp* __p = static_cast<_Tp*>(malloc(__n * sizeof(_Tp)));
    if (__p == nullptr) abort();
    return __p;
  }
  void __destroy_from(size_t __n) {
    for (size_t __i = __n; __i < __size_; ++__i) __begin_[__i].~_Tp();
    __size_ = __n;
  }
  void __relocate(size_t __cap) {
    _Tp* __p = __alloc(__cap);
    for (size_t __i = 0; __i < __size_; ++__i) {
      new (static_cast<void*>(__p + __i)) _Tp(std::move(__begin_[__i]));
      __begin_[__i].~_Tp();
    }
    free(__begin_);
    __begin_ = __p;
    __cap_ = __cap;
  }
  void __grow_for(size_t __n) {
    if (__n > __cap_) __relocate(__n > 2 * __cap_ ? __n : 2 * __cap_);
  }
  // Opens a gap of __n constructed-as-raw elements at __i.
  void __open_gap(size_t __i, size_t __n) {
    __grow_for(__size_ + __n);
    for (size_t __k = __size_; __k > __i; --__k) {
      new (static_cast<void*>(__begin_ + __k - 1 + __n)) _Tp(std::move(__begin_[__k - 1]));
      __begin_[__k - 1].~_Tp();
    }
    __size_ += __n;
  }

public:
  using value_type             = _Tp;
  using allocator_type         = _Allocator;
  using size_type              = size_t;
  using difference_type        = ptrdiff_t;
  using reference              = _Tp&;
  using const_reference        = const _Tp&;
  using pointer                = _Tp*;
  using const_pointer          = const _Tp*;
  using iterator               = _Tp*;
  using const_iterator         = const _Tp*;
  using reverse_iterator       = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  vector() : __begin_(nullptr), __size_(0), __cap_(0) {}
  explicit vector(const _Allocator&) : vector() {}
  explicit vector(size_t __n, const _Allocator& = _Allocator()) : vector() { resize(__n); }
  vector(size_t __n, const _Tp& __v, const _Allocator& = _Allocator()) : vector() { assign(__n, __v); }
  template <class _It, __enable_if_t<!is_integral<_It>::value, int> = 0>
  vector(_It __first, _It __last, const _Allocator& = _Allocator()) : vector() { assign(__first, __last); }
  vector(initializer_list<_Tp> __il, const _Allocator& = _Allocator()) : vector() { assign(__il.begin(), __il.end()); }
  vector(const vector& __o) : vector() { assign(__o.begin(), __o.end()); }
  vector(const vector& __o, const _Allocator&) : vector(__o) {}
  vector(vector&& __o) noexcept : __begin_(__o.__begin_), __size_(__o.__size_), __cap_(__o.__cap_) {
    __o.__begin_ = nullptr;
    __o.__size_ = __o.__cap_ = 0;
  }
  vector(vector&& __o, const _Allocator&) noexcept : vector(std::move(__o)) {}
  ~vector() {
    __destroy_from(0);
    free(__begin_);
  }
  vector& operator=(const vector& __o) {
    if (this != &__o) assign(__o.begin(), __o.end());
    return *this;
  }
  vector& operator=(vector&& __o) noexcept {
    if (this != &__o) {
      __destroy_from(0);
      free(__begin_);
      __begin_ = __o.__begin_;
      __size_ = __o.__size_;
      __cap_ = __o.__cap_;
      __o.__begin_ = nullptr;
      __o.__size_ = __o.__cap_ = 0;
    }
    return *this;
  }
  vector& operator=(initializer_list<_Tp> __il) { assign(__il.begin(), __il.end()); return *this; }
  allocator_type get_allocator() const { return allocator_type(); }

  void assign(size_t __n, const _Tp& __v) {
    __destroy_from(0);
    reserve(__n);
    for (size_t __i = 0; __i < __n; ++__i) new (static_cast<void*>(__begin_ + __i)) _Tp(__v);
    __size_ = __n;
  }
  template <class _It, __enable_if_t<!is_integral<_It>::value, int> = 0>
  void assign(_It __first, _It __last) {
    __destroy_from(0);
    for (; __first != __last; ++__first) push_back(*__first);
  }
  void assign(initializer_list<_Tp> __il) { assign(__il.begin(), __il.end()); }

  reference operator[](size_t __i) { return __begin_[__i]; }
  const_reference operator[](size_t __i) const { return __begin_[__i]; }
  reference at(size_t __i) {
    if (__i >= __size_) std::__throw_out_of_range("vector");
    return __begin_[__i];
  }
  const_reference at(size_t __i) const {
    if (__i >= __size_) std::__throw_out_of_range("vector");
    return __begin_[__i];
  }
  reference front() { return __begin_[0]; }
  const_reference front() const { return __begin_[0]; }
  reference back() { return __begin_[__size_ - 1]; }
  const_reference back() const { return __begin_[__size_ - 1]; }
  _Tp* data() noexcept { return __begin_; }
  const _Tp* data() const noexcept { return __begin_; }

  iterator begin() noexcept { return __begin_; }
  const_iterator begin() const noexcept { return __begin_; }
  const_iterator cbegin() const noexcept { return __begin_; }
  iterator end() noexcept { return __begin_ + __size_; }
  const_iterator end() const noexcept { return __begin_ + __size_; }
  const_iterator cend() const noexcept { return __begin_ + __size_; }
  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
  const_reverse_iterator crend() const noexcept { return rend(); }

  bool empty() const noexcept { return __size_ == 0; }
  size_t size() const noexcept { return __size_; }
  size_t capacity() const noexcept { return __cap_; }
  size_t max_size() const noexcept { return size_t(-1) / sizeof(_Tp); }
  void reserve(size_t __n) {
    if (__n > max_size()) std::__throw_length_error("vector");
    if (__n > __cap_) __relocate(__n);
  }
  void shrink_to_fit() {
    if (__cap_ > __size_) __relocate(__size_);
  }
  void clear() noexcept { __destroy_from(0); }
  void resize(size_t __n) {
    if (__n < __size_) { __destroy_from(__n); return; }
    reserve(__n);
    for (size_t __i = __size_; __i < __n; ++__i) new (static_cast<void*>(__begin_ + __i)) _Tp();
    __size_ = __n;
  }
  void resize(size_t __n, const _Tp& __v) {
    if (__n < __size_) { __destroy_from(__n); return; }
    reserve(__n);
    for (size_t __i = __size_; __i < __n; ++__i) new (static_cast<void*>(__begin_ + __i)) _Tp(__v);
    __size_ = __n;
  }

  void push_back(const _Tp& __v) { emplace_back(__v); }
  void push_back(_Tp&& __v) { emplace_back(std::move(__v)); }
  template <class... _Args>
  reference emplace_back(_Args&&... __args) {
    if (__size_ == __cap_) {
      // The argument may live in this vector: build the new element first.
      _Tp __tmp(std::forward<_Args>(__args)...);
      __grow_for(__size_ + 1);
      new (static_cast<void*>(__begin_ + __size_)) _Tp(std::move(__tmp));
    } else {
      new (static_cast<void*>(__begin_ + __size_)) _Tp(std::forward<_Args>(__args)...);
    }
    ++__size_;
    return back();
  }
  void pop_back() { __destroy_from(__size_ - 1); }

  template <class... _Args>
  iterator emplace(const_iterator __pos, _Args&&... __args) {
    size_t __i = __pos - __begin_;
    _Tp __tmp(std::forward<_Args>(__args)...);
    __open_gap(__i, 1);
    new (static_cast<void*>(__begin_ + __i)) _Tp(std::move(__tmp));
    return __begin_ + __i;
  }
  iterator insert(const_iterator __pos, const _Tp& __v) { return emplace(__pos, __v); }
  iterator insert(const_iterator __pos, _Tp&& __v) { return emplace(__pos, std::move(__v)); }
  iterator insert(const_iterator __pos, size_t __n, const _Tp& __v) {
    size_t __i = __pos - __begin_;
    _Tp __tmp(__v);
    __open_gap(__i, __n);
    for (size_t __k = 0; __k < __n; ++__k) new (static_cast<void*>(__begin_ + __i + __k)) _Tp(__tmp);
    return __begin_ + __i;
  }
  template <class _It, __enable_if_t<!is_integral<_It>::value, int> = 0>
  iterator insert(const_iterator __pos, _It __first, _It __last) {
    size_t __i = __pos - __begin_;
    size_t __k = __i;
    for (; __first != __last; ++__first, ++__k) emplace(__begin_ + __k, *__first);
    return __begin_ + __i;
  }
  iterator insert(const_iterator __pos, initializer_list<_Tp> __il) { return insert(__pos, __il.begin(), __il.end()); }
  iterator erase(const_iterator __pos) { return erase(__pos, __pos + 1); }
  iterator erase(const_iterator __first, const_iterator __last) {
    size_t __i = __first - __begin_;
    size_t __n = __last - __first;
    if (__n != 0) {
      for (size_t __k = __i; __k + __n < __size_; ++__k) __begin_[__k] = std::move(__begin_[__k + __n]);
      __destroy_from(__size_ - __n);
    }
    return __begin_ + __i;
  }
  void swap(vector& __o) noexcept {
    _Tp* __b = __begin_; __begin_ = __o.__begin_; __o.__begin_ = __b;
    size_t __s = __size_; __size_ = __o.__size_; __o.__size_ = __s;
    size_t __c = __cap_; __cap_ = __o.__cap_; __o.__cap_ = __c;
  }
};

_LIBCPP_END_NAMESPACE_STD

#endif
