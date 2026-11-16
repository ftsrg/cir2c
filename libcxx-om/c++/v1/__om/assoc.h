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
// The ordered and hashed associative containers, on the shared node list: elements are
// kept sorted (ordered) or in insertion order (hashed), and found by a linear scan. The
// hash is never computed, which also spares verifiers from modelling it.
#ifndef _OM_ASSOC_H
#define _OM_ASSOC_H

#include <__config>
#include <__functional/hash.h>
#include <__functional/operations.h>
#include <__memory/allocator.h>
#include <__om/node_list.h>
#include <utility>
#include <__type_traits/enable_if.h>
#include <__type_traits/is_integral.h>
#include <__utility/pair.h>
#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <tuple>

_LIBCPP_BEGIN_NAMESPACE_STD

struct __om_key_of_self {
  template <class _V>
  static const _V& __get(const _V& __v) { return __v; }
};
struct __om_key_of_pair {
  template <class _V>
  static const typename _V::first_type& __get(const _V& __v) { return __v.first; }
};

// Ordered: _Cmp is a strict weak ordering, position is the first element not before the key.
// Hashed: _Cmp is an equality, position is the end unless a key equal to it is present.
template <class _Value, class _Key, class _KeyOf, class _Cmp, bool _Ordered, bool _Multi>
class __om_assoc : public __om_node_list<_Value> {
  using _Base = __om_node_list<_Value>;

protected:
  _Cmp __cmp_;

  bool __same(const _Key& __a, const _Key& __b) const {
    if constexpr (_Ordered)
      return !__cmp_(__a, __b) && !__cmp_(__b, __a);
    else
      return __cmp_(__a, __b);
  }

public:
  using typename _Base::const_iterator;
  using typename _Base::iterator;
  using key_type        = _Key;
  using value_type      = _Value;
  using size_type       = size_t;
  using difference_type = ptrdiff_t;
  using reference       = _Value&;
  using const_reference = const _Value&;
  using pointer         = _Value*;
  using const_pointer   = const _Value*;

  __om_assoc() {}
  explicit __om_assoc(const _Cmp& __c) : __cmp_(__c) {}

  iterator lower_bound(const _Key& __k) {
    iterator __i = this->begin();
    if constexpr (_Ordered) {
      while (__i != this->end() && __cmp_(_KeyOf::__get(*__i), __k)) ++__i;
    } else {
      while (__i != this->end() && !__cmp_(_KeyOf::__get(*__i), __k)) ++__i;
    }
    return __i;
  }
  const_iterator lower_bound(const _Key& __k) const { return const_cast<__om_assoc*>(this)->lower_bound(__k); }
  iterator upper_bound(const _Key& __k) {
    iterator __i = lower_bound(__k);
    while (__i != this->end() && __same(_KeyOf::__get(*__i), __k)) ++__i;
    return __i;
  }
  const_iterator upper_bound(const _Key& __k) const { return const_cast<__om_assoc*>(this)->upper_bound(__k); }
  iterator find(const _Key& __k) {
    iterator __i = lower_bound(__k);
    return __i != this->end() && __same(_KeyOf::__get(*__i), __k) ? __i : this->end();
  }
  const_iterator find(const _Key& __k) const { return const_cast<__om_assoc*>(this)->find(__k); }
  size_t count(const _Key& __k) const {
    size_t __n = 0;
    for (const_iterator __i = lower_bound(__k); __i != this->end() && __same(_KeyOf::__get(*__i), __k); ++__i) ++__n;
    return __n;
  }
  pair<iterator, iterator> equal_range(const _Key& __k) { return pair<iterator, iterator>(lower_bound(__k), upper_bound(__k)); }
  pair<const_iterator, const_iterator> equal_range(const _Key& __k) const {
    return pair<const_iterator, const_iterator>(lower_bound(__k), upper_bound(__k));
  }

