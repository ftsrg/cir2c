// Operational Model: <stddef.h>
// Minimal C stddef for verification — fully self-contained, no system includes

#ifndef _LIBCXX_OM_STDDEF_H
#define _LIBCXX_OM_STDDEF_H

namespace std {
    typedef unsigned long size_t;
    typedef long ptrdiff_t;
}

#endif // _LIBCXX_OM_STDDEF_H
