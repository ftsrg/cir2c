// Part of the cir2c operational models of libc++. See OM_DESIGN.md.
// Copyright 2025 Budapest University of Technology and Economics
// Licensed under the Apache License, Version 2.0
// The stream classes, without locales, facets and stream buffers, whose members libc++
// compiles. Standard streams and files discard what is written. String streams keep it
// as text, since programs read it back with str(). Formatting honours the base, width
// and fill; floats print only their whole part. Files open or fail nondeterministically
// and have no content to read.
#ifndef _OM_IOSTREAM_H
#define _OM_IOSTREAM_H

#include <__config>
#include <__fwd/fstream.h>
#include <__fwd/ios.h>
#include <__fwd/istream.h>
#include <__fwd/ostream.h>
#include <__fwd/sstream.h>
#include <__fwd/streambuf.h>
#include <__fwd/string.h>
#include <cstddef>
#include <stdexcept>
#include <string>

extern "C" int __VERIFIER_nondet_int(void);

_LIBCPP_BEGIN_NAMESPACE_STD

using streamsize = long;
using streamoff  = long long;

// Formatting is not localized, so there is only one locale.
class locale {
public:
  static const locale& classic() {
    static const locale __l;
    return __l;
  }
};

class ios_base {
public:
  using fmtflags = unsigned;
  static const fmtflags boolalpha = 1, dec = 2, fixed = 4, hex = 8, internal = 16, left = 32, oct = 64, right = 128,
                        scientific = 256, showbase = 512, showpoint = 1024, showpos = 2048, skipws = 4096,
                        unitbuf = 8192, uppercase = 16384;
  static const fmtflags adjustfield = left | right | internal, basefield = dec | oct | hex,
                        floatfield = scientific | fixed;
  using iostate = unsigned;
  static const iostate goodbit = 0, badbit = 1, eofbit = 2, failbit = 4;
  using openmode = unsigned;
  static const openmode app = 1, ate = 2, binary = 4, in = 8, out = 16, trunc = 32;
  using seekdir = int;
  static const seekdir beg = 0, cur = 1, end = 2;

  class failure : public runtime_error {
  public:
    explicit failure(const char* __m) : runtime_error(__m) {}
  };

  fmtflags flags() const { return __flags_; }
  fmtflags flags(fmtflags __f) { fmtflags __o = __flags_; __flags_ = __f; return __o; }
  fmtflags setf(fmtflags __f) { fmtflags __o = __flags_; __flags_ |= __f; return __o; }
  fmtflags setf(fmtflags __f, fmtflags __mask) { fmtflags __o = __flags_; __flags_ = (__flags_ & ~__mask) | (__f & __mask); return __o; }
  void unsetf(fmtflags __f) { __flags_ &= ~__f; }
  streamsize width() const { return __width_; }
  streamsize width(streamsize __w) { streamsize __o = __width_; __width_ = __w; return __o; }
  streamsize precision() const { return __precision_; }
  streamsize precision(streamsize __p) { streamsize __o = __precision_; __precision_ = __p; return __o; }
  static bool sync_with_stdio(bool = true) { return true; }

  bool good() const { return __state_ == goodbit; }
  bool eof() const { return (__state_ & eofbit) != 0; }
  bool fail() const { return (__state_ & (failbit | badbit)) != 0; }
  bool bad() const { return (__state_ & badbit) != 0; }
  iostate rdstate() const { return __state_; }
  void setstate(iostate __s) { __state_ |= __s; }
  void clear(iostate __s = goodbit) { __state_ = __s; }
  iostate exceptions() const { return goodbit; }
  void exceptions(iostate) {}
  explicit operator bool() const { return !fail(); }
  bool operator!() const { return fail(); }
  locale imbue(const locale& __l) { return __l; }
  locale getloc() const { return locale(); }

protected:
  fmtflags __flags_ = dec | skipws | right;
  iostate __state_ = goodbit;
  streamsize __width_ = 0;
  streamsize __precision_ = 6;
};

