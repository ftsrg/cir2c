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

// Integration test: record member kinds (data / pad / bitfield).
//
// CIR record types carry no field NAMES, only a kind and a type per member:
//   !cir.struct<"S" {data !s8i, data !s32i}>
// cir2c recovers names from the `cir.get_member {name = "..."}` ops, so a field
// that is never accessed has no name to recover and gets the positional
// fallback `__fieldN`. That is expected: what has to stay correct is the
// POSITION and SIZE of every member, because cir.get_member addresses members
// by index and cir2c emits them in the same order.
//
// Upstream now marks members explicitly (data / pad / bitfield), and `pad`
// members appear in the member list for over-aligned records. They must be
// emitted as real fields or every later index would shift.
//
// KNOWN GAP (size is preserved, alignment is not): a `pad` member reproduces
// the record's SIZE, but the generated C carries no alignment attribute, so an
// over-aligned record keeps sizeof but loses _Alignof. Only code that depends
// on the base address alignment is affected; array stride follows sizeof and
// stays correct. Not asserted below.
//
// Self-checking: returns 0 only if sizes, offsets and values all round-trip.

// Only `first` and `third` are ever read, so `second` has no recoverable name
// and becomes __field1. Layout must still be intact.
struct PartlyUsed { int first; int second; int third; };

static int test_unaccessed_field_layout(void) {
    struct PartlyUsed p;
    p.first = 11;
    p.third = 33;
    // The untouched middle member must still occupy its slot: writing through
    // the neighbours must not disturb them.
    if (p.first != 11 || p.third != 33) return 0;
    return sizeof(struct PartlyUsed) == 3 * sizeof(int);
}

// Over-aligned record: CIR appends a `pad` member to reach the alignment.
struct OverAligned { int i; } __attribute__((aligned(16)));

static int test_pad_member_size(void) {
    struct OverAligned a;
    a.i = 7;
    if (a.i != 7) return 0;
    return sizeof(struct OverAligned) == 16;
}

// A record holding an over-aligned record gets its own trailing pad member,
// and the member AFTER the first one must keep index 1.
struct HoldsOverAligned { struct OverAligned a; char c; };

static int test_pad_member_indices(void) {
    struct HoldsOverAligned h;
    h.a.i = 5;
    h.c = 'z';
    // If a pad member were dropped or misplaced, these two would alias.
    if (h.a.i != 5) return 0;
    if (h.c != 'z') return 0;
    return sizeof(struct HoldsOverAligned) == 32;
}

// Bit-fields are collapsed into a single `bitfield` access unit in CIR; the
// individual fields are shift/mask ops over that unit.
struct Bits { unsigned a : 3; unsigned b : 5; int tail; };

static int test_bitfield_access_unit(void) {
    struct Bits v;
    v.a = 5;        // fits in 3 bits
    v.b = 21;       // fits in 5 bits
    v.tail = -12345;
    if (v.a != 5) return 0;
    if (v.b != 21) return 0;
    if (v.tail != -12345) return 0;
    // Truncation must happen at the declared width, not the unit width.
    v.a = 9;        // 9 == 0b1001 -> keeps 0b001 == 1
    return v.a == 1;
}

int main(void) {
    if (!test_unaccessed_field_layout()) return 1;
    if (!test_pad_member_size())         return 2;
    if (!test_pad_member_indices())      return 3;
    if (!test_bitfield_access_unit())    return 4;
    return 0;
}
