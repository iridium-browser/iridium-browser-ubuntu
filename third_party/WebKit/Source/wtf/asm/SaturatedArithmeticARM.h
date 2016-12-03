// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SaturatedArithmeticARM_h
#define SaturatedArithmeticARM_h

#include "wtf/CPU.h"
#include <limits>
#include <stdint.h>

ALWAYS_INLINE int32_t saturatedAddition(int32_t a, int32_t b)
{
    int32_t result;

    asm("qadd %[output],%[first],%[second]"
        :   [output]  "=r"  (result)
        :   [first]   "r"   (a),
            [second]  "r"   (b));

    return result;
}

ALWAYS_INLINE int32_t saturatedSubtraction(int32_t a, int32_t b)
{
    int32_t result;

    asm("qsub %[output],%[first],%[second]"
        :   [output] "=r"  (result)
        :   [first]  "r"   (a),
            [second] "r"   (b));

    return result;
}

ALWAYS_INLINE int32_t saturatedNegative(int32_t a)
{
    return saturatedSubtraction(0, a);
}

inline int getMaxSaturatedSetResultForTesting(int FractionalShift)
{
    // For ARM Asm version the set function maxes out to the biggest
    // possible integer part with the fractional part zero'd out.
    // e.g. 0x7fffffc0.
    return std::numeric_limits<int>::max() & ~((1 << FractionalShift)-1);
}

inline int getMinSaturatedSetResultForTesting(int FractionalShift)
{
    return std::numeric_limits<int>::min();
}

template <int FractionalShift>
ALWAYS_INLINE int saturatedSet(int value)
{
    // Figure out how many bits are left for storing the integer part of
    // the fixed point number, and saturate our input to that
    enum { Saturate = 32 - FractionalShift };

    int result;

    // The following ARM code will Saturate the passed value to the number of
    // bits used for the whole part of the fixed point representation, then
    // shift it up into place. This will result in the low <FractionShift> bits
    // all being 0's. When the value saturates this gives a different result
    // to from the C++ case; in the C++ code a saturated value has all the low
    // bits set to 1 (for a +ve number at least). This cannot be done rapidly
    // in ARM ... we live with the difference, for the sake of speed.

    asm("ssat %[output],%[saturate],%[value]\n\t"
        "lsl  %[output],%[shift]"
        :   [output]    "=r"  (result)
        :   [value]     "r"   (value),
            [saturate]  "n"   (Saturate),
            [shift]     "n"   (FractionalShift));

    return result;
}


template <int FractionalShift>
ALWAYS_INLINE int saturatedSet(unsigned value)
{
    // Here we are being passed an unsigned value to saturate,
    // even though the result is returned as a signed integer. The ARM
    // instruction for unsigned saturation therefore needs to be given one
    // less bit (i.e. the sign bit) for the saturation to work correctly; hence
    // the '31' below.
    enum { Saturate = 31 - FractionalShift };

    // The following ARM code will Saturate the passed value to the number of
    // bits used for the whole part of the fixed point representation, then
    // shift it up into place. This will result in the low <FractionShift> bits
    // all being 0's. When the value saturates this gives a different result
    // to from the C++ case; in the C++ code a saturated value has all the low
    // bits set to 1. This cannot be done rapidly in ARM, so we live with the
    // difference, for the sake of speed.

    int result;

    asm("usat %[output],%[saturate],%[value]\n\t"
        "lsl  %[output],%[shift]"
        :   [output]    "=r"  (result)
        :   [value]     "r"   (value),
            [saturate]  "n"   (Saturate),
            [shift]     "n"   (FractionalShift));

    return result;
}

#endif // SaturatedArithmeticARM_h
