/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "./vpx_config.h"
#include "./vp9_rtcd.h"
#include "vpx_ports/mem.h"

typedef void filter8_1dfunction (
  const unsigned char *src_ptr,
  const ptrdiff_t src_pitch,
  unsigned char *output_ptr,
  ptrdiff_t out_pitch,
  unsigned int output_height,
  const short *filter
);

#define FUN_CONV_1D(name, step_q4, filter, dir, src_start, avg, opt) \
  void vp9_convolve8_##name##_##opt(const uint8_t *src, ptrdiff_t src_stride, \
                                   uint8_t *dst, ptrdiff_t dst_stride, \
                                   const int16_t *filter_x, int x_step_q4, \
                                   const int16_t *filter_y, int y_step_q4, \
                                   int w, int h) { \
  if (step_q4 == 16 && filter[3] != 128) { \
    if (filter[0] || filter[1] || filter[2]) { \
      while (w >= 16) { \
        vp9_filter_block1d16_##dir##8_##avg##opt(src_start, \
                                                 src_stride, \
                                                 dst, \
                                                 dst_stride, \
                                                 h, \
                                                 filter); \
        src += 16; \
        dst += 16; \
        w -= 16; \
      } \
      while (w >= 8) { \
        vp9_filter_block1d8_##dir##8_##avg##opt(src_start, \
                                                src_stride, \
                                                dst, \
                                                dst_stride, \
                                                h, \
                                                filter); \
        src += 8; \
        dst += 8; \
        w -= 8; \
      } \
      while (w >= 4) { \
        vp9_filter_block1d4_##dir##8_##avg##opt(src_start, \
                                                src_stride, \
                                                dst, \
                                                dst_stride, \
                                                h, \
                                                filter); \
        src += 4; \
        dst += 4; \
        w -= 4; \
      } \
    } else { \
      while (w >= 16) { \
        vp9_filter_block1d16_##dir##2_##avg##opt(src, \
                                                 src_stride, \
                                                 dst, \
                                                 dst_stride, \
                                                 h, \
                                                 filter); \
        src += 16; \
        dst += 16; \
        w -= 16; \
      } \
      while (w >= 8) { \
        vp9_filter_block1d8_##dir##2_##avg##opt(src, \
                                                src_stride, \
                                                dst, \
                                                dst_stride, \
                                                h, \
                                                filter); \
        src += 8; \
        dst += 8; \
        w -= 8; \
      } \
      while (w >= 4) { \
        vp9_filter_block1d4_##dir##2_##avg##opt(src, \
                                                src_stride, \
                                                dst, \
                                                dst_stride, \
                                                h, \
                                                filter); \
        src += 4; \
        dst += 4; \
        w -= 4; \
      } \
    } \
  } \
  if (w) { \
    vp9_convolve8_##name##_c(src, src_stride, dst, dst_stride, \
                             filter_x, x_step_q4, filter_y, y_step_q4, \
                             w, h); \
  } \
}

#define FUN_CONV_2D(avg, opt) \
void vp9_convolve8_##avg##opt(const uint8_t *src, ptrdiff_t src_stride, \
                              uint8_t *dst, ptrdiff_t dst_stride, \
                              const int16_t *filter_x, int x_step_q4, \
                              const int16_t *filter_y, int y_step_q4, \
                              int w, int h) { \
  assert(w <= 64); \
  assert(h <= 64); \
  if (x_step_q4 == 16 && y_step_q4 == 16) { \
    if (filter_x[0] || filter_x[1] || filter_x[2] || filter_x[3] == 128 || \
        filter_y[0] || filter_y[1] || filter_y[2] || filter_y[3] == 128) { \
      DECLARE_ALIGNED(16, unsigned char, fdata2[64 * 71]); \
      vp9_convolve8_horiz_##opt(src - 3 * src_stride, src_stride, fdata2, 64, \
                                filter_x, x_step_q4, filter_y, y_step_q4, \
                                w, h + 7); \
      vp9_convolve8_##avg##vert_##opt(fdata2 + 3 * 64, 64, dst, dst_stride, \
                                      filter_x, x_step_q4, filter_y, \
                                      y_step_q4, w, h); \
    } else { \
      DECLARE_ALIGNED(16, unsigned char, fdata2[64 * 65]); \
      vp9_convolve8_horiz_##opt(src, src_stride, fdata2, 64, \
                                filter_x, x_step_q4, filter_y, y_step_q4, \
                                w, h + 1); \
      vp9_convolve8_##avg##vert_##opt(fdata2, 64, dst, dst_stride, \
                                      filter_x, x_step_q4, filter_y, \
                                      y_step_q4, w, h); \
    } \
  } else { \
    vp9_convolve8_##avg##c(src, src_stride, dst, dst_stride, \
                           filter_x, x_step_q4, filter_y, y_step_q4, w, h); \
  } \
}

