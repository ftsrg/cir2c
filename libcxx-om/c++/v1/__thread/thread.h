// Part of the cir2c operational models of libc++. See OM_DESIGN.md.
// Copyright 2025 Budapest University of Technology and Economics
// Licensed under the Apache License, Version 2.0
// std::thread directly on pthread_create, without thread-specific state.
#ifndef _LIBCPP___THREAD_THREAD_H
#define _LIBCPP___THREAD_THREAD_H

#include <__condition_variable/condition_variable.h>
#include <__config>
#include <__exception/terminate.h>
#include <__functional/hash.h>
#include <__functional/unary_function.h>
#include <__mutex/mutex.h>
#include <__thread/id.h>
#include <__thread/support.h>
#include <__type_traits/decay.h>
#include <__type_traits/enable_if.h>
#include <__type_traits/is_same.h>
#include <__type_traits/remove_cvref.h>
#include <__utility/forward.h>
#include <new>
#include <pthread.h>
#include <cstdlib>
#include <tuple>

_LIBCPP_BEGIN_NAMESPACE_STD

template <>
struct hash<__thread_id> : __unary_function<__thread_id, size_t> {
  size_t operator()(__thread_id __v) const noexcept { return hash<__libcpp_thread_id>()(__v.__id_); }
};

template <class _State>
void* __om_thread_start(void* __p) {
  _State* __state = static_cast<_State*>(__p);
  std::apply([](auto&& __f, auto&&... __a) { std::__invoke(std::move(__f), std::move(__a)...); }, std::move(*__state));
  __state->~_State();
  free(__state);
  return nullptr;
}

class thread {
  pthread_t __t_;

public:
  typedef __thread_id id;
  typedef pthread_t native_handle_type;

  thread() noexcept : __t_(pthread_t()) {}
  template <class _Fp, class... _Args, __enable_if_t<!is_same<__remove_cvref_t<_Fp>, thread>::value, int> = 0>
  explicit thread(_Fp&& __f, _Args&&... __args) {
    using _State = tuple<__decay_t<_Fp>, __decay_t<_Args>...>;
    void* __p = malloc(sizeof(_State));
    if (__p == nullptr) abort();
    new (__p) _State(std::forward<_Fp>(__f), std::forward<_Args>(__args)...);
    pthread_create(&__t_, nullptr, &__om_thread_start<_State>, __p);
  }
  thread(const thread&) = delete;
  thread(thread&& __o) noexcept : __t_(__o.__t_) { __o.__t_ = pthread_t(); }
  thread& operator=(const thread&) = delete;
  thread& operator=(thread&& __o) noexcept {
    if (joinable()) std::terminate();
    __t_ = __o.__t_;
    __o.__t_ = pthread_t();
    return *this;
  }
  ~thread() {
    if (joinable()) std::terminate();
  }
  void swap(thread& __o) noexcept {
    pthread_t __t = __t_;
    __t_ = __o.__t_;
    __o.__t_ = __t;
  }
  bool joinable() const noexcept { return __t_ != pthread_t(); }
  void join() {
    pthread_join(__t_, nullptr);
    __t_ = pthread_t();
  }
  void detach() {
    pthread_detach(__t_);
    __t_ = pthread_t();
  }
  id get_id() const noexcept { return __t_; }
  native_handle_type native_handle() noexcept { return __t_; }
  static unsigned hardware_concurrency() noexcept { return 1; }
};

inline void swap(thread& __x, thread& __y) noexcept { __x.swap(__y); }

_LIBCPP_END_NAMESPACE_STD

#endif
