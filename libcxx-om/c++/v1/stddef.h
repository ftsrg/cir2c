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

// Operational Model: <stddef.h>
// Minimal C stddef for verification — fully self-contained, no system includes

#ifndef _LIBCXX_OM_STDDEF_H
#define _LIBCXX_OM_STDDEF_H

namespace std {
    typedef unsigned long size_t;
    typedef long ptrdiff_t;
}

#endif // _LIBCXX_OM_STDDEF_H
