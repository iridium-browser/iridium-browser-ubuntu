/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "../skcms.h"
#include "LinearAlgebra.h"
#include "PortableMath.h"
#include "TransferFunction.h"
#include "Transform.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#if defined(__clang__)
    typedef float    __attribute__((ext_vector_type(4)))   Fx4;
    typedef int32_t  __attribute__((ext_vector_type(4))) I32x4;
    typedef uint64_t __attribute__((ext_vector_type(4))) U64x4;
    typedef uint32_t __attribute__((ext_vector_type(4))) U32x4;
    typedef uint16_t __attribute__((ext_vector_type(4))) U16x4;
    typedef uint8_t  __attribute__((ext_vector_type(4)))  U8x4;

    typedef float    __attribute__((ext_vector_type(8)))   Fx8;
    typedef int32_t  __attribute__((ext_vector_type(8))) I32x8;
    typedef uint64_t __attribute__((ext_vector_type(8))) U64x8;
    typedef uint32_t __attribute__((ext_vector_type(8))) U32x8;
    typedef uint16_t __attribute__((ext_vector_type(8))) U16x8;
    typedef uint8_t  __attribute__((ext_vector_type(8)))  U8x8;

    typedef float    __attribute__((ext_vector_type(16)))   Fx16;
    typedef int32_t  __attribute__((ext_vector_type(16))) I32x16;
    typedef uint64_t __attribute__((ext_vector_type(16))) U64x16;
    typedef uint32_t __attribute__((ext_vector_type(16))) U32x16;
    typedef uint16_t __attribute__((ext_vector_type(16))) U16x16;
    typedef uint8_t  __attribute__((ext_vector_type(16)))  U8x16;
#elif defined(__GNUC__)
    typedef float    __attribute__((vector_size(16)))   Fx4;
    typedef int32_t  __attribute__((vector_size(16))) I32x4;
    typedef uint64_t __attribute__((vector_size(32))) U64x4;
    typedef uint32_t __attribute__((vector_size(16))) U32x4;
    typedef uint16_t __attribute__((vector_size( 8))) U16x4;
    typedef uint8_t  __attribute__((vector_size( 4)))  U8x4;

    typedef float    __attribute__((vector_size(32)))   Fx8;
    typedef int32_t  __attribute__((vector_size(32))) I32x8;
    typedef uint64_t __attribute__((vector_size(64))) U64x8;
    typedef uint32_t __attribute__((vector_size(32))) U32x8;
    typedef uint16_t __attribute__((vector_size(16))) U16x8;
    typedef uint8_t  __attribute__((vector_size( 8)))  U8x8;

    typedef float    __attribute__((vector_size( 64)))   Fx16;
    typedef int32_t  __attribute__((vector_size( 64))) I32x16;
    typedef uint64_t __attribute__((vector_size(128))) U64x16;
    typedef uint32_t __attribute__((vector_size( 64))) U32x16;
    typedef uint16_t __attribute__((vector_size( 32))) U16x16;
    typedef uint8_t  __attribute__((vector_size( 16)))  U8x16;
#endif

// First, instantiate our default exec_ops() implementation using the default compiliation target.

#if defined(SKCMS_PORTABLE) || !(defined(__clang__) || defined(__GNUC__))
    #define N 1

    #define F   float
    #define U64 uint64_t
    #define U32 uint32_t
    #define I32 int32_t
    #define U16 uint16_t
    #define U8  uint8_t

    #define F0 0.0f
    #define F1 1.0f

#elif defined(__AVX512F__)
    #define N 16

    #define F     Fx16
    #define U64 U64x16
    #define U32 U32x16
    #define I32 I32x16
    #define U16 U16x16
    #define U8   U8x16

    #define F0 (F){0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}
    #define F1 (F){1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1}
#elif defined(__AVX__)
    #define N 8

    #define F     Fx8
    #define U64 U64x8
    #define U32 U32x8
    #define I32 I32x8
    #define U16 U16x8
    #define U8   U8x8

    #define F0 (F){0,0,0,0, 0,0,0,0}
    #define F1 (F){1,1,1,1, 1,1,1,1}
#else
    #define N 4

    #define F     Fx4
    #define U64 U64x4
    #define U32 U32x4
    #define I32 I32x4
    #define U16 U16x4
    #define U8   U8x4

    #define F0 (F){0,0,0,0}
    #define F1 (F){1,1,1,1}
#endif

