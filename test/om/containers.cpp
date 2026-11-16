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
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Base { virtual ~Base() {} virtual int f() const = 0; };
struct D : Base, std::enable_shared_from_this<D> { int v; explicit D(int v) : v(v) {} int f() const override { return v; } };
static int plus3(int x) { return x + 3; }
struct Counted { static int live; int v; Counted(int v = 0) : v(v) { ++live; } Counted(const Counted& o) : v(o.v) { ++live; } ~Counted() { --live; } };
int Counted::live = 0;

int main() {
  std::vector<int> v{5, 1, 4};
  v.push_back(2); v.insert(v.begin() + 1, 9); v.insert(v.end(), 2, 7); v.erase(v.begin());
  std::sort(v.begin(), v.end());
  for (int x : v) printf("%d ", x);
  printf("| %zu %d %d\n", v.size(), v.front(), v.back());
  std::vector<std::string> vs(2, "ab"); vs.emplace_back("cd"); vs.resize(4); vs[3] = vs[2] + vs[0];
  printf("%s %zu\n", vs[3].c_str(), vs.size());
  std::string s = "hello"; s += " world"; s.insert(5, ","); s.replace(0, 1, "J"); s.erase(s.size() - 1);
  printf("%s %zu %zu %zu %s %d\n", s.c_str(), s.find("wor"), s.rfind('l'), s.find_first_of("xyz,"), s.substr(1, 3).c_str(), (s.compare("Jello") > 0));
  printf("%s %d %ld\n", std::to_string(-42).c_str(), std::stoi("123"), std::stol("-77"));
  std::map<std::string, int> m; m["b"] = 2; m["a"] = 1; m.emplace("c", 3); m.insert({"a", 9}); m.erase("b");
  for (auto& p : m) printf("%s=%d ", p.first.c_str(), p.second);
  printf("| %zu %d %d\n", m.size(), (int)m.count("z"), m.at("c"));
  std::multimap<int, int> mm; mm.insert({1, 1}); mm.insert({1, 2}); mm.insert({0, 3}); printf("%zu %d\n", mm.count(1), mm.begin()->second);
  std::set<int> st{3, 1, 2, 3}; auto lb = st.lower_bound(2); printf("%zu %d %d\n", st.size(), *lb, *st.rbegin());
  std::unordered_map<int, std::string> um; um[1] = "x"; um.emplace(2, "y"); um[1] += "z"; printf("%zu %s %d\n", um.size(), um[1].c_str(), (int)um.count(5));
  std::unordered_set<std::string> us{"p", "q", "p"}; printf("%zu %d\n", us.size(), (int)us.count("q"));
  std::list<int> l{1, 2, 3}; l.push_front(0); l.remove(2); auto it = l.begin(); ++it; l.insert(it, 7); int sum = 0; for (int x : l) sum = sum * 10 + x; printf("%d %zu\n", sum, l.size());
  std::deque<int> d; for (int i = 0; i < 20; ++i) { if (i % 3 == 0) d.push_front(i); else d.push_back(i); } d.pop_back(); d.pop_front(); printf("%d %d %d %zu\n", d[0], d[5], d.back(), d.size());
  std::queue<int> q; q.push(1); q.push(2); q.pop(); std::priority_queue<int> pq; pq.push(1); pq.push(5); pq.push(3); printf("%d %d\n", q.front(), pq.top());
  auto sp = std::make_shared<D>(11); std::shared_ptr<Base> b = sp; std::weak_ptr<D> w = sp; printf("%d %ld %d\n", b->f(), sp.use_count(), (int)w.expired());
  sp.reset(); b.reset(); printf("%d\n", (int)w.expired());
  std::unique_ptr<Base> u(new D(4)); printf("%d\n", u->f());
  { std::vector<Counted> c(3, Counted(1)); c.pop_back(); c.emplace_back(2); printf("%d ", Counted::live); } printf("%d\n", Counted::live);
  std::function<int(int)> fn = plus3; int k = 10; std::function<int(int)> fn2 = [k](int x) { return x * k; }; auto bd = std::bind(plus3, std::placeholders::_1); printf("%d %d %d %d\n", fn(1), fn2(2), bd(5), (int)(bool)std::function<void()>());
  std::ostringstream os; os << "n=" << 42 << ' ' << std::hex << 255 << std::dec << std::setw(4) << std::setfill('0') << 7 << std::string("!") << true; printf("%s\n", os.str().c_str());
  std::istringstream is("12 abc\nline two\n"); int ni; std::string w1, ln; is >> ni >> w1; std::getline(is, ln); std::getline(is, ln); printf("%d %s %s\n", ni, w1.c_str(), ln.c_str());
  try { v.at(100) = 1; } catch (const std::out_of_range&) { printf("caught\n"); }
  std::atomic<int> at{1}; at.fetch_add(2); std::mutex mx; int shared = 0; { std::lock_guard<std::mutex> g(mx); shared += at.load(); }
  std::thread t([&]() { std::lock_guard<std::mutex> g(mx); shared *= 2; }); t.join(); printf("%d\n", shared);

  return 0;
}
