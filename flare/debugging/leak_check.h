// Copyright (c) 2021, gottingen group.
// All rights reserved.
// Created by liyinbin lijippy@163.com
//
// -----------------------------------------------------------------------------
// File: leak_check.h
// -----------------------------------------------------------------------------
//
// This file contains functions that affect leak checking behavior within
// targets built with the LeakSanitizer (LSan), a memory leak detector that is
// integrated within the AddressSanitizer (ASan) as an additional component, or
// which can be used standalone. LSan and ASan are included (or can be provided)
// as additional components for most compilers such as Clang, gcc and MSVC.
// Note: this leak checking API is not yet supported in MSVC.
// Leak checking is enabled by default in all ASan builds.
//
// See https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer
//
// -----------------------------------------------------------------------------
#ifndef FLARE_DEBUGGING_LEAK_CHECK_H_
#define FLARE_DEBUGGING_LEAK_CHECK_H_

#include <cstddef>

#include "flare/base/profile.h"

namespace flare::debugging {


    // have_leak_sanitizer()
    //
    // Returns true if a leak-checking sanitizer (either ASan or standalone LSan) is
    // currently built into this target.
    bool have_leak_sanitizer();

    // do_ignore_leak()
    //
    // Implements `ignore_leak()` below. This function should usually
    // not be called directly; calling `ignore_leak()` is preferred.
    void do_ignore_leak(const void *ptr);

    // ignore_leak()
    //
    // Instruct the leak sanitizer to ignore leak warnings on the object referenced
    // by the passed pointer, as well as all heap objects transitively referenced
    // by it. The passed object pointer can point to either the beginning of the
    // object or anywhere within it.
    //
    // Example:
    //
    //   static T* obj = ignore_leak(new T(...));
    //
    // If the passed `ptr` does not point to an actively allocated object at the
    // time `ignore_leak()` is called, the call is a no-op; if it is actively
    // allocated, the object must not get deallocated later.
    //
    template<typename T>
    T *ignore_leak(T *ptr) {
        do_ignore_leak(ptr);
        return ptr;
    }

    // leak_check_disabler
    //
    // This helper class indicates that any heap allocations done in the code block
    // covered by the scoped object, which should be allocated on the stack, will
    // not be reported as leaks. Leak check disabling will occur within the code
    // block and any nested function calls within the code block.
    //
    // Example:
    //
    //   void Foo() {
    //     leak_check_disabler disabler;
    //     ... code that allocates objects whose leaks should be ignored ...
    //   }
    //
    // REQUIRES: Destructor runs in same thread as constructor
    class leak_check_disabler {
    public:
        leak_check_disabler();

        leak_check_disabler(const leak_check_disabler &) = delete;

        leak_check_disabler &operator=(const leak_check_disabler &) = delete;

        ~leak_check_disabler();
    };

    // register_live_pointers()
    //
    // Registers `ptr[0,size-1]` as pointers to memory that is still actively being
    // referenced and for which leak checking should be ignored. This function is
    // useful if you store pointers in mapped memory, for memory ranges that we know
    // are correct but for which normal analysis would flag as leaked code.
    void register_live_pointers(const void *ptr, size_t size);

    // unregister_live_pointers()
    //
    // Deregisters the pointers previously marked as active in
    // `register_live_pointers()`, enabling leak checking of those pointers.
    void unregister_live_pointers(const void *ptr, size_t size);


}  // namespace flare::debugging

#endif  // FLARE_DEBUGGING_LEAK_CHECK_H_