template <class _CharT, class _Traits>
class basic_ios : public ios_base {
  _CharT __fill_ = _CharT(' ');

public:
  using char_type   = _CharT;
  using traits_type = _Traits;
  _CharT fill() const { return __fill_; }
  _CharT fill(_CharT __c) { _CharT __o = __fill_; __fill_ = __c; return __o; }
  _CharT widen(char __c) const { return _CharT(__c); }
  char narrow(_CharT __c, char) const { return char(__c); }
  basic_ios& copyfmt(const basic_ios& __o) {
    __flags_ = __o.__flags_;
    __width_ = __o.__width_;
    __precision_ = __o.__precision_;
    __fill_ = __o.__fill_;
    return *this;
  }
};

// Just the reading side of a stream, as `rdbuf()` is used to read all of one.
template <class _CharT, class _Traits>
class basic_streambuf {
  basic_istream<_CharT, _Traits>* __is_;

public:
  using char_type   = _CharT;
  using traits_type = _Traits;
  using int_type    = typename _Traits::int_type;
  explicit basic_streambuf(basic_istream<_CharT, _Traits>* __is = nullptr) : __is_(__is) {}
  int_type sgetc() {
    _CharT __c;
    return __is_ != nullptr && __is_->__om_peek(__c) ? _Traits::to_int_type(__c) : _Traits::eof();
  }
  int_type sbumpc() {
    int_type __r = sgetc();
    if (!_Traits::eq_int_type(__r, _Traits::eof())) __is_->__om_bump();
    return __r;
  }
  int_type snextc() {
    sbumpc();
    return sgetc();
  }
};

template <class _CharT, class _Traits>
class basic_ostream : public basic_ios<_CharT, _Traits> {
  using _Ios = basic_ios<_CharT, _Traits>;

protected:
  virtual void __om_write(const _CharT*, size_t) {}

  void __om_pad_and_write(const _CharT* __s, size_t __n) {
    size_t __w = size_t(this->width());
    this->width(0);
    bool __left = (this->flags() & ios_base::left) != 0;
    _CharT __f = this->fill();
    if (!__left) for (size_t __i = __n; __i < __w; ++__i) __om_write(&__f, 1);
    __om_write(__s, __n);
    if (__left) for (size_t __i = __n; __i < __w; ++__i) __om_write(&__f, 1);
  }
  basic_ostream& __om_put_int(unsigned long long __u, bool __neg) {
    unsigned __base = (this->flags() & ios_base::hex) != 0 ? 16 : ((this->flags() & ios_base::oct) != 0 ? 8 : 10);
    bool __upper = (this->flags() & ios_base::uppercase) != 0;
    _CharT __b[32];
    int __n = 32;
    do {
      unsigned __d = unsigned(__u % __base);
      __b[--__n] = _CharT(__d < 10 ? '0' + __d : (__upper ? 'A' : 'a') + (__d - 10));
      __u /= __base;
    } while (__u != 0);
    if (__neg) __b[--__n] = _CharT('-');
    __om_pad_and_write(__b + __n, size_t(32 - __n));
    return *this;
  }

public:
  using char_type   = _CharT;
  using traits_type = _Traits;
  basic_ostream() {}
  explicit basic_ostream(void*) {}
  virtual ~basic_ostream() {}