#define NS(id) id
#define ATTR
    #include "Transform_inl.h"
#undef N
#undef F
#undef U64
#undef U32
#undef I32
#undef U16
#undef U8
#undef F0
#undef F1
#undef NS
#undef ATTR

// Now, instantiate any other versions of run_program() we may want for runtime detection.
#if !defined(SKCMS_PORTABLE) && (defined(__clang__) || defined(__GNUC__)) \
        && defined(__x86_64__) && !defined(__AVX2__)
    #define N 8
    #define F     Fx8
    #define U64 U64x8
    #define U32 U32x8
    #define I32 I32x8
    #define U16 U16x8
    #define U8   U8x8
    #define F0 (F){0,0,0,0, 0,0,0,0}
    #define F1 (F){1,1,1,1, 1,1,1,1}

    #define NS(id) id ## _hsw
    #define ATTR __attribute__((target("avx2,f16c")))

    // We check these guards to see if we have support for these features.
    // They're likely _not_ defined here in our baseline build config.
    #ifndef __AVX__
        #define __AVX__ 1
        #define UNDEF_AVX
    #endif
    #ifndef __F16C__
        #define __F16C__ 1
        #define UNDEF_F16C
    #endif
    #ifndef __AVX2__
        #define __AVX2__ 1
        #define UNDEF_AVX2
    #endif

    #include "Transform_inl.h"

    #undef N
    #undef F
    #undef U64
    #undef U32
    #undef I32
    #undef U16
    #undef U8
    #undef F0
    #undef F1
    #undef NS
    #undef ATTR

    #ifdef UNDEF_AVX
        #undef __AVX__
        #undef UNDEF_AVX
    #endif
    #ifdef UNDEF_F16C
        #undef __F16C__
        #undef UNDEF_F16C
    #endif
    #ifdef UNDEF_AVX2
        #undef __AVX2__
        #undef UNDEF_AVX2
    #endif

    #define TEST_FOR_HSW

    static bool hsw_ok_ = false;
    static void check_hsw_ok() {
        // See http://www.sandpile.org/x86/cpuid.htm

        // First, a basic cpuid(1).
        uint32_t eax, ebx, ecx, edx;
        __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                                     : "0"(1), "2"(0));

        // Sanity check for prerequisites.
        if ((edx & (1<<25)) != (1<<25)) { return; }   // SSE
        if ((edx & (1<<26)) != (1<<26)) { return; }   // SSE2
        if ((ecx & (1<< 0)) != (1<< 0)) { return; }   // SSE3
        if ((ecx & (1<< 9)) != (1<< 9)) { return; }   // SSSE3
        if ((ecx & (1<<19)) != (1<<19)) { return; }   // SSE4.1
        if ((ecx & (1<<20)) != (1<<20)) { return; }   // SSE4.2

        if ((ecx & (3<<26)) != (3<<26)) { return; }   // XSAVE + OSXSAVE

        {
            uint32_t eax_xgetbv, edx_xgetbv;
            __asm__ __volatile__("xgetbv" : "=a"(eax_xgetbv), "=d"(edx_xgetbv) : "c"(0));
            if ((eax_xgetbv & (3<<1)) != (3<<1)) { return; }  // XMM+YMM state saved?
        }

        if ((ecx & (1<<28)) != (1<<28)) { return; }   // AVX
        if ((ecx & (1<<29)) != (1<<29)) { return; }   // F16C
        if ((ecx & (1<<12)) != (1<<12)) { return; }   // FMA  (TODO: not currently used)

        // Call cpuid(7) to check for our final AVX2 feature bit!
        __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                                     : "0"(7), "2"(0));
        if ((ebx & (1<< 5)) != (1<< 5)) { return; }   // AVX2

        hsw_ok_ = true;
    }

    #if defined(_MSC_VER)
        #include <Windows.h>
        INIT_ONCE check_hsw_ok_once = INIT_ONCE_STATIC_INIT;

        static BOOL check_hsw_ok_InitOnce_wrapper(INIT_ONCE* once, void* param, void** ctx) {
            (void)once;
            (void)param;
            (void)ctx;
            check_hsw_ok();
            return TRUE;
        }

        static bool hsw_ok() {
            InitOnceExecuteOnce(&check_hsw_ok_once, check_hsw_ok_InitOnce_wrapper, NULL, NULL);
            return hsw_ok_;
        }
    #else
        #include <pthread.h>
        static pthread_once_t check_hsw_ok_once = PTHREAD_ONCE_INIT;

        static bool hsw_ok() {
            pthread_once(&check_hsw_ok_once, check_hsw_ok);
            return hsw_ok_;
        }
    #endif