#if CONFIG_VP9_HIGHBITDEPTH

typedef void highbd_filter8_1dfunction (
  const uint16_t *src_ptr,
  const ptrdiff_t src_pitch,
  uint16_t *output_ptr,
  ptrdiff_t out_pitch,
  unsigned int output_height,
  const int16_t *filter,
  int bd
);

#define HIGH_FUN_CONV_1D(name, step_q4, filter, dir, src_start, avg, opt) \
  void vp9_highbd_convolve8_##name##_##opt(const uint8_t *src8, \
                                           ptrdiff_t src_stride, \
                                           uint8_t *dst8, \
                                           ptrdiff_t dst_stride, \
                                           const int16_t *filter_x, \
                                           int x_step_q4, \
                                           const int16_t *filter_y, \
                                           int y_step_q4, \
                                           int w, int h, int bd) { \
  if (step_q4 == 16 && filter[3] != 128) { \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8); \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8); \
    if (filter[0] || filter[1] || filter[2]) { \
      while (w >= 16) { \
        vp9_highbd_filter_block1d16_##dir##8_##avg##opt(src_start, \
                                                        src_stride, \
                                                        dst, \
                                                        dst_stride, \
                                                        h, \
                                                        filter, \
                                                        bd); \
        src += 16; \
        dst += 16; \
        w -= 16; \
      } \
      while (w >= 8) { \
        vp9_highbd_filter_block1d8_##dir##8_##avg##opt(src_start, \
                                                       src_stride, \
                                                       dst, \
                                                       dst_stride, \
                                                       h, \
                                                       filter, \
                                                       bd); \
        src += 8; \
        dst += 8; \
        w -= 8; \
      } \
      while (w >= 4) { \
        vp9_highbd_filter_block1d4_##dir##8_##avg##opt(src_start, \
                                                       src_stride, \
                                                       dst, \
                                                       dst_stride, \
                                                       h, \
                                                       filter, \
                                                       bd); \
        src += 4; \
        dst += 4; \
        w -= 4; \
      } \
    } else { \
      while (w >= 16) { \
        vp9_highbd_filter_block1d16_##dir##2_##avg##opt(src, \
                                                        src_stride, \
                                                        dst, \
                                                        dst_stride, \
                                                        h, \
                                                        filter, \
                                                        bd); \
        src += 16; \
        dst += 16; \
        w -= 16; \
      } \
      while (w >= 8) { \
        vp9_highbd_filter_block1d8_##dir##2_##avg##opt(src, \
                                                       src_stride, \
                                                       dst, \
                                                       dst_stride, \
                                                       h, \
                                                       filter, \
                                                       bd); \
        src += 8; \
        dst += 8; \
        w -= 8; \
      } \
      while (w >= 4) { \
        vp9_highbd_filter_block1d4_##dir##2_##avg##opt(src, \
                                                       src_stride, \
                                                       dst, \
                                                       dst_stride, \
                                                       h, \
                                                       filter, \
                                                       bd); \
        src += 4; \
        dst += 4; \
        w -= 4; \
      } \
    } \
  } \
  if (w) { \
    vp9_highbd_convolve8_##name##_c(src8, src_stride, dst8, dst_stride, \
                                    filter_x, x_step_q4, filter_y, y_step_q4, \
                                    w, h, bd); \
  } \
}