  basic_ostream& put(_CharT __c) { __om_write(&__c, 1); return *this; }
  basic_ostream& write(const _CharT* __s, streamsize __n) { __om_write(__s, size_t(__n)); return *this; }
  basic_ostream& flush() { return *this; }
  basic_ostream& operator<<(bool __v) {
    if ((this->flags() & ios_base::boolalpha) != 0) {
      const char* __s = __v ? "true" : "false";
      for (; *__s != 0; ++__s) put(_CharT(*__s));
      return *this;
    }
    return __om_put_int(__v ? 1 : 0, false);
  }
  basic_ostream& operator<<(short __v) { return __om_put_int(__v < 0 ? 0ULL - (unsigned long long)__v : (unsigned long long)__v, __v < 0); }
  basic_ostream& operator<<(unsigned short __v) { return __om_put_int(__v, false); }
  basic_ostream& operator<<(int __v) { return __om_put_int(__v < 0 ? 0ULL - (unsigned long long)__v : (unsigned long long)__v, __v < 0); }
  basic_ostream& operator<<(unsigned __v) { return __om_put_int(__v, false); }
  basic_ostream& operator<<(long __v) { return __om_put_int(__v < 0 ? 0ULL - (unsigned long long)__v : (unsigned long long)__v, __v < 0); }
  basic_ostream& operator<<(unsigned long __v) { return __om_put_int(__v, false); }
  basic_ostream& operator<<(long long __v) { return __om_put_int(__v < 0 ? 0ULL - (unsigned long long)__v : (unsigned long long)__v, __v < 0); }
  basic_ostream& operator<<(unsigned long long __v) { return __om_put_int(__v, false); }
  basic_ostream& operator<<(float __v) { return *this << (long long)__v; }
  basic_ostream& operator<<(double __v) { return *this << (long long)__v; }
  basic_ostream& operator<<(long double __v) { return *this << (long long)__v; }
  basic_ostream& operator<<(const void* __p) { return __om_put_int((unsigned long long)__p, false); }
  basic_ostream& operator<<(basic_ostream& (*__f)(basic_ostream&)) { return __f(*this); }
  basic_ostream& operator<<(basic_streambuf<_CharT, _Traits>* __sb) {
    while (!_Traits::eq_int_type(__sb->sgetc(), _Traits::eof())) put(_Traits::to_char_type(__sb->sbumpc()));
    return *this;
  }
  basic_ostream& operator<<(_Ios& (*__f)(_Ios&)) { __f(*this); return *this; }
  basic_ostream& operator<<(ios_base& (*__f)(ios_base&)) { __f(*this); return *this; }
  friend basic_ostream& operator<<(basic_ostream& __os, _CharT __c) { __os.__om_pad_and_write(&__c, 1); return __os; }
  friend basic_ostream& operator<<(basic_ostream& __os, const _CharT* __s) { __os.__om_pad_and_write(__s, _Traits::length(__s)); return __os; }
  template <class _A>
  friend basic_ostream& operator<<(basic_ostream& __os, const basic_string<_CharT, _Traits, _A>& __s) {
    __os.__om_pad_and_write(__s.data(), __s.size());
    return __os;
  }
};

template <class _Traits>
basic_ostream<wchar_t, _Traits>& operator<<(basic_ostream<wchar_t, _Traits>& __os, char __c) { return __os << wchar_t(__c); }
template <class _Traits>
basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& __os, signed char __c) { return __os << char(__c); }
template <class _Traits>
basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& __os, unsigned char __c) { return __os << char(__c); }
template <class _Traits>
basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& __os, const unsigned char* __s) { return __os << reinterpret_cast<const char*>(__s); }
template <class _Traits>
basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& __os, const signed char* __s) { return __os << reinterpret_cast<const char*>(__s); }

template <class _CharT, class _Traits>
class basic_istream : public basic_ios<_CharT, _Traits> {
  streamsize __gcount_ = 0;
  basic_streambuf<_CharT, _Traits> __sb_{this};

public:
  basic_streambuf<_CharT, _Traits>* rdbuf() { return &__sb_; }
  // The next character, or false at the end of input.
  virtual bool __om_peek(_CharT&) { return false; }
  virtual void __om_bump() {}

protected:
  bool __om_skip_ws() {
    _CharT __c;
    while (__om_peek(__c)) {
      if (__c != _CharT(' ') && __c != _CharT('\t') && __c != _CharT('\n') && __c != _CharT('\r')) return true;
      __om_bump();
    }
    this->setstate(ios_base::eofbit | ios_base::failbit);
    return false;
  }
  template <class _Int>
  basic_istream& __om_get_int(_Int& __v) {
    if (!__om_skip_ws()) return *this;
    unsigned __base = (this->flags() & ios_base::hex) != 0 ? 16 : ((this->flags() & ios_base::oct) != 0 ? 8 : 10);
    _CharT __c;
    bool __neg = false, __any = false;
    __om_peek(__c);
    if (__c == _CharT('-') || __c == _CharT('+')) { __neg = __c == _CharT('-'); __om_bump(); }
    unsigned long long __u = 0;
    while (__om_peek(__c)) {
      unsigned __d = __c >= _CharT('0') && __c <= _CharT('9') ? unsigned(__c - _CharT('0'))
                     : __c >= _CharT('a') && __c <= _CharT('f') ? unsigned(__c - _CharT('a')) + 10
                     : __c >= _CharT('A') && __c <= _CharT('F') ? unsigned(__c - _CharT('A')) + 10 : 99;
      if (__d >= __base) break;
      __u = __u * __base + __d;
      __any = true;
      __om_bump();
    }
    if (!__any) this->setstate(ios_base::failbit);
    else __v = __neg ? _Int(0ULL - __u) : _Int(__u);
    return *this;
  }

public:
  using char_type   = _CharT;
  using traits_type = _Traits;
  basic_istream() {}
  explicit basic_istream(void*) {}
  virtual ~basic_istream() {}

