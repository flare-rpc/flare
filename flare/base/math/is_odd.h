// Copyright (c) 2021, gottingen group.
// All rights reserved.
// Created by liyinbin lijippy@163.com

#ifndef FLARE_BASE_MATH_IS_ODD_H_
#define FLARE_BASE_MATH_IS_ODD_H_

namespace flare::base {

    constexpr bool is_odd(const long long x) noexcept {
        return (x & 1U) != 0;
    }

}  // namespace flare::base

#endif  // FLARE_BASE_MATH_IS_ODD_H_