#define HIGH_FUN_CONV_2D(avg, opt) \
void vp9_highbd_convolve8_##avg##opt(const uint8_t *src, ptrdiff_t src_stride, \
                                     uint8_t *dst, ptrdiff_t dst_stride, \
                                     const int16_t *filter_x, int x_step_q4, \
                                     const int16_t *filter_y, int y_step_q4, \
                                     int w, int h, int bd) { \
  assert(w <= 64); \
  assert(h <= 64); \
  if (x_step_q4 == 16 && y_step_q4 == 16) { \
    if (filter_x[0] || filter_x[1] || filter_x[2] || filter_x[3] == 128 || \
        filter_y[0] || filter_y[1] || filter_y[2] || filter_y[3] == 128) { \
      DECLARE_ALIGNED(16, uint16_t, fdata2[64 * 71]); \
      vp9_highbd_convolve8_horiz_##opt(src - 3 * src_stride, src_stride, \
                                       CONVERT_TO_BYTEPTR(fdata2), 64, \
                                       filter_x, x_step_q4, \
                                       filter_y, y_step_q4, \
                                       w, h + 7, bd); \
      vp9_highbd_convolve8_##avg##vert_##opt(CONVERT_TO_BYTEPTR(fdata2) + 192, \
                                             64, dst, dst_stride, \
                                             filter_x, x_step_q4, \
                                             filter_y, y_step_q4, \
                                             w, h, bd); \
    } else { \
      DECLARE_ALIGNED(16, uint16_t, fdata2[64 * 65]); \
      vp9_highbd_convolve8_horiz_##opt(src, src_stride, \
                                       CONVERT_TO_BYTEPTR(fdata2), 64, \
                                       filter_x, x_step_q4, \
                                       filter_y, y_step_q4, \
                                       w, h + 1, bd); \
      vp9_highbd_convolve8_##avg##vert_##opt(CONVERT_TO_BYTEPTR(fdata2), 64, \
                                             dst, dst_stride, \
                                             filter_x, x_step_q4, \
                                             filter_y, y_step_q4, \
                                             w, h, bd); \
    } \
  } else { \
    vp9_highbd_convolve8_##avg##c(src, src_stride, dst, dst_stride, \
                                  filter_x, x_step_q4, filter_y, y_step_q4, w, \
                                  h, bd); \
  } \
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if HAVE_AVX2 && HAVE_SSSE3
filter8_1dfunction vp9_filter_block1d16_v8_avx2;
filter8_1dfunction vp9_filter_block1d16_h8_avx2;
filter8_1dfunction vp9_filter_block1d4_v8_ssse3;
#if ARCH_X86_64
filter8_1dfunction vp9_filter_block1d8_v8_intrin_ssse3;
filter8_1dfunction vp9_filter_block1d8_h8_intrin_ssse3;
filter8_1dfunction vp9_filter_block1d4_h8_intrin_ssse3;
#define vp9_filter_block1d8_v8_avx2 vp9_filter_block1d8_v8_intrin_ssse3
#define vp9_filter_block1d8_h8_avx2 vp9_filter_block1d8_h8_intrin_ssse3
#define vp9_filter_block1d4_h8_avx2 vp9_filter_block1d4_h8_intrin_ssse3
#else  // ARCH_X86
filter8_1dfunction vp9_filter_block1d8_v8_ssse3;
filter8_1dfunction vp9_filter_block1d8_h8_ssse3;
filter8_1dfunction vp9_filter_block1d4_h8_ssse3;
#define vp9_filter_block1d8_v8_avx2 vp9_filter_block1d8_v8_ssse3
#define vp9_filter_block1d8_h8_avx2 vp9_filter_block1d8_h8_ssse3
#define vp9_filter_block1d4_h8_avx2 vp9_filter_block1d4_h8_ssse3
#endif  // ARCH_X86_64 / ARCH_X86
filter8_1dfunction vp9_filter_block1d16_v2_ssse3;
filter8_1dfunction vp9_filter_block1d16_h2_ssse3;
filter8_1dfunction vp9_filter_block1d8_v2_ssse3;
filter8_1dfunction vp9_filter_block1d8_h2_ssse3;
filter8_1dfunction vp9_filter_block1d4_v2_ssse3;
filter8_1dfunction vp9_filter_block1d4_h2_ssse3;
#define vp9_filter_block1d4_v8_avx2 vp9_filter_block1d4_v8_ssse3
#define vp9_filter_block1d16_v2_avx2 vp9_filter_block1d16_v2_ssse3
#define vp9_filter_block1d16_h2_avx2 vp9_filter_block1d16_h2_ssse3
#define vp9_filter_block1d8_v2_avx2  vp9_filter_block1d8_v2_ssse3
#define vp9_filter_block1d8_h2_avx2  vp9_filter_block1d8_h2_ssse3
#define vp9_filter_block1d4_v2_avx2  vp9_filter_block1d4_v2_ssse3
#define vp9_filter_block1d4_h2_avx2  vp9_filter_block1d4_h2_ssse3
// void vp9_convolve8_horiz_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                                uint8_t *dst, ptrdiff_t dst_stride,
//                                const int16_t *filter_x, int x_step_q4,
//                                const int16_t *filter_y, int y_step_q4,
//                                int w, int h);
// void vp9_convolve8_vert_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                               uint8_t *dst, ptrdiff_t dst_stride,
//                               const int16_t *filter_x, int x_step_q4,
//                               const int16_t *filter_y, int y_step_q4,
//                               int w, int h);
FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , avx2);
FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , avx2);