  basic_istream& operator>>(short& __v) { return __om_get_int(__v); }
  basic_istream& operator>>(unsigned short& __v) { return __om_get_int(__v); }
  basic_istream& operator>>(int& __v) { return __om_get_int(__v); }
  basic_istream& operator>>(unsigned& __v) { return __om_get_int(__v); }
  basic_istream& operator>>(long& __v) { return __om_get_int(__v); }
  basic_istream& operator>>(unsigned long& __v) { return __om_get_int(__v); }
  basic_istream& operator>>(long long& __v) { return __om_get_int(__v); }
  basic_istream& operator>>(unsigned long long& __v) { return __om_get_int(__v); }
  basic_istream& operator>>(bool& __v) { int __i = 0; __om_get_int(__i); __v = __i != 0; return *this; }
  basic_istream& operator>>(double& __v) { long long __i = 0; __om_get_int(__i); __v = double(__i); return *this; }
  basic_istream& operator>>(float& __v) { long long __i = 0; __om_get_int(__i); __v = float(__i); return *this; }
  basic_istream& operator>>(ios_base& (*__f)(ios_base&)) { __f(*this); return *this; }
  basic_istream& operator>>(basic_istream& (*__f)(basic_istream&)) { return __f(*this); }
  friend basic_istream& operator>>(basic_istream& __is, _CharT& __c) {
    if (__is.__om_skip_ws() && __is.__om_peek(__c)) __is.__om_bump();
    return __is;
  }
  template <class _A>
  friend basic_istream& operator>>(basic_istream& __is, basic_string<_CharT, _Traits, _A>& __s) {
    __s.clear();
    if (!__is.__om_skip_ws()) return __is;
    _CharT __c;
    while (__is.__om_peek(__c) && __c != _CharT(' ') && __c != _CharT('\t') && __c != _CharT('\n') && __c != _CharT('\r')) {
      __s.push_back(__c);
      __is.__om_bump();
    }
    return __is;
  }
  int peek() {
    _CharT __c;
    return __om_peek(__c) ? int(__c) : -1;
  }
  int get() {
    _CharT __c;
    if (!__om_peek(__c)) { this->setstate(ios_base::eofbit | ios_base::failbit); return -1; }
    __om_bump();
    __gcount_ = 1;
    return int(__c);
  }
  basic_istream& get(_CharT& __c) {
    if (__om_peek(__c)) __om_bump();
    else this->setstate(ios_base::eofbit | ios_base::failbit);
    return *this;
  }
  basic_istream& read(_CharT* __s, streamsize __n) {
    __gcount_ = 0;
    while (__gcount_ < __n && __om_peek(__s[__gcount_])) { __om_bump(); ++__gcount_; }
    if (__gcount_ < __n) this->setstate(ios_base::eofbit | ios_base::failbit);
    return *this;
  }
  basic_istream& ignore(streamsize __n = 1, int __delim = -1) {
    _CharT __c;
    for (streamsize __i = 0; __i < __n && __om_peek(__c); ++__i) { __om_bump(); if (int(__c) == __delim) break; }
    return *this;
  }
  streamsize gcount() const { return __gcount_; }
};

template <class _CharT, class _Traits, class _A>
basic_istream<_CharT, _Traits>& getline(basic_istream<_CharT, _Traits>& __is, basic_string<_CharT, _Traits, _A>& __s, _CharT __delim) {
  __s.clear();
  _CharT __c;
  bool __any = false;
  while (__is.__om_peek(__c)) {
    __any = true;
    __is.__om_bump();
    if (__c == __delim) return __is;
    __s.push_back(__c);
  }
  __is.setstate(__any ? ios_base::eofbit : (ios_base::eofbit | ios_base::failbit));
  return __is;
}
template <class _CharT, class _Traits, class _A>
basic_istream<_CharT, _Traits>& getline(basic_istream<_CharT, _Traits>& __is, basic_string<_CharT, _Traits, _A>& __s) {
  return std::getline(__is, __s, _CharT('\n'));
}
template <class _CharT, class _Traits, class _A>
basic_istream<_CharT, _Traits>& getline(basic_istream<_CharT, _Traits>&& __is, basic_string<_CharT, _Traits, _A>& __s) {
  return std::getline(__is, __s, _CharT('\n'));
}

