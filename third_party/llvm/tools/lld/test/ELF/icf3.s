# REQUIRES: x86

# RUN: llvm-mc -filetype=obj -triple=x86_64-unknown-linux %s -o %t1
# RUN: llvm-mc -filetype=obj -triple=x86_64-unknown-linux %p/Inputs/icf2.s -o %t2
# RUN: ld.lld %t1 %t2 -o %t --icf=all --verbose 2>&1 | FileCheck %s

# CHECK-NOT: selected section '.text.f1' from file
# CHECK-NOT: selected section '.text.f2' from file

.globl _start, f1, f2
_start:
  ret

# This section is not mergeable because the content is different from f2.
.section .text.f1, "ax"
f1:
  mov $60, %rdi
  call f2
  mov $0, %rax