// void vp9_convolve8_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                          uint8_t *dst, ptrdiff_t dst_stride,
//                          const int16_t *filter_x, int x_step_q4,
//                          const int16_t *filter_y, int y_step_q4,
//                          int w, int h);
FUN_CONV_2D(, avx2);
#endif  // HAVE_AX2 && HAVE_SSSE3
#if HAVE_SSSE3
#if ARCH_X86_64
filter8_1dfunction vp9_filter_block1d16_v8_intrin_ssse3;
filter8_1dfunction vp9_filter_block1d16_h8_intrin_ssse3;
filter8_1dfunction vp9_filter_block1d8_v8_intrin_ssse3;
filter8_1dfunction vp9_filter_block1d8_h8_intrin_ssse3;
filter8_1dfunction vp9_filter_block1d4_v8_ssse3;
filter8_1dfunction vp9_filter_block1d4_h8_intrin_ssse3;
#define vp9_filter_block1d16_v8_ssse3 vp9_filter_block1d16_v8_intrin_ssse3
#define vp9_filter_block1d16_h8_ssse3 vp9_filter_block1d16_h8_intrin_ssse3
#define vp9_filter_block1d8_v8_ssse3 vp9_filter_block1d8_v8_intrin_ssse3
#define vp9_filter_block1d8_h8_ssse3 vp9_filter_block1d8_h8_intrin_ssse3
#define vp9_filter_block1d4_h8_ssse3 vp9_filter_block1d4_h8_intrin_ssse3
#else  // ARCH_X86
filter8_1dfunction vp9_filter_block1d16_v8_ssse3;
filter8_1dfunction vp9_filter_block1d16_h8_ssse3;
filter8_1dfunction vp9_filter_block1d8_v8_ssse3;
filter8_1dfunction vp9_filter_block1d8_h8_ssse3;
filter8_1dfunction vp9_filter_block1d4_v8_ssse3;
filter8_1dfunction vp9_filter_block1d4_h8_ssse3;
#endif  // ARCH_X86_64 / ARCH_X86
filter8_1dfunction vp9_filter_block1d16_v8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d16_h8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d8_v8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d8_h8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d4_v8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d4_h8_avg_ssse3;

filter8_1dfunction vp9_filter_block1d16_v2_ssse3;
filter8_1dfunction vp9_filter_block1d16_h2_ssse3;
filter8_1dfunction vp9_filter_block1d8_v2_ssse3;
filter8_1dfunction vp9_filter_block1d8_h2_ssse3;
filter8_1dfunction vp9_filter_block1d4_v2_ssse3;
filter8_1dfunction vp9_filter_block1d4_h2_ssse3;
filter8_1dfunction vp9_filter_block1d16_v2_avg_ssse3;
filter8_1dfunction vp9_filter_block1d16_h2_avg_ssse3;
filter8_1dfunction vp9_filter_block1d8_v2_avg_ssse3;
filter8_1dfunction vp9_filter_block1d8_h2_avg_ssse3;
filter8_1dfunction vp9_filter_block1d4_v2_avg_ssse3;
filter8_1dfunction vp9_filter_block1d4_h2_avg_ssse3;

