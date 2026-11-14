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

// cir2c model "libcxx-out-of-line" (README 8.2).
//
// libc++ keeps some members and objects in its compiled library, so ClangIR
// sees only their declarations. libc++'s own header code uses them: std::vector
// throws length_error and out_of_range, std::allocator throws
// bad_array_new_length, and std::stable_sort uses std::nothrow. This file
// defines them as libc++ does, with the same what() texts. It is not an
// abstraction.
//
// clang reads this file before the program (-include). The definitions come
// after the class definitions of libc++ and before any use of the members.

#ifndef CIR2C_MODEL_LIBCXX_OUT_OF_LINE
#define CIR2C_MODEL_LIBCXX_OUT_OF_LINE

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>

// The argument of the operator new that returns a null pointer instead of
// throwing bad_alloc. As an inline variable, it is in the output only when the
// program uses it.
inline const std::nothrow_t std::nothrow{};

inline std::exception::~exception() noexcept {}
inline const char *std::exception::what() const noexcept { return "std::exception"; }

inline std::bad_alloc::bad_alloc() noexcept {}
inline std::bad_alloc::~bad_alloc() noexcept {}
inline const char *std::bad_alloc::what() const noexcept { return "std::bad_alloc"; }

inline std::bad_array_new_length::bad_array_new_length() noexcept {}
inline std::bad_array_new_length::~bad_array_new_length() noexcept {}
inline const char *std::bad_array_new_length::what() const noexcept {
  return "bad_array_new_length";
}

// libc++ keeps the message of a logic_error in the private member __imp_, of
// the private type __libcpp_refstring. No standard name gives access to it, so
// this part uses those two libc++ names. libc++ lets copies share the buffer
// through a reference count. The model defines no copy, so each buffer has
// one owner: the constructor copies the message, the destructor frees it.
inline std::__libcpp_refstring::__libcpp_refstring(const char *message) {
  std::size_t size = std::strlen(message) + 1;
  char *copy = static_cast<char *>(std::malloc(size));
  // libc++ allocates with operator new, which throws bad_alloc.
  if (!copy)
    throw std::bad_alloc();
  std::memcpy(copy, message, size);
  __imp_ = copy;
}
inline std::__libcpp_refstring::~__libcpp_refstring() { std::free(const_cast<char *>(__imp_)); }

inline std::logic_error::logic_error(const char *message) : __imp_(message) {}
inline std::logic_error::~logic_error() noexcept {}
inline const char *std::logic_error::what() const noexcept { return __imp_.c_str(); }

inline std::length_error::~length_error() noexcept {}
inline std::out_of_range::~out_of_range() noexcept {}

#endif
