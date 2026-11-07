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
// The node list that map, set, list and the unordered containers are made of: a
// circular doubly linked list around a sentinel. Search is linear, so that what a
// verifier sees is one simple loop instead of a balanced tree or a hash table, and
// nodes never move, so references stay valid as the standard requires.
#ifndef _OM_NODE_LIST_H
#define _OM_NODE_LIST_H

#include <__config>
#include <__iterator/iterator_traits.h>
#include <__iterator/reverse_iterator.h>
#include <__type_traits/conditional.h>
#include <__utility/forward.h>
#include <__utility/move.h>
#include <cstddef>
#include <new>
#include <cstdlib>

_LIBCPP_BEGIN_NAMESPACE_STD

struct __om_link {
  __om_link* __prev;
  __om_link* __next;
};

template <class _Tp>
struct __om_node : __om_link {
  _Tp __value;
  template <class... _Args>
  explicit __om_node(_Args&&... __args) : __value(std::forward<_Args>(__args)...) {}
};

template <class _Tp, bool _Const>
class __om_list_iterator {
  __om_link* __l_;

public:
  using iterator_category = bidirectional_iterator_tag;
  using value_type        = _Tp;
  using difference_type   = ptrdiff_t;
  using pointer           = __conditional_t<_Const, const _Tp*, _Tp*>;
  using reference         = __conditional_t<_Const, const _Tp&, _Tp&>;

  __om_list_iterator() : __l_(nullptr) {}
  explicit __om_list_iterator(const __om_link* __l) : __l_(const_cast<__om_link*>(__l)) {}
  template <bool _C = _Const, __enable_if_t<_C, int> = 0>
  __om_list_iterator(const __om_list_iterator<_Tp, false>& __o) : __l_(__o.__link()) {}

  __om_link* __link() const { return __l_; }
  reference operator*() const { return static_cast<__om_node<_Tp>*>(__l_)->__value; }
  pointer operator->() const { return &static_cast<__om_node<_Tp>*>(__l_)->__value; }
  __om_list_iterator& operator++() { __l_ = __l_->__next; return *this; }
  __om_list_iterator operator++(int) { __om_list_iterator __t = *this; __l_ = __l_->__next; return __t; }
  __om_list_iterator& operator--() { __l_ = __l_->__prev; return *this; }
  __om_list_iterator operator--(int) { __om_list_iterator __t = *this; __l_ = __l_->__prev; return __t; }
  friend bool operator==(const __om_list_iterator& __a, const __om_list_iterator& __b) { return __a.__l_ == __b.__l_; }
  friend bool operator!=(const __om_list_iterator& __a, const __om_list_iterator& __b) { return __a.__l_ != __b.__l_; }
};

template <class _Tp>
class __om_node_list {
protected:
  __om_link __end_;
  size_t __size_;

  void __reset() { __end_.__prev = __end_.__next = &__end_; __size_ = 0; }
  void __link_before(__om_link* __pos, __om_link* __n) {
    __n->__prev = __pos->__prev;
    __n->__next = __pos;
    __pos->__prev->__next = __n;
    __pos->__prev = __n;
    ++__size_;
  }
  void __unlink(__om_link* __n) {
    __n->__prev->__next = __n->__next;
    __n->__next->__prev = __n->__prev;
    --__size_;
  }
  void __take(__om_node_list& __o) {
    if (__o.__size_ == 0) { __reset(); return; }
    __end_ = __o.__end_;
    __size_ = __o.__size_;
    __end_.__next->__prev = &__end_;
    __end_.__prev->__next = &__end_;
    __o.__reset();
  }

public:
  using iterator               = __om_list_iterator<_Tp, false>;
  using const_iterator         = __om_list_iterator<_Tp, true>;
  using reverse_iterator       = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  __om_node_list() { __reset(); }
  __om_node_list(const __om_node_list&) = delete;
  __om_node_list& operator=(const __om_node_list&) = delete;
  ~__om_node_list() { clear(); }

  iterator begin() { return iterator(__end_.__next); }
  const_iterator begin() const { return const_iterator(__end_.__next); }
  const_iterator cbegin() const { return begin(); }
  iterator end() { return iterator(&__end_); }
  const_iterator end() const { return const_iterator(&__end_); }
  const_iterator cend() const { return end(); }
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return rbegin(); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return rend(); }
  bool empty() const { return __size_ == 0; }
  size_t size() const { return __size_; }
  size_t max_size() const { return size_t(-1) / sizeof(__om_node<_Tp>); }

  template <class... _Args>
  iterator __emplace_before(const_iterator __pos, _Args&&... __args) {
    void* __p = malloc(sizeof(__om_node<_Tp>));
    if (__p == nullptr) abort();
    __om_node<_Tp>* __n = new (__p) __om_node<_Tp>(std::forward<_Args>(__args)...);
    __link_before(__pos.__link(), __n);
    return iterator(__n);
  }
  iterator erase(const_iterator __pos) {
    __om_link* __l = __pos.__link();
    __om_link* __next = __l->__next;
    __unlink(__l);
    __om_node<_Tp>* __n = static_cast<__om_node<_Tp>*>(__l);
    __n->~__om_node<_Tp>();
    free(__n);
    return iterator(__next);
  }
  iterator erase(const_iterator __first, const_iterator __last) {
    while (__first != __last) __first = const_iterator(erase(__first).__link());
    return iterator(__last.__link());
  }
  void clear() {
    while (__size_ != 0) erase(const_iterator(__end_.__next));
  }
};

_LIBCPP_END_NAMESPACE_STD

#endif