// void vp9_convolve8_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride,
//                                uint8_t *dst, ptrdiff_t dst_stride,
//                                const int16_t *filter_x, int x_step_q4,
//                                const int16_t *filter_y, int y_step_q4,
//                                int w, int h);
// void vp9_convolve8_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride,
//                               uint8_t *dst, ptrdiff_t dst_stride,
//                               const int16_t *filter_x, int x_step_q4,
//                               const int16_t *filter_y, int y_step_q4,
//                               int w, int h);
// void vp9_convolve8_avg_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride,
//                                    uint8_t *dst, ptrdiff_t dst_stride,
//                                    const int16_t *filter_x, int x_step_q4,
//                                    const int16_t *filter_y, int y_step_q4,
//                                    int w, int h);
// void vp9_convolve8_avg_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride,
//                                   uint8_t *dst, ptrdiff_t dst_stride,
//                                   const int16_t *filter_x, int x_step_q4,
//                                   const int16_t *filter_y, int y_step_q4,
//                                   int w, int h);
FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , ssse3);
FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , ssse3);
FUN_CONV_1D(avg_horiz, x_step_q4, filter_x, h, src, avg_, ssse3);
FUN_CONV_1D(avg_vert, y_step_q4, filter_y, v, src - src_stride * 3, avg_,
            ssse3);

// void vp9_convolve8_ssse3(const uint8_t *src, ptrdiff_t src_stride,
//                          uint8_t *dst, ptrdiff_t dst_stride,
//                          const int16_t *filter_x, int x_step_q4,
//                          const int16_t *filter_y, int y_step_q4,
//                          int w, int h);
// void vp9_convolve8_avg_ssse3(const uint8_t *src, ptrdiff_t src_stride,
//                              uint8_t *dst, ptrdiff_t dst_stride,
//                              const int16_t *filter_x, int x_step_q4,
//                              const int16_t *filter_y, int y_step_q4,
//                              int w, int h);
FUN_CONV_2D(, ssse3);
FUN_CONV_2D(avg_ , ssse3);
#endif  // HAVE_SSSE3

#if HAVE_SSE2
filter8_1dfunction vp9_filter_block1d16_v8_sse2;
filter8_1dfunction vp9_filter_block1d16_h8_sse2;
filter8_1dfunction vp9_filter_block1d8_v8_sse2;
filter8_1dfunction vp9_filter_block1d8_h8_sse2;
filter8_1dfunction vp9_filter_block1d4_v8_sse2;
filter8_1dfunction vp9_filter_block1d4_h8_sse2;
filter8_1dfunction vp9_filter_block1d16_v8_avg_sse2;
filter8_1dfunction vp9_filter_block1d16_h8_avg_sse2;
filter8_1dfunction vp9_filter_block1d8_v8_avg_sse2;
filter8_1dfunction vp9_filter_block1d8_h8_avg_sse2;
filter8_1dfunction vp9_filter_block1d4_v8_avg_sse2;
filter8_1dfunction vp9_filter_block1d4_h8_avg_sse2;

filter8_1dfunction vp9_filter_block1d16_v2_sse2;
filter8_1dfunction vp9_filter_block1d16_h2_sse2;
filter8_1dfunction vp9_filter_block1d8_v2_sse2;
filter8_1dfunction vp9_filter_block1d8_h2_sse2;
filter8_1dfunction vp9_filter_block1d4_v2_sse2;
filter8_1dfunction vp9_filter_block1d4_h2_sse2;
filter8_1dfunction vp9_filter_block1d16_v2_avg_sse2;
filter8_1dfunction vp9_filter_block1d16_h2_avg_sse2;
filter8_1dfunction vp9_filter_block1d8_v2_avg_sse2;
filter8_1dfunction vp9_filter_block1d8_h2_avg_sse2;
filter8_1dfunction vp9_filter_block1d4_v2_avg_sse2;
filter8_1dfunction vp9_filter_block1d4_h2_avg_sse2;

// void vp9_convolve8_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                               uint8_t *dst, ptrdiff_t dst_stride,
//                               const int16_t *filter_x, int x_step_q4,
//                               const int16_t *filter_y, int y_step_q4,
//                               int w, int h);
// void vp9_convolve8_vert_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                              uint8_t *dst, ptrdiff_t dst_stride,
//                              const int16_t *filter_x, int x_step_q4,
//                              const int16_t *filter_y, int y_step_q4,
//                              int w, int h);
// void vp9_convolve8_avg_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                                   uint8_t *dst, ptrdiff_t dst_stride,
//                                   const int16_t *filter_x, int x_step_q4,
//                                   const int16_t *filter_y, int y_step_q4,
//                                   int w, int h);
// void vp9_convolve8_avg_vert_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                                  uint8_t *dst, ptrdiff_t dst_stride,
//                                  const int16_t *filter_x, int x_step_q4,
//                                  const int16_t *filter_y, int y_step_q4,
//                                  int w, int h);
FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , sse2);
FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , sse2);
FUN_CONV_1D(avg_horiz, x_step_q4, filter_x, h, src, avg_, sse2);
FUN_CONV_1D(avg_vert, y_step_q4, filter_y, v, src - src_stride * 3, avg_, sse2);

