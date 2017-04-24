/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkJumper.h"
#include <string.h>

// It's tricky to relocate code referencing ordinary constants, so we read them from this struct.
using K = const SkJumper_constants;

template <typename T, typename P>
static T unaligned_load(const P* p) {
    T v;
    memcpy(&v, p, sizeof(v));
    return v;
}

template <typename Dst, typename Src>
static Dst bit_cast(const Src& src) {
    static_assert(sizeof(Dst) == sizeof(Src), "");
    return unaligned_load<Dst>(&src);
}

#if !defined(JUMPER)
    // This path should lead to portable code that can be compiled directly into Skia.
    // (All other paths are compiled offline by Clang into SkJumper_generated.h.)
    #include <math.h>

    using F   = float;
    using I32 =  int32_t;
    using U32 = uint32_t;
    using U16 = uint16_t;
    using U8  = uint8_t;

    static F   mad(F f, F m, F a)  { return f*m+a; }
    static F   min(F a, F b)       { return fminf(a,b); }
    static F   max(F a, F b)       { return fmaxf(a,b); }
    static F   abs_ (F v)          { return fabsf(v); }
    static F   floor(F v, K*)      { return floorf(v); }
    static F   rcp  (F v)          { return 1.0f / v; }
    static F   rsqrt(F v)          { return 1.0f / sqrtf(v); }
    static U32 round(F v, F scale) { return (uint32_t)lrintf(v*scale); }
    static U16 pack(U32 v)         { return (U16)v; }
    static U8  pack(U16 v)         { return  (U8)v; }

    static F if_then_else(I32 c, F t, F e) { return c ? t : e; }

    static F gather(const float* p, U32 ix) { return p[ix]; }

    #define WRAP(name) sk_##name

#elif defined(__aarch64__)
    #include <arm_neon.h>

    // Since we know we're using Clang, we can use its vector extensions.
    using F   = float    __attribute__((ext_vector_type(4)));
    using I32 =  int32_t __attribute__((ext_vector_type(4)));
    using U32 = uint32_t __attribute__((ext_vector_type(4)));
    using U16 = uint16_t __attribute__((ext_vector_type(4)));
    using U8  = uint8_t  __attribute__((ext_vector_type(4)));

    // We polyfill a few routines that Clang doesn't build into ext_vector_types.
    static F   mad(F f, F m, F a)                   { return vfmaq_f32(a,f,m);        }
    static F   min(F a, F b)                        { return vminq_f32(a,b);          }
    static F   max(F a, F b)                        { return vmaxq_f32(a,b);          }
    static F   abs_ (F v)                           { return vabsq_f32(v);            }
    static F   floor(F v, K*)                       { return vrndmq_f32(v);           }
    static F   rcp  (F v) { auto e = vrecpeq_f32 (v); return vrecpsq_f32 (v,e  ) * e; }
    static F   rsqrt(F v) { auto e = vrsqrteq_f32(v); return vrsqrtsq_f32(v,e*e) * e; }
    static U32 round(F v, F scale)                  { return vcvtnq_u32_f32(v*scale); }
    static U16 pack(U32 v)                          { return __builtin_convertvector(v, U16); }
    static U8  pack(U16 v)                          { return __builtin_convertvector(v,  U8); }

    static F if_then_else(I32 c, F t, F e) { return vbslq_f32((U32)c,t,e); }

    static F gather(const float* p, U32 ix) { return {p[ix[0]], p[ix[1]], p[ix[2]], p[ix[3]]}; }

    #define WRAP(name) sk_##name##_aarch64

