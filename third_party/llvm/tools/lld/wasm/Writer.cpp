//===- Writer.cpp ---------------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Writer.h"
#include "Config.h"
#include "InputChunks.h"
#include "OutputSections.h"
#include "OutputSegment.h"
#include "SymbolTable.h"
#include "WriterUtils.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "lld/Common/Threads.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/LEB128.h"

#include <cstdarg>
#include <map>

#define DEBUG_TYPE "lld"

using namespace llvm;
using namespace llvm::wasm;
using namespace lld;
using namespace lld::wasm;

static constexpr int kStackAlignment = 16;
static constexpr int kInitialTableOffset = 1;

namespace {

// Traits for using WasmSignature in a DenseMap.
struct WasmSignatureDenseMapInfo {
  static WasmSignature getEmptyKey() {
    WasmSignature Sig;
    Sig.ReturnType = 1;
    return Sig;
  }
  static WasmSignature getTombstoneKey() {
    WasmSignature Sig;
    Sig.ReturnType = 2;
    return Sig;
  }
  static unsigned getHashValue(const WasmSignature &Sig) {
    uintptr_t Value = 0;
    Value += DenseMapInfo<int32_t>::getHashValue(Sig.ReturnType);
    for (int32_t Param : Sig.ParamTypes)
      Value += DenseMapInfo<int32_t>::getHashValue(Param);
    return Value;
  }
  static bool isEqual(const WasmSignature &LHS, const WasmSignature &RHS) {
    return LHS == RHS;
  }
};

// A Wasm export to be written into the export section.
struct WasmExportEntry {
  const Symbol *Sym;
  StringRef FieldName; // may not match the Symbol name
};

// The writer writes a SymbolTable result to a file.
class Writer {
public:
  void run();

private:
  void openFile();

  uint32_t lookupType(const WasmSignature &Sig);
  uint32_t registerType(const WasmSignature &Sig);
  void createCtorFunction();
  void calculateInitFunctions();
  void assignIndexes();
  void calculateImports();
  void calculateExports();
  void calculateTypes();
  void createOutputSegments();
  void layoutMemory();
  void createHeader();
  void createSections();
  SyntheticSection *createSyntheticSection(uint32_t Type,
                                           StringRef Name = "");

  // Builtin sections
  void createTypeSection();
  void createFunctionSection();
  void createTableSection();
  void createGlobalSection();
  void createExportSection();
  void createImportSection();
  void createMemorySection();
  void createElemSection();
  void createStartSection();
  void createCodeSection();
  void createDataSection();

  // Custom sections
  void createRelocSections();
  void createLinkingSection();
  void createNameSection();

  void writeHeader();
  void writeSections();

  uint64_t FileSize = 0;
  uint32_t DataSize = 0;
  uint32_t NumMemoryPages = 0;

  std::vector<const WasmSignature *> Types;
  DenseMap<WasmSignature, int32_t, WasmSignatureDenseMapInfo> TypeIndices;
  std::vector<const FunctionSymbol *> ImportedFunctions;
  std::vector<const DataSymbol *> ImportedGlobals;
  std::vector<WasmExportEntry> ExportedSymbols;
  std::vector<const DefinedData *> DefinedDataSymbols;
  std::vector<InputFunction *> DefinedFunctions;
  std::vector<const FunctionSymbol *> IndirectFunctions;
  std::vector<WasmInitFunc> InitFunctions;

  // Elements that are used to construct the final output
  std::string Header;
  std::vector<OutputSection *> OutputSections;

  std::unique_ptr<FileOutputBuffer> Buffer;
  std::unique_ptr<SyntheticFunction> CtorFunction;
  std::string CtorFunctionBody;

  std::vector<OutputSegment *> Segments;
  llvm::SmallDenseMap<StringRef, OutputSegment *> SegmentMap;
};

} // anonymous namespace

