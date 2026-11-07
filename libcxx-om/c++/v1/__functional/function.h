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
// std::function as one heap object behind a virtual call, without libc++'s small buffer
// and its policy tables, whose function pointers a verifier cannot see through.
#ifndef _LIBCPP___FUNCTIONAL_FUNCTION_H
#define _LIBCPP___FUNCTIONAL_FUNCTION_H

#include <__config>
#include <__cstddef/nullptr_t.h>
#include <__exception/exception.h>
#include <__functional/invoke.h>
#include <__fwd/functional.h>
#include <__type_traits/decay.h>
#include <__type_traits/enable_if.h>
#include <__type_traits/is_same.h>
#include <__type_traits/is_void.h>
#include <__type_traits/remove_cvref.h>
#include <__utility/forward.h>
#include <__utility/move.h>
#include <new>
#include <cstdlib>
#include <type_traits>

_LIBCPP_BEGIN_NAMESPACE_STD

class bad_function_call : public exception {
public:
  const char* what() const noexcept override { return "std::bad_function_call"; }
};
[[noreturn]] inline void __throw_bad_function_call() { throw bad_function_call(); }

template <class _Fp>
class function;

template <class _Rp, class... _Args>
struct __om_callable {
  virtual ~__om_callable() {}
  virtual __om_callable* __clone() const = 0;
  virtual _Rp __call(_Args&&...) = 0;
};

template <class _Fn, class _Rp, class... _Args>
struct __om_callable_of final : __om_callable<_Rp, _Args...> {
  _Fn __f_;
  template <class _F>
  explicit __om_callable_of(_F&& __f) : __f_(std::forward<_F>(__f)) {}
  __om_callable<_Rp, _Args...>* __clone() const override {
    void* __p = malloc(sizeof(__om_callable_of));
    if (__p == nullptr) abort();
    return new (__p) __om_callable_of(__f_);
  }
  _Rp __call(_Args&&... __a) override {
    if constexpr (is_void<_Rp>::value)
      std::__invoke(__f_, std::forward<_Args>(__a)...);
    else
      return std::__invoke(__f_, std::forward<_Args>(__a)...);
  }
};

template <class _Rp, class... _Args>
class function<_Rp(_Args...)> {
  using _Callable = __om_callable<_Rp, _Args...>;
  _Callable* __c_;

  void __drop() {
    if (__c_ != nullptr) {
      __c_->~_Callable();
      free(__c_);
      __c_ = nullptr;
    }
  }

public:
  using result_type = _Rp;

  function() noexcept : __c_(nullptr) {}
  function(nullptr_t) noexcept : __c_(nullptr) {}
  function(const function& __o) : __c_(__o.__c_ != nullptr ? __o.__c_->__clone() : nullptr) {}
  function(function&& __o) noexcept : __c_(__o.__c_) { __o.__c_ = nullptr; }
  template <class _Fp,
            __enable_if_t<!is_same<__remove_cvref_t<_Fp>, function>::value && is_invocable_r<_Rp, __decay_t<_Fp>&, _Args...>::value, int> = 0>
  function(_Fp&& __f) : __c_(nullptr) {
    using _Of = __om_callable_of<__decay_t<_Fp>, _Rp, _Args...>;
    if constexpr (is_pointer<__decay_t<_Fp>>::value || is_member_pointer<__decay_t<_Fp>>::value) {
      if (__f == nullptr) return;
    }
    void* __p = malloc(sizeof(_Of));
    if (__p == nullptr) abort();
    __c_ = new (__p) _Of(std::forward<_Fp>(__f));
  }
  ~function() { __drop(); }
  function& operator=(const function& __o) { function(__o).swap(*this); return *this; }
  function& operator=(function&& __o) noexcept { function(std::move(__o)).swap(*this); return *this; }
  function& operator=(nullptr_t) noexcept { __drop(); return *this; }
  template <class _Fp, __enable_if_t<!is_same<__remove_cvref_t<_Fp>, function>::value, int> = 0>
  function& operator=(_Fp&& __f) { function(std::forward<_Fp>(__f)).swap(*this); return *this; }
  void swap(function& __o) noexcept {
    _Callable* __t = __c_;
    __c_ = __o.__c_;
    __o.__c_ = __t;
  }
  explicit operator bool() const noexcept { return __c_ != nullptr; }
  _Rp operator()(_Args... __a) const {
    if (__c_ == nullptr) std::__throw_bad_function_call();
    return __c_->__call(std::forward<_Args>(__a)...);
  }
};

template <class _Rp, class... _Args>
function(_Rp (*)(_Args...)) -> function<_Rp(_Args...)>;

template <class _Fp>
bool operator==(const function<_Fp>& __f, nullptr_t) noexcept { return !__f; }
template <class _Fp>
bool operator==(nullptr_t, const function<_Fp>& __f) noexcept { return !__f; }
template <class _Fp>
bool operator!=(const function<_Fp>& __f, nullptr_t) noexcept { return (bool)__f; }
template <class _Fp>
bool operator!=(nullptr_t, const function<_Fp>& __f) noexcept { return (bool)__f; }
template <class _Fp>
void swap(function<_Fp>& __a, function<_Fp>& __b) noexcept { __a.swap(__b); }

_LIBCPP_END_NAMESPACE_STD

#endif
