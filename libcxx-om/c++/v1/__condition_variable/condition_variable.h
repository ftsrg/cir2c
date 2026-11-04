// Part of the cir2c operational models of libc++. See OM_DESIGN.md.
// Copyright 2025 Budapest University of Technology and Economics
// Licensed under the Apache License, Version 2.0
// std::condition_variable directly on pthreads. A timed wait may time out at any moment
// or wait for a notification, since time passing is not part of the model.
#ifndef _LIBCPP___CONDITION_VARIABLE_CONDITION_VARIABLE_H
#define _LIBCPP___CONDITION_VARIABLE_CONDITION_VARIABLE_H

#include <__chrono/duration.h>
#include <__chrono/steady_clock.h>
#include <__chrono/system_clock.h>
#include <__chrono/time_point.h>
#include <__config>
#include <__mutex/mutex.h>
#include <__mutex/unique_lock.h>
#include <__utility/move.h>
#include <pthread.h>

extern "C" int __VERIFIER_nondet_int(void);

_LIBCPP_BEGIN_NAMESPACE_STD

enum class cv_status { no_timeout, timeout };

class condition_variable {
  pthread_cond_t __cv_ = PTHREAD_COND_INITIALIZER;

public:
  constexpr condition_variable() noexcept = default;
  condition_variable(const condition_variable&) = delete;
  condition_variable& operator=(const condition_variable&) = delete;
  ~condition_variable() {}
  void notify_one() noexcept { pthread_cond_signal(&__cv_); }
  void notify_all() noexcept { pthread_cond_broadcast(&__cv_); }
  void wait(unique_lock<mutex>& __lk) noexcept { pthread_cond_wait(&__cv_, __lk.mutex()->native_handle()); }
  template <class _Predicate>
  void wait(unique_lock<mutex>& __lk, _Predicate __pred) {
    while (!__pred()) wait(__lk);
  }
  template <class _Clock, class _Duration>
  cv_status wait_until(unique_lock<mutex>& __lk, const chrono::time_point<_Clock, _Duration>&) {
    if (__VERIFIER_nondet_int()) return cv_status::timeout;
    wait(__lk);
    return cv_status::no_timeout;
  }
  template <class _Clock, class _Duration, class _Predicate>
  bool wait_until(unique_lock<mutex>& __lk, const chrono::time_point<_Clock, _Duration>& __t, _Predicate __pred) {
    while (!__pred())
      if (wait_until(__lk, __t) == cv_status::timeout) return __pred();
    return true;
  }
  template <class _Rep, class _Period>
  cv_status wait_for(unique_lock<mutex>& __lk, const chrono::duration<_Rep, _Period>&) {
    if (__VERIFIER_nondet_int()) return cv_status::timeout;
    wait(__lk);
    return cv_status::no_timeout;
  }
  template <class _Rep, class _Period, class _Predicate>
  bool wait_for(unique_lock<mutex>& __lk, const chrono::duration<_Rep, _Period>& __d, _Predicate __pred) {
    while (!__pred())
      if (wait_for(__lk, __d) == cv_status::timeout) return __pred();
    return true;
  }
  typedef pthread_cond_t* native_handle_type;
  native_handle_type native_handle() noexcept { return &__cv_; }
};

_LIBCPP_END_NAMESPACE_STD

#endif
