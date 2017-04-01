/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkSplicer_shared.h"
#include <string.h>

#if !defined(__clang__)
    #error This file is not like the rest of Skia.  It must be compiled with clang.
#endif

#if defined(__aarch64__)
    #include <arm_neon.h>

    // Since we know we're using Clang, we can use its vector extensions.
    using F   = float    __attribute__((ext_vector_type(4)));
    using I32 =  int32_t __attribute__((ext_vector_type(4)));
    using U32 = uint32_t __attribute__((ext_vector_type(4)));
    using U8  = uint8_t  __attribute__((ext_vector_type(4)));

    // We polyfill a few routines that Clang doesn't build into ext_vector_types.
    static F   min(F a, F b)                        { return vminq_f32(a,b);          }
    static F   max(F a, F b)                        { return vmaxq_f32(a,b);          }
    static F   fma(F f, F m, F a)                   { return vfmaq_f32(a,f,m);        }
    static F   rcp  (F v) { auto e = vrecpeq_f32 (v); return vrecpsq_f32 (v,e  ) * e; }
    static F   rsqrt(F v) { auto e = vrsqrteq_f32(v); return vrsqrtsq_f32(v,e*e) * e; }
    static F   if_then_else(I32 c, F t, F e)        { return vbslq_f32((U32)c,t,e);   }
    static U32 round(F v, F scale)                  { return vcvtnq_u32_f32(v*scale); }

    static F gather(const float* p, U32 ix) { return {p[ix[0]], p[ix[1]], p[ix[2]], p[ix[3]]}; }
#elif defined(__ARM_NEON__)
    #if defined(__thumb2__) || !defined(__ARM_ARCH_7A__) || !defined(__ARM_VFPV4__)
        #error On ARMv7, compile with -march=armv7-a -mfpu=neon-vfp4, without -mthumb.
    #endif
    #include <arm_neon.h>

    // We can pass {s0-s15} as arguments under AAPCS-VFP.  We'll slice that as 8 d-registers.
    using F   = float    __attribute__((ext_vector_type(2)));
    using I32 =  int32_t __attribute__((ext_vector_type(2)));
    using U32 = uint32_t __attribute__((ext_vector_type(2)));
    using U8  = uint8_t  __attribute__((ext_vector_type(2)));

    static F   min(F a, F b)                        { return vmin_f32(a,b);          }
    static F   max(F a, F b)                        { return vmax_f32(a,b);          }
    static F   fma(F f, F m, F a)                   { return vfma_f32(a,f,m);        }
    static F   rcp  (F v)  { auto e = vrecpe_f32 (v); return vrecps_f32 (v,e  ) * e; }
    static F   rsqrt(F v)  { auto e = vrsqrte_f32(v); return vrsqrts_f32(v,e*e) * e; }
    static F   if_then_else(I32 c, F t, F e)        { return vbsl_f32((U32)c,t,e);   }
    static U32 round(F v, F scale)                  { return vcvt_u32_f32(fma(v,scale,0.5f)); }

    static F gather(const float* p, U32 ix) { return {p[ix[0]], p[ix[1]]}; }
#else
    #if !defined(__AVX2__) || !defined(__FMA__) || !defined(__F16C__)
        #error On x86, compile with -mavx2 -mfma -mf16c.
    #endif
    #include <immintrin.h>

    // These are __m256 and __m256i, but friendlier and strongly-typed.
    using F   = float    __attribute__((ext_vector_type(8)));
    using I32 =  int32_t __attribute__((ext_vector_type(8)));
    using U32 = uint32_t __attribute__((ext_vector_type(8)));
    using U8  = uint8_t  __attribute__((ext_vector_type(8)));

    static F   min(F a, F b)                 { return _mm256_min_ps  (a,b);  }
    static F   max(F a, F b)                 { return _mm256_max_ps  (a,b);  }
    static F   fma(F f, F m, F a)            { return _mm256_fmadd_ps(f,m,a);}
    static F   rcp  (F v)                    { return _mm256_rcp_ps     (v); }
    static F   rsqrt(F v)                    { return _mm256_rsqrt_ps   (v); }
    static F   if_then_else(I32 c, F t, F e) { return _mm256_blendv_ps(e,t,c); }
    static U32 round(F v, F scale)           { return _mm256_cvtps_epi32(v*scale); }

    static F gather(const float* p, U32 ix) { return _mm256_i32gather_ps(p, ix, 4); }
