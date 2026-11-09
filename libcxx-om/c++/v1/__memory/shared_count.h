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
// The reference counts of shared_ptr. libc++ defines their members in the compiled
// library, which a verifier has no body for; shared_ptr itself is libc++'s own.
#ifndef _LIBCPP___MEMORY_SHARED_COUNT_H
#define _LIBCPP___MEMORY_SHARED_COUNT_H

#include <__config>
#include <typeinfo>

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _Tp>
inline _Tp __libcpp_atomic_refcount_increment(_Tp& __t) noexcept { return __atomic_add_fetch(&__t, 1, __ATOMIC_RELAXED); }
template <class _Tp>
inline _Tp __libcpp_atomic_refcount_decrement(_Tp& __t) noexcept { return __atomic_add_fetch(&__t, -1, __ATOMIC_ACQ_REL); }

class __shared_count {
  __shared_count(const __shared_count&);
  __shared_count& operator=(const __shared_count&);

protected:
  long __shared_owners_;
  virtual ~__shared_count() {}

private:
  virtual void __on_zero_shared() noexcept = 0;

public:
  explicit __shared_count(long __refs = 0) noexcept : __shared_owners_(__refs) {}
  void __add_shared() noexcept { __libcpp_atomic_refcount_increment(__shared_owners_); }
  bool __release_shared() noexcept {
    if (__libcpp_atomic_refcount_decrement(__shared_owners_) == -1) {
      __on_zero_shared();
      return true;
    }
    return false;
  }
  long use_count() const noexcept { return __atomic_load_n(&__shared_owners_, __ATOMIC_RELAXED) + 1; }
};

class __shared_weak_count : private __shared_count {
  long __shared_weak_owners_;

public:
  explicit __shared_weak_count(long __refs = 0) noexcept : __shared_count(__refs), __shared_weak_owners_(__refs) {}

protected:
  ~__shared_weak_count() override {}

public:
  void __add_shared() noexcept { __shared_count::__add_shared(); }
  void __add_weak() noexcept { __libcpp_atomic_refcount_increment(__shared_weak_owners_); }
  void __release_shared() noexcept {
    if (__shared_count::__release_shared()) __release_weak();
  }
  void __release_weak() noexcept {
    if (__libcpp_atomic_refcount_decrement(__shared_weak_owners_) == -1) __on_zero_shared_weak();
  }
  long use_count() const noexcept { return __shared_count::use_count(); }
  __shared_weak_count* lock() noexcept {
    long __object_owners = __atomic_load_n(&__shared_owners_, __ATOMIC_SEQ_CST);
    while (__object_owners != -1) {
      if (__atomic_compare_exchange_n(&__shared_owners_, &__object_owners, __object_owners + 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        return this;
    }
    return nullptr;
  }
  virtual const void* __get_deleter(const type_info&) const noexcept { return nullptr; }

private:
  virtual void __on_zero_shared_weak() noexcept = 0;
};

_LIBCPP_END_NAMESPACE_STD

#endif