// The bases share no basic_ios, as a virtual base needs construction tables that cir2c does
// not lower yet, so reads and writes keep separate states. Both views answer as the output one.
template <class _CharT, class _Traits>
class basic_iostream : public basic_istream<_CharT, _Traits>, public basic_ostream<_CharT, _Traits> {
  using _Out = basic_ostream<_CharT, _Traits>;

public:
  basic_iostream() {}
  explicit basic_iostream(void*) {}
  using _Out::bad;
  using _Out::clear;
  using _Out::eof;
  using _Out::fail;
  using _Out::fill;
  using _Out::flags;
  using _Out::good;
  using _Out::precision;
  using _Out::rdstate;
  using _Out::setf;
  using _Out::setstate;
  using _Out::unsetf;
  using _Out::width;
  using _Out::operator bool;
  using _Out::operator!;
};

template <class _CharT, class _Traits, class _Allocator>
class __om_string_store {
protected:
  basic_string<_CharT, _Traits, _Allocator> __str_;
  size_t __rd_ = 0;
  void __om_set(const basic_string<_CharT, _Traits, _Allocator>& __s) { __str_ = __s; __rd_ = 0; }
};

#define _OM_STRING_STREAM_READ                                                                                         \
  bool __om_peek(_CharT& __c) override {                                                                               \
    if (this->__rd_ >= this->__str_.size()) return false;                                                              \
    __c = this->__str_[this->__rd_];                                                                                   \
    return true;                                                                                                       \
  }                                                                                                                    \
  void __om_bump() override { ++this->__rd_; }
#define _OM_STRING_STREAM_WRITE                                                                                        \
  void __om_write(const _CharT* __s, size_t __n) override { this->__str_.append(__s, __n); }
#define _OM_STRING_STREAM_COMMON(_Name)                                                                                \
  using _String = basic_string<_CharT, _Traits, _Allocator>;                                                           \
  _Name() {}                                                                                                           \
  explicit _Name(ios_base::openmode) {}                                                                                \
  explicit _Name(const _String& __s, ios_base::openmode = 0) { this->__om_set(__s); }                                  \
  _String str() const { return this->__str_; }                                                                         \
  void str(const _String& __s) { this->__om_set(__s); }

template <class _CharT, class _Traits, class _Allocator>
class basic_ostringstream : public basic_ostream<_CharT, _Traits>, private __om_string_store<_CharT, _Traits, _Allocator> {
  _OM_STRING_STREAM_WRITE
public:
  _OM_STRING_STREAM_COMMON(basic_ostringstream)
};
template <class _CharT, class _Traits, class _Allocator>
class basic_istringstream : public basic_istream<_CharT, _Traits>, private __om_string_store<_CharT, _Traits, _Allocator> {
  _OM_STRING_STREAM_READ
public:
  _OM_STRING_STREAM_COMMON(basic_istringstream)
};
template <class _CharT, class _Traits, class _Allocator>
class basic_stringstream : public basic_iostream<_CharT, _Traits>, private __om_string_store<_CharT, _Traits, _Allocator> {
  _OM_STRING_STREAM_READ
  _OM_STRING_STREAM_WRITE
public:
  _OM_STRING_STREAM_COMMON(basic_stringstream)
};

#define _OM_FILE_STREAM(_Name, _Base, _Mode)                                                                           \
  template <class _CharT, class _Traits>                                                                               \
  class _Name : public _Base<_CharT, _Traits> {                                                                        \
    bool __open_ = false;                                                                                              \
                                                                                                                       \
  public:                                                                                                              \
    _Name() {}                                                                                                         \
    explicit _Name(const char* __f, ios_base::openmode __m = _Mode) { open(__f, __m); }                                \
    explicit _Name(const string& __f, ios_base::openmode __m = _Mode) { open(__f.c_str(), __m); }                      \
    void open(const char*, ios_base::openmode = _Mode) {                                                               \
      __open_ = __VERIFIER_nondet_int() != 0;                                                                          \
      if (!__open_) this->setstate(ios_base::failbit);                                                                 \
    }                                                                                                                  \
    void open(const string& __f, ios_base::openmode __m = _Mode) { open(__f.c_str(), __m); }                           \
    bool is_open() const { return __open_; }                                                                           \
    void close() { __open_ = false; }                                                                                  \
  };