#endif

static bool is_identity_tf(const skcms_TransferFunction* tf) {
    static const skcms_TransferFunction I = {1,1,0,0,0,0,0};
    return 0 == memcmp(&I, tf, sizeof(I));
}

typedef struct {
    Op          op;
    const void* arg;
} OpAndArg;

static OpAndArg select_curve_op(const skcms_Curve* curve, int channel) {
    static const struct { Op parametric, table_8, table_16; } ops[] = {
        { Op_tf_r, Op_table_8_r, Op_table_16_r },
        { Op_tf_g, Op_table_8_g, Op_table_16_g },
        { Op_tf_b, Op_table_8_b, Op_table_16_b },
        { Op_tf_a, Op_table_8_a, Op_table_16_a },
    };

    if (curve->table_entries == 0) {
        return is_identity_tf(&curve->parametric)
            ? (OpAndArg){ Op_noop, NULL }
            : (OpAndArg){ ops[channel].parametric, &curve->parametric };
    } else if (curve->table_8) {
        return (OpAndArg){ ops[channel].table_8,  curve };
    } else if (curve->table_16) {
        return (OpAndArg){ ops[channel].table_16, curve };
    }

    assert(false);
    return (OpAndArg){Op_noop,NULL};
}

static size_t bytes_per_pixel(skcms_PixelFormat fmt) {
    switch (fmt >> 1) {   // ignore rgb/bgr
        case skcms_PixelFormat_RGB_565       >> 1: return  2;
        case skcms_PixelFormat_RGB_888       >> 1: return  3;
        case skcms_PixelFormat_RGBA_8888     >> 1: return  4;
        case skcms_PixelFormat_RGBA_1010102  >> 1: return  4;
        case skcms_PixelFormat_RGB_161616    >> 1: return  6;
        case skcms_PixelFormat_RGBA_16161616 >> 1: return  8;
        case skcms_PixelFormat_RGB_hhh       >> 1: return  6;
        case skcms_PixelFormat_RGBA_hhhh     >> 1: return  8;
        case skcms_PixelFormat_RGB_fff       >> 1: return 12;
        case skcms_PixelFormat_RGBA_ffff     >> 1: return 16;
    }
    assert(false);
    return 0;
}