#endif

static F   cast  (U32 v) { return __builtin_convertvector((I32)v, F);   }
static U32 expand(U8  v) { return __builtin_convertvector(     v, U32); }

template <typename T, typename P>
static T unaligned_load(const P* p) {
    T v;
    memcpy(&v, p, sizeof(v));
    return v;
}

// We'll be compiling this file to an object file, then extracting parts of it into
// SkSplicer_generated.h.  It's easier to do if the function names are not C++ mangled.
// On ARMv7, use aapcs-vfp calling convention to pass as much data in registers as possible.
#if defined(__ARM_NEON__)
    #define C extern "C" __attribute__((pcs("aapcs-vfp")))
#else
    #define C extern "C"
#endif

// Stages all fit a common interface that allows SkSplicer to splice them together.
using K = const SkSplicer_constants;
using Stage = void(size_t x, size_t limit, void* ctx, K* k, F,F,F,F, F,F,F,F);

// Stage's arguments act as the working set of registers within the final spliced function.
// Here's a little primer on the x86-64/aarch64 ABIs:
//   x:         rdi/x0          x and limit work to drive the loop, see loop_start in SkSplicer.cpp.
//   limit:     rsi/x1
//   ctx:       rdx/x2          Look for set_ctx in SkSplicer.cpp to see how this works.
//   k:         rcx/x3
//   vectors:   ymm0-ymm7/v0-v7


// done() is the key to this entire splicing strategy.
//
// It matches the signature of Stage, so all the registers are kept live.
// Every Stage calls done() and so will end in a single jmp (i.e. tail-call) into done(),
// which marks the point where we can splice one Stage onto the next.
//
// The lovely bit is that we don't have to define done(), just declare it.
C void done(size_t, size_t, void*, K*, F,F,F,F, F,F,F,F);

// This should feel familiar to anyone who's read SkRasterPipeline_opts.h.
// It's just a convenience to make a valid, spliceable Stage, nothing magic.
#define STAGE(name)                                                           \
    static void name##_k(size_t x, size_t limit, void* ctx, K* k,             \
                         F& r, F& g, F& b, F& a, F& dr, F& dg, F& db, F& da); \
    C void name(size_t x, size_t limit, void* ctx, K* k,                      \
                F r, F g, F b, F a, F dr, F dg, F db, F da) {                 \
        name##_k(x,limit,ctx,k, r,g,b,a, dr,dg,db,da);                        \
        done    (x,limit,ctx,k, r,g,b,a, dr,dg,db,da);                        \
    }                                                                         \
    static void name##_k(size_t x, size_t limit, void* ctx, K* k,             \
                         F& r, F& g, F& b, F& a, F& dr, F& dg, F& db, F& da)

// We can now define Stages!

// Some things to keep in mind while writing Stages:
//   - do not branch;                                       (i.e. avoid jmp)
//   - do not call functions that don't inline;             (i.e. avoid call, ret, stack use)
//   - do not use constant literals other than 0 and 0.0f.  (i.e. avoid rip relative addressing)
//
// Some things that should work fine:
//   - 0 and 0.0f;
//   - arithmetic;
//   - functions of F and U32 that we've defined above;
//   - temporary values;
//   - lambdas;
//   - memcpy() with a compile-time constant size argument.

STAGE(clear) {
    r = g = b = a = 0;
}

STAGE(plus) {
    r = r + dr;
    g = g + dg;
    b = b + db;
    a = a + da;
}

STAGE(srcover) {
    auto A = k->_1 - a;
    r = fma(dr, A, r);
    g = fma(dg, A, g);
    b = fma(db, A, b);
    a = fma(da, A, a);
}
STAGE(dstover) { srcover_k(x,limit,ctx,k, dr,dg,db,da, r,g,b,a); }

STAGE(clamp_0) {
    r = max(r, 0);
    g = max(g, 0);
    b = max(b, 0);
    a = max(a, 0);
}