#elif defined(__arm__)
    #if defined(__thumb2__) || !defined(__ARM_ARCH_7A__) || !defined(__ARM_VFPV4__)
        #error On ARMv7, compile with -march=armv7-a -mfpu=neon-vfp4, without -mthumb.
    #endif
    #include <arm_neon.h>

    // We can pass {s0-s15} as arguments under AAPCS-VFP.  We'll slice that as 8 d-registers.
    using F   = float    __attribute__((ext_vector_type(2)));
    using I32 =  int32_t __attribute__((ext_vector_type(2)));
    using U32 = uint32_t __attribute__((ext_vector_type(2)));
    using U16 = uint16_t __attribute__((ext_vector_type(2)));
    using U8  = uint8_t  __attribute__((ext_vector_type(2)));

    static F   mad(F f, F m, F a)                  { return vfma_f32(a,f,m);        }
    static F   min(F a, F b)                       { return vmin_f32(a,b);          }
    static F   max(F a, F b)                       { return vmax_f32(a,b);          }
    static F   abs_ (F v)                          { return vabs_f32(v);            }
    static F   rcp  (F v) { auto e = vrecpe_f32 (v); return vrecps_f32 (v,e  ) * e; }
    static F   rsqrt(F v) { auto e = vrsqrte_f32(v); return vrsqrts_f32(v,e*e) * e; }
    static U32 round(F v, F scale)                 { return vcvt_u32_f32(mad(v,scale,0.5f)); }
    static U16 pack(U32 v)                         { return __builtin_convertvector(v, U16); }
    static U8  pack(U16 v)                         { return __builtin_convertvector(v,  U8); }

    static F if_then_else(I32 c, F t, F e) { return vbsl_f32((U32)c,t,e); }

    static F floor(F v, K* k) {
        F roundtrip = vcvt_f32_s32(vcvt_s32_f32(v));
        return roundtrip - if_then_else(roundtrip > v, k->_1, 0);
    }

    static F gather(const float* p, U32 ix) { return {p[ix[0]], p[ix[1]]}; }

    #define WRAP(name) sk_##name##_vfp4

#elif defined(__AVX__)
    #include <immintrin.h>

    // These are __m256 and __m256i, but friendlier and strongly-typed.
    using F   = float    __attribute__((ext_vector_type(8)));
    using I32 =  int32_t __attribute__((ext_vector_type(8)));
    using U32 = uint32_t __attribute__((ext_vector_type(8)));
    using U16 = uint16_t __attribute__((ext_vector_type(8)));
    using U8  = uint8_t  __attribute__((ext_vector_type(8)));

    static F mad(F f, F m, F a)  {
    #if defined(__FMA__)
        return _mm256_fmadd_ps(f,m,a);
    #else
        return f*m+a;
    #endif
    }

    static F   min(F a, F b)       { return _mm256_min_ps(a,b);    }
    static F   max(F a, F b)       { return _mm256_max_ps(a,b);    }
    static F   abs_(F v)           { return _mm256_and_ps(v, 0-v); }
    static F   floor(F v, K*)      { return _mm256_floor_ps(v);    }
    static F   rcp  (F v)          { return _mm256_rcp_ps  (v);    }
    static F   rsqrt(F v)          { return _mm256_rsqrt_ps(v);    }
    static U32 round(F v, F scale) { return _mm256_cvtps_epi32(v*scale); }

    static U16 pack(U32 v) {
        return _mm_packus_epi32(_mm256_extractf128_si256(v, 0),
                                _mm256_extractf128_si256(v, 1));
    }
    static U8 pack(U16 v) {
        auto r = _mm_packus_epi16(v,v);
        return unaligned_load<U8>(&r);
    }

    static F if_then_else(I32 c, F t, F e) { return _mm256_blendv_ps(e,t,c); }

    static F gather(const float* p, U32 ix) {
    #if defined(__AVX2__)
        return _mm256_i32gather_ps(p, ix, 4);
    #else
        return { p[ix[0]], p[ix[1]], p[ix[2]], p[ix[3]],
                 p[ix[4]], p[ix[5]], p[ix[6]], p[ix[7]], };
    #endif
    }

    #if defined(__AVX2__) && defined(__F16C__) && defined(__FMA__)
        #define WRAP(name) sk_##name##_hsw
    #else
        #define WRAP(name) sk_##name##_avx
    #endif

