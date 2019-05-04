//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// UNSUPPORTED: c++98, c++03, c++11, c++14, c++17

// <chrono>

// class month_weekday_last;

#include <chrono>
#include <type_traits>
#include <cassert>

#include "test_macros.h"

int main()
{
    using month_weekday_last = std::chrono::month_weekday_last;

    static_assert(std::is_trivially_copyable_v<month_weekday_last>, "");
    static_assert(std::is_standard_layout_v<month_weekday_last>, "");
}
