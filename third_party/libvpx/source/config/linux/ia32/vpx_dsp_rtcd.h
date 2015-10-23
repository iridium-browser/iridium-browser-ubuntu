#ifndef VPX_DSP_RTCD_H_
#define VPX_DSP_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * DSP
 */

#include "vpx/vpx_integer.h"
#include "vpx_dsp/vpx_dsp_common.h"


#ifdef __cplusplus
extern "C" {
#endif

void vp9_idct16x16_10_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_10_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct16x16_10_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct16x16_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_1_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct16x16_1_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct16x16_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_256_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct16x16_256_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct32x32_1024_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_1024_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct32x32_1024_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct32x32_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_1_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct32x32_1_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct32x32_34_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_34_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct32x32_34_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct4x4_16_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct4x4_16_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct4x4_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct4x4_1_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct4x4_1_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct8x8_12_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_12_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct8x8_12_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct8x8_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_1_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct8x8_1_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_idct8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_64_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct8x8_64_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_iwht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
void vp9_iwht4x4_16_add_sse2(const tran_low_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_iwht4x4_16_add)(const tran_low_t *input, uint8_t *dest, int dest_stride);

void vp9_iwht4x4_1_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride);
#define vp9_iwht4x4_1_add vp9_iwht4x4_1_add_c

void vp9_quantize_b_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void vp9_quantize_b_sse2(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
RTCD_EXTERN void (*vp9_quantize_b)(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);

void vp9_quantize_b_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp9_quantize_b_32x32 vp9_quantize_b_32x32_c

void vpx_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride);
#define vpx_comp_avg_pred vpx_comp_avg_pred_c

void vpx_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_avx2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vpx_convolve8)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vpx_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_avg_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_avg_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vpx_convolve8_avg)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vpx_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_avg_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_avg_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vpx_convolve8_avg_horiz)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vpx_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_avg_vert_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_avg_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vpx_convolve8_avg_vert)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vpx_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_horiz_avx2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vpx_convolve8_horiz)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vpx_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_vert_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve8_vert_avx2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vpx_convolve8_vert)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vpx_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve_avg_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vpx_convolve_avg)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vpx_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vpx_convolve_copy_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vpx_convolve_copy)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vpx_d117_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_d117_predictor_16x16 vpx_d117_predictor_16x16_c

void vpx_d117_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_d117_predictor_32x32 vpx_d117_predictor_32x32_c

void vpx_d117_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_d117_predictor_4x4 vpx_d117_predictor_4x4_c

void vpx_d117_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_d117_predictor_8x8 vpx_d117_predictor_8x8_c

void vpx_d135_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_d135_predictor_16x16 vpx_d135_predictor_16x16_c

void vpx_d135_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_d135_predictor_32x32 vpx_d135_predictor_32x32_c

void vpx_d135_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_d135_predictor_4x4 vpx_d135_predictor_4x4_c

void vpx_d135_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_d135_predictor_8x8 vpx_d135_predictor_8x8_c

void vpx_d153_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d153_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d153_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d153_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d153_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d153_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d153_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d153_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d153_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d153_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d153_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d153_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d207_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d207_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d207_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d207_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d207_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d207_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d207_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d207_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d207_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d207_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d207_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d207_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d45_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d45_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d45_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d45_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d45_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d45_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d45_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d45_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d45_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d45_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d45_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d45_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d63_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d63_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d63_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d63_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d63_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d63_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d63_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d63_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d63_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_d63_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_d63_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_d63_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_128_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_128_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_128_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_128_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_128_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_128_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_128_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_128_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_128_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_128_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_128_predictor_8x8_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_128_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_left_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_left_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_left_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_left_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_left_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_left_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_left_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_left_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_left_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_left_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_left_predictor_8x8_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_left_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_predictor_8x8_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_top_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_top_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_top_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_top_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_top_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_top_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_top_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_top_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_top_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_dc_top_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_dc_top_predictor_8x8_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_dc_top_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_fdct16x16_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct16x16_sse2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct16x16)(const int16_t *input, tran_low_t *output, int stride);

void vpx_fdct16x16_1_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct16x16_1_sse2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct16x16_1)(const int16_t *input, tran_low_t *output, int stride);

void vpx_fdct32x32_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct32x32_sse2(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct32x32_avx2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct32x32)(const int16_t *input, tran_low_t *output, int stride);

void vpx_fdct32x32_1_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct32x32_1_sse2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct32x32_1)(const int16_t *input, tran_low_t *output, int stride);

void vpx_fdct32x32_rd_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct32x32_rd_sse2(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct32x32_rd_avx2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct32x32_rd)(const int16_t *input, tran_low_t *output, int stride);

void vpx_fdct4x4_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct4x4_sse2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct4x4)(const int16_t *input, tran_low_t *output, int stride);

void vpx_fdct4x4_1_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct4x4_1_sse2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct4x4_1)(const int16_t *input, tran_low_t *output, int stride);

void vpx_fdct8x8_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct8x8_sse2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct8x8)(const int16_t *input, tran_low_t *output, int stride);

void vpx_fdct8x8_1_c(const int16_t *input, tran_low_t *output, int stride);
void vpx_fdct8x8_1_sse2(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*vpx_fdct8x8_1)(const int16_t *input, tran_low_t *output, int stride);

void vpx_get16x16var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vpx_get16x16var_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vpx_get16x16var_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
RTCD_EXTERN void (*vpx_get16x16var)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);

unsigned int vpx_get4x4sse_cs_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride);
#define vpx_get4x4sse_cs vpx_get4x4sse_cs_c

void vpx_get8x8var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vpx_get8x8var_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vpx_get8x8var_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
RTCD_EXTERN void (*vpx_get8x8var)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);

unsigned int vpx_get_mb_ss_c(const int16_t *);
unsigned int vpx_get_mb_ss_mmx(const int16_t *);
unsigned int vpx_get_mb_ss_sse2(const int16_t *);
RTCD_EXTERN unsigned int (*vpx_get_mb_ss)(const int16_t *);

void vpx_h_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_h_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_h_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_h_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_h_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_h_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_h_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_h_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_h_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_h_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_h_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_h_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_lpf_horizontal_16_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vpx_lpf_horizontal_16_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vpx_lpf_horizontal_16_avx2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vpx_lpf_horizontal_16)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vpx_lpf_horizontal_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vpx_lpf_horizontal_4_mmx(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vpx_lpf_horizontal_4)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vpx_lpf_horizontal_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vpx_lpf_horizontal_4_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*vpx_lpf_horizontal_4_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void vpx_lpf_horizontal_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vpx_lpf_horizontal_8_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vpx_lpf_horizontal_8)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vpx_lpf_horizontal_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vpx_lpf_horizontal_8_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*vpx_lpf_horizontal_8_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void vpx_lpf_vertical_16_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void vpx_lpf_vertical_16_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*vpx_lpf_vertical_16)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void vpx_lpf_vertical_16_dual_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void vpx_lpf_vertical_16_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*vpx_lpf_vertical_16_dual)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void vpx_lpf_vertical_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vpx_lpf_vertical_4_mmx(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vpx_lpf_vertical_4)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vpx_lpf_vertical_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vpx_lpf_vertical_4_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*vpx_lpf_vertical_4_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void vpx_lpf_vertical_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vpx_lpf_vertical_8_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vpx_lpf_vertical_8)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vpx_lpf_vertical_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vpx_lpf_vertical_8_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*vpx_lpf_vertical_8_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

