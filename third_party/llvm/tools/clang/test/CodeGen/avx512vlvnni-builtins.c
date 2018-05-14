//  RUN: %clang_cc1 -ffreestanding %s -triple=x86_64-apple-darwin -target-feature +avx512vnni -target-feature +avx512vl -emit-llvm -o - -Wall -Werror | FileCheck %s

#include <immintrin.h>

__m256i test_mm256_mask_dpbusd_epi32(__m256i __S, __mmask8 __U, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_mask_dpbusd_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpbusd.256
  return _mm256_mask_dpbusd_epi32(__S, __U, __A, __B);
}

__m256i test_mm256_maskz_dpbusd_epi32(__mmask8 __U, __m256i __S, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_maskz_dpbusd_epi32
  // CHECK: @llvm.x86.avx512.maskz.vpdpbusd.256
  return _mm256_maskz_dpbusd_epi32(__U, __S, __A, __B);
}

__m256i test_mm256_dpbusd_epi32(__m256i __S, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_dpbusd_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpbusd.256
  return _mm256_dpbusd_epi32(__S, __A, __B);
}

__m256i test_mm256_mask_dpbusds_epi32(__m256i __S, __mmask8 __U, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_mask_dpbusds_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpbusds.256
  return _mm256_mask_dpbusds_epi32(__S, __U, __A, __B);
}

__m256i test_mm256_maskz_dpbusds_epi32(__mmask8 __U, __m256i __S, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_maskz_dpbusds_epi32
  // CHECK: @llvm.x86.avx512.maskz.vpdpbusds.256
  return _mm256_maskz_dpbusds_epi32(__U, __S, __A, __B);
}

__m256i test_mm256_dpbusds_epi32(__m256i __S, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_dpbusds_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpbusds.256
  return _mm256_dpbusds_epi32(__S, __A, __B);
}

__m256i test_mm256_mask_dpwssd_epi32(__m256i __S, __mmask8 __U, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_mask_dpwssd_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpwssd.256
  return _mm256_mask_dpwssd_epi32(__S, __U, __A, __B);
}

__m256i test_mm256_maskz_dpwssd_epi32(__mmask8 __U, __m256i __S, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_maskz_dpwssd_epi32
  // CHECK: @llvm.x86.avx512.maskz.vpdpwssd.256
  return _mm256_maskz_dpwssd_epi32(__U, __S, __A, __B);
}

__m256i test_mm256_dpwssd_epi32(__m256i __S, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_dpwssd_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpwssd.256
  return _mm256_dpwssd_epi32(__S, __A, __B);
}

__m256i test_mm256_mask_dpwssds_epi32(__m256i __S, __mmask8 __U, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_mask_dpwssds_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpwssds.256
  return _mm256_mask_dpwssds_epi32(__S, __U, __A, __B);
}

__m256i test_mm256_maskz_dpwssds_epi32(__mmask8 __U, __m256i __S, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_maskz_dpwssds_epi32
  // CHECK: @llvm.x86.avx512.maskz.vpdpwssds.256
  return _mm256_maskz_dpwssds_epi32(__U, __S, __A, __B);
}

__m256i test_mm256_dpwssds_epi32(__m256i __S, __m256i __A, __m256i __B) {
  // CHECK-LABEL: @test_mm256_dpwssds_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpwssds.256
  return _mm256_dpwssds_epi32(__S, __A, __B);
}

__m128i test_mm128_mask_dpbusd_epi32(__m128i __S, __mmask8 __U, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_mask_dpbusd_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpbusd.128
  return _mm128_mask_dpbusd_epi32(__S, __U, __A, __B);
}

__m128i test_mm128_maskz_dpbusd_epi32(__mmask8 __U, __m128i __S, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_maskz_dpbusd_epi32
  // CHECK: @llvm.x86.avx512.maskz.vpdpbusd.128
  return _mm128_maskz_dpbusd_epi32(__U, __S, __A, __B);
}

__m128i test_mm128_dpbusd_epi32(__m128i __S, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_dpbusd_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpbusd.128
  return _mm128_dpbusd_epi32(__S, __A, __B);
}

__m128i test_mm128_mask_dpbusds_epi32(__m128i __S, __mmask8 __U, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_mask_dpbusds_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpbusds.128
  return _mm128_mask_dpbusds_epi32(__S, __U, __A, __B);
}

__m128i test_mm128_maskz_dpbusds_epi32(__mmask8 __U, __m128i __S, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_maskz_dpbusds_epi32
  // CHECK: @llvm.x86.avx512.maskz.vpdpbusds.128
  return _mm128_maskz_dpbusds_epi32(__U, __S, __A, __B);
}

__m128i test_mm128_dpbusds_epi32(__m128i __S, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_dpbusds_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpbusds.128
  return _mm128_dpbusds_epi32(__S, __A, __B);
}

__m128i test_mm128_mask_dpwssd_epi32(__m128i __S, __mmask8 __U, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_mask_dpwssd_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpwssd.128
  return _mm128_mask_dpwssd_epi32(__S, __U, __A, __B);
}

__m128i test_mm128_maskz_dpwssd_epi32(__mmask8 __U, __m128i __S, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_maskz_dpwssd_epi32
  // CHECK: @llvm.x86.avx512.maskz.vpdpwssd.128
  return _mm128_maskz_dpwssd_epi32(__U, __S, __A, __B);
}

__m128i test_mm128_dpwssd_epi32(__m128i __S, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_dpwssd_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpwssd.128
  return _mm128_dpwssd_epi32(__S, __A, __B);
}

__m128i test_mm128_mask_dpwssds_epi32(__m128i __S, __mmask8 __U, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_mask_dpwssds_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpwssds.128
  return _mm128_mask_dpwssds_epi32(__S, __U, __A, __B);
}

__m128i test_mm128_maskz_dpwssds_epi32(__mmask8 __U, __m128i __S, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_maskz_dpwssds_epi32
  // CHECK: @llvm.x86.avx512.maskz.vpdpwssds.128
  return _mm128_maskz_dpwssds_epi32(__U, __S, __A, __B);
}

__m128i test_mm128_dpwssds_epi32(__m128i __S, __m128i __A, __m128i __B) {
  // CHECK-LABEL: @test_mm128_dpwssds_epi32
  // CHECK: @llvm.x86.avx512.mask.vpdpwssds.128
  return _mm128_dpwssds_epi32(__S, __A, __B);
}