// void vp9_convolve8_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                         uint8_t *dst, ptrdiff_t dst_stride,
//                         const int16_t *filter_x, int x_step_q4,
//                         const int16_t *filter_y, int y_step_q4,
//                         int w, int h);
// void vp9_convolve8_avg_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                             uint8_t *dst, ptrdiff_t dst_stride,
//                             const int16_t *filter_x, int x_step_q4,
//                             const int16_t *filter_y, int y_step_q4,
//                             int w, int h);
FUN_CONV_2D(, sse2);
FUN_CONV_2D(avg_ , sse2);

#if CONFIG_VP9_HIGHBITDEPTH && ARCH_X86_64
highbd_filter8_1dfunction vp9_highbd_filter_block1d16_v8_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d16_h8_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d8_v8_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d8_h8_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d4_v8_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d4_h8_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d16_v8_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d16_h8_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d8_v8_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d8_h8_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d4_v8_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d4_h8_avg_sse2;

highbd_filter8_1dfunction vp9_highbd_filter_block1d16_v2_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d16_h2_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d8_v2_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d8_h2_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d4_v2_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d4_h2_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d16_v2_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d16_h2_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d8_v2_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d8_h2_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d4_v2_avg_sse2;
highbd_filter8_1dfunction vp9_highbd_filter_block1d4_h2_avg_sse2;

// void vp9_highbd_convolve8_horiz_sse2(const uint8_t *src,
//                                      ptrdiff_t src_stride,
//                                      uint8_t *dst,
//                                      ptrdiff_t dst_stride,
//                                      const int16_t *filter_x,
//                                      int x_step_q4,
//                                      const int16_t *filter_y,
//                                      int y_step_q4,
//                                      int w, int h, int bd);
// void vp9_highbd_convolve8_vert_sse2(const uint8_t *src,
//                                     ptrdiff_t src_stride,
//                                     uint8_t *dst,
//                                     ptrdiff_t dst_stride,
//                                     const int16_t *filter_x,
//                                     int x_step_q4,
//                                     const int16_t *filter_y,
//                                     int y_step_q4,
//                                     int w, int h, int bd);
// void vp9_highbd_convolve8_avg_horiz_sse2(const uint8_t *src,
//                                          ptrdiff_t src_stride,
//                                          uint8_t *dst,
//                                          ptrdiff_t dst_stride,
//                                          const int16_t *filter_x,
//                                          int x_step_q4,
//                                          const int16_t *filter_y,
//                                          int y_step_q4,
//                                          int w, int h, int bd);
// void vp9_highbd_convolve8_avg_vert_sse2(const uint8_t *src,
//                                         ptrdiff_t src_stride,
//                                         uint8_t *dst,
//                                         ptrdiff_t dst_stride,
//                                         const int16_t *filter_x,
//                                         int x_step_q4,
//                                         const int16_t *filter_y,
//                                         int y_step_q4,
//                                         int w, int h, int bd);
HIGH_FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , sse2);
HIGH_FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , sse2);
HIGH_FUN_CONV_1D(avg_horiz, x_step_q4, filter_x, h, src, avg_, sse2);
HIGH_FUN_CONV_1D(avg_vert, y_step_q4, filter_y, v, src - src_stride * 3, avg_,
                 sse2);

// void vp9_highbd_convolve8_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                                uint8_t *dst, ptrdiff_t dst_stride,
//                                const int16_t *filter_x, int x_step_q4,
//                                const int16_t *filter_y, int y_step_q4,
//                                int w, int h, int bd);
// void vp9_highbd_convolve8_avg_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                                    uint8_t *dst, ptrdiff_t dst_stride,
//                                    const int16_t *filter_x, int x_step_q4,
//                                    const int16_t *filter_y, int y_step_q4,
//                                    int w, int h, int bd);
HIGH_FUN_CONV_2D(, sse2);
HIGH_FUN_CONV_2D(avg_ , sse2);
#endif  // CONFIG_VP9_HIGHBITDEPTH && ARCH_X86_64
#endif  // HAVE_SSE2