unsigned int vpx_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse16x16_mmx(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse16x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse16x16_avx2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_mse16x16)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vpx_mse16x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse16x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_mse16x8)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vpx_mse8x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse8x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_mse8x16)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vpx_mse8x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse8x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_mse8x8)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vpx_sad16x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad16x16_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad16x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad16x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad16x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad16x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad16x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad16x16x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x16x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x16x3_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x16x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad16x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad16x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x16x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad16x16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x16x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x16x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad16x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad16x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad16x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad16x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad16x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad16x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad16x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad16x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad16x8_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad16x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad16x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad16x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad16x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad16x8_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad16x8x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x8x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x8x3_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x8x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad16x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad16x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x8x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad16x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x8x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x8x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad32x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x16_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad32x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x16_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad32x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad32x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad32x16x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad32x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad32x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad32x32x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad32x32x3 vpx_sad32x32x3_c

void vpx_sad32x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad32x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad32x32x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad32x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad32x32x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad32x32x8 vpx_sad32x32x8_c

unsigned int vpx_sad32x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad32x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad32x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad32x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad32x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad4x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad4x4_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad4x4_sse(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad4x4)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad4x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad4x4_avg_sse(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad4x4_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad4x4x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad4x4x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad4x4x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad4x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad4x4x4d_sse(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad4x4x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad4x4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad4x4x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad4x4x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad4x8_sse(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad4x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad4x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad4x8_avg_sse(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad4x8_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad4x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad4x8x4d_sse(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad4x8x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad4x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad4x8x8 vpx_sad4x8x8_c

unsigned int vpx_sad64x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad64x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad64x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad64x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad64x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad64x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad64x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad64x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad64x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad64x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad64x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad64x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad64x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad64x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad64x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad64x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad64x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad64x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad64x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad64x64x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad64x64x3 vpx_sad64x64x3_c

void vpx_sad64x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad64x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad64x64x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad64x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad64x64x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad64x64x8 vpx_sad64x64x8_c

unsigned int vpx_sad8x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad8x16_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad8x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad8x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad8x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad8x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad8x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad8x16x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad8x16x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x16x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad8x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad8x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x16x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad8x16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad8x16x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x16x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad8x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad8x4_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad8x4)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad8x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad8x4_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad8x4_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad8x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad8x4x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x4x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad8x4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad8x4x8 vpx_sad8x4x8_c

unsigned int vpx_sad8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad8x8_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad8x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad8x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad8x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad8x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad8x8_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad8x8x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad8x8x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x8x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad8x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad8x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x8x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad8x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad8x8x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x8x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

uint32_t vpx_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance16x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance16x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance16x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance16x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance16x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance16x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance32x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance32x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance32x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance32x32_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance32x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance32x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance32x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance4x4_sse(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance4x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance4x8_sse(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance4x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance4x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance64x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance64x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance64x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance64x64_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance64x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance8x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance8x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance8x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance8x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t vpx_sub_pixel_avg_variance8x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance8x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t vpx_sub_pixel_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance16x16_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance16x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance16x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance16x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance16x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance16x8_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance16x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance16x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance32x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance32x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance32x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance32x32_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance32x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance32x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance32x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance4x4_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance4x4_sse(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance4x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance4x8_sse(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance4x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance4x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance64x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance64x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance64x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance64x64_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance64x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance8x16_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance8x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance8x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance8x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance8x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t vpx_sub_pixel_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance8x8_mmx(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t vpx_sub_pixel_variance8x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance8x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

void vpx_subtract_block_c(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);
void vpx_subtract_block_sse2(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);
RTCD_EXTERN void (*vpx_subtract_block)(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);

void vpx_tm_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_tm_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_tm_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_tm_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vpx_tm_predictor_32x32 vpx_tm_predictor_32x32_c

void vpx_tm_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_tm_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_tm_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_tm_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_tm_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_tm_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_v_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_v_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_v_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_v_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_v_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_v_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_v_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_v_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_v_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vpx_v_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vpx_v_predictor_8x8_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vpx_v_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

unsigned int vpx_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x16_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance16x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance16x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x8_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance16x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance32x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance32x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance32x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance4x4_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance4x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance4x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance4x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance4x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance64x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance64x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance64x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance64x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x16_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance8x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance8x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x8_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance8x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

uint32_t vpx_variance_halfpixvar16x16_h_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
uint32_t vpx_variance_halfpixvar16x16_h_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
uint32_t vpx_variance_halfpixvar16x16_h_sse2(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_variance_halfpixvar16x16_h)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);

uint32_t vpx_variance_halfpixvar16x16_hv_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
uint32_t vpx_variance_halfpixvar16x16_hv_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
uint32_t vpx_variance_halfpixvar16x16_hv_sse2(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_variance_halfpixvar16x16_hv)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);

uint32_t vpx_variance_halfpixvar16x16_v_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
uint32_t vpx_variance_halfpixvar16x16_v_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
uint32_t vpx_variance_halfpixvar16x16_v_sse2(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*vpx_variance_halfpixvar16x16_v)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse);

void vpx_dsp_rtcd(void);

#ifdef RTCD_C
#include "vpx_ports/x86.h"
static void setup_rtcd_internal(void)
{
    int flags = x86_simd_caps();

    (void)flags;

    vp9_idct16x16_10_add = vp9_idct16x16_10_add_c;
    if (flags & HAS_SSE2) vp9_idct16x16_10_add = vp9_idct16x16_10_add_sse2;
    vp9_idct16x16_1_add = vp9_idct16x16_1_add_c;
    if (flags & HAS_SSE2) vp9_idct16x16_1_add = vp9_idct16x16_1_add_sse2;
    vp9_idct16x16_256_add = vp9_idct16x16_256_add_c;
    if (flags & HAS_SSE2) vp9_idct16x16_256_add = vp9_idct16x16_256_add_sse2;
    vp9_idct32x32_1024_add = vp9_idct32x32_1024_add_c;
    if (flags & HAS_SSE2) vp9_idct32x32_1024_add = vp9_idct32x32_1024_add_sse2;
    vp9_idct32x32_1_add = vp9_idct32x32_1_add_c;
    if (flags & HAS_SSE2) vp9_idct32x32_1_add = vp9_idct32x32_1_add_sse2;
    vp9_idct32x32_34_add = vp9_idct32x32_34_add_c;
    if (flags & HAS_SSE2) vp9_idct32x32_34_add = vp9_idct32x32_34_add_sse2;
    vp9_idct4x4_16_add = vp9_idct4x4_16_add_c;
    if (flags & HAS_SSE2) vp9_idct4x4_16_add = vp9_idct4x4_16_add_sse2;
    vp9_idct4x4_1_add = vp9_idct4x4_1_add_c;
    if (flags & HAS_SSE2) vp9_idct4x4_1_add = vp9_idct4x4_1_add_sse2;
    vp9_idct8x8_12_add = vp9_idct8x8_12_add_c;
    if (flags & HAS_SSE2) vp9_idct8x8_12_add = vp9_idct8x8_12_add_sse2;
    vp9_idct8x8_1_add = vp9_idct8x8_1_add_c;
    if (flags & HAS_SSE2) vp9_idct8x8_1_add = vp9_idct8x8_1_add_sse2;
    vp9_idct8x8_64_add = vp9_idct8x8_64_add_c;
    if (flags & HAS_SSE2) vp9_idct8x8_64_add = vp9_idct8x8_64_add_sse2;
    vp9_iwht4x4_16_add = vp9_iwht4x4_16_add_c;
    if (flags & HAS_SSE2) vp9_iwht4x4_16_add = vp9_iwht4x4_16_add_sse2;
    vp9_quantize_b = vp9_quantize_b_c;
    if (flags & HAS_SSE2) vp9_quantize_b = vp9_quantize_b_sse2;
    vpx_convolve8 = vpx_convolve8_c;
    if (flags & HAS_SSE2) vpx_convolve8 = vpx_convolve8_sse2;
    if (flags & HAS_SSSE3) vpx_convolve8 = vpx_convolve8_ssse3;
    if (flags & HAS_AVX2) vpx_convolve8 = vpx_convolve8_avx2;
    vpx_convolve8_avg = vpx_convolve8_avg_c;
    if (flags & HAS_SSE2) vpx_convolve8_avg = vpx_convolve8_avg_sse2;
    if (flags & HAS_SSSE3) vpx_convolve8_avg = vpx_convolve8_avg_ssse3;
    vpx_convolve8_avg_horiz = vpx_convolve8_avg_horiz_c;
    if (flags & HAS_SSE2) vpx_convolve8_avg_horiz = vpx_convolve8_avg_horiz_sse2;
    if (flags & HAS_SSSE3) vpx_convolve8_avg_horiz = vpx_convolve8_avg_horiz_ssse3;
    vpx_convolve8_avg_vert = vpx_convolve8_avg_vert_c;
    if (flags & HAS_SSE2) vpx_convolve8_avg_vert = vpx_convolve8_avg_vert_sse2;
    if (flags & HAS_SSSE3) vpx_convolve8_avg_vert = vpx_convolve8_avg_vert_ssse3;
    vpx_convolve8_horiz = vpx_convolve8_horiz_c;
    if (flags & HAS_SSE2) vpx_convolve8_horiz = vpx_convolve8_horiz_sse2;
    if (flags & HAS_SSSE3) vpx_convolve8_horiz = vpx_convolve8_horiz_ssse3;
    if (flags & HAS_AVX2) vpx_convolve8_horiz = vpx_convolve8_horiz_avx2;
    vpx_convolve8_vert = vpx_convolve8_vert_c;
    if (flags & HAS_SSE2) vpx_convolve8_vert = vpx_convolve8_vert_sse2;
    if (flags & HAS_SSSE3) vpx_convolve8_vert = vpx_convolve8_vert_ssse3;
    if (flags & HAS_AVX2) vpx_convolve8_vert = vpx_convolve8_vert_avx2;
    vpx_convolve_avg = vpx_convolve_avg_c;
    if (flags & HAS_SSE2) vpx_convolve_avg = vpx_convolve_avg_sse2;
    vpx_convolve_copy = vpx_convolve_copy_c;
    if (flags & HAS_SSE2) vpx_convolve_copy = vpx_convolve_copy_sse2;
    vpx_d153_predictor_16x16 = vpx_d153_predictor_16x16_c;
    if (flags & HAS_SSSE3) vpx_d153_predictor_16x16 = vpx_d153_predictor_16x16_ssse3;
    vpx_d153_predictor_32x32 = vpx_d153_predictor_32x32_c;
    if (flags & HAS_SSSE3) vpx_d153_predictor_32x32 = vpx_d153_predictor_32x32_ssse3;
    vpx_d153_predictor_4x4 = vpx_d153_predictor_4x4_c;
    if (flags & HAS_SSSE3) vpx_d153_predictor_4x4 = vpx_d153_predictor_4x4_ssse3;
    vpx_d153_predictor_8x8 = vpx_d153_predictor_8x8_c;
    if (flags & HAS_SSSE3) vpx_d153_predictor_8x8 = vpx_d153_predictor_8x8_ssse3;
    vpx_d207_predictor_16x16 = vpx_d207_predictor_16x16_c;
    if (flags & HAS_SSSE3) vpx_d207_predictor_16x16 = vpx_d207_predictor_16x16_ssse3;
    vpx_d207_predictor_32x32 = vpx_d207_predictor_32x32_c;
    if (flags & HAS_SSSE3) vpx_d207_predictor_32x32 = vpx_d207_predictor_32x32_ssse3;
    vpx_d207_predictor_4x4 = vpx_d207_predictor_4x4_c;
    if (flags & HAS_SSSE3) vpx_d207_predictor_4x4 = vpx_d207_predictor_4x4_ssse3;
    vpx_d207_predictor_8x8 = vpx_d207_predictor_8x8_c;
    if (flags & HAS_SSSE3) vpx_d207_predictor_8x8 = vpx_d207_predictor_8x8_ssse3;
    vpx_d45_predictor_16x16 = vpx_d45_predictor_16x16_c;
    if (flags & HAS_SSSE3) vpx_d45_predictor_16x16 = vpx_d45_predictor_16x16_ssse3;
    vpx_d45_predictor_32x32 = vpx_d45_predictor_32x32_c;
    if (flags & HAS_SSSE3) vpx_d45_predictor_32x32 = vpx_d45_predictor_32x32_ssse3;
    vpx_d45_predictor_4x4 = vpx_d45_predictor_4x4_c;
    if (flags & HAS_SSSE3) vpx_d45_predictor_4x4 = vpx_d45_predictor_4x4_ssse3;
    vpx_d45_predictor_8x8 = vpx_d45_predictor_8x8_c;
    if (flags & HAS_SSSE3) vpx_d45_predictor_8x8 = vpx_d45_predictor_8x8_ssse3;
    vpx_d63_predictor_16x16 = vpx_d63_predictor_16x16_c;
    if (flags & HAS_SSSE3) vpx_d63_predictor_16x16 = vpx_d63_predictor_16x16_ssse3;
    vpx_d63_predictor_32x32 = vpx_d63_predictor_32x32_c;
    if (flags & HAS_SSSE3) vpx_d63_predictor_32x32 = vpx_d63_predictor_32x32_ssse3;
    vpx_d63_predictor_4x4 = vpx_d63_predictor_4x4_c;
    if (flags & HAS_SSSE3) vpx_d63_predictor_4x4 = vpx_d63_predictor_4x4_ssse3;
    vpx_d63_predictor_8x8 = vpx_d63_predictor_8x8_c;
    if (flags & HAS_SSSE3) vpx_d63_predictor_8x8 = vpx_d63_predictor_8x8_ssse3;
    vpx_dc_128_predictor_16x16 = vpx_dc_128_predictor_16x16_c;
    if (flags & HAS_SSE2) vpx_dc_128_predictor_16x16 = vpx_dc_128_predictor_16x16_sse2;
    vpx_dc_128_predictor_32x32 = vpx_dc_128_predictor_32x32_c;
    if (flags & HAS_SSE2) vpx_dc_128_predictor_32x32 = vpx_dc_128_predictor_32x32_sse2;
    vpx_dc_128_predictor_4x4 = vpx_dc_128_predictor_4x4_c;
    if (flags & HAS_SSE) vpx_dc_128_predictor_4x4 = vpx_dc_128_predictor_4x4_sse;
    vpx_dc_128_predictor_8x8 = vpx_dc_128_predictor_8x8_c;
    if (flags & HAS_SSE) vpx_dc_128_predictor_8x8 = vpx_dc_128_predictor_8x8_sse;
    vpx_dc_left_predictor_16x16 = vpx_dc_left_predictor_16x16_c;
    if (flags & HAS_SSE2) vpx_dc_left_predictor_16x16 = vpx_dc_left_predictor_16x16_sse2;
    vpx_dc_left_predictor_32x32 = vpx_dc_left_predictor_32x32_c;
    if (flags & HAS_SSE2) vpx_dc_left_predictor_32x32 = vpx_dc_left_predictor_32x32_sse2;
    vpx_dc_left_predictor_4x4 = vpx_dc_left_predictor_4x4_c;
    if (flags & HAS_SSE) vpx_dc_left_predictor_4x4 = vpx_dc_left_predictor_4x4_sse;
    vpx_dc_left_predictor_8x8 = vpx_dc_left_predictor_8x8_c;
    if (flags & HAS_SSE) vpx_dc_left_predictor_8x8 = vpx_dc_left_predictor_8x8_sse;
    vpx_dc_predictor_16x16 = vpx_dc_predictor_16x16_c;
    if (flags & HAS_SSE2) vpx_dc_predictor_16x16 = vpx_dc_predictor_16x16_sse2;
    vpx_dc_predictor_32x32 = vpx_dc_predictor_32x32_c;
    if (flags & HAS_SSE2) vpx_dc_predictor_32x32 = vpx_dc_predictor_32x32_sse2;
    vpx_dc_predictor_4x4 = vpx_dc_predictor_4x4_c;
    if (flags & HAS_SSE) vpx_dc_predictor_4x4 = vpx_dc_predictor_4x4_sse;
    vpx_dc_predictor_8x8 = vpx_dc_predictor_8x8_c;
    if (flags & HAS_SSE) vpx_dc_predictor_8x8 = vpx_dc_predictor_8x8_sse;
    vpx_dc_top_predictor_16x16 = vpx_dc_top_predictor_16x16_c;
    if (flags & HAS_SSE2) vpx_dc_top_predictor_16x16 = vpx_dc_top_predictor_16x16_sse2;
    vpx_dc_top_predictor_32x32 = vpx_dc_top_predictor_32x32_c;
    if (flags & HAS_SSE2) vpx_dc_top_predictor_32x32 = vpx_dc_top_predictor_32x32_sse2;
    vpx_dc_top_predictor_4x4 = vpx_dc_top_predictor_4x4_c;
    if (flags & HAS_SSE) vpx_dc_top_predictor_4x4 = vpx_dc_top_predictor_4x4_sse;
    vpx_dc_top_predictor_8x8 = vpx_dc_top_predictor_8x8_c;
    if (flags & HAS_SSE) vpx_dc_top_predictor_8x8 = vpx_dc_top_predictor_8x8_sse;
    vpx_fdct16x16 = vpx_fdct16x16_c;
    if (flags & HAS_SSE2) vpx_fdct16x16 = vpx_fdct16x16_sse2;
    vpx_fdct16x16_1 = vpx_fdct16x16_1_c;
    if (flags & HAS_SSE2) vpx_fdct16x16_1 = vpx_fdct16x16_1_sse2;
    vpx_fdct32x32 = vpx_fdct32x32_c;
    if (flags & HAS_SSE2) vpx_fdct32x32 = vpx_fdct32x32_sse2;
    if (flags & HAS_AVX2) vpx_fdct32x32 = vpx_fdct32x32_avx2;
    vpx_fdct32x32_1 = vpx_fdct32x32_1_c;
    if (flags & HAS_SSE2) vpx_fdct32x32_1 = vpx_fdct32x32_1_sse2;
    vpx_fdct32x32_rd = vpx_fdct32x32_rd_c;
    if (flags & HAS_SSE2) vpx_fdct32x32_rd = vpx_fdct32x32_rd_sse2;
    if (flags & HAS_AVX2) vpx_fdct32x32_rd = vpx_fdct32x32_rd_avx2;
    vpx_fdct4x4 = vpx_fdct4x4_c;
    if (flags & HAS_SSE2) vpx_fdct4x4 = vpx_fdct4x4_sse2;
    vpx_fdct4x4_1 = vpx_fdct4x4_1_c;
    if (flags & HAS_SSE2) vpx_fdct4x4_1 = vpx_fdct4x4_1_sse2;
    vpx_fdct8x8 = vpx_fdct8x8_c;
    if (flags & HAS_SSE2) vpx_fdct8x8 = vpx_fdct8x8_sse2;
    vpx_fdct8x8_1 = vpx_fdct8x8_1_c;
    if (flags & HAS_SSE2) vpx_fdct8x8_1 = vpx_fdct8x8_1_sse2;
    vpx_get16x16var = vpx_get16x16var_c;
    if (flags & HAS_SSE2) vpx_get16x16var = vpx_get16x16var_sse2;
    if (flags & HAS_AVX2) vpx_get16x16var = vpx_get16x16var_avx2;
    vpx_get8x8var = vpx_get8x8var_c;
    if (flags & HAS_MMX) vpx_get8x8var = vpx_get8x8var_mmx;
    if (flags & HAS_SSE2) vpx_get8x8var = vpx_get8x8var_sse2;
    vpx_get_mb_ss = vpx_get_mb_ss_c;
    if (flags & HAS_MMX) vpx_get_mb_ss = vpx_get_mb_ss_mmx;
    if (flags & HAS_SSE2) vpx_get_mb_ss = vpx_get_mb_ss_sse2;
    vpx_h_predictor_16x16 = vpx_h_predictor_16x16_c;
    if (flags & HAS_SSSE3) vpx_h_predictor_16x16 = vpx_h_predictor_16x16_ssse3;
    vpx_h_predictor_32x32 = vpx_h_predictor_32x32_c;
    if (flags & HAS_SSSE3) vpx_h_predictor_32x32 = vpx_h_predictor_32x32_ssse3;
    vpx_h_predictor_4x4 = vpx_h_predictor_4x4_c;
    if (flags & HAS_SSSE3) vpx_h_predictor_4x4 = vpx_h_predictor_4x4_ssse3;
    vpx_h_predictor_8x8 = vpx_h_predictor_8x8_c;
    if (flags & HAS_SSSE3) vpx_h_predictor_8x8 = vpx_h_predictor_8x8_ssse3;
    vpx_lpf_horizontal_16 = vpx_lpf_horizontal_16_c;
    if (flags & HAS_SSE2) vpx_lpf_horizontal_16 = vpx_lpf_horizontal_16_sse2;
    if (flags & HAS_AVX2) vpx_lpf_horizontal_16 = vpx_lpf_horizontal_16_avx2;
    vpx_lpf_horizontal_4 = vpx_lpf_horizontal_4_c;
    if (flags & HAS_MMX) vpx_lpf_horizontal_4 = vpx_lpf_horizontal_4_mmx;
    vpx_lpf_horizontal_4_dual = vpx_lpf_horizontal_4_dual_c;
    if (flags & HAS_SSE2) vpx_lpf_horizontal_4_dual = vpx_lpf_horizontal_4_dual_sse2;
    vpx_lpf_horizontal_8 = vpx_lpf_horizontal_8_c;
    if (flags & HAS_SSE2) vpx_lpf_horizontal_8 = vpx_lpf_horizontal_8_sse2;
    vpx_lpf_horizontal_8_dual = vpx_lpf_horizontal_8_dual_c;
    if (flags & HAS_SSE2) vpx_lpf_horizontal_8_dual = vpx_lpf_horizontal_8_dual_sse2;
    vpx_lpf_vertical_16 = vpx_lpf_vertical_16_c;
    if (flags & HAS_SSE2) vpx_lpf_vertical_16 = vpx_lpf_vertical_16_sse2;
    vpx_lpf_vertical_16_dual = vpx_lpf_vertical_16_dual_c;
    if (flags & HAS_SSE2) vpx_lpf_vertical_16_dual = vpx_lpf_vertical_16_dual_sse2;
    vpx_lpf_vertical_4 = vpx_lpf_vertical_4_c;
    if (flags & HAS_MMX) vpx_lpf_vertical_4 = vpx_lpf_vertical_4_mmx;
    vpx_lpf_vertical_4_dual = vpx_lpf_vertical_4_dual_c;
    if (flags & HAS_SSE2) vpx_lpf_vertical_4_dual = vpx_lpf_vertical_4_dual_sse2;
    vpx_lpf_vertical_8 = vpx_lpf_vertical_8_c;
    if (flags & HAS_SSE2) vpx_lpf_vertical_8 = vpx_lpf_vertical_8_sse2;
    vpx_lpf_vertical_8_dual = vpx_lpf_vertical_8_dual_c;
    if (flags & HAS_SSE2) vpx_lpf_vertical_8_dual = vpx_lpf_vertical_8_dual_sse2;
    vpx_mse16x16 = vpx_mse16x16_c;
    if (flags & HAS_MMX) vpx_mse16x16 = vpx_mse16x16_mmx;
    if (flags & HAS_SSE2) vpx_mse16x16 = vpx_mse16x16_sse2;
    if (flags & HAS_AVX2) vpx_mse16x16 = vpx_mse16x16_avx2;
    vpx_mse16x8 = vpx_mse16x8_c;
    if (flags & HAS_SSE2) vpx_mse16x8 = vpx_mse16x8_sse2;
    vpx_mse8x16 = vpx_mse8x16_c;
    if (flags & HAS_SSE2) vpx_mse8x16 = vpx_mse8x16_sse2;
    vpx_mse8x8 = vpx_mse8x8_c;
    if (flags & HAS_SSE2) vpx_mse8x8 = vpx_mse8x8_sse2;
    vpx_sad16x16 = vpx_sad16x16_c;
    if (flags & HAS_MMX) vpx_sad16x16 = vpx_sad16x16_mmx;
    if (flags & HAS_SSE2) vpx_sad16x16 = vpx_sad16x16_sse2;
    vpx_sad16x16_avg = vpx_sad16x16_avg_c;
    if (flags & HAS_SSE2) vpx_sad16x16_avg = vpx_sad16x16_avg_sse2;
    vpx_sad16x16x3 = vpx_sad16x16x3_c;
    if (flags & HAS_SSE3) vpx_sad16x16x3 = vpx_sad16x16x3_sse3;
    if (flags & HAS_SSSE3) vpx_sad16x16x3 = vpx_sad16x16x3_ssse3;
    vpx_sad16x16x4d = vpx_sad16x16x4d_c;
    if (flags & HAS_SSE2) vpx_sad16x16x4d = vpx_sad16x16x4d_sse2;
    vpx_sad16x16x8 = vpx_sad16x16x8_c;
    if (flags & HAS_SSE4_1) vpx_sad16x16x8 = vpx_sad16x16x8_sse4_1;
    vpx_sad16x32 = vpx_sad16x32_c;
    if (flags & HAS_SSE2) vpx_sad16x32 = vpx_sad16x32_sse2;
    vpx_sad16x32_avg = vpx_sad16x32_avg_c;
    if (flags & HAS_SSE2) vpx_sad16x32_avg = vpx_sad16x32_avg_sse2;
    vpx_sad16x32x4d = vpx_sad16x32x4d_c;
    if (flags & HAS_SSE2) vpx_sad16x32x4d = vpx_sad16x32x4d_sse2;
    vpx_sad16x8 = vpx_sad16x8_c;
    if (flags & HAS_MMX) vpx_sad16x8 = vpx_sad16x8_mmx;
    if (flags & HAS_SSE2) vpx_sad16x8 = vpx_sad16x8_sse2;
    vpx_sad16x8_avg = vpx_sad16x8_avg_c;
    if (flags & HAS_SSE2) vpx_sad16x8_avg = vpx_sad16x8_avg_sse2;
    vpx_sad16x8x3 = vpx_sad16x8x3_c;
    if (flags & HAS_SSE3) vpx_sad16x8x3 = vpx_sad16x8x3_sse3;
    if (flags & HAS_SSSE3) vpx_sad16x8x3 = vpx_sad16x8x3_ssse3;
    vpx_sad16x8x4d = vpx_sad16x8x4d_c;
    if (flags & HAS_SSE2) vpx_sad16x8x4d = vpx_sad16x8x4d_sse2;
    vpx_sad16x8x8 = vpx_sad16x8x8_c;
    if (flags & HAS_SSE4_1) vpx_sad16x8x8 = vpx_sad16x8x8_sse4_1;
    vpx_sad32x16 = vpx_sad32x16_c;
    if (flags & HAS_SSE2) vpx_sad32x16 = vpx_sad32x16_sse2;
    if (flags & HAS_AVX2) vpx_sad32x16 = vpx_sad32x16_avx2;
    vpx_sad32x16_avg = vpx_sad32x16_avg_c;
    if (flags & HAS_SSE2) vpx_sad32x16_avg = vpx_sad32x16_avg_sse2;
    if (flags & HAS_AVX2) vpx_sad32x16_avg = vpx_sad32x16_avg_avx2;
    vpx_sad32x16x4d = vpx_sad32x16x4d_c;
    if (flags & HAS_SSE2) vpx_sad32x16x4d = vpx_sad32x16x4d_sse2;
    vpx_sad32x32 = vpx_sad32x32_c;
    if (flags & HAS_SSE2) vpx_sad32x32 = vpx_sad32x32_sse2;
    if (flags & HAS_AVX2) vpx_sad32x32 = vpx_sad32x32_avx2;
    vpx_sad32x32_avg = vpx_sad32x32_avg_c;
    if (flags & HAS_SSE2) vpx_sad32x32_avg = vpx_sad32x32_avg_sse2;
    if (flags & HAS_AVX2) vpx_sad32x32_avg = vpx_sad32x32_avg_avx2;
    vpx_sad32x32x4d = vpx_sad32x32x4d_c;
    if (flags & HAS_SSE2) vpx_sad32x32x4d = vpx_sad32x32x4d_sse2;
    if (flags & HAS_AVX2) vpx_sad32x32x4d = vpx_sad32x32x4d_avx2;
    vpx_sad32x64 = vpx_sad32x64_c;
    if (flags & HAS_SSE2) vpx_sad32x64 = vpx_sad32x64_sse2;
    if (flags & HAS_AVX2) vpx_sad32x64 = vpx_sad32x64_avx2;
    vpx_sad32x64_avg = vpx_sad32x64_avg_c;
    if (flags & HAS_SSE2) vpx_sad32x64_avg = vpx_sad32x64_avg_sse2;
    if (flags & HAS_AVX2) vpx_sad32x64_avg = vpx_sad32x64_avg_avx2;
    vpx_sad32x64x4d = vpx_sad32x64x4d_c;
    if (flags & HAS_SSE2) vpx_sad32x64x4d = vpx_sad32x64x4d_sse2;
    vpx_sad4x4 = vpx_sad4x4_c;
    if (flags & HAS_MMX) vpx_sad4x4 = vpx_sad4x4_mmx;
    if (flags & HAS_SSE) vpx_sad4x4 = vpx_sad4x4_sse;
    vpx_sad4x4_avg = vpx_sad4x4_avg_c;
    if (flags & HAS_SSE) vpx_sad4x4_avg = vpx_sad4x4_avg_sse;
    vpx_sad4x4x3 = vpx_sad4x4x3_c;
    if (flags & HAS_SSE3) vpx_sad4x4x3 = vpx_sad4x4x3_sse3;
    vpx_sad4x4x4d = vpx_sad4x4x4d_c;
    if (flags & HAS_SSE) vpx_sad4x4x4d = vpx_sad4x4x4d_sse;
    vpx_sad4x4x8 = vpx_sad4x4x8_c;
    if (flags & HAS_SSE4_1) vpx_sad4x4x8 = vpx_sad4x4x8_sse4_1;
    vpx_sad4x8 = vpx_sad4x8_c;
    if (flags & HAS_SSE) vpx_sad4x8 = vpx_sad4x8_sse;
    vpx_sad4x8_avg = vpx_sad4x8_avg_c;
    if (flags & HAS_SSE) vpx_sad4x8_avg = vpx_sad4x8_avg_sse;
    vpx_sad4x8x4d = vpx_sad4x8x4d_c;
    if (flags & HAS_SSE) vpx_sad4x8x4d = vpx_sad4x8x4d_sse;
    vpx_sad64x32 = vpx_sad64x32_c;
    if (flags & HAS_SSE2) vpx_sad64x32 = vpx_sad64x32_sse2;
    if (flags & HAS_AVX2) vpx_sad64x32 = vpx_sad64x32_avx2;
    vpx_sad64x32_avg = vpx_sad64x32_avg_c;
    if (flags & HAS_SSE2) vpx_sad64x32_avg = vpx_sad64x32_avg_sse2;
    if (flags & HAS_AVX2) vpx_sad64x32_avg = vpx_sad64x32_avg_avx2;
    vpx_sad64x32x4d = vpx_sad64x32x4d_c;
    if (flags & HAS_SSE2) vpx_sad64x32x4d = vpx_sad64x32x4d_sse2;
    vpx_sad64x64 = vpx_sad64x64_c;
    if (flags & HAS_SSE2) vpx_sad64x64 = vpx_sad64x64_sse2;
    if (flags & HAS_AVX2) vpx_sad64x64 = vpx_sad64x64_avx2;
    vpx_sad64x64_avg = vpx_sad64x64_avg_c;
    if (flags & HAS_SSE2) vpx_sad64x64_avg = vpx_sad64x64_avg_sse2;
    if (flags & HAS_AVX2) vpx_sad64x64_avg = vpx_sad64x64_avg_avx2;
    vpx_sad64x64x4d = vpx_sad64x64x4d_c;
    if (flags & HAS_SSE2) vpx_sad64x64x4d = vpx_sad64x64x4d_sse2;
    if (flags & HAS_AVX2) vpx_sad64x64x4d = vpx_sad64x64x4d_avx2;
    vpx_sad8x16 = vpx_sad8x16_c;
    if (flags & HAS_MMX) vpx_sad8x16 = vpx_sad8x16_mmx;
    if (flags & HAS_SSE2) vpx_sad8x16 = vpx_sad8x16_sse2;
    vpx_sad8x16_avg = vpx_sad8x16_avg_c;
    if (flags & HAS_SSE2) vpx_sad8x16_avg = vpx_sad8x16_avg_sse2;
    vpx_sad8x16x3 = vpx_sad8x16x3_c;
    if (flags & HAS_SSE3) vpx_sad8x16x3 = vpx_sad8x16x3_sse3;
    vpx_sad8x16x4d = vpx_sad8x16x4d_c;
    if (flags & HAS_SSE2) vpx_sad8x16x4d = vpx_sad8x16x4d_sse2;
    vpx_sad8x16x8 = vpx_sad8x16x8_c;
    if (flags & HAS_SSE4_1) vpx_sad8x16x8 = vpx_sad8x16x8_sse4_1;
    vpx_sad8x4 = vpx_sad8x4_c;
    if (flags & HAS_SSE2) vpx_sad8x4 = vpx_sad8x4_sse2;
    vpx_sad8x4_avg = vpx_sad8x4_avg_c;
    if (flags & HAS_SSE2) vpx_sad8x4_avg = vpx_sad8x4_avg_sse2;
    vpx_sad8x4x4d = vpx_sad8x4x4d_c;
    if (flags & HAS_SSE2) vpx_sad8x4x4d = vpx_sad8x4x4d_sse2;
    vpx_sad8x8 = vpx_sad8x8_c;
    if (flags & HAS_MMX) vpx_sad8x8 = vpx_sad8x8_mmx;
    if (flags & HAS_SSE2) vpx_sad8x8 = vpx_sad8x8_sse2;
    vpx_sad8x8_avg = vpx_sad8x8_avg_c;
    if (flags & HAS_SSE2) vpx_sad8x8_avg = vpx_sad8x8_avg_sse2;
    vpx_sad8x8x3 = vpx_sad8x8x3_c;
    if (flags & HAS_SSE3) vpx_sad8x8x3 = vpx_sad8x8x3_sse3;
    vpx_sad8x8x4d = vpx_sad8x8x4d_c;
    if (flags & HAS_SSE2) vpx_sad8x8x4d = vpx_sad8x8x4d_sse2;
    vpx_sad8x8x8 = vpx_sad8x8x8_c;
    if (flags & HAS_SSE4_1) vpx_sad8x8x8 = vpx_sad8x8x8_sse4_1;
    vpx_sub_pixel_avg_variance16x16 = vpx_sub_pixel_avg_variance16x16_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance16x16 = vpx_sub_pixel_avg_variance16x16_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance16x16 = vpx_sub_pixel_avg_variance16x16_ssse3;
    vpx_sub_pixel_avg_variance16x32 = vpx_sub_pixel_avg_variance16x32_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance16x32 = vpx_sub_pixel_avg_variance16x32_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance16x32 = vpx_sub_pixel_avg_variance16x32_ssse3;
    vpx_sub_pixel_avg_variance16x8 = vpx_sub_pixel_avg_variance16x8_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance16x8 = vpx_sub_pixel_avg_variance16x8_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance16x8 = vpx_sub_pixel_avg_variance16x8_ssse3;
    vpx_sub_pixel_avg_variance32x16 = vpx_sub_pixel_avg_variance32x16_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance32x16 = vpx_sub_pixel_avg_variance32x16_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance32x16 = vpx_sub_pixel_avg_variance32x16_ssse3;
    vpx_sub_pixel_avg_variance32x32 = vpx_sub_pixel_avg_variance32x32_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance32x32 = vpx_sub_pixel_avg_variance32x32_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance32x32 = vpx_sub_pixel_avg_variance32x32_ssse3;
    if (flags & HAS_AVX2) vpx_sub_pixel_avg_variance32x32 = vpx_sub_pixel_avg_variance32x32_avx2;
    vpx_sub_pixel_avg_variance32x64 = vpx_sub_pixel_avg_variance32x64_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance32x64 = vpx_sub_pixel_avg_variance32x64_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance32x64 = vpx_sub_pixel_avg_variance32x64_ssse3;
    vpx_sub_pixel_avg_variance4x4 = vpx_sub_pixel_avg_variance4x4_c;
    if (flags & HAS_SSE) vpx_sub_pixel_avg_variance4x4 = vpx_sub_pixel_avg_variance4x4_sse;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance4x4 = vpx_sub_pixel_avg_variance4x4_ssse3;
    vpx_sub_pixel_avg_variance4x8 = vpx_sub_pixel_avg_variance4x8_c;
    if (flags & HAS_SSE) vpx_sub_pixel_avg_variance4x8 = vpx_sub_pixel_avg_variance4x8_sse;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance4x8 = vpx_sub_pixel_avg_variance4x8_ssse3;
    vpx_sub_pixel_avg_variance64x32 = vpx_sub_pixel_avg_variance64x32_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance64x32 = vpx_sub_pixel_avg_variance64x32_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance64x32 = vpx_sub_pixel_avg_variance64x32_ssse3;
    vpx_sub_pixel_avg_variance64x64 = vpx_sub_pixel_avg_variance64x64_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance64x64 = vpx_sub_pixel_avg_variance64x64_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance64x64 = vpx_sub_pixel_avg_variance64x64_ssse3;
    if (flags & HAS_AVX2) vpx_sub_pixel_avg_variance64x64 = vpx_sub_pixel_avg_variance64x64_avx2;
    vpx_sub_pixel_avg_variance8x16 = vpx_sub_pixel_avg_variance8x16_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance8x16 = vpx_sub_pixel_avg_variance8x16_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance8x16 = vpx_sub_pixel_avg_variance8x16_ssse3;
    vpx_sub_pixel_avg_variance8x4 = vpx_sub_pixel_avg_variance8x4_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance8x4 = vpx_sub_pixel_avg_variance8x4_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance8x4 = vpx_sub_pixel_avg_variance8x4_ssse3;
    vpx_sub_pixel_avg_variance8x8 = vpx_sub_pixel_avg_variance8x8_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_avg_variance8x8 = vpx_sub_pixel_avg_variance8x8_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_avg_variance8x8 = vpx_sub_pixel_avg_variance8x8_ssse3;
    vpx_sub_pixel_variance16x16 = vpx_sub_pixel_variance16x16_c;
    if (flags & HAS_MMX) vpx_sub_pixel_variance16x16 = vpx_sub_pixel_variance16x16_mmx;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance16x16 = vpx_sub_pixel_variance16x16_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance16x16 = vpx_sub_pixel_variance16x16_ssse3;
    vpx_sub_pixel_variance16x32 = vpx_sub_pixel_variance16x32_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance16x32 = vpx_sub_pixel_variance16x32_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance16x32 = vpx_sub_pixel_variance16x32_ssse3;
    vpx_sub_pixel_variance16x8 = vpx_sub_pixel_variance16x8_c;
    if (flags & HAS_MMX) vpx_sub_pixel_variance16x8 = vpx_sub_pixel_variance16x8_mmx;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance16x8 = vpx_sub_pixel_variance16x8_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance16x8 = vpx_sub_pixel_variance16x8_ssse3;
    vpx_sub_pixel_variance32x16 = vpx_sub_pixel_variance32x16_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance32x16 = vpx_sub_pixel_variance32x16_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance32x16 = vpx_sub_pixel_variance32x16_ssse3;
    vpx_sub_pixel_variance32x32 = vpx_sub_pixel_variance32x32_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance32x32 = vpx_sub_pixel_variance32x32_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance32x32 = vpx_sub_pixel_variance32x32_ssse3;
    if (flags & HAS_AVX2) vpx_sub_pixel_variance32x32 = vpx_sub_pixel_variance32x32_avx2;
    vpx_sub_pixel_variance32x64 = vpx_sub_pixel_variance32x64_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance32x64 = vpx_sub_pixel_variance32x64_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance32x64 = vpx_sub_pixel_variance32x64_ssse3;
    vpx_sub_pixel_variance4x4 = vpx_sub_pixel_variance4x4_c;
    if (flags & HAS_MMX) vpx_sub_pixel_variance4x4 = vpx_sub_pixel_variance4x4_mmx;
    if (flags & HAS_SSE) vpx_sub_pixel_variance4x4 = vpx_sub_pixel_variance4x4_sse;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance4x4 = vpx_sub_pixel_variance4x4_ssse3;
    vpx_sub_pixel_variance4x8 = vpx_sub_pixel_variance4x8_c;
    if (flags & HAS_SSE) vpx_sub_pixel_variance4x8 = vpx_sub_pixel_variance4x8_sse;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance4x8 = vpx_sub_pixel_variance4x8_ssse3;
    vpx_sub_pixel_variance64x32 = vpx_sub_pixel_variance64x32_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance64x32 = vpx_sub_pixel_variance64x32_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance64x32 = vpx_sub_pixel_variance64x32_ssse3;
    vpx_sub_pixel_variance64x64 = vpx_sub_pixel_variance64x64_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance64x64 = vpx_sub_pixel_variance64x64_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance64x64 = vpx_sub_pixel_variance64x64_ssse3;
    if (flags & HAS_AVX2) vpx_sub_pixel_variance64x64 = vpx_sub_pixel_variance64x64_avx2;
    vpx_sub_pixel_variance8x16 = vpx_sub_pixel_variance8x16_c;
    if (flags & HAS_MMX) vpx_sub_pixel_variance8x16 = vpx_sub_pixel_variance8x16_mmx;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance8x16 = vpx_sub_pixel_variance8x16_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance8x16 = vpx_sub_pixel_variance8x16_ssse3;
    vpx_sub_pixel_variance8x4 = vpx_sub_pixel_variance8x4_c;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance8x4 = vpx_sub_pixel_variance8x4_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance8x4 = vpx_sub_pixel_variance8x4_ssse3;
    vpx_sub_pixel_variance8x8 = vpx_sub_pixel_variance8x8_c;
    if (flags & HAS_MMX) vpx_sub_pixel_variance8x8 = vpx_sub_pixel_variance8x8_mmx;
    if (flags & HAS_SSE2) vpx_sub_pixel_variance8x8 = vpx_sub_pixel_variance8x8_sse2;
    if (flags & HAS_SSSE3) vpx_sub_pixel_variance8x8 = vpx_sub_pixel_variance8x8_ssse3;
    vpx_subtract_block = vpx_subtract_block_c;
    if (flags & HAS_SSE2) vpx_subtract_block = vpx_subtract_block_sse2;
    vpx_tm_predictor_16x16 = vpx_tm_predictor_16x16_c;
    if (flags & HAS_SSE2) vpx_tm_predictor_16x16 = vpx_tm_predictor_16x16_sse2;
    vpx_tm_predictor_4x4 = vpx_tm_predictor_4x4_c;
    if (flags & HAS_SSE) vpx_tm_predictor_4x4 = vpx_tm_predictor_4x4_sse;
    vpx_tm_predictor_8x8 = vpx_tm_predictor_8x8_c;
    if (flags & HAS_SSE2) vpx_tm_predictor_8x8 = vpx_tm_predictor_8x8_sse2;
    vpx_v_predictor_16x16 = vpx_v_predictor_16x16_c;
    if (flags & HAS_SSE2) vpx_v_predictor_16x16 = vpx_v_predictor_16x16_sse2;
    vpx_v_predictor_32x32 = vpx_v_predictor_32x32_c;
    if (flags & HAS_SSE2) vpx_v_predictor_32x32 = vpx_v_predictor_32x32_sse2;
    vpx_v_predictor_4x4 = vpx_v_predictor_4x4_c;
    if (flags & HAS_SSE) vpx_v_predictor_4x4 = vpx_v_predictor_4x4_sse;
    vpx_v_predictor_8x8 = vpx_v_predictor_8x8_c;
    if (flags & HAS_SSE) vpx_v_predictor_8x8 = vpx_v_predictor_8x8_sse;
    vpx_variance16x16 = vpx_variance16x16_c;
    if (flags & HAS_MMX) vpx_variance16x16 = vpx_variance16x16_mmx;
    if (flags & HAS_SSE2) vpx_variance16x16 = vpx_variance16x16_sse2;
    if (flags & HAS_AVX2) vpx_variance16x16 = vpx_variance16x16_avx2;
    vpx_variance16x32 = vpx_variance16x32_c;
    if (flags & HAS_SSE2) vpx_variance16x32 = vpx_variance16x32_sse2;
    vpx_variance16x8 = vpx_variance16x8_c;
    if (flags & HAS_MMX) vpx_variance16x8 = vpx_variance16x8_mmx;
    if (flags & HAS_SSE2) vpx_variance16x8 = vpx_variance16x8_sse2;
    vpx_variance32x16 = vpx_variance32x16_c;
    if (flags & HAS_SSE2) vpx_variance32x16 = vpx_variance32x16_sse2;
    if (flags & HAS_AVX2) vpx_variance32x16 = vpx_variance32x16_avx2;
    vpx_variance32x32 = vpx_variance32x32_c;
    if (flags & HAS_SSE2) vpx_variance32x32 = vpx_variance32x32_sse2;
    if (flags & HAS_AVX2) vpx_variance32x32 = vpx_variance32x32_avx2;
    vpx_variance32x64 = vpx_variance32x64_c;
    if (flags & HAS_SSE2) vpx_variance32x64 = vpx_variance32x64_sse2;
    vpx_variance4x4 = vpx_variance4x4_c;
    if (flags & HAS_MMX) vpx_variance4x4 = vpx_variance4x4_mmx;
    if (flags & HAS_SSE2) vpx_variance4x4 = vpx_variance4x4_sse2;
    vpx_variance4x8 = vpx_variance4x8_c;
    if (flags & HAS_SSE2) vpx_variance4x8 = vpx_variance4x8_sse2;
    vpx_variance64x32 = vpx_variance64x32_c;
    if (flags & HAS_SSE2) vpx_variance64x32 = vpx_variance64x32_sse2;
    if (flags & HAS_AVX2) vpx_variance64x32 = vpx_variance64x32_avx2;
    vpx_variance64x64 = vpx_variance64x64_c;
    if (flags & HAS_SSE2) vpx_variance64x64 = vpx_variance64x64_sse2;
    if (flags & HAS_AVX2) vpx_variance64x64 = vpx_variance64x64_avx2;
    vpx_variance8x16 = vpx_variance8x16_c;
    if (flags & HAS_MMX) vpx_variance8x16 = vpx_variance8x16_mmx;
    if (flags & HAS_SSE2) vpx_variance8x16 = vpx_variance8x16_sse2;
    vpx_variance8x4 = vpx_variance8x4_c;
    if (flags & HAS_SSE2) vpx_variance8x4 = vpx_variance8x4_sse2;
    vpx_variance8x8 = vpx_variance8x8_c;
    if (flags & HAS_MMX) vpx_variance8x8 = vpx_variance8x8_mmx;
    if (flags & HAS_SSE2) vpx_variance8x8 = vpx_variance8x8_sse2;
    vpx_variance_halfpixvar16x16_h = vpx_variance_halfpixvar16x16_h_c;
    if (flags & HAS_MMX) vpx_variance_halfpixvar16x16_h = vpx_variance_halfpixvar16x16_h_mmx;
    if (flags & HAS_SSE2) vpx_variance_halfpixvar16x16_h = vpx_variance_halfpixvar16x16_h_sse2;
    vpx_variance_halfpixvar16x16_hv = vpx_variance_halfpixvar16x16_hv_c;
    if (flags & HAS_MMX) vpx_variance_halfpixvar16x16_hv = vpx_variance_halfpixvar16x16_hv_mmx;
    if (flags & HAS_SSE2) vpx_variance_halfpixvar16x16_hv = vpx_variance_halfpixvar16x16_hv_sse2;
    vpx_variance_halfpixvar16x16_v = vpx_variance_halfpixvar16x16_v_c;
    if (flags & HAS_MMX) vpx_variance_halfpixvar16x16_v = vpx_variance_halfpixvar16x16_v_mmx;
    if (flags & HAS_SSE2) vpx_variance_halfpixvar16x16_v = vpx_variance_halfpixvar16x16_v_sse2;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
