// Copyright (c) 2021, gottingen group.
// All rights reserved.
// Created by liyinbin lijippy@163.com


#ifndef FLARE_ATOMIC_UNALIGNED_ACCESS_H_
#define FLARE_ATOMIC_UNALIGNED_ACCESS_H_

#include <string.h>
#include <cstdint>
#include "flare/base/profile.h"

// unaligned APIs

// Portable handling of unaligned loads, stores, and copies.

// The unaligned API is C++ only.  The declarations use C++ features
// (namespaces, inline) which are absent or incompatible in C.
#if defined(__cplusplus)

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
// Consider we have an unaligned load/store of 4 bytes from address 0x...05.
// AddressSanitizer will treat it as a 3-byte access to the range 05:07 and
// will miss a bug if 08 is the first unaddressable byte.
// ThreadSanitizer will also treat this as a 3-byte access to 05:07 and will
// miss a race between this access and some other accesses to 08.
// MemorySanitizer will correctly propagate the shadow on unaligned stores
// and correctly report bugs on unaligned loads, but it may not properly
// update and report the origin of the uninitialized memory.
// For all three tools, replacing an unaligned access with a tool-specific
// callback solves the problem.

// Make sure uint16_t/uint32_t/uint64_t are defined.
#include <stdint.h>

extern "C" {
uint16_t __sanitizer_unaligned_load16(const void *p);
uint32_t __sanitizer_unaligned_load32(const void *p);
uint64_t __sanitizer_unaligned_load64(const void *p);
void __sanitizer_unaligned_store16(void *p, uint16_t v);
void __sanitizer_unaligned_store32(void *p, uint32_t v);
void __sanitizer_unaligned_store64(void *p, uint64_t v);
}  // extern "C"

namespace flare::base {

FLARE_FORCE_INLINE uint16_t unaligned_load16(const void *p) {
  return __sanitizer_unaligned_load16(p);
}

FLARE_FORCE_INLINE uint32_t unaligned_load32(const void *p) {
  return __sanitizer_unaligned_load32(p);
}

FLARE_FORCE_INLINE uint64_t unaligned_load64(const void *p) {
  return __sanitizer_unaligned_load64(p);
}

FLARE_FORCE_INLINE void unaligned_store16(void *p, uint16_t v) {
  __sanitizer_unaligned_store16(p, v);
}

FLARE_FORCE_INLINE void unaligned_store32(void *p, uint32_t v) {
  __sanitizer_unaligned_store32(p, v);
}

FLARE_FORCE_INLINE void unaligned_store64(void *p, uint64_t v) {
  __sanitizer_unaligned_store64(p, v);
}


}  // namespace flare::base

#define FLARE_INTERNAL_UNALIGNED_LOAD16(_p) \
  (flare::base::unaligned_load16(_p))
#define FLARE_INTERNAL_UNALIGNED_LOAD32(_p) \
  (flare::base::unaligned_load32(_p))
#define FLARE_INTERNAL_UNALIGNED_LOAD64(_p) \
  (flare::base::unaligned_load64(_p))

#define FLARE_INTERNAL_UNALIGNED_STORE16(_p, _val) \
  (flare::base::unaligned_store16(_p, _val))
#define FLARE_INTERNAL_UNALIGNED_STORE32(_p, _val) \
  (flare::base::unaligned_store32(_p, _val))
#define FLARE_INTERNAL_UNALIGNED_STORE64(_p, _val) \
  (flare::base::unaligned_store64(_p, _val))

#else

namespace flare::base {


    FLARE_FORCE_INLINE uint16_t unaligned_load16(const void *p) {
        uint16_t t;
        memcpy(&t, p, sizeof t);
        return t;
    }

    FLARE_FORCE_INLINE uint32_t unaligned_load32(const void *p) {
        uint32_t t;
        memcpy(&t, p, sizeof t);
        return t;
    }

    FLARE_FORCE_INLINE uint64_t unaligned_load64(const void *p) {
        uint64_t t;
        memcpy(&t, p, sizeof t);
        return t;
    }

    FLARE_FORCE_INLINE void unaligned_store16(void *p, uint16_t v) { memcpy(p, &v, sizeof v); }

    FLARE_FORCE_INLINE void unaligned_store32(void *p, uint32_t v) { memcpy(p, &v, sizeof v); }

    FLARE_FORCE_INLINE void unaligned_store64(void *p, uint64_t v) { memcpy(p, &v, sizeof v); }


}  // namespace flare::base

#define FLARE_INTERNAL_UNALIGNED_LOAD16(_p) \
  (::flare::base::unaligned_load16(_p))
#define FLARE_INTERNAL_UNALIGNED_LOAD32(_p) \
  (::flare::base::unaligned_load32(_p))
#define FLARE_INTERNAL_UNALIGNED_LOAD64(_p) \
  (::flare::base::unaligned_load64(_p))

#define FLARE_INTERNAL_UNALIGNED_STORE16(_p, _val) \
  (::flare::base::unaligned_store16(_p, _val))
#define FLARE_INTERNAL_UNALIGNED_STORE32(_p, _val) \
  (::flare::base::unaligned_store32(_p, _val))
#define FLARE_INTERNAL_UNALIGNED_STORE64(_p, _val) \
  (::flare::base::unaligned_store64(_p, _val))

#endif

#endif  // defined(__cplusplus), end of unaligned API

#endif  // FLARE_ATOMIC_UNALIGNED_ACCESS_H_