#elif defined(__SSE2__)
    #include <immintrin.h>

    using F   = float    __attribute__((ext_vector_type(4)));
    using I32 =  int32_t __attribute__((ext_vector_type(4)));
    using U32 = uint32_t __attribute__((ext_vector_type(4)));
    using U16 = uint16_t __attribute__((ext_vector_type(4)));
    using U8  = uint8_t  __attribute__((ext_vector_type(4)));

    static F   mad(F f, F m, F a)  { return f*m+a;              }
    static F   min(F a, F b)       { return _mm_min_ps(a,b);    }
    static F   max(F a, F b)       { return _mm_max_ps(a,b);    }
    static F   abs_(F v)           { return _mm_and_ps(v, 0-v); }
    static F   rcp  (F v)          { return _mm_rcp_ps  (v);    }
    static F   rsqrt(F v)          { return _mm_rsqrt_ps(v);    }
    static U32 round(F v, F scale) { return _mm_cvtps_epi32(v*scale); }

    static U16 pack(U32 v) {
    #if defined(__SSE4_1__)
        auto p = _mm_packus_epi32(v,v);
    #else
        // Sign extend so that _mm_packs_epi32() does the pack we want.
        auto p = _mm_srai_epi32(_mm_slli_epi32(v, 16), 16);
        p = _mm_packs_epi32(p,p);
    #endif
        return unaligned_load<U16>(&p);  // We have two copies.  Return (the lower) one.
    }
    static U8 pack(U16 v) {
        __m128i r;
        memcpy(&r, &v, sizeof(v));
        r = _mm_packus_epi16(r,r);
        return unaligned_load<U8>(&r);
    }

    static F if_then_else(I32 c, F t, F e) {
        return _mm_or_ps(_mm_and_ps(c, t), _mm_andnot_ps(c, e));
    }

    static F floor(F v, K* k) {
    #if defined(__SSE4_1__)
        return _mm_floor_ps(v);
    #else
        F roundtrip = _mm_cvtepi32_ps(_mm_cvttps_epi32(v));
        return roundtrip - if_then_else(roundtrip > v, k->_1, 0);
    #endif
    }

    static F gather(const float* p, U32 ix) { return {p[ix[0]], p[ix[1]], p[ix[2]], p[ix[3]]}; }

    #if defined(__SSE4_1__)
        #define WRAP(name) sk_##name##_sse41
    #else
        #define WRAP(name) sk_##name##_sse2
    #endif
#endif

// We need to be a careful with casts.
// (F)x means cast x to float in the portable path, but bit_cast x to float in the others.
// These named casts and bit_cast() are always what they seem to be.
#if defined(JUMPER)
    static F   cast  (U32 v) { return __builtin_convertvector((I32)v, F);   }
    static U32 expand(U16 v) { return __builtin_convertvector(     v, U32); }
    static U32 expand(U8  v) { return __builtin_convertvector(     v, U32); }
#else
    static F   cast  (U32 v) { return   (F)v; }
    static U32 expand(U16 v) { return (U32)v; }
    static U32 expand(U8  v) { return (U32)v; }
#endif


static F lerp(F from, F to, F t) {
    return mad(to-from, t, from);
}

static void from_565(U16 _565, F* r, F* g, F* b, K* k) {
    U32 wide = expand(_565);
    *r = cast(wide & k->r_565_mask) * k->r_565_scale;
    *g = cast(wide & k->g_565_mask) * k->g_565_scale;
    *b = cast(wide & k->b_565_mask) * k->b_565_scale;
}

// Sometimes we want to work with 4 floats directly, regardless of the depth of the F vector.
#if defined(JUMPER)
    using F4 = float __attribute__((ext_vector_type(4)));
#else
    struct F4 {
        float vals[4];
        float operator[](int i) const { return vals[i]; }
    };
#endif

// Stages tail call between each other by following program,
// an interlaced sequence of Stage pointers and context pointers.
using Stage = void(size_t x, void** program, K* k, F,F,F,F, F,F,F,F);

static void* load_and_inc(void**& program) {
#if defined(__GNUC__) && defined(__x86_64__)
    // Passing program as the second Stage argument makes it likely that it's in %rsi,
    // so this is usually a single instruction *program++.
    void* rax;
    asm("lodsq" : "=a"(rax), "+S"(program));  // Write-only %rax, read-write %rsi.
    return rax;
    // When a Stage uses its ctx pointer, this optimization typically cuts an instruction:
    //    mov    (%rsi), %rcx     // ctx  = program[0]
    //    ...
    //    mov 0x8(%rsi), %rax     // next = program[1]
    //    add $0x10, %rsi         // program += 2
    //    jmpq *%rax              // JUMP!
    // becomes
    //    lods   %ds:(%rsi),%rax  // ctx  = *program++;
    //    ...
    //    lods   %ds:(%rsi),%rax  // next = *program++;
    //    jmpq *%rax              // JUMP!
    //
    // When a Stage doesn't use its ctx pointer, it's 3 instructions either way,
    // but using lodsq (a 2-byte instruction) tends to trim a few bytes.
#else
    // On ARM *program++ compiles into a single instruction without any handholding.
    return *program++;
#endif
}