STAGE(clamp_1) {
    r = min(r, k->_1);
    g = min(g, k->_1);
    b = min(b, k->_1);
    a = min(a, k->_1);
}

STAGE(clamp_a) {
    a = min(a, k->_1);
    r = min(r, a);
    g = min(g, a);
    b = min(b, a);
}

STAGE(swap) {
    auto swap = [](F& v, F& dv) {
        auto tmp = v;
        v = dv;
        dv = tmp;
    };
    swap(r, dr);
    swap(g, dg);
    swap(b, db);
    swap(a, da);
}
STAGE(move_src_dst) {
    dr = r;
    dg = g;
    db = b;
    da = a;
}
STAGE(move_dst_src) {
    r = dr;
    g = dg;
    b = db;
    a = da;
}

STAGE(premul) {
    r = r * a;
    g = g * a;
    b = b * a;
}
STAGE(unpremul) {
    auto scale = if_then_else(a == 0, 0, k->_1 / a);
    r = r * scale;
    g = g * scale;
    b = b * scale;
}

STAGE(from_srgb) {
    auto fn = [&](F s) {
        auto lo = s * k->_1_1292;
        auto hi = fma(s*s, fma(s, k->_03000, k->_06975), k->_00025);
        return if_then_else(s < k->_0055, lo, hi);
    };
    r = fn(r);
    g = fn(g);
    b = fn(b);
}
STAGE(to_srgb) {
    auto fn = [&](F l) {
        F sqrt = rcp  (rsqrt(l)),
          ftrt = rsqrt(rsqrt(l));
        auto lo = l * k->_1246;
        auto hi = min(k->_1, fma(k->_0411192, ftrt,
                             fma(k->_0689206, sqrt,
                                 k->n_00988)));
        return if_then_else(l < k->_00043, lo, hi);
    };
    r = fn(r);
    g = fn(g);
    b = fn(b);
}

STAGE(scale_u8) {
    auto ptr = *(const uint8_t**)ctx + x;

    auto scales = unaligned_load<U8>(ptr);
    auto c = cast(expand(scales)) * k->_1_255;

    r = r * c;
    g = g * c;
    b = b * c;
    a = a * c;
}

STAGE(load_tables) {
    struct Ctx {
        const uint32_t* src;
        const float *r, *g, *b;
    };
    auto c = (const Ctx*)ctx;

    auto px = unaligned_load<U32>(c->src + x);
    r = gather(c->r, (px      ) & k->_0x000000ff);
    g = gather(c->g, (px >>  8) & k->_0x000000ff);
    b = gather(c->b, (px >> 16) & k->_0x000000ff);
    a = cast(        (px >> 24)) * k->_1_255;
}

STAGE(load_8888) {
    auto ptr = *(const uint32_t**)ctx + x;

    auto px = unaligned_load<U32>(ptr);
    r = cast((px      ) & k->_0x000000ff) * k->_1_255;
    g = cast((px >>  8) & k->_0x000000ff) * k->_1_255;
    b = cast((px >> 16) & k->_0x000000ff) * k->_1_255;
    a = cast((px >> 24)                 ) * k->_1_255;
}

STAGE(store_8888) {
    auto ptr = *(uint32_t**)ctx + x;

    U32 px = round(r, k->_255)
           | round(g, k->_255) <<  8
           | round(b, k->_255) << 16
           | round(a, k->_255) << 24;
    memcpy(ptr, &px, sizeof(px));
}