static void debugPrint(const char *fmt, ...) {
  if (!errorHandler().Verbose)
    return;
  fprintf(stderr, "lld: ");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

void Writer::createImportSection() {
  uint32_t NumImports = ImportedFunctions.size() + ImportedGlobals.size();
  if (Config->ImportMemory)
    ++NumImports;

  if (NumImports == 0)
    return;

  SyntheticSection *Section = createSyntheticSection(WASM_SEC_IMPORT);
  raw_ostream &OS = Section->getStream();

  writeUleb128(OS, NumImports, "import count");

  for (const FunctionSymbol *Sym : ImportedFunctions) {
    WasmImport Import;
    Import.Module = "env";
    Import.Field = Sym->getName();
    Import.Kind = WASM_EXTERNAL_FUNCTION;
    Import.SigIndex = lookupType(*Sym->getFunctionType());
    writeImport(OS, Import);
  }

  if (Config->ImportMemory) {
    WasmImport Import;
    Import.Module = "env";
    Import.Field = "memory";
    Import.Kind = WASM_EXTERNAL_MEMORY;
    Import.Memory.Flags = 0;
    Import.Memory.Initial = NumMemoryPages;
    writeImport(OS, Import);
  }

  for (const Symbol *Sym : ImportedGlobals) {
    WasmImport Import;
    Import.Module = "env";
    Import.Field = Sym->getName();
    Import.Kind = WASM_EXTERNAL_GLOBAL;
    Import.Global.Mutable = false;
    Import.Global.Type = WASM_TYPE_I32;
    writeImport(OS, Import);
  }
}

void Writer::createTypeSection() {
  SyntheticSection *Section = createSyntheticSection(WASM_SEC_TYPE);
  raw_ostream &OS = Section->getStream();
  writeUleb128(OS, Types.size(), "type count");
  for (const WasmSignature *Sig : Types)
    writeSig(OS, *Sig);
}

void Writer::createFunctionSection() {
  if (DefinedFunctions.empty())
    return;

  SyntheticSection *Section = createSyntheticSection(WASM_SEC_FUNCTION);
  raw_ostream &OS = Section->getStream();

  writeUleb128(OS, DefinedFunctions.size(), "function count");
  for (const InputFunction *Func : DefinedFunctions)
    writeUleb128(OS, lookupType(Func->Signature), "sig index");
}

void Writer::createMemorySection() {
  if (Config->ImportMemory)
    return;

  SyntheticSection *Section = createSyntheticSection(WASM_SEC_MEMORY);
  raw_ostream &OS = Section->getStream();

  writeUleb128(OS, 1, "memory count");
  writeUleb128(OS, 0, "memory limits flags");
  writeUleb128(OS, NumMemoryPages, "initial pages");
}

void Writer::createGlobalSection() {
  if (DefinedDataSymbols.empty())
    return;

  SyntheticSection *Section = createSyntheticSection(WASM_SEC_GLOBAL);
  raw_ostream &OS = Section->getStream();

  writeUleb128(OS, DefinedDataSymbols.size(), "global count");
  for (const DefinedData *Sym : DefinedDataSymbols) {
    WasmGlobal Global;
    Global.Type.Type = WASM_TYPE_I32;
    Global.Type.Mutable = Sym == WasmSym::StackPointer;
    Global.InitExpr.Opcode = WASM_OPCODE_I32_CONST;
    Global.InitExpr.Value.Int32 = Sym->getVirtualAddress();
    writeGlobal(OS, Global);
  }
}

void Writer::createTableSection() {
  // Always output a table section, even if there are no indirect calls.
  // There are two reasons for this:
  //  1. For executables it is useful to have an empty table slot at 0
  //     which can be filled with a null function call handler.
  //  2. If we don't do this, any program that contains a call_indirect but
  //     no address-taken function will fail at validation time since it is
  //     a validation error to include a call_indirect instruction if there
  //     is not table.
  uint32_t TableSize = kInitialTableOffset + IndirectFunctions.size();

  SyntheticSection *Section = createSyntheticSection(WASM_SEC_TABLE);
  raw_ostream &OS = Section->getStream();

  writeUleb128(OS, 1, "table count");
  writeSleb128(OS, WASM_TYPE_ANYFUNC, "table type");
  writeUleb128(OS, WASM_LIMITS_FLAG_HAS_MAX, "table flags");
  writeUleb128(OS, TableSize, "table initial size");
  writeUleb128(OS, TableSize, "table max size");
}

void Writer::createExportSection() {
  bool ExportMemory = !Config->Relocatable && !Config->ImportMemory;

  uint32_t NumExports = (ExportMemory ? 1 : 0) + ExportedSymbols.size();
  if (!NumExports)
    return;

  SyntheticSection *Section = createSyntheticSection(WASM_SEC_EXPORT);
  raw_ostream &OS = Section->getStream();

  writeUleb128(OS, NumExports, "export count");

  if (ExportMemory) {
    WasmExport MemoryExport;
    MemoryExport.Name = "memory";
    MemoryExport.Kind = WASM_EXTERNAL_MEMORY;
    MemoryExport.Index = 0;
    writeExport(OS, MemoryExport);
  }

  for (const WasmExportEntry &E : ExportedSymbols) {
    DEBUG(dbgs() << "Export: " << E.Sym->getName() << "\n");
    WasmExport Export;
    Export.Name = E.FieldName;
    Export.Index = E.Sym->getOutputIndex();
    if (isa<FunctionSymbol>(E.Sym))
      Export.Kind = WASM_EXTERNAL_FUNCTION;
    else
      Export.Kind = WASM_EXTERNAL_GLOBAL;
    writeExport(OS, Export);
  }
}

void Writer::createStartSection() {}

void Writer::createElemSection() {
  if (IndirectFunctions.empty())
    return;

  SyntheticSection *Section = createSyntheticSection(WASM_SEC_ELEM);
  raw_ostream &OS = Section->getStream();

  writeUleb128(OS, 1, "segment count");
  writeUleb128(OS, 0, "table index");
  WasmInitExpr InitExpr;
  InitExpr.Opcode = WASM_OPCODE_I32_CONST;
  InitExpr.Value.Int32 = kInitialTableOffset;
  writeInitExpr(OS, InitExpr);
  writeUleb128(OS, IndirectFunctions.size(), "elem count");

  uint32_t TableIndex = kInitialTableOffset;
  for (const FunctionSymbol *Sym : IndirectFunctions) {
    assert(Sym->getTableIndex() == TableIndex);
    writeUleb128(OS, Sym->getOutputIndex(), "function index");
    ++TableIndex;
  }
}

void Writer::createCodeSection() {
  if (DefinedFunctions.empty())
    return;

  log("createCodeSection");

  auto Section = make<CodeSection>(DefinedFunctions);
  OutputSections.push_back(Section);
}

void Writer::createDataSection() {
  if (!Segments.size())
    return;

  log("createDataSection");
  auto Section = make<DataSection>(Segments);
  OutputSections.push_back(Section);
}

// Create relocations sections in the final output.
// These are only created when relocatable output is requested.
void Writer::createRelocSections() {
  log("createRelocSections");
  // Don't use iterator here since we are adding to OutputSection
  size_t OrigSize = OutputSections.size();
  for (size_t i = 0; i < OrigSize; i++) {
    OutputSection *S = OutputSections[i];
    const char *name;
    uint32_t Count = S->numRelocations();
    if (!Count)
      continue;

    if (S->Type == WASM_SEC_DATA)
      name = "reloc.DATA";
    else if (S->Type == WASM_SEC_CODE)
      name = "reloc.CODE";
    else
      llvm_unreachable("relocations only supported for code and data");

    SyntheticSection *Section = createSyntheticSection(WASM_SEC_CUSTOM, name);
    raw_ostream &OS = Section->getStream();
    writeUleb128(OS, S->Type, "reloc section");
    writeUleb128(OS, Count, "reloc count");
    S->writeRelocations(OS);
  }
}

// Create the custom "linking" section containing linker metadata.
// This is only created when relocatable output is requested.
void Writer::createLinkingSection() {
  SyntheticSection *Section =
      createSyntheticSection(WASM_SEC_CUSTOM, "linking");
  raw_ostream &OS = Section->getStream();

  SubSection DataSizeSubSection(WASM_DATA_SIZE);
  writeUleb128(DataSizeSubSection.getStream(), DataSize, "data size");
  DataSizeSubSection.finalizeContents();
  DataSizeSubSection.writeToStream(OS);

  if (!Config->Relocatable)
    return;

  std::vector<std::pair<StringRef, uint32_t>> SymbolInfo;
  auto addSymInfo = [&](const Symbol *Sym, StringRef ExternalName) {
    uint32_t Flags =
        (Sym->isLocal() ? WASM_SYMBOL_BINDING_LOCAL :
         Sym->isWeak() ? WASM_SYMBOL_BINDING_WEAK : 0) |
        (Sym->isHidden() ? WASM_SYMBOL_VISIBILITY_HIDDEN : 0);
    if (Flags)
      SymbolInfo.emplace_back(ExternalName, Flags);
  };
  // (Imports can't have internal linkage, their names don't need to be budged.)
  for (const Symbol *Sym : ImportedFunctions)
    addSymInfo(Sym, Sym->getName());
  for (const Symbol *Sym : ImportedGlobals)
    addSymInfo(Sym, Sym->getName());
  for (const WasmExportEntry &E : ExportedSymbols)
    addSymInfo(E.Sym, E.FieldName);
  if (!SymbolInfo.empty()) {
    SubSection SubSection(WASM_SYMBOL_INFO);
    writeUleb128(SubSection.getStream(), SymbolInfo.size(), "num sym info");
    for (auto Pair: SymbolInfo) {
      writeStr(SubSection.getStream(), Pair.first, "sym name");
      writeUleb128(SubSection.getStream(), Pair.second, "sym flags");
    }
    SubSection.finalizeContents();
    SubSection.writeToStream(OS);
  }

  if (Segments.size()) {
    SubSection SubSection(WASM_SEGMENT_INFO);
    writeUleb128(SubSection.getStream(), Segments.size(), "num data segments");
    for (const OutputSegment *S : Segments) {
      writeStr(SubSection.getStream(), S->Name, "segment name");
      writeUleb128(SubSection.getStream(), S->Alignment, "alignment");
      writeUleb128(SubSection.getStream(), 0, "flags");
    }
    SubSection.finalizeContents();
    SubSection.writeToStream(OS);
  }

  if (!InitFunctions.empty()) {
    SubSection SubSection(WASM_INIT_FUNCS);
    writeUleb128(SubSection.getStream(), InitFunctions.size(),
                 "num init functions");
    for (const WasmInitFunc &F : InitFunctions) {
      writeUleb128(SubSection.getStream(), F.Priority, "priority");
      writeUleb128(SubSection.getStream(), F.FunctionIndex, "function index");
    }
    SubSection.finalizeContents();
    SubSection.writeToStream(OS);
  }

  struct ComdatEntry { unsigned Kind; uint32_t Index; };
  std::map<StringRef,std::vector<ComdatEntry>> Comdats;

  for (const InputFunction *F : DefinedFunctions) {
    StringRef Comdat = F->getComdat();
    if (!Comdat.empty())
      Comdats[Comdat].emplace_back(
          ComdatEntry{WASM_COMDAT_FUNCTION, F->getOutputIndex()});
  }
  for (uint32_t I = 0; I < Segments.size(); ++I) {
    const auto &InputSegments = Segments[I]->InputSegments;
    if (InputSegments.empty())
      continue;
    StringRef Comdat = InputSegments[0]->getComdat();
#ifndef NDEBUG
    for (const InputSegment *IS : InputSegments)
      assert(IS->getComdat() == Comdat);
#endif
    if (!Comdat.empty())
      Comdats[Comdat].emplace_back(ComdatEntry{WASM_COMDAT_DATA, I});
  }

  if (!Comdats.empty()) {
    SubSection SubSection(WASM_COMDAT_INFO);
    writeUleb128(SubSection.getStream(), Comdats.size(), "num comdats");
    for (const auto &C : Comdats) {
      writeStr(SubSection.getStream(), C.first, "comdat name");
      writeUleb128(SubSection.getStream(), 0, "comdat flags"); // flags for future use
      writeUleb128(SubSection.getStream(), C.second.size(), "num entries");
      for (const ComdatEntry &Entry : C.second) {
        writeUleb128(SubSection.getStream(), Entry.Kind, "entry kind");
        writeUleb128(SubSection.getStream(), Entry.Index, "entry index");
      }
    }
    SubSection.finalizeContents();
    SubSection.writeToStream(OS);
  }
}

// Create the custom "name" section containing debug symbol names.
void Writer::createNameSection() {
  unsigned NumNames = ImportedFunctions.size();
  for (const InputFunction *F : DefinedFunctions)
    if (!F->getName().empty())
      ++NumNames;

  if (NumNames == 0)
    return;

  SyntheticSection *Section = createSyntheticSection(WASM_SEC_CUSTOM, "name");

  SubSection FunctionSubsection(WASM_NAMES_FUNCTION);
  raw_ostream &OS = FunctionSubsection.getStream();
  writeUleb128(OS, NumNames, "name count");

  // Names must appear in function index order.  As it happens ImportedFunctions
  // and DefinedFunctions are numbers in order with imported functions coming
  // first.
  for (const Symbol *S : ImportedFunctions) {
    writeUleb128(OS, S->getOutputIndex(), "import index");
    writeStr(OS, S->getName(), "symbol name");
  }
  for (const InputFunction *F : DefinedFunctions) {
    if (!F->getName().empty()) {
      writeUleb128(OS, F->getOutputIndex(), "func index");
      writeStr(OS, F->getName(), "symbol name");
    }
  }

  FunctionSubsection.finalizeContents();
  FunctionSubsection.writeToStream(Section->getStream());
}

void Writer::writeHeader() {
  memcpy(Buffer->getBufferStart(), Header.data(), Header.size());
}

void Writer::writeSections() {
  uint8_t *Buf = Buffer->getBufferStart();
  parallelForEach(OutputSections, [Buf](OutputSection *S) { S->writeTo(Buf); });
}

// Fix the memory layout of the output binary.  This assigns memory offsets
// to each of the input data sections as well as the explicit stack region.
// The memory layout is as follows, from low to high.
//  - initialized data (starting at Config->GlobalBase)
//  - BSS data (not currently implemented in llvm)
//  - explicit stack (Config->ZStackSize)
//  - heap start / unallocated
void Writer::layoutMemory() {
  uint32_t MemoryPtr = 0;
  if (!Config->Relocatable) {
    MemoryPtr = Config->GlobalBase;
    debugPrint("mem: global base = %d\n", Config->GlobalBase);
  }

  createOutputSegments();

  // Arbitrarily set __dso_handle handle to point to the start of the data
  // segments.
  if (WasmSym::DsoHandle)
    WasmSym::DsoHandle->setVirtualAddress(MemoryPtr);

  for (OutputSegment *Seg : Segments) {
    MemoryPtr = alignTo(MemoryPtr, Seg->Alignment);
    Seg->StartVA = MemoryPtr;
    debugPrint("mem: %-15s offset=%-8d size=%-8d align=%d\n",
               Seg->Name.str().c_str(), MemoryPtr, Seg->Size, Seg->Alignment);
    MemoryPtr += Seg->Size;
  }

  // TODO: Add .bss space here.
  if (WasmSym::DataEnd)
    WasmSym::DataEnd->setVirtualAddress(MemoryPtr);

  DataSize = MemoryPtr;
  if (!Config->Relocatable)
    DataSize -= Config->GlobalBase;
  debugPrint("mem: static data = %d\n", DataSize);

  // Stack comes after static data and bss
  if (!Config->Relocatable) {
    MemoryPtr = alignTo(MemoryPtr, kStackAlignment);
    if (Config->ZStackSize != alignTo(Config->ZStackSize, kStackAlignment))
      error("stack size must be " + Twine(kStackAlignment) + "-byte aligned");
    debugPrint("mem: stack size  = %d\n", Config->ZStackSize);
    debugPrint("mem: stack base  = %d\n", MemoryPtr);
    MemoryPtr += Config->ZStackSize;
    WasmSym::StackPointer->setVirtualAddress(MemoryPtr);
    debugPrint("mem: stack top   = %d\n", MemoryPtr);
    // Set `__heap_base` to directly follow the end of the stack.  We don't
    // allocate any heap memory up front, but instead really on the malloc/brk
    // implementation growing the memory at runtime.
    WasmSym::HeapBase->setVirtualAddress(MemoryPtr);
    debugPrint("mem: heap base   = %d\n", MemoryPtr);
  }

  uint32_t MemSize = alignTo(MemoryPtr, WasmPageSize);
  NumMemoryPages = MemSize / WasmPageSize;
  debugPrint("mem: total pages = %d\n", NumMemoryPages);
}

SyntheticSection *Writer::createSyntheticSection(uint32_t Type,
                                                 StringRef Name) {
  auto Sec = make<SyntheticSection>(Type, Name);
  log("createSection: " + toString(*Sec));
  OutputSections.push_back(Sec);
  return Sec;
}

void Writer::createSections() {
  // Known sections
  createTypeSection();
  createImportSection();
  createFunctionSection();
  createTableSection();
  createMemorySection();
  createGlobalSection();
  createExportSection();
  createStartSection();
  createElemSection();
  createCodeSection();
  createDataSection();

  // Custom sections
  if (Config->Relocatable)
    createRelocSections();
  createLinkingSection();
  if (!Config->StripDebug && !Config->StripAll)
    createNameSection();

  for (OutputSection *S : OutputSections) {
    S->setOffset(FileSize);
    S->finalizeContents();
    FileSize += S->getSize();
  }
}

void Writer::calculateImports() {
  for (Symbol *Sym : Symtab->getSymbols()) {
    if (!Sym->isUndefined() || (Sym->isWeak() && !Config->Relocatable))
      continue;

    if (auto *F = dyn_cast<FunctionSymbol>(Sym)) {
      F->setOutputIndex(ImportedFunctions.size());
      ImportedFunctions.push_back(F);
    } else if (auto *G = dyn_cast<DataSymbol>(Sym)) {
      G->setOutputIndex(ImportedGlobals.size());
      ImportedGlobals.push_back(G);
    }
  }
}

void Writer::calculateExports() {
  bool ExportHidden = Config->Relocatable;
  StringSet<> UsedNames;

  auto BudgeLocalName = [&](const Symbol *Sym) {
    StringRef SymName = Sym->getName();
    // We can't budge non-local names.
    if (!Sym->isLocal())
      return SymName;
    // We must budge local names that have a collision with a symbol that we
    // haven't yet processed.
    if (!Symtab->find(SymName) && UsedNames.insert(SymName).second)
      return SymName;
    for (unsigned I = 1; ; ++I) {
      std::string NameBuf = (SymName + "." + Twine(I)).str();
      if (!UsedNames.count(NameBuf)) {
        StringRef Name = Saver.save(NameBuf);
        UsedNames.insert(Name); // Insert must use safe StringRef from save()
        return Name;
      }
    }
  };

  if (WasmSym::CallCtors && (!WasmSym::CallCtors->isHidden() || ExportHidden))
    ExportedSymbols.emplace_back(
        WasmExportEntry{WasmSym::CallCtors, WasmSym::CallCtors->getName()});

  for (ObjFile *File : Symtab->ObjectFiles) {
    for (Symbol *Sym : File->getSymbols()) {
      if (!Sym->isDefined() || File != Sym->getFile())
        continue;
      if (!isa<FunctionSymbol>(Sym))
        continue;
      if (!Sym->getChunk()->Live)
        continue;

      if ((Sym->isHidden() || Sym->isLocal()) && !ExportHidden)
        continue;
      ExportedSymbols.emplace_back(WasmExportEntry{Sym, BudgeLocalName(Sym)});
    }
  }

  for (const Symbol *Sym : DefinedDataSymbols) {
    // Can't export the SP right now because its mutable, and mutuable globals
    // are yet supported in the official binary format.
    // TODO(sbc): Remove this if/when the "mutable global" proposal is accepted.
    if (Sym == WasmSym::StackPointer)
      continue;
    ExportedSymbols.emplace_back(WasmExportEntry{Sym, BudgeLocalName(Sym)});
  }
}

uint32_t Writer::lookupType(const WasmSignature &Sig) {
  auto It = TypeIndices.find(Sig);
  if (It == TypeIndices.end()) {
    error("type not found: " + toString(Sig));
    return 0;
  }
  return It->second;
}

uint32_t Writer::registerType(const WasmSignature &Sig) {
  auto Pair = TypeIndices.insert(std::make_pair(Sig, Types.size()));
  if (Pair.second) {
    DEBUG(dbgs() << "type " << toString(Sig) << "\n");
    Types.push_back(&Sig);
  }
  return Pair.first->second;
}

void Writer::calculateTypes() {
  // The output type section is the union of the following sets:
  // 1. Any signature used in the TYPE relocation
  // 2. The signatures of all imported functions
  // 3. The signatures of all defined functions

  for (ObjFile *File : Symtab->ObjectFiles) {
    ArrayRef<WasmSignature> Types = File->getWasmObj()->types();
    for (uint32_t I = 0; I < Types.size(); I++)
      if (File->TypeIsUsed[I])
        File->TypeMap[I] = registerType(Types[I]);
  }

  for (const FunctionSymbol *Sym : ImportedFunctions)
    registerType(*Sym->getFunctionType());

  for (const InputFunction *F : DefinedFunctions)
    registerType(F->Signature);
}

void Writer::assignIndexes() {
  uint32_t GlobalIndex = ImportedGlobals.size() + DefinedDataSymbols.size();
  uint32_t FunctionIndex = ImportedFunctions.size() + DefinedFunctions.size();

  auto AddDefinedData = [&](DefinedData *Sym) {
    if (Sym) {
      DefinedDataSymbols.emplace_back(Sym);
      Sym->setOutputIndex(GlobalIndex++);
    }
  };
  AddDefinedData(WasmSym::StackPointer);
  AddDefinedData(WasmSym::HeapBase);
  AddDefinedData(WasmSym::DataEnd);

  if (Config->Relocatable)
    DefinedDataSymbols.reserve(Symtab->getSymbols().size());

  uint32_t TableIndex = kInitialTableOffset;

  if (Config->Relocatable) {
    for (ObjFile *File : Symtab->ObjectFiles) {
      DEBUG(dbgs() << "Globals: " << File->getName() << "\n");
      for (Symbol *Sym : File->getSymbols()) {
        // Create wasm globals for data symbols defined in this file
        if (File != Sym->getFile())
          continue;
        if (auto *G = dyn_cast<DefinedData>(Sym))
          AddDefinedData(G);
      }
    }
  }

  for (ObjFile *File : Symtab->ObjectFiles) {
    DEBUG(dbgs() << "Functions: " << File->getName() << "\n");
    for (InputFunction *Func : File->Functions) {
      if (!Func->Live)
        continue;
      DefinedFunctions.emplace_back(Func);
      Func->setOutputIndex(FunctionIndex++);
    }
  }

  for (ObjFile *File : Symtab->ObjectFiles) {
    DEBUG(dbgs() << "Handle relocs: " << File->getName() << "\n");
    auto HandleRelocs = [&](InputChunk *Chunk) {
      if (!Chunk->Live)
        return;
      ArrayRef<WasmSignature> Types = File->getWasmObj()->types();
      for (const WasmRelocation& Reloc : Chunk->getRelocations()) {
        if (Reloc.Type == R_WEBASSEMBLY_TABLE_INDEX_I32 ||
            Reloc.Type == R_WEBASSEMBLY_TABLE_INDEX_SLEB) {
          FunctionSymbol *Sym = File->getFunctionSymbol(Reloc.Index);
          if (Sym->hasTableIndex() || !Sym->hasOutputIndex())
            continue;
          Sym->setTableIndex(TableIndex++);
          IndirectFunctions.emplace_back(Sym);
        } else if (Reloc.Type == R_WEBASSEMBLY_TYPE_INDEX_LEB) {
          Chunk->File->TypeMap[Reloc.Index] = registerType(Types[Reloc.Index]);
          Chunk->File->TypeIsUsed[Reloc.Index] = true;
        }
      }
    };

    for (InputFunction* Function : File->Functions)
      HandleRelocs(Function);
    for (InputSegment* Segment : File->Segments)
      HandleRelocs(Segment);
  }
}

static StringRef getOutputDataSegmentName(StringRef Name) {
  if (Config->Relocatable)
    return Name;

  for (StringRef V :
       {".text.", ".rodata.", ".data.rel.ro.", ".data.", ".bss.rel.ro.",
        ".bss.", ".init_array.", ".fini_array.", ".ctors.", ".dtors.", ".tbss.",
        ".gcc_except_table.", ".tdata.", ".ARM.exidx.", ".ARM.extab."}) {
    StringRef Prefix = V.drop_back();
    if (Name.startswith(V) || Name == Prefix)
      return Prefix;
  }

  return Name;
}

void Writer::createOutputSegments() {
  for (ObjFile *File : Symtab->ObjectFiles) {
    for (InputSegment *Segment : File->Segments) {
      if (!Segment->Live)
        continue;
      StringRef Name = getOutputDataSegmentName(Segment->getName());
      OutputSegment *&S = SegmentMap[Name];
      if (S == nullptr) {
        DEBUG(dbgs() << "new segment: " << Name << "\n");
        S = make<OutputSegment>(Name);
        Segments.push_back(S);
      }
      S->addInputSegment(Segment);
      DEBUG(dbgs() << "added data: " << Name << ": " << S->Size << "\n");
    }
  }
}

static const int OPCODE_CALL = 0x10;
static const int OPCODE_END = 0xb;

// Create synthetic "__wasm_call_ctors" function based on ctor functions
// in input object.
void Writer::createCtorFunction() {
  uint32_t FunctionIndex = ImportedFunctions.size() + DefinedFunctions.size();
  WasmSym::CallCtors->setOutputIndex(FunctionIndex);

  // First write the body bytes to a string.
  std::string FunctionBody;
  static WasmSignature Signature = {{}, WASM_TYPE_NORESULT};
  {
    raw_string_ostream OS(FunctionBody);
    writeUleb128(OS, 0, "num locals");
    for (const WasmInitFunc &F : InitFunctions) {
      writeU8(OS, OPCODE_CALL, "CALL");
      writeUleb128(OS, F.FunctionIndex, "function index");
    }
    writeU8(OS, OPCODE_END, "END");
  }

  // Once we know the size of the body we can create the final function body
  raw_string_ostream OS(CtorFunctionBody);
  writeUleb128(OS, FunctionBody.size(), "function size");
  OS.flush();
  CtorFunctionBody += FunctionBody;
  ArrayRef<uint8_t> BodyArray(
      reinterpret_cast<const uint8_t *>(CtorFunctionBody.data()),
      CtorFunctionBody.size());
  CtorFunction = llvm::make_unique<SyntheticFunction>(
      Signature, BodyArray, WasmSym::CallCtors->getName());
  CtorFunction->setOutputIndex(FunctionIndex);
  DefinedFunctions.emplace_back(CtorFunction.get());
}

// Populate InitFunctions vector with init functions from all input objects.
// This is then used either when creating the output linking section or to
// synthesize the "__wasm_call_ctors" function.
void Writer::calculateInitFunctions() {
  for (ObjFile *File : Symtab->ObjectFiles) {
    const WasmLinkingData &L = File->getWasmObj()->linkingData();
    InitFunctions.reserve(InitFunctions.size() + L.InitFunctions.size());
    for (const WasmInitFunc &F : L.InitFunctions)
      InitFunctions.emplace_back(WasmInitFunc{
          F.Priority, File->relocateFunctionIndex(F.FunctionIndex)});
  }
  // Sort in order of priority (lowest first) so that they are called
  // in the correct order.
  std::stable_sort(InitFunctions.begin(), InitFunctions.end(),
                   [](const WasmInitFunc &L, const WasmInitFunc &R) {
                     return L.Priority < R.Priority;
                   });
}

void Writer::run() {
  log("-- calculateImports");
  calculateImports();
  log("-- assignIndexes");
  assignIndexes();
  log("-- calculateExports");
  calculateExports();
  log("-- calculateInitFunctions");
  calculateInitFunctions();
  if (!Config->Relocatable)
    createCtorFunction();
  log("-- calculateTypes");
  calculateTypes();

  if (errorHandler().Verbose) {
    log("Defined Functions: " + Twine(DefinedFunctions.size()));
    log("Defined Data Syms: " + Twine(DefinedDataSymbols.size()));
    log("Function Imports : " + Twine(ImportedFunctions.size()));
    log("Global Imports   : " + Twine(ImportedGlobals.size()));
    log("Total Imports    : " +
        Twine(ImportedFunctions.size() + ImportedGlobals.size()));
    for (ObjFile *File : Symtab->ObjectFiles)
      File->dumpInfo();
  }

  log("-- layoutMemory");
  layoutMemory();

  createHeader();
  log("-- createSections");
  createSections();

  log("-- openFile");
  openFile();
  if (errorCount())
    return;

  writeHeader();

  log("-- writeSections");
  writeSections();
  if (errorCount())
    return;

  if (Error E = Buffer->commit())
    fatal("failed to write the output file: " + toString(std::move(E)));
}

// Open a result file.
void Writer::openFile() {
  log("writing: " + Config->OutputFile);
  ::remove(Config->OutputFile.str().c_str());

  Expected<std::unique_ptr<FileOutputBuffer>> BufferOrErr =
      FileOutputBuffer::create(Config->OutputFile, FileSize,
                               FileOutputBuffer::F_executable);

  if (!BufferOrErr)
    error("failed to open " + Config->OutputFile + ": " +
          toString(BufferOrErr.takeError()));
  else
    Buffer = std::move(*BufferOrErr);
}

void Writer::createHeader() {
  raw_string_ostream OS(Header);
  writeBytes(OS, WasmMagic, sizeof(WasmMagic), "wasm magic");
  writeU32(OS, WasmVersion, "wasm version");
  OS.flush();
  FileSize += Header.size();
}

void lld::wasm::writeResult() { Writer().run(); }
