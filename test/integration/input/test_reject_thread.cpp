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

// The output of a C++ program has one thread: the exception state is a set of
// plain globals, and run-cir2c.sh compiles C++ with -fno-threadsafe-statics
// (README 5.1). Thus cir2c rejects a C++ program that starts a thread.

// cir2c-test-expect-error: the program starts a thread (pthread_create)

#include <pthread.h>

static int counter = 0;

static void *work(void *) {
  static int calls = 0;
  ++calls;
  ++counter;
  return nullptr;
}

int main() {
  pthread_t thread;
  if (pthread_create(&thread, nullptr, work, nullptr) != 0) return 2;
  pthread_join(thread, nullptr);
  return counter == 1 ? 0 : 1;
}