STAGE(load_f16) {
    auto ptr = *(const uint64_t**)ctx + x;

#if defined(__aarch64__)
    auto halfs = vld4_f16((const float16_t*)ptr);
    r = vcvt_f32_f16(halfs.val[0]);
    g = vcvt_f32_f16(halfs.val[1]);
    b = vcvt_f32_f16(halfs.val[2]);
    a = vcvt_f32_f16(halfs.val[3]);
#elif defined(__ARM_NEON__)
    auto rb_ga = vld2_f16((const float16_t*)ptr);
    auto rb = vcvt_f32_f16(rb_ga.val[0]),
         ga = vcvt_f32_f16(rb_ga.val[1]);
    r = {rb[0], rb[2]};
    g = {ga[0], ga[2]};
    b = {rb[1], rb[3]};
    a = {ga[1], ga[3]};
#else
    auto _01 = _mm_loadu_si128(((__m128i*)ptr) + 0),
         _23 = _mm_loadu_si128(((__m128i*)ptr) + 1),
         _45 = _mm_loadu_si128(((__m128i*)ptr) + 2),
         _67 = _mm_loadu_si128(((__m128i*)ptr) + 3);

    auto _02 = _mm_unpacklo_epi16(_01, _23),  // r0 r2 g0 g2 b0 b2 a0 a2
         _13 = _mm_unpackhi_epi16(_01, _23),  // r1 r3 g1 g3 b1 b3 a1 a3
         _46 = _mm_unpacklo_epi16(_45, _67),
         _57 = _mm_unpackhi_epi16(_45, _67);

    auto rg0123 = _mm_unpacklo_epi16(_02, _13),  // r0 r1 r2 r3 g0 g1 g2 g3
         ba0123 = _mm_unpackhi_epi16(_02, _13),  // b0 b1 b2 b3 a0 a1 a2 a3
         rg4567 = _mm_unpacklo_epi16(_46, _57),
         ba4567 = _mm_unpackhi_epi16(_46, _57);

    r = _mm256_cvtph_ps(_mm_unpacklo_epi64(rg0123, rg4567));
    g = _mm256_cvtph_ps(_mm_unpackhi_epi64(rg0123, rg4567));
    b = _mm256_cvtph_ps(_mm_unpacklo_epi64(ba0123, ba4567));
    a = _mm256_cvtph_ps(_mm_unpackhi_epi64(ba0123, ba4567));
#endif
}

STAGE(store_f16) {
    auto ptr = *(uint64_t**)ctx + x;

#if defined(__aarch64__)
    float16x4x4_t halfs = {{
        vcvt_f16_f32(r),
        vcvt_f16_f32(g),
        vcvt_f16_f32(b),
        vcvt_f16_f32(a),
    }};
    vst4_f16((float16_t*)ptr, halfs);
#elif defined(__ARM_NEON__)
    float16x4x2_t rb_ga = {{
        vcvt_f16_f32(float32x4_t{r[0], b[0], r[1], b[1]}),
        vcvt_f16_f32(float32x4_t{g[0], a[0], g[1], a[1]}),
    }};
    vst2_f16((float16_t*)ptr, rb_ga);
#else
    auto R = _mm256_cvtps_ph(r, _MM_FROUND_CUR_DIRECTION),
         G = _mm256_cvtps_ph(g, _MM_FROUND_CUR_DIRECTION),
         B = _mm256_cvtps_ph(b, _MM_FROUND_CUR_DIRECTION),
         A = _mm256_cvtps_ph(a, _MM_FROUND_CUR_DIRECTION);

    auto rg0123 = _mm_unpacklo_epi16(R, G),  // r0 g0 r1 g1 r2 g2 r3 g3
         rg4567 = _mm_unpackhi_epi16(R, G),  // r4 g4 r5 g5 r6 g6 r7 g7
         ba0123 = _mm_unpacklo_epi16(B, A),
         ba4567 = _mm_unpackhi_epi16(B, A);

    _mm_storeu_si128((__m128i*)ptr + 0, _mm_unpacklo_epi32(rg0123, ba0123));
    _mm_storeu_si128((__m128i*)ptr + 1, _mm_unpackhi_epi32(rg0123, ba0123));
    _mm_storeu_si128((__m128i*)ptr + 2, _mm_unpacklo_epi32(rg4567, ba4567));
    _mm_storeu_si128((__m128i*)ptr + 3, _mm_unpackhi_epi32(rg4567, ba4567));
#endif
}

STAGE(matrix_3x4) {
    auto m = (const float*)ctx;

    auto R = fma(r,m[0], fma(g,m[3], fma(b,m[6], m[ 9]))),
         G = fma(r,m[1], fma(g,m[4], fma(b,m[7], m[10]))),
         B = fma(r,m[2], fma(g,m[5], fma(b,m[8], m[11])));
    r = R;
    g = G;
    b = B;
}