bool skcms_Transform(const void*             src,
                     skcms_PixelFormat       srcFmt,
                     skcms_AlphaFormat       srcAlpha,
                     const skcms_ICCProfile* srcProfile,
                     void*                   dst,
                     skcms_PixelFormat       dstFmt,
                     skcms_AlphaFormat       dstAlpha,
                     const skcms_ICCProfile* dstProfile,
                     size_t                  nz) {
    const size_t dst_bpp = bytes_per_pixel(dstFmt),
                 src_bpp = bytes_per_pixel(srcFmt);
    // Let's just refuse if the request is absurdly big.
    if (nz * dst_bpp > INT_MAX || nz * src_bpp > INT_MAX) {
        return false;
    }
    int n = (int)nz;

    // Both profiles can be null if we're just doing format conversion, otherwise both are needed
    if (!dstProfile != !srcProfile) {
        return false;
    }

    // We can't transform in place unless the PixelFormats are the same size.
    if (dst == src && (dstFmt >> 1) != (srcFmt >> 1)) {
        return false;
    }
    // TODO: this check lazilly disallows U16 <-> F16, but that would actually be fine.
    // TODO: more careful alias rejection (like, dst == src + 1)?

    Op          program  [32];
    const void* arguments[32];

    Op*          ops  = program;
    const void** args = arguments;

    skcms_TransferFunction inv_dst_tf_r, inv_dst_tf_g, inv_dst_tf_b;
    skcms_Matrix3x3        from_xyz;

    switch (srcFmt >> 1) {
        default: return false;
        case skcms_PixelFormat_RGB_565       >> 1: *ops++ = Op_load_565;      break;
        case skcms_PixelFormat_RGB_888       >> 1: *ops++ = Op_load_888;      break;
        case skcms_PixelFormat_RGBA_8888     >> 1: *ops++ = Op_load_8888;     break;
        case skcms_PixelFormat_RGBA_1010102  >> 1: *ops++ = Op_load_1010102;  break;
        case skcms_PixelFormat_RGB_161616    >> 1: *ops++ = Op_load_161616;   break;
        case skcms_PixelFormat_RGBA_16161616 >> 1: *ops++ = Op_load_16161616; break;
        case skcms_PixelFormat_RGB_hhh       >> 1: *ops++ = Op_load_hhh;      break;
        case skcms_PixelFormat_RGBA_hhhh     >> 1: *ops++ = Op_load_hhhh;     break;
        case skcms_PixelFormat_RGB_fff       >> 1: *ops++ = Op_load_fff;      break;
        case skcms_PixelFormat_RGBA_ffff     >> 1: *ops++ = Op_load_ffff;     break;
    }
    if (srcFmt & 1) {
        *ops++ = Op_swap_rb;
    }

    if (srcProfile->data_color_space == 0x434D594B /*'CMYK*/) {
        // Photoshop creates CMYK images as inverse CMYK.
        // These happen to be the only ones we've _ever_ seen.
        *ops++ = Op_invert;
    }

    if (srcAlpha == skcms_AlphaFormat_Opaque) {
        *ops++ = Op_force_opaque;
    } else if (srcAlpha == skcms_AlphaFormat_PremulAsEncoded) {
        *ops++ = Op_unpremul;
    }

    // TODO: We can skip this work if both srcAlpha and dstAlpha are PremulLinear, and the profiles
    // are the same. Also, if dstAlpha is PremulLinear, and SrcAlpha is Opaque.
    if (dstProfile != srcProfile ||
        srcAlpha == skcms_AlphaFormat_PremulLinear ||
        dstAlpha == skcms_AlphaFormat_PremulLinear) {

        if (srcProfile->has_A2B) {
            if (srcProfile->A2B.input_channels) {
                for (int i = 0; i < (int)srcProfile->A2B.input_channels; i++) {
                    OpAndArg oa = select_curve_op(&srcProfile->A2B.input_curves[i], i);
                    if (oa.op != Op_noop) {
                        *ops++  = oa.op;
                        *args++ = oa.arg;
                    }
                }
                switch (srcProfile->A2B.input_channels) {
                    case 3: *ops++ = srcProfile->A2B.grid_8 ? Op_clut_3D_8 : Op_clut_3D_16; break;
                    case 4: *ops++ = srcProfile->A2B.grid_8 ? Op_clut_4D_8 : Op_clut_4D_16; break;
                    default: return false;
                }
                *args++ = &srcProfile->A2B;
            }

            if (srcProfile->A2B.matrix_channels == 3) {
                for (int i = 0; i < 3; i++) {
                    OpAndArg oa = select_curve_op(&srcProfile->A2B.matrix_curves[i], i);
                    if (oa.op != Op_noop) {
                        *ops++  = oa.op;
                        *args++ = oa.arg;
                    }
                }

                static const skcms_Matrix3x4 I = {{
                    {1,0,0,0},
                    {0,1,0,0},
                    {0,0,1,0},
                }};
                if (0 != memcmp(&I, &srcProfile->A2B.matrix, sizeof(I))) {
                    *ops++  = Op_matrix_3x4;
                    *args++ = &srcProfile->A2B.matrix;
                }
            }

            if (srcProfile->A2B.output_channels == 3) {
                for (int i = 0; i < 3; i++) {
                    OpAndArg oa = select_curve_op(&srcProfile->A2B.output_curves[i], i);
                    if (oa.op != Op_noop) {
                        *ops++  = oa.op;
                        *args++ = oa.arg;
                    }
                }
            }

            if (srcProfile->pcs == 0x4C616220 /* 'Lab ' */) {
                *ops++ = Op_lab_to_xyz;
            }

        } else if (srcProfile->has_trc && srcProfile->has_toXYZD50) {
            for (int i = 0; i < 3; i++) {
                OpAndArg oa = select_curve_op(&srcProfile->trc[i], i);
                if (oa.op != Op_noop) {
                    *ops++  = oa.op;
                    *args++ = oa.arg;
                }
            }
        } else {
            return false;
        }

        // At this point our source colors are linear, either RGB (XYZ-type profiles)
        // or XYZ (A2B-type profiles). Unpremul is a linear operation (multiply by a
        // constant 1/a), so either way we can do it now if needed.
        if (srcAlpha == skcms_AlphaFormat_PremulLinear) {
            *ops++ = Op_unpremul;
        }

        // We only support destination gamuts that can be transformed from XYZD50.
        if (!dstProfile->has_toXYZD50) {
            return false;
        }

        // A2B sources should already be in XYZD50 at this point.
        // Others still need to be transformed using their toXYZD50 matrix.
        // N.B. There are profiles that contain both A2B tags and toXYZD50 matrices.
        // If we use the A2B tags, we need to ignore the XYZD50 matrix entirely.
        assert (srcProfile->has_A2B || srcProfile->has_toXYZD50);
        static const skcms_Matrix3x3 I = {{
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
        }};
        const skcms_Matrix3x3* to_xyz = srcProfile->has_A2B ? &I : &srcProfile->toXYZD50;

        // There's a chance the source and destination gamuts are identical,
        // in which case we can skip the gamut transform.
        if (0 != memcmp(&dstProfile->toXYZD50, to_xyz, sizeof(skcms_Matrix3x3))) {
            if (!skcms_Matrix3x3_invert(&dstProfile->toXYZD50, &from_xyz)) {
                return false;
            }
            // TODO: concat these here and only append one matrix_3x3 op.
            *ops++ = Op_matrix_3x3; *args++ =    to_xyz;
            *ops++ = Op_matrix_3x3; *args++ = &from_xyz;
        }

        // Encode back to dst RGB using its parametric transfer functions.
        if (dstProfile->has_trc &&
            dstProfile->trc[0].table_entries == 0 &&
            dstProfile->trc[1].table_entries == 0 &&
            dstProfile->trc[2].table_entries == 0 &&
            skcms_TransferFunction_invert(&dstProfile->trc[0].parametric, &inv_dst_tf_r) &&
            skcms_TransferFunction_invert(&dstProfile->trc[1].parametric, &inv_dst_tf_g) &&
            skcms_TransferFunction_invert(&dstProfile->trc[2].parametric, &inv_dst_tf_b)) {

            if (dstAlpha == skcms_AlphaFormat_PremulLinear) {
                *ops++ = Op_premul;
            }

            if (!is_identity_tf(&inv_dst_tf_r)) { *ops++ = Op_tf_r; *args++ = &inv_dst_tf_r; }
            if (!is_identity_tf(&inv_dst_tf_g)) { *ops++ = Op_tf_g; *args++ = &inv_dst_tf_g; }
            if (!is_identity_tf(&inv_dst_tf_b)) { *ops++ = Op_tf_b; *args++ = &inv_dst_tf_b; }
        } else {
            return false;
        }
    }

    if (dstAlpha == skcms_AlphaFormat_Opaque) {
        *ops++ = Op_force_opaque;
    } else if (dstAlpha == skcms_AlphaFormat_PremulAsEncoded) {
        *ops++ = Op_premul;
    }
    if (dstFmt & 1) {
        *ops++ = Op_swap_rb;
    }
    if (dstFmt < skcms_PixelFormat_RGB_hhh) {
        *ops++ = Op_clamp;
    }
    switch (dstFmt >> 1) {
        default: return false;
        case skcms_PixelFormat_RGB_565       >> 1: *ops++ = Op_store_565;      break;
        case skcms_PixelFormat_RGB_888       >> 1: *ops++ = Op_store_888;      break;
        case skcms_PixelFormat_RGBA_8888     >> 1: *ops++ = Op_store_8888;     break;
        case skcms_PixelFormat_RGBA_1010102  >> 1: *ops++ = Op_store_1010102;  break;
        case skcms_PixelFormat_RGB_161616    >> 1: *ops++ = Op_store_161616;   break;
        case skcms_PixelFormat_RGBA_16161616 >> 1: *ops++ = Op_store_16161616; break;
        case skcms_PixelFormat_RGB_hhh       >> 1: *ops++ = Op_store_hhh;      break;
        case skcms_PixelFormat_RGBA_hhhh     >> 1: *ops++ = Op_store_hhhh;     break;
        case skcms_PixelFormat_RGB_fff       >> 1: *ops++ = Op_store_fff;      break;
        case skcms_PixelFormat_RGBA_ffff     >> 1: *ops++ = Op_store_ffff;     break;
    }

    void (*run)(const Op*, const void**, const char*, char*, int, size_t,size_t) = run_program;
#if defined(TEST_FOR_HSW)
    if (hsw_ok()) {
        run = run_program_hsw;
    }
#endif
    run(program, arguments, src, dst, n, src_bpp,dst_bpp);
    return true;
}