#define STAGE(name)                                                           \
    static void name##_k(size_t& x, void* ctx, K* k,                          \
                         F& r, F& g, F& b, F& a, F& dr, F& dg, F& db, F& da); \
    extern "C" void WRAP(name)(size_t x, void** program, K* k,                \
                              F r, F g, F b, F a, F dr, F dg, F db, F da) {   \
        auto ctx = load_and_inc(program);                                     \
        name##_k(x,ctx,k, r,g,b,a, dr,dg,db,da);                              \
        auto next = (Stage*)load_and_inc(program);                            \
        next(x,program,k, r,g,b,a, dr,dg,db,da);                              \
    }                                                                         \
    static void name##_k(size_t& x, void* ctx, K* k,                          \
                         F& r, F& g, F& b, F& a, F& dr, F& dg, F& db, F& da)

// Some glue stages that don't fit the normal pattern of stages.

#if defined(JUMPER) && defined(WIN)
__attribute__((ms_abi))
#endif
extern "C" size_t WRAP(start_pipeline)(size_t x, void** program, K* k, size_t limit) {
    F v{};
    size_t stride = sizeof(F) / sizeof(float);
    auto start = (Stage*)load_and_inc(program);
    while (x + stride <= limit) {
        start(x,program,k, v,v,v,v, v,v,v,v);
        x += stride;
    }
    return x;
}

// Ends the chain of tail calls, returning back up to start_pipeline (and from there to the caller).
extern "C" void WRAP(just_return)(size_t, void**, K*, F,F,F,F, F,F,F,F) {}

// We can now define Stages!

// Some things to keep in mind while writing Stages:
//   - do not branch;                                           (i.e. avoid jmp)
//   - do not call functions that don't inline;                 (i.e. avoid call, ret)
//   - do not use constant literals other than 0, ~0 and 0.0f.  (i.e. avoid rip relative addressing)
//
// Some things that should work fine:
//   - 0, ~0, and 0.0f;
//   - arithmetic;
//   - functions of F and U32 that we've defined above;
//   - temporary values;
//   - lambdas;
//   - memcpy() with a compile-time constant size argument.

STAGE(seed_shader) {
    auto y = *(const int*)ctx;

    // It's important for speed to explicitly cast(x) and cast(y),
    // which has the effect of splatting them to vectors before converting to floats.
    // On Intel this breaks a data dependency on previous loop iterations' registers.

    r = cast(x) + k->_0_5 + unaligned_load<F>(k->iota);
    g = cast(y) + k->_0_5;
    b = k->_1;
    a = 0;
    dr = dg = db = da = 0;
}

STAGE(constant_color) {
    auto rgba = unaligned_load<F4>(ctx);
    r = rgba[0];
    g = rgba[1];
    b = rgba[2];
    a = rgba[3];
}

STAGE(clear) {
    r = g = b = a = 0;
}

STAGE(plus_) {
    r = r + dr;
    g = g + dg;
    b = b + db;
    a = a + da;
}

STAGE(srcover) {
    auto A = k->_1 - a;
    r = mad(dr, A, r);
    g = mad(dg, A, g);
    b = mad(db, A, b);
    a = mad(da, A, a);
}
STAGE(dstover) {
    auto DA = k->_1 - da;
    r = mad(r, DA, dr);
    g = mad(g, DA, dg);
    b = mad(b, DA, db);
    a = mad(a, DA, da);
}

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

STAGE(set_rgb) {
    auto rgb = (const float*)ctx;
    r = rgb[0];
    g = rgb[1];
    b = rgb[2];
}
STAGE(swap_rb) {
    auto tmp = r;
    r = b;
    b = tmp;
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
        auto hi = mad(s*s, mad(s, k->_03000, k->_06975), k->_00025);
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
        auto hi = min(k->_1, mad(k->_0411192, ftrt,
                             mad(k->_0689206, sqrt,
                                 k->n_00988)));
        return if_then_else(l < k->_00043, lo, hi);
    };
    r = fn(r);
    g = fn(g);
    b = fn(b);
}

