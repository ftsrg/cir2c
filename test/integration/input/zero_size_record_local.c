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

// A local variable of a record of size 0 (only a zero-length array, a GNU
// extension) gets no initializer: clang rejects `{0}` for an aggregate without
// elements (llvm-test-suite va-arg-22).

// cir2c-test-output-lacks: = {0};

struct Empty { char x[0]; };

static int size(struct Empty e) { return (int)sizeof e; }

int main(void) {
  struct Empty e;
  return size(e);
}