_OM_FILE_STREAM(basic_ifstream, basic_istream, ios_base::in)
_OM_FILE_STREAM(basic_ofstream, basic_ostream, ios_base::out)
_OM_FILE_STREAM(basic_fstream, basic_iostream, ios_base::in | ios_base::out)
#undef _OM_FILE_STREAM
#undef _OM_STRING_STREAM_READ
#undef _OM_STRING_STREAM_WRITE
#undef _OM_STRING_STREAM_COMMON


template <class _CharT, class _Traits>
basic_ostream<_CharT, _Traits>& endl(basic_ostream<_CharT, _Traits>& __os) { return __os.put(_CharT('\n')); }
template <class _CharT, class _Traits>
basic_ostream<_CharT, _Traits>& ends(basic_ostream<_CharT, _Traits>& __os) { return __os.put(_CharT()); }
template <class _CharT, class _Traits>
basic_ostream<_CharT, _Traits>& flush(basic_ostream<_CharT, _Traits>& __os) { return __os; }
template <class _CharT, class _Traits>
basic_istream<_CharT, _Traits>& ws(basic_istream<_CharT, _Traits>& __is) { return __is; }

#define _OM_FLAG(_Name, _Set, _Mask)                                                                                   \
  inline ios_base& _Name(ios_base& __s) { __s.setf(_Set, _Mask); return __s; }
_OM_FLAG(dec, ios_base::dec, ios_base::basefield)
_OM_FLAG(hex, ios_base::hex, ios_base::basefield)
_OM_FLAG(oct, ios_base::oct, ios_base::basefield)
_OM_FLAG(left, ios_base::left, ios_base::adjustfield)
_OM_FLAG(right, ios_base::right, ios_base::adjustfield)
_OM_FLAG(internal, ios_base::internal, ios_base::adjustfield)
_OM_FLAG(fixed, ios_base::fixed, ios_base::floatfield)
_OM_FLAG(scientific, ios_base::scientific, ios_base::floatfield)
_OM_FLAG(boolalpha, ios_base::boolalpha, ios_base::boolalpha)
_OM_FLAG(noboolalpha, 0, ios_base::boolalpha)
_OM_FLAG(showbase, ios_base::showbase, ios_base::showbase)
_OM_FLAG(noshowbase, 0, ios_base::showbase)
_OM_FLAG(uppercase, ios_base::uppercase, ios_base::uppercase)
_OM_FLAG(nouppercase, 0, ios_base::uppercase)
#undef _OM_FLAG

struct __om_setw { int __n; };
struct __om_setprecision { int __n; };
struct __om_setbase { int __n; };
template <class _CharT>
struct __om_setfill { _CharT __c; };
inline __om_setw setw(int __n) { return {__n}; }
inline __om_setprecision setprecision(int __n) { return {__n}; }
inline __om_setbase setbase(int __n) { return {__n}; }
template <class _CharT>
__om_setfill<_CharT> setfill(_CharT __c) { return {__c}; }
template <class _C, class _T>
basic_ostream<_C, _T>& operator<<(basic_ostream<_C, _T>& __os, __om_setw __m) { __os.width(__m.__n); return __os; }
template <class _C, class _T>
basic_ostream<_C, _T>& operator<<(basic_ostream<_C, _T>& __os, __om_setprecision __m) { __os.precision(__m.__n); return __os; }
template <class _C, class _T>
basic_ostream<_C, _T>& operator<<(basic_ostream<_C, _T>& __os, __om_setbase __m) {
  __os.setf(__m.__n == 16 ? ios_base::hex : (__m.__n == 8 ? ios_base::oct : ios_base::dec), ios_base::basefield);
  return __os;
}
template <class _C, class _T, class _F>
basic_ostream<_C, _T>& operator<<(basic_ostream<_C, _T>& __os, __om_setfill<_F> __m) { __os.fill(_C(__m.__c)); return __os; }

inline ostream cout;
inline ostream cerr;
inline ostream clog;
inline istream cin;
inline wostream wcout;
inline wostream wcerr;
inline wostream wclog;
inline wistream wcin;

_LIBCPP_END_NAMESPACE_STD

#endif
