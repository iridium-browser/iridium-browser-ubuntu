/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkPM_DEFINED
#define SkPM_DEFINED

#include "SkTypes.h"
#include "SkColor.h"
#include "SkColorPriv.h"
#include "SkNx.h"

// A pre-multiplied color storing each component in the same order as SkPMColor,
// but as a float in the range [0, 255].
class SK_STRUCT_ALIGN(16) SkPMFloat {
public:
    static SkPMFloat FromPMColor(SkPMColor c) { return SkPMFloat(c); }
    static SkPMFloat FromARGB(float a, float r, float g, float b) { return SkPMFloat(a,r,g,b); }

    // May be more efficient than one at a time.  No special alignment assumed for SkPMColors.
    static void From4PMColors(const SkPMColor[4], SkPMFloat*, SkPMFloat*, SkPMFloat*, SkPMFloat*);

    // Uninitialized.
    SkPMFloat() {}
    explicit SkPMFloat(SkPMColor);
    SkPMFloat(float a, float r, float g, float b)
    #ifdef SK_PMCOLOR_IS_RGBA
        : fColors(r,g,b,a) {}
    #else
        : fColors(b,g,r,a) {}
    #endif


    // Freely autoconvert between SkPMFloat and Sk4f.
    /*implicit*/ SkPMFloat(const Sk4f& fs) { fColors = fs; }
    /*implicit*/ operator Sk4f() const { return fColors; }

    float a() const { return fColors[SK_A32_SHIFT / 8]; }
    float r() const { return fColors[SK_R32_SHIFT / 8]; }
    float g() const { return fColors[SK_G32_SHIFT / 8]; }
    float b() const { return fColors[SK_B32_SHIFT / 8]; }

    // N.B. All methods returning an SkPMColor call SkPMColorAssert on that result before returning.

    // get() and clamped() round component values to the nearest integer.
    SkPMColor     get() const;  // Assumes all values in [0, 255].  Some implementations may clamp.
    SkPMColor clamped() const;  // Will clamp all values to [0, 255].

    // Like get(), but truncates instead of rounding.
    // The domain of this function is (-1.0f, 256.0f).  Values in (-1.0f, 0.0f] trunc to a zero.
    SkPMColor trunc() const;

    // 4-at-a-time versions of get() and clamped().  Like From4PMColors(), no alignment assumed.
    static void To4PMColors(
            const SkPMFloat&, const SkPMFloat&, const SkPMFloat&, const SkPMFloat&, SkPMColor[4]);
    static void ClampTo4PMColors(
            const SkPMFloat&, const SkPMFloat&, const SkPMFloat&, const SkPMFloat&, SkPMColor[4]);

    bool isValid() const {
        return this->a() >= 0 && this->a() <= 255
            && this->r() >= 0 && this->r() <= this->a()
            && this->g() >= 0 && this->g() <= this->a()
            && this->b() >= 0 && this->b() <= this->a();
    }

private:
    Sk4f fColors;
};

#ifdef SKNX_NO_SIMD
    // Platform implementations of SkPMFloat assume Sk4f uses SSE or NEON.  _none is generic.
    #include "../opts/SkPMFloat_none.h"
#else
    #if SK_CPU_SSE_LEVEL >= SK_CPU_SSE_LEVEL_SSSE3
        #include "../opts/SkPMFloat_SSSE3.h"
    #elif SK_CPU_SSE_LEVEL >= SK_CPU_SSE_LEVEL_SSE2
        #include "../opts/SkPMFloat_SSE2.h"
    #elif defined(SK_ARM_HAS_NEON)
        #include "../opts/SkPMFloat_neon.h"
    #else
        #include "../opts/SkPMFloat_none.h"
    #endif
#endif

#endif//SkPM_DEFINED