STAGE(scale_1_float) {
    auto c = *(const float*)ctx;

    r = r * c;
    g = g * c;
    b = b * c;
    a = a * c;
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

STAGE(lerp_1_float) {
    auto c = *(const float*)ctx;

    r = lerp(dr, r, c);
    g = lerp(dg, g, c);
    b = lerp(db, b, c);
    a = lerp(da, a, c);
}
STAGE(lerp_u8) {
    auto ptr = *(const uint8_t**)ctx + x;

    auto scales = unaligned_load<U8>(ptr);
    auto c = cast(expand(scales)) * k->_1_255;

    r = lerp(dr, r, c);
    g = lerp(dg, g, c);
    b = lerp(db, b, c);
    a = lerp(da, a, c);
}
STAGE(lerp_565) {
    auto ptr = *(const uint16_t**)ctx + x;

    F cr,cg,cb;
    from_565(unaligned_load<U16>(ptr), &cr, &cg, &cb, k);

    r = lerp(dr, r, cr);
    g = lerp(dg, g, cg);
    b = lerp(db, b, cb);
    a = k->_1;
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

STAGE(load_a8) {
    auto ptr = *(const uint8_t**)ctx + x;

    r = g = b = 0.0f;
    a = cast(expand(unaligned_load<U8>(ptr))) * k->_1_255;
}
STAGE(store_a8) {
    auto ptr = *(uint8_t**)ctx + x;

    U8 packed = pack(pack(round(a, k->_255)));
    memcpy(ptr, &packed, sizeof(packed));
}

STAGE(load_565) {
    auto ptr = *(const uint16_t**)ctx + x;

    from_565(unaligned_load<U16>(ptr), &r,&g,&b, k);
    a = k->_1;
}
STAGE(store_565) {
    auto ptr = *(uint16_t**)ctx + x;

    U16 px = pack( round(r, k->_31) << 11
                 | round(g, k->_63) <<  5
                 | round(b, k->_31)      );
    memcpy(ptr, &px, sizeof(px));
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

#if !defined(JUMPER)
    auto half_to_float = [&](int16_t h) {
        if (h < 0x0400) { h = 0; }                // Flush denorm and negative to zero.
        return bit_cast<F>(h << 13)               // Line up the mantissa,
             * bit_cast<F>(U32(k->_0x77800000));  // then fix up the exponent.
    };
    auto rgba = (const int16_t*)ptr;
    r = half_to_float(rgba[0]);
    g = half_to_float(rgba[1]);
    b = half_to_float(rgba[2]);
    a = half_to_float(rgba[3]);
#elif defined(__aarch64__)
    auto halfs = vld4_f16((const float16_t*)ptr);
    r = vcvt_f32_f16(halfs.val[0]);
    g = vcvt_f32_f16(halfs.val[1]);
    b = vcvt_f32_f16(halfs.val[2]);
    a = vcvt_f32_f16(halfs.val[3]);
#elif defined(__arm__)
    auto rb_ga = vld2_f16((const float16_t*)ptr);
    auto rb = vcvt_f32_f16(rb_ga.val[0]),
         ga = vcvt_f32_f16(rb_ga.val[1]);
    r = {rb[0], rb[2]};
    g = {ga[0], ga[2]};
    b = {rb[1], rb[3]};
    a = {ga[1], ga[3]};
#elif defined(__AVX2__) && defined(__FMA__) && defined(__F16C__)
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
#elif defined(__AVX__)
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

    // half_to_float() slows down ~10x for denorm inputs, so we flush them to zero.
    // With a signed comparison this conveniently also flushes negative half floats to zero.
    auto ftz = [k](__m128i v) {
        return _mm_andnot_si128(_mm_cmplt_epi16(v, _mm_set1_epi32(k->_0x04000400)), v);
    };
    rg0123 = ftz(rg0123);
    ba0123 = ftz(ba0123);
    rg4567 = ftz(rg4567);
    ba4567 = ftz(ba4567);

    U32 R = _mm256_setr_m128i(_mm_unpacklo_epi16(rg0123, _mm_setzero_si128()),
                              _mm_unpacklo_epi16(rg4567, _mm_setzero_si128())),
        G = _mm256_setr_m128i(_mm_unpackhi_epi16(rg0123, _mm_setzero_si128()),
                              _mm_unpackhi_epi16(rg4567, _mm_setzero_si128())),
        B = _mm256_setr_m128i(_mm_unpacklo_epi16(ba0123, _mm_setzero_si128()),
                              _mm_unpacklo_epi16(ba4567, _mm_setzero_si128())),
        A = _mm256_setr_m128i(_mm_unpackhi_epi16(ba0123, _mm_setzero_si128()),
                              _mm_unpackhi_epi16(ba4567, _mm_setzero_si128()));

    auto half_to_float = [&](U32 h) {
        return bit_cast<F>(h << 13)               // Line up the mantissa,
             * bit_cast<F>(U32(k->_0x77800000));  // then fix up the exponent.
    };

    r = half_to_float(R);
    g = half_to_float(G);
    b = half_to_float(B);
    a = half_to_float(A);

#elif defined(__SSE2__)
    auto _01 = _mm_loadu_si128(((__m128i*)ptr) + 0),
         _23 = _mm_loadu_si128(((__m128i*)ptr) + 1);

    auto _02 = _mm_unpacklo_epi16(_01, _23),  // r0 r2 g0 g2 b0 b2 a0 a2
         _13 = _mm_unpackhi_epi16(_01, _23);  // r1 r3 g1 g3 b1 b3 a1 a3

    auto rg = _mm_unpacklo_epi16(_02, _13),  // r0 r1 r2 r3 g0 g1 g2 g3
         ba = _mm_unpackhi_epi16(_02, _13);  // b0 b1 b2 b3 a0 a1 a2 a3

    // Same deal as AVX, flush denorms and negatives to zero.
    auto ftz = [k](__m128i v) {
        return _mm_andnot_si128(_mm_cmplt_epi16(v, _mm_set1_epi32(k->_0x04000400)), v);
    };
    rg = ftz(rg);
    ba = ftz(ba);

    auto half_to_float = [&](U32 h) {
        return bit_cast<F>(h << 13)               // Line up the mantissa,
             * bit_cast<F>(U32(k->_0x77800000));  // then fix up the exponent.
    };

    r = half_to_float(_mm_unpacklo_epi16(rg, _mm_setzero_si128()));
    g = half_to_float(_mm_unpackhi_epi16(rg, _mm_setzero_si128()));
    b = half_to_float(_mm_unpacklo_epi16(ba, _mm_setzero_si128()));
    a = half_to_float(_mm_unpackhi_epi16(ba, _mm_setzero_si128()));
#endif
}

STAGE(store_f16) {
    auto ptr = *(uint64_t**)ctx + x;

#if !defined(JUMPER)
    auto float_to_half = [&](F f) {
        return bit_cast<U32>(f * bit_cast<F>(U32(k->_0x07800000)))  // Fix up the exponent,
            >> 13;                                                  // then line up the mantissa.
    };
    auto rgba = (int16_t*)ptr;
    rgba[0] = float_to_half(r);
    rgba[1] = float_to_half(g);
    rgba[2] = float_to_half(b);
    rgba[3] = float_to_half(a);
#elif defined(__aarch64__)
    float16x4x4_t halfs = {{
        vcvt_f16_f32(r),
        vcvt_f16_f32(g),
        vcvt_f16_f32(b),
        vcvt_f16_f32(a),
    }};
    vst4_f16((float16_t*)ptr, halfs);
#elif defined(__arm__)
    float16x4x2_t rb_ga = {{
        vcvt_f16_f32(float32x4_t{r[0], b[0], r[1], b[1]}),
        vcvt_f16_f32(float32x4_t{g[0], a[0], g[1], a[1]}),
    }};
    vst2_f16((float16_t*)ptr, rb_ga);
#elif defined(__AVX2__) && defined(__FMA__) && defined(__F16C__)
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
#elif defined(__AVX__)
    auto float_to_half = [&](F f) {
        return bit_cast<U32>(f * bit_cast<F>(U32(k->_0x07800000)))  // Fix up the exponent,
            >> 13;                                                  // then line up the mantissa.
    };
    U32 R = float_to_half(r),
        G = float_to_half(g),
        B = float_to_half(b),
        A = float_to_half(a);
    auto r0123 = _mm256_extractf128_si256(R, 0),
         r4567 = _mm256_extractf128_si256(R, 1),
         g0123 = _mm256_extractf128_si256(G, 0),
         g4567 = _mm256_extractf128_si256(G, 1),
         b0123 = _mm256_extractf128_si256(B, 0),
         b4567 = _mm256_extractf128_si256(B, 1),
         a0123 = _mm256_extractf128_si256(A, 0),
         a4567 = _mm256_extractf128_si256(A, 1);
    auto rg0123 = r0123 | _mm_slli_si128(g0123,2),
         rg4567 = r4567 | _mm_slli_si128(g4567,2),
         ba0123 = b0123 | _mm_slli_si128(a0123,2),
         ba4567 = b4567 | _mm_slli_si128(a4567,2);
    _mm_storeu_si128((__m128i*)ptr + 0, _mm_unpacklo_epi32(rg0123, ba0123));
    _mm_storeu_si128((__m128i*)ptr + 1, _mm_unpackhi_epi32(rg0123, ba0123));
    _mm_storeu_si128((__m128i*)ptr + 2, _mm_unpacklo_epi32(rg4567, ba4567));
    _mm_storeu_si128((__m128i*)ptr + 3, _mm_unpackhi_epi32(rg4567, ba4567));
#elif defined(__SSE2__)
    auto float_to_half = [&](F f) {
        return bit_cast<U32>(f * bit_cast<F>(U32(k->_0x07800000)))  // Fix up the exponent,
            >> 13;                                                  // then line up the mantissa.
    };
    U32 R = float_to_half(r),
        G = float_to_half(g),
        B = float_to_half(b),
        A = float_to_half(a);
    U32 rg = R | _mm_slli_si128(G,2),
        ba = B | _mm_slli_si128(A,2);
    _mm_storeu_si128((__m128i*)ptr + 0, _mm_unpacklo_epi32(rg, ba));
    _mm_storeu_si128((__m128i*)ptr + 1, _mm_unpackhi_epi32(rg, ba));
#endif
}

static F ulp_before(F v) {
    return bit_cast<F>(bit_cast<U32>(v) + U32(0xffffffff));
}
static F clamp(F v, float limit, K*) {
    v = max(0, v);
    return min(v, ulp_before(limit));
}
static F repeat(F v, float limit, K* k) {
    v = v - floor(v/limit, k)*limit;
    return min(v, ulp_before(limit));
}
static F mirror(F v, float limit, K* k) {
    v = abs_( (v-limit) - (limit+limit)*floor((v-limit)/(limit+limit),k) - limit );
    return min(v, ulp_before(limit));
}
STAGE(clamp_x)  { r = clamp (r, *(const float*)ctx, k); }
STAGE(clamp_y)  { g = clamp (g, *(const float*)ctx, k); }
STAGE(repeat_x) { r = repeat(r, *(const float*)ctx, k); }
STAGE(repeat_y) { g = repeat(g, *(const float*)ctx, k); }
STAGE(mirror_x) { r = mirror(r, *(const float*)ctx, k); }
STAGE(mirror_y) { g = mirror(g, *(const float*)ctx, k); }

STAGE(matrix_2x3) {
    auto m = (const float*)ctx;

    auto R = mad(r,m[0], mad(g,m[2], m[4])),
         G = mad(r,m[1], mad(g,m[3], m[5]));
    r = R;
    g = G;
}
STAGE(matrix_3x4) {
    auto m = (const float*)ctx;

    auto R = mad(r,m[0], mad(g,m[3], mad(b,m[6], m[ 9]))),
         G = mad(r,m[1], mad(g,m[4], mad(b,m[7], m[10]))),
         B = mad(r,m[2], mad(g,m[5], mad(b,m[8], m[11])));
    r = R;
    g = G;
    b = B;
}
STAGE(matrix_perspective) {
    // N.B. Unlike the other matrix_ stages, this matrix is row-major.
    auto m = (const float*)ctx;

    auto R = mad(r,m[0], mad(g,m[1], m[2])),
         G = mad(r,m[3], mad(g,m[4], m[5])),
         Z = mad(r,m[6], mad(g,m[7], m[8]));
    r = R * rcp(Z);
    g = G * rcp(Z);
}

STAGE(linear_gradient_2stops) {
    struct Ctx { F4 c0, dc; };
    auto c = unaligned_load<Ctx>(ctx);

    auto t = r;
    r = mad(t, c.dc[0], c.c0[0]);
    g = mad(t, c.dc[1], c.c0[1]);
    b = mad(t, c.dc[2], c.c0[2]);
    a = mad(t, c.dc[3], c.c0[3]);
}
