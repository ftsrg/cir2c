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
// bad_array_new_length, std::stable_sort uses std::nothrow, and each new
// expression calls an allocation function. This file
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

// The replaceable allocation functions ([new.delete], issue #17). libc++ keeps
// them in its compiled library. A throwing operator new never returns a null
// pointer: when malloc fails, it throws std::bad_alloc. Each call, also for 0
// bytes, gives another pointer. The forms with std::nothrow_t return the null
// pointer instead. The forms with std::align_val_t use aligned_alloc, whose
// size must be a multiple of the alignment (C11 7.22.3.1), and free releases
// both kinds of memory.
//
// libc++ calls the new handler of std::set_new_handler again and again before
// it throws. This model does not: set_new_handler is in the compiled library,
// so a program that uses it has an undefined symbol and cir2c rejects it.
//
// [replacement.functions]/3 forbids inline replacements (no diagnostic is
// required). Clang accepts them with a warning. Here inline gives one
// definition in each translation unit that cir-merge can merge, and a
// definition in the output only for the forms that the program uses.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winline-new-delete"
inline void *operator new(std::size_t size) {
  void *p = std::malloc(size ? size : 1);
  if (!p)
    throw std::bad_alloc();
  return p;
}
inline void *operator new[](std::size_t size) { return ::operator new(size); }
inline void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
  return std::malloc(size ? size : 1);
}
inline void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
  return std::malloc(size ? size : 1);
}
inline void *operator new(std::size_t size, std::align_val_t alignment) {
  std::size_t align = static_cast<std::size_t>(alignment);
  std::size_t bytes = size ? size : 1;
  void *p = bytes <= static_cast<std::size_t>(-1) - align
                ? std::aligned_alloc(align, (bytes + align - 1) / align * align)
                : nullptr;
  if (!p)
    throw std::bad_alloc();
  return p;
}
inline void *operator new[](std::size_t size, std::align_val_t alignment) {
  return ::operator new(size, alignment);
}
inline void *operator new(std::size_t size, std::align_val_t alignment,
                          const std::nothrow_t &) noexcept {
  std::size_t align = static_cast<std::size_t>(alignment);
  std::size_t bytes = size ? size : 1;
  return bytes <= static_cast<std::size_t>(-1) - align
             ? std::aligned_alloc(align, (bytes + align - 1) / align * align)
             : nullptr;
}
inline void *operator new[](std::size_t size, std::align_val_t alignment,
                            const std::nothrow_t &tag) noexcept {
  return ::operator new(size, alignment, tag);
}
inline void operator delete(void *p) noexcept { std::free(p); }
inline void operator delete[](void *p) noexcept { std::free(p); }
inline void operator delete(void *p, std::size_t) noexcept { std::free(p); }
inline void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
inline void operator delete(void *p, const std::nothrow_t &) noexcept { std::free(p); }
inline void operator delete[](void *p, const std::nothrow_t &) noexcept { std::free(p); }
inline void operator delete(void *p, std::align_val_t) noexcept { std::free(p); }
inline void operator delete[](void *p, std::align_val_t) noexcept { std::free(p); }
inline void operator delete(void *p, std::size_t, std::align_val_t) noexcept { std::free(p); }
inline void operator delete[](void *p, std::size_t, std::align_val_t) noexcept { std::free(p); }
inline void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept {
  std::free(p);
}
inline void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept {
  std::free(p);
}
#pragma clang diagnostic pop

#endif