  // Builds the value first, as the key can come from arguments that have to be moved once.
  template <class... _Args>
  pair<iterator, bool> emplace(_Args&&... __args) {
    iterator __n = this->__emplace_before(this->end(), std::forward<_Args>(__args)...);
    const _Key& __k = _KeyOf::__get(*__n);
    iterator __at = this->begin();
    if constexpr (_Ordered) {
      while (__at != __n && !__cmp_(__k, _KeyOf::__get(*__at))) {
        if constexpr (!_Multi) {
          if (!__cmp_(_KeyOf::__get(*__at), __k)) { this->erase(__n); return pair<iterator, bool>(__at, false); }
        }
        ++__at;
      }
    } else if constexpr (!_Multi) {
      for (; __at != __n; ++__at)
        if (__cmp_(_KeyOf::__get(*__at), __k)) { this->erase(__n); return pair<iterator, bool>(__at, false); }
    } else {
      __at = __n;
    }
    if (__at != __n) {
      __om_link* __l = __n.__link();
      this->__unlink(__l);
      this->__link_before(__at.__link(), __l);
    }
    return pair<iterator, bool>(__n, true);
  }
  pair<iterator, bool> insert(const _Value& __v) { return emplace(__v); }
  pair<iterator, bool> insert(_Value&& __v) { return emplace(std::move(__v)); }
  iterator insert(const_iterator, const _Value& __v) { return emplace(__v).first; }
  iterator insert(const_iterator, _Value&& __v) { return emplace(std::move(__v)).first; }
  template <class... _Args>
  iterator emplace_hint(const_iterator, _Args&&... __args) { return emplace(std::forward<_Args>(__args)...).first; }
  template <class _It, __enable_if_t<!is_integral<_It>::value, int> = 0>
  void insert(_It __first, _It __last) { for (; __first != __last; ++__first) emplace(*__first); }
  void insert(initializer_list<_Value> __il) { insert(__il.begin(), __il.end()); }

  using _Base::erase;
  size_t erase(const _Key& __k) {
    size_t __n = 0;
    for (iterator __i = lower_bound(__k); __i != this->end() && __same(_KeyOf::__get(*__i), __k);) {
      __i = this->erase(const_iterator(__i));
      ++__n;
    }
    return __n;
  }
};

// The copy and move members every associative container shares.
#define _OM_ASSOC_MEMBERS(_Name)                                                                                       \
  _Name(const _Name& __o) : _Base(__o.__cmp_) { for (const auto& __v : __o) this->__emplace_before(this->end(), __v); }  \
  _Name(_Name&& __o) noexcept : _Base(__o.__cmp_) { this->__take(__o); }                                               \
  _Name& operator=(const _Name& __o) {                                                                                 \
    if (this != &__o) { this->clear(); for (const auto& __v : __o) this->__emplace_before(this->end(), __v); }         \
    return *this;                                                                                                      \
  }                                                                                                                    \
  _Name& operator=(_Name&& __o) noexcept {                                                                             \
    if (this != &__o) { this->clear(); this->__take(__o); }                                                            \
    return *this;                                                                                                      \
  }                                                                                                                    \
  void swap(_Name& __o) noexcept { _Name __t(std::move(__o)); __o = std::move(*this); *this = std::move(__t); }

template <class _Key, class _Tp, class _Base>
class __om_map_access : public _Base {
public:
  using _Base::_Base;
  using mapped_type = _Tp;
  _Tp& operator[](const _Key& __k) {
    return this->emplace(piecewise_construct, std::forward_as_tuple(__k), std::forward_as_tuple()).first->second;
  }
  _Tp& operator[](_Key&& __k) {
    return this->emplace(piecewise_construct, std::forward_as_tuple(std::move(__k)), std::forward_as_tuple()).first->second;
  }
  _Tp& at(const _Key& __k) {
    auto __i = this->find(__k);
    if (__i == this->end()) std::__throw_out_of_range("map::at");
    return __i->second;
  }
  const _Tp& at(const _Key& __k) const {
    auto __i = this->find(__k);
    if (__i == this->end()) std::__throw_out_of_range("map::at");
    return __i->second;
  }
};

template <class _C>
bool __om_assoc_equal(const _C& __a, const _C& __b) {
  return __a.size() == __b.size() && std::equal(__a.begin(), __a.end(), __b.begin());
}

_LIBCPP_END_NAMESPACE_STD

#endif
