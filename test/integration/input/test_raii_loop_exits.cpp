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

// Integration test: cir.cleanup.scope must run on EVERY exit path.
//
// A scoped object in a loop body puts the destructor in the cleanup region of
// a cir.cleanup.scope. The cleanup has to be emitted on all three exits:
// normal fall-through, `continue`, and `break`. cir2c used to mark a cleanup
// region "consumed" the first time any early exit emitted it, so the remaining
// paths silently lost the destructor. The generated C stayed valid, which is
// why a syntax-only check could not see the bug.
//
// Self-checking: `live` returns to 0 only if every destructor ran.

static int live = 0;
static int constructed = 0;

struct Tracer {
    Tracer()  { live++; constructed++; }
    ~Tracer() { live--; }
};

// continue + break + fall-through, all crossing the same cleanup scope.
static bool all_exit_paths() {
    live = 0; constructed = 0;
    for (int i = 0; i < 5; i++) {
        Tracer t;
        if (i == 1) continue;   // early exit 1
        if (i == 3) break;      // early exit 2
    }                           // fall-through exit for i == 0, 2
    return live == 0 && constructed == 4;
}

// Two conditional breaks out of one scope: neither may consume the other's
// cleanup, and the fall-through path still needs its own.
static bool two_breaks() {
    live = 0; constructed = 0;
    for (int i = 0; i < 6; i++) {
        Tracer t;
        if (i == 2) break;
        if (i == 4) break;
    }
    return live == 0 && constructed == 3;
}

// Nested loops: the inner cleanup must not be consumed by the outer loop's exit.
static bool nested_loops() {
    live = 0; constructed = 0;
    for (int i = 0; i < 3; i++) {
        Tracer outer;
        for (int j = 0; j < 3; j++) {
            Tracer inner;
            if (j == 1) continue;
            if (j == 2) break;
        }
        if (i == 2) break;
    }
    return live == 0 && constructed == 12;   // 3 outer + 3*3 inner
}

int main() {
    if (!all_exit_paths()) return 1;
    if (!two_breaks())     return 2;
    if (!nested_loops())   return 3;
    return 0;
}
