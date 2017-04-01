// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
#include <stddef.h>
#include <stdint.h>

// Test for libFuzzer's "equivalence" fuzzing, part A.
extern "C" void LLVMFuzzerAnnounceOutput(const uint8_t *Data, size_t Size);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size > 100) return 0;
  uint8_t Result[100];
  for (size_t i = 0; i < Size; i++)
    Result[Size - i - 1] = Data[i];
  LLVMFuzzerAnnounceOutput(Result, Size);
  return 0;
}
