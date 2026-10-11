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

// Issue #12: clang writes a class whose tail padding a derived class can reuse
// as a packed struct with explicit padding. In C, such a type has the
// alignment 1, so a local variable of it could be placed at any address, and
// the address of a member (here &node.right) would not be aligned. The CIR
// gives each variable the alignment of the C++ object, so the output writes it
// on the declaration. (Globals had it already.)
//
// cir2c-test-output-contains: struct Node node2 __attribute__((aligned(8)));
// cir2c-test-output-contains: struct Red red4 __attribute__((aligned(8)));

class Node {
public:
  Node *right = nullptr;
  Node *parent = nullptr;
  bool black = false;
};
struct Red : Node { char tag = 'r'; };  // tag is in the tail padding of Node
static Node global;

int main() {
  Node node;
  Node **slot = &node.right;
  *slot = &global;
  Red red;
  Node **up = &red.parent;
  *up = &node;
  return (node.right == &global && red.parent == &node && red.tag == 'r') ? 0 : 1;
}
