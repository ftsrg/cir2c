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
// std::mutex directly on pthreads, which verifiers model. libc++ compiles its members.
#ifndef _LIBCPP___MUTEX_MUTEX_H
#define _LIBCPP___MUTEX_MUTEX_H

#include <__config>
#include <__thread/support.h>
#include <pthread.h>

_LIBCPP_BEGIN_NAMESPACE_STD

class mutex {
  pthread_mutex_t __m_ = PTHREAD_MUTEX_INITIALIZER;

public:
  constexpr mutex() = default;
  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;
  ~mutex() {}
  void lock() { pthread_mutex_lock(&__m_); }
  bool try_lock() noexcept { return pthread_mutex_trylock(&__m_) == 0; }
  void unlock() noexcept { pthread_mutex_unlock(&__m_); }
  typedef pthread_mutex_t* native_handle_type;
  native_handle_type native_handle() noexcept { return &__m_; }
};

_LIBCPP_END_NAMESPACE_STD

#endif
