// RUN: %clang_cc1 -verify -fopenmp -x c++ -triple powerpc64le-unknown-unknown -fopenmp-targets=powerpc64le-ibm-linux-gnu -emit-llvm-bc %s -o %t-ppc-host.bc
// RUN: %clang_cc1 -verify -fopenmp -x c++ -triple powerpc64le-unknown-unknown -fopenmp-targets=powerpc64le-ibm-linux-gnu -emit-llvm %s -fopenmp-is-device -fopenmp-host-ir-file-path %t-ppc-host.bc -o - | FileCheck %s
// RUN: %clang_cc1 -verify -fopenmp -x c++ -triple powerpc64le-unknown-unknown -fopenmp-targets=powerpc64le-ibm-linux-gnu -emit-llvm %s -fopenmp-is-device -fopenmp-host-ir-file-path %t-ppc-host.bc -emit-pch -o %t
// RUN: %clang_cc1 -verify -fopenmp -x c++ -triple powerpc64le-unknown-unknown -fopenmp-targets=powerpc64le-ibm-linux-gnu -emit-llvm %s -fopenmp-is-device -fopenmp-host-ir-file-path %t-ppc-host.bc -include-pch %t -o - | FileCheck %s

// RUN: %clang_cc1 -verify -fopenmp-simd -x c++ -triple powerpc64le-unknown-unknown -fopenmp-targets=powerpc64le-ibm-linux-gnu -emit-llvm-bc %s -o %t-ppc-host.bc
// RUN: %clang_cc1 -verify -fopenmp-simd -x c++ -triple powerpc64le-unknown-unknown -fopenmp-targets=powerpc64le-ibm-linux-gnu -emit-llvm %s -fopenmp-is-device -fopenmp-host-ir-file-path %t-ppc-host.bc -o -| FileCheck %s --check-prefix SIMD-ONLY
// RUN: %clang_cc1 -verify -fopenmp-simd -x c++ -triple powerpc64le-unknown-unknown -fopenmp-targets=powerpc64le-ibm-linux-gnu -emit-llvm %s -fopenmp-is-device -fopenmp-host-ir-file-path %t-ppc-host.bc -emit-pch -o %t
// RUN: %clang_cc1 -verify -fopenmp-simd -x c++ -triple powerpc64le-unknown-unknown -fopenmp-targets=powerpc64le-ibm-linux-gnu -emit-llvm %s -fopenmp-is-device -fopenmp-host-ir-file-path %t-ppc-host.bc -include-pch %t -verify -o - | FileCheck %s --check-prefix SIMD-ONLY

// expected-no-diagnostics

// SIMD-ONLY-NOT: {{__kmpc|__tgt}}

// CHECK-NOT: define {{.*}}{{baz1|baz4|maini1}}
// CHECK-DAG: @{{.*}}maini1{{.*}}aaa = internal global i64 23,
// CHECK-DAG: @b = global i32 15,
// CHECK-DAG: @d = global i32 0,
// CHECK-DAG: @c = external global i32,

// CHECK-DAG: define {{.*}}i32 @{{.*}}{{foo|bar|baz2|baz3}}{{.*}}()

#ifndef HEADER
#define HEADER

int foo();

int baz1();

int baz2();

int baz4() { return 5; }

#pragma omp declare target
int foo() { return 0; }
int b = 15;
int d;
#pragma omp end declare target
int c;

int bar() { return 1 + foo() + bar() + baz1() + baz2(); }

int maini1() {
  int a;
  static long aa = 32;
// CHECK-DAG: define void @__omp_offloading_{{.*}}maini1{{.*}}_l[[@LINE+1]](i32* dereferenceable{{.*}}, i64 {{.*}}, i64 {{.*}})
#pragma omp target map(tofrom \
                       : a)
  {
    static long aaa = 23;
    a = foo() + bar() + b + c + d + aa + aaa;
  }
  return baz4();
}

int baz3();
int baz2() {
// CHECK-DAG: define void @__omp_offloading_{{.*}}baz2{{.*}}_l[[@LINE+1]](i64 {{.*}})
#pragma omp target
  ++c;
  return 2 + baz3();
}
int baz3() { return 2 + baz2(); }

// CHECK-NOT: define {{.*}}{{baz1|baz4|maini1}}
#endif // HEADER
