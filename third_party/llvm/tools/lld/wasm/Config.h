//===- Config.h -------------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_WASM_CONFIG_H
#define LLD_WASM_CONFIG_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/BinaryFormat/Wasm.h"

namespace lld {
namespace wasm {

class Symbol;

struct Configuration {
  bool AllowUndefined;
  bool CheckSignatures;
  bool Demangle;
  bool GcSections;
  bool ImportMemory;
  bool PrintGcSections;
  bool Relocatable;
  bool StripAll;
  bool StripDebug;
  uint32_t GlobalBase;
  uint32_t InitialMemory;
  uint32_t MaxMemory;
  uint32_t ZStackSize;
  llvm::StringRef Entry;
  llvm::StringRef OutputFile;

  llvm::StringSet<> AllowUndefinedSymbols;
  std::vector<llvm::StringRef> SearchPaths;
};

// The only instance of Configuration struct.
extern Configuration *Config;

} // namespace wasm
} // namespace lld

#endif
