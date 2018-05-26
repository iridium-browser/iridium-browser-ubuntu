//===- InputFiles.h ---------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_WASM_INPUT_FILES_H
#define LLD_WASM_INPUT_FILES_H

#include "Symbols.h"
#include "lld/Common/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/Wasm.h"
#include "llvm/Support/MemoryBuffer.h"
#include <vector>

using llvm::object::Archive;
using llvm::object::WasmObjectFile;
using llvm::object::WasmSection;
using llvm::object::WasmSymbol;
using llvm::wasm::WasmGlobal;
using llvm::wasm::WasmImport;
using llvm::wasm::WasmRelocation;
using llvm::wasm::WasmSignature;

namespace lld {
namespace wasm {

class InputChunk;
class InputFunction;
class InputSegment;
class InputGlobal;

class InputFile {
public:
  enum Kind {
    ObjectKind,
    ArchiveKind,
  };

  virtual ~InputFile() {}

  // Returns the filename.
  StringRef getName() const { return MB.getBufferIdentifier(); }

  // Reads a file (the constructor doesn't do that).
  virtual void parse() = 0;

  Kind kind() const { return FileKind; }

  // An archive file name if this file is created from an archive.
  StringRef ParentName;

protected:
  InputFile(Kind K, MemoryBufferRef M) : MB(M), FileKind(K) {}
  MemoryBufferRef MB;

private:
  const Kind FileKind;
};

// .a file (ar archive)
class ArchiveFile : public InputFile {
public:
  explicit ArchiveFile(MemoryBufferRef M) : InputFile(ArchiveKind, M) {}
  static bool classof(const InputFile *F) { return F->kind() == ArchiveKind; }

  void addMember(const Archive::Symbol *Sym);

  void parse() override;

private:
  std::unique_ptr<Archive> File;
  llvm::DenseSet<uint64_t> Seen;
};

// .o file (wasm object file)
class ObjFile : public InputFile {
public:
  explicit ObjFile(MemoryBufferRef M) : InputFile(ObjectKind, M) {}
  static bool classof(const InputFile *F) { return F->kind() == ObjectKind; }

  void parse() override;

  // Returns the underlying wasm file.
  const WasmObjectFile *getWasmObj() const { return WasmObj.get(); }

  void dumpInfo() const;

  uint32_t calcNewIndex(const WasmRelocation &Reloc) const;
  uint32_t calcNewValue(const WasmRelocation &Reloc) const;
  uint32_t calcExpectedValue(const WasmRelocation &Reloc) const;

  const WasmSection *CodeSection = nullptr;
  const WasmSection *DataSection = nullptr;

  // Maps input type indices to output type indices
  std::vector<uint32_t> TypeMap;
  std::vector<bool> TypeIsUsed;
  // Maps function indices to table indices
  std::vector<uint32_t> TableEntries;
  std::vector<bool> UsedComdats;
  std::vector<InputSegment *> Segments;
  std::vector<InputFunction *> Functions;
  std::vector<InputGlobal *> Globals;

  ArrayRef<Symbol *> getSymbols() const { return Symbols; }
  Symbol *getSymbol(uint32_t Index) const { return Symbols[Index]; }
  FunctionSymbol *getFunctionSymbol(uint32_t Index) const;
  DataSymbol *getDataSymbol(uint32_t Index) const;
  GlobalSymbol *getGlobalSymbol(uint32_t Index) const;

private:
  Symbol *createDefined(const WasmSymbol &Sym);
  Symbol *createUndefined(const WasmSymbol &Sym);

  bool isExcludedByComdat(InputChunk *Chunk) const;

  // List of all symbols referenced or defined by this file.
  std::vector<Symbol *> Symbols;

  std::unique_ptr<WasmObjectFile> WasmObj;
};

// Opens a given file.
llvm::Optional<MemoryBufferRef> readFile(StringRef Path);

} // namespace wasm

std::string toString(const wasm::InputFile *File);

} // namespace lld

#endif
