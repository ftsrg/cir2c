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


// The ostream-sink model (README 8.2): output to std::cout, std::cerr and
// std::clog is lost, and only clear() and setstate() change the stream state.
// The native run uses the real library, so equal exit codes show that the
// model keeps the stream state and evaluates the operands as the library does.

#include <iostream>
#include <string>

struct Point {
  int x, y;
};

std::ostream &operator<<(std::ostream &os, const Point &p) {
  return os << '(' << p.x << ", " << p.y << ')';
}

static int calls = 0;
static int nextValue() { return ++calls; }

// The constructor of a global writes: the streams are usable during the
// initialization of globals.
struct Greeter {
  bool ok;
  Greeter() { ok = static_cast<bool>(std::cout << "init" << std::endl); }
} greeter;

int main() {
  if (!greeter.ok) return 1;
  std::cout << "text " << 42 << ' ' << 3.5 << true << 7u << -7L << 8ull << 1.5f
            << static_cast<const void *>(&calls) << nullptr << std::endl;
  std::cout << std::string("string") << Point{1, 2} << std::ends << std::flush;
  std::cerr << "to cerr" << std::endl;
  std::clog << nextValue() << nextValue() << std::endl;
  if (calls != 2) return 2;
  if (!std::cout.good() || !std::cout || std::cout.rdstate() != std::ios_base::goodbit)
    return 3;

  std::ostream &out = std::cerr;
  out << "through a reference" << std::endl;
  std::cout.put('x').flush();

  std::cout.setstate(std::ios_base::failbit);
  if (std::cout.good() || !std::cout.fail() || std::cout.bad() || std::cout) return 4;
  // A stream that is not good writes nothing and keeps its state.
  std::cout << "lost";
  if (std::cout.rdstate() != std::ios_base::failbit) return 5;
  std::cout.setstate(std::ios_base::badbit);
  if (!std::cout.bad() || !(std::cout.rdstate() & std::ios_base::failbit)) return 6;
  std::cout.clear();
  if (!std::cout.good() || std::cout.eof()) return 7;
  std::cout.clear(std::ios_base::eofbit);
  if (!std::cout.eof() || std::cout.fail() || !std::cout) return 8;
  std::cout.clear();
  return 0;
}
