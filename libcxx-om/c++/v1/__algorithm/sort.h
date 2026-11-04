// Part of the cir2c operational models of libc++. See OM_DESIGN.md.
// Copyright 2025 Budapest University of Technology and Economics
// Licensed under the Apache License, Version 2.0
// std::sort as an insertion sort. libc++'s introsort is long, and for the arithmetic types
// it calls an instantiation in the compiled library that a verifier has no body for.
#ifndef _LIBCPP___ALGORITHM_SORT_H
#define _LIBCPP___ALGORITHM_SORT_H

#include <__algorithm/comp.h>
#include <__algorithm/comp_ref_type.h>
#include <__algorithm/iterator_operations.h>
#include <__config>
#include <__iterator/iterator_traits.h>
#include <__utility/move.h>

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _AlgPolicy, class _Compare, class _It>
void __insertion_sort(_It __first, _It __last, _Compare __comp) {
  using _Ops = _IterOps<_AlgPolicy>;
  if (__first == __last) return;
  for (_It __i = _Ops::next(__first); __i != __last; ++__i) {
    typename iterator_traits<_It>::value_type __v(_Ops::__iter_move(__i));
    _It __j = __i;
    while (__j != __first) {
      _It __k = _Ops::prev(__j);
      if (!__comp(__v, *__k)) break;
      *__j = _Ops::__iter_move(__k);
      __j = __k;
    }
    *__j = std::move(__v);
  }
}

template <class _AlgPolicy, class _Compare, class _It>
unsigned __sort3(_It __x, _It __y, _It __z, _Compare __c) {
  using _Ops = _IterOps<_AlgPolicy>;
  unsigned __r = 0;
  if (__c(*__y, *__x)) { _Ops::iter_swap(__x, __y); ++__r; }
  if (__c(*__z, *__y)) {
    _Ops::iter_swap(__y, __z);
    ++__r;
    if (__c(*__y, *__x)) { _Ops::iter_swap(__x, __y); ++__r; }
  }
  return __r;
}

template <class _AlgPolicy, class _Compare, class _It>
void __selection_sort(_It __first, _It __last, _Compare __comp) {
  std::__insertion_sort<_AlgPolicy, _Compare>(__first, __last, __comp);
}

template <class _AlgPolicy, class _It, class _Comp>
void __sort_impl(_It __first, _It __last, _Comp& __comp) {
  std::__insertion_sort<_AlgPolicy, __comp_ref_type<_Comp> >(__first, __last, __comp);
}

template <class _It, class _Comp>
void sort(_It __first, _It __last, _Comp __comp) {
  std::__sort_impl<_ClassicAlgPolicy>(std::move(__first), std::move(__last), __comp);
}

template <class _It>
void sort(_It __first, _It __last) {
  std::sort(__first, __last, __less<>());
}

_LIBCPP_END_NAMESPACE_STD

#endif
