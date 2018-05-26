//===- InputFiles.cpp -----------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "InputSection.h"
#include "LinkerScript.h"
#include "SymbolTable.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/Analysis.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/LTO/LTO.h"
#include "llvm/MC/StringTableBuilder.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/ARMAttributeParser.h"
#include "llvm/Support/ARMBuildAttributes.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TarWriter.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::object;
using namespace llvm::sys;
using namespace llvm::sys::fs;

using namespace lld;
using namespace lld::elf;

std::vector<BinaryFile *> elf::BinaryFiles;
std::vector<BitcodeFile *> elf::BitcodeFiles;
std::vector<InputFile *> elf::ObjectFiles;
std::vector<InputFile *> elf::SharedFiles;

TarWriter *elf::Tar;

InputFile::InputFile(Kind K, MemoryBufferRef M) : MB(M), FileKind(K) {}

Optional<MemoryBufferRef> elf::readFile(StringRef Path) {
  // The --chroot option changes our virtual root directory.
  // This is useful when you are dealing with files created by --reproduce.
  if (!Config->Chroot.empty() && Path.startswith("/"))
    Path = Saver.save(Config->Chroot + Path);

  log(Path);

  auto MBOrErr = MemoryBuffer::getFile(Path);
  if (auto EC = MBOrErr.getError()) {
    error("cannot open " + Path + ": " + EC.message());
    return None;
  }

  std::unique_ptr<MemoryBuffer> &MB = *MBOrErr;
  MemoryBufferRef MBRef = MB->getMemBufferRef();
  make<std::unique_ptr<MemoryBuffer>>(std::move(MB)); // take MB ownership

  if (Tar)
    Tar->append(relativeToRoot(Path), MBRef.getBuffer());
  return MBRef;
}

// Concatenates arguments to construct a string representing an error location.
static std::string createFileLineMsg(StringRef Path, unsigned Line) {
  std::string Filename = path::filename(Path);
  std::string Lineno = ":" + std::to_string(Line);
  if (Filename == Path)
    return Filename + Lineno;
  return Filename + Lineno + " (" + Path.str() + Lineno + ")";
}

template <class ELFT>
static std::string getSrcMsgAux(ObjFile<ELFT> &File, const Symbol &Sym,
                                InputSectionBase &Sec, uint64_t Offset) {
  // In DWARF, functions and variables are stored to different places.
  // First, lookup a function for a given offset.
  if (Optional<DILineInfo> Info = File.getDILineInfo(&Sec, Offset))
    return createFileLineMsg(Info->FileName, Info->Line);

  // If it failed, lookup again as a variable.
  if (Optional<std::pair<std::string, unsigned>> FileLine =
          File.getVariableLoc(Sym.getName()))
    return createFileLineMsg(FileLine->first, FileLine->second);

  // File.SourceFile contains STT_FILE symbol, and that is a last resort.
  return File.SourceFile;
}

std::string InputFile::getSrcMsg(const Symbol &Sym, InputSectionBase &Sec,
                                 uint64_t Offset) {
  if (kind() != ObjKind)
    return "";
  switch (Config->EKind) {
  default:
    llvm_unreachable("Invalid kind");
  case ELF32LEKind:
    return getSrcMsgAux(cast<ObjFile<ELF32LE>>(*this), Sym, Sec, Offset);
  case ELF32BEKind:
    return getSrcMsgAux(cast<ObjFile<ELF32BE>>(*this), Sym, Sec, Offset);
  case ELF64LEKind:
    return getSrcMsgAux(cast<ObjFile<ELF64LE>>(*this), Sym, Sec, Offset);
  case ELF64BEKind:
    return getSrcMsgAux(cast<ObjFile<ELF64BE>>(*this), Sym, Sec, Offset);
  }
}

template <class ELFT> void ObjFile<ELFT>::initializeDwarf() {
  Dwarf = llvm::make_unique<DWARFContext>(make_unique<LLDDwarfObj<ELFT>>(this));
  const DWARFObject &Obj = Dwarf->getDWARFObj();
  DwarfLine.reset(new DWARFDebugLine);
  DWARFDataExtractor LineData(Obj, Obj.getLineSection(), Config->IsLE,
                              Config->Wordsize);

  for (std::unique_ptr<DWARFCompileUnit> &CU : Dwarf->compile_units()) {
    const DWARFDebugLine::LineTable *LT = Dwarf->getLineTableForUnit(CU.get());
    if (!LT)
      continue;
    LineTables.push_back(LT);

    // Loop over variable records and insert them to VariableLoc.
    for (const auto &Entry : CU->dies()) {
      DWARFDie Die(CU.get(), &Entry);
      // Skip all tags that are not variables.
      if (Die.getTag() != dwarf::DW_TAG_variable)
        continue;

      // Skip if a local variable because we don't need them for generating
      // error messages. In general, only non-local symbols can fail to be
      // linked.
      if (!dwarf::toUnsigned(Die.find(dwarf::DW_AT_external), 0))
        continue;

      // Get the source filename index for the variable.
      unsigned File = dwarf::toUnsigned(Die.find(dwarf::DW_AT_decl_file), 0);
      if (!LT->hasFileAtIndex(File))
        continue;

      // Get the line number on which the variable is declared.
      unsigned Line = dwarf::toUnsigned(Die.find(dwarf::DW_AT_decl_line), 0);

      // Get the name of the variable and add the collected information to
      // VariableLoc. Usually Name is non-empty, but it can be empty if the
      // input object file lacks some debug info.
      StringRef Name = dwarf::toString(Die.find(dwarf::DW_AT_name), "");
      if (!Name.empty())
        VariableLoc.insert({Name, {LT, File, Line}});
    }
  }
}

// Returns the pair of file name and line number describing location of data
// object (variable, array, etc) definition.
template <class ELFT>
Optional<std::pair<std::string, unsigned>>
ObjFile<ELFT>::getVariableLoc(StringRef Name) {
  llvm::call_once(InitDwarfLine, [this]() { initializeDwarf(); });

  // Return if we have no debug information about data object.
  auto It = VariableLoc.find(Name);
  if (It == VariableLoc.end())
    return None;

  // Take file name string from line table.
  std::string FileName;
  if (!It->second.LT->getFileNameByIndex(
          It->second.File, nullptr,
          DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath, FileName))
    return None;

  return std::make_pair(FileName, It->second.Line);
}

// Returns source line information for a given offset
// using DWARF debug info.
template <class ELFT>
Optional<DILineInfo> ObjFile<ELFT>::getDILineInfo(InputSectionBase *S,
                                                  uint64_t Offset) {
  llvm::call_once(InitDwarfLine, [this]() { initializeDwarf(); });

  // Use fake address calcuated by adding section file offset and offset in
  // section. See comments for ObjectInfo class.
  DILineInfo Info;
  for (const llvm::DWARFDebugLine::LineTable *LT : LineTables)
    if (LT->getFileLineInfoForAddress(
            S->getOffsetInFile() + Offset, nullptr,
            DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath, Info))
      return Info;
  return None;
}

// Returns source line information for a given offset using DWARF debug info.
template <class ELFT>
std::string ObjFile<ELFT>::getLineInfo(InputSectionBase *S, uint64_t Offset) {
  if (Optional<DILineInfo> Info = getDILineInfo(S, Offset))
    return Info->FileName + ":" + std::to_string(Info->Line);
  return "";
}

// Returns "<internal>", "foo.a(bar.o)" or "baz.o".
std::string lld::toString(const InputFile *F) {
  if (!F)
    return "<internal>";

  if (F->ToStringCache.empty()) {
    if (F->ArchiveName.empty())
      F->ToStringCache = F->getName();
    else
      F->ToStringCache = (F->ArchiveName + "(" + F->getName() + ")").str();
  }
  return F->ToStringCache;
}

template <class ELFT>
ELFFileBase<ELFT>::ELFFileBase(Kind K, MemoryBufferRef MB) : InputFile(K, MB) {
  if (ELFT::TargetEndianness == support::little)
    EKind = ELFT::Is64Bits ? ELF64LEKind : ELF32LEKind;
  else
    EKind = ELFT::Is64Bits ? ELF64BEKind : ELF32BEKind;

  EMachine = getObj().getHeader()->e_machine;
  OSABI = getObj().getHeader()->e_ident[llvm::ELF::EI_OSABI];
}

template <class ELFT>
typename ELFT::SymRange ELFFileBase<ELFT>::getGlobalELFSyms() {
  return makeArrayRef(ELFSyms.begin() + FirstNonLocal, ELFSyms.end());
}

template <class ELFT>
uint32_t ELFFileBase<ELFT>::getSectionIndex(const Elf_Sym &Sym) const {
  return CHECK(getObj().getSectionIndex(&Sym, ELFSyms, SymtabSHNDX), this);
}

template <class ELFT>
void ELFFileBase<ELFT>::initSymtab(ArrayRef<Elf_Shdr> Sections,
                                   const Elf_Shdr *Symtab) {
  FirstNonLocal = Symtab->sh_info;
  ELFSyms = CHECK(getObj().symbols(Symtab), this);
  if (FirstNonLocal == 0 || FirstNonLocal > ELFSyms.size())
    fatal(toString(this) + ": invalid sh_info in symbol table");

  StringTable =
      CHECK(getObj().getStringTableForSymtab(*Symtab, Sections), this);
}

template <class ELFT>
ObjFile<ELFT>::ObjFile(MemoryBufferRef M, StringRef ArchiveName)
    : ELFFileBase<ELFT>(Base::ObjKind, M) {
  this->ArchiveName = ArchiveName;
}

template <class ELFT> ArrayRef<Symbol *> ObjFile<ELFT>::getLocalSymbols() {
  if (this->Symbols.empty())
    return {};
  return makeArrayRef(this->Symbols).slice(1, this->FirstNonLocal - 1);
}

template <class ELFT>
void ObjFile<ELFT>::parse(DenseSet<CachedHashStringRef> &ComdatGroups) {
  // Read section and symbol tables.
  initializeSections(ComdatGroups);
  initializeSymbols();
}

// Sections with SHT_GROUP and comdat bits define comdat section groups.
// They are identified and deduplicated by group name. This function
// returns a group name.
template <class ELFT>
StringRef ObjFile<ELFT>::getShtGroupSignature(ArrayRef<Elf_Shdr> Sections,
                                              const Elf_Shdr &Sec) {
  // Group signatures are stored as symbol names in object files.
  // sh_info contains a symbol index, so we fetch a symbol and read its name.
  if (this->ELFSyms.empty())
    this->initSymtab(
        Sections, CHECK(object::getSection<ELFT>(Sections, Sec.sh_link), this));

  const Elf_Sym *Sym =
      CHECK(object::getSymbol<ELFT>(this->ELFSyms, Sec.sh_info), this);
  StringRef Signature = CHECK(Sym->getName(this->StringTable), this);

  // As a special case, if a symbol is a section symbol and has no name,
  // we use a section name as a signature.
  //
  // Such SHT_GROUP sections are invalid from the perspective of the ELF
  // standard, but GNU gold 1.14 (the newest version as of July 2017) or
  // older produce such sections as outputs for the -r option, so we need
  // a bug-compatibility.
  if (Signature.empty() && Sym->getType() == STT_SECTION)
    return getSectionName(Sec);
  return Signature;
}

template <class ELFT>
ArrayRef<typename ObjFile<ELFT>::Elf_Word>
ObjFile<ELFT>::getShtGroupEntries(const Elf_Shdr &Sec) {
  const ELFFile<ELFT> &Obj = this->getObj();
  ArrayRef<Elf_Word> Entries =
      CHECK(Obj.template getSectionContentsAsArray<Elf_Word>(&Sec), this);
  if (Entries.empty() || Entries[0] != GRP_COMDAT)
    fatal(toString(this) + ": unsupported SHT_GROUP format");
  return Entries.slice(1);
}

template <class ELFT> bool ObjFile<ELFT>::shouldMerge(const Elf_Shdr &Sec) {
  // On a regular link we don't merge sections if -O0 (default is -O1). This
  // sometimes makes the linker significantly faster, although the output will
  // be bigger.
  //
  // Doing the same for -r would create a problem as it would combine sections
  // with different sh_entsize. One option would be to just copy every SHF_MERGE
  // section as is to the output. While this would produce a valid ELF file with
  // usable SHF_MERGE sections, tools like (llvm-)?dwarfdump get confused when
  // they see two .debug_str. We could have separate logic for combining
  // SHF_MERGE sections based both on their name and sh_entsize, but that seems
  // to be more trouble than it is worth. Instead, we just use the regular (-O1)
  // logic for -r.
  if (Config->Optimize == 0 && !Config->Relocatable)
    return false;

  // A mergeable section with size 0 is useless because they don't have
  // any data to merge. A mergeable string section with size 0 can be
  // argued as invalid because it doesn't end with a null character.
  // We'll avoid a mess by handling them as if they were non-mergeable.
  if (Sec.sh_size == 0)
    return false;

  // Check for sh_entsize. The ELF spec is not clear about the zero
  // sh_entsize. It says that "the member [sh_entsize] contains 0 if
  // the section does not hold a table of fixed-size entries". We know
  // that Rust 1.13 produces a string mergeable section with a zero
  // sh_entsize. Here we just accept it rather than being picky about it.
  uint64_t EntSize = Sec.sh_entsize;
  if (EntSize == 0)
    return false;
  if (Sec.sh_size % EntSize)
    fatal(toString(this) +
          ": SHF_MERGE section size must be a multiple of sh_entsize");

  uint64_t Flags = Sec.sh_flags;
  if (!(Flags & SHF_MERGE))
    return false;
  if (Flags & SHF_WRITE)
    fatal(toString(this) + ": writable SHF_MERGE section is not supported");

  return true;
}

template <class ELFT>
void ObjFile<ELFT>::initializeSections(
    DenseSet<CachedHashStringRef> &ComdatGroups) {
  const ELFFile<ELFT> &Obj = this->getObj();

  ArrayRef<Elf_Shdr> ObjSections = CHECK(this->getObj().sections(), this);
  uint64_t Size = ObjSections.size();
  this->Sections.resize(Size);
  this->SectionStringTable =
      CHECK(Obj.getSectionStringTable(ObjSections), this);

  for (size_t I = 0, E = ObjSections.size(); I < E; I++) {
    if (this->Sections[I] == &InputSection::Discarded)
      continue;
    const Elf_Shdr &Sec = ObjSections[I];

    // SHF_EXCLUDE'ed sections are discarded by the linker. However,
    // if -r is given, we'll let the final link discard such sections.
    // This is compatible with GNU.
    if ((Sec.sh_flags & SHF_EXCLUDE) && !Config->Relocatable) {
      this->Sections[I] = &InputSection::Discarded;
      continue;
    }

    switch (Sec.sh_type) {
    case SHT_GROUP: {
      // De-duplicate section groups by their signatures.
      StringRef Signature = getShtGroupSignature(ObjSections, Sec);
      bool IsNew = ComdatGroups.insert(CachedHashStringRef(Signature)).second;
      this->Sections[I] = &InputSection::Discarded;

      // If it is a new section group, we want to keep group members.
      // Group leader sections, which contain indices of group members, are
      // discarded because they are useless beyond this point. The only
      // exception is the -r option because in order to produce re-linkable
      // object files, we want to pass through basically everything.
      if (IsNew) {
        if (Config->Relocatable)
          this->Sections[I] = createInputSection(Sec);
        continue;
      }

      // Otherwise, discard group members.
      for (uint32_t SecIndex : getShtGroupEntries(Sec)) {
        if (SecIndex >= Size)
          fatal(toString(this) +
                ": invalid section index in group: " + Twine(SecIndex));
        this->Sections[SecIndex] = &InputSection::Discarded;
      }
      break;
    }
    case SHT_SYMTAB:
      this->initSymtab(ObjSections, &Sec);
      break;
    case SHT_SYMTAB_SHNDX:
      this->SymtabSHNDX = CHECK(Obj.getSHNDXTable(Sec, ObjSections), this);
      break;
    case SHT_STRTAB:
    case SHT_NULL:
      break;
    default:
      this->Sections[I] = createInputSection(Sec);
    }

    // .ARM.exidx sections have a reverse dependency on the InputSection they
    // have a SHF_LINK_ORDER dependency, this is identified by the sh_link.
    if (Sec.sh_flags & SHF_LINK_ORDER) {
      if (Sec.sh_link >= this->Sections.size())
        fatal(toString(this) +
              ": invalid sh_link index: " + Twine(Sec.sh_link));

      InputSectionBase *LinkSec = this->Sections[Sec.sh_link];
      InputSection *IS = cast<InputSection>(this->Sections[I]);
      LinkSec->DependentSections.push_back(IS);
      if (!isa<InputSection>(LinkSec))
        error("a section " + IS->Name +
              " with SHF_LINK_ORDER should not refer a non-regular "
              "section: " +
              toString(LinkSec));
    }
  }
}

// The ARM support in lld makes some use of instructions that are not available
// on all ARM architectures. Namely:
// - Use of BLX instruction for interworking between ARM and Thumb state.
// - Use of the extended Thumb branch encoding in relocation.
// - Use of the MOVT/MOVW instructions in Thumb Thunks.
// The ARM Attributes section contains information about the architecture chosen
// at compile time. We follow the convention that if at least one input object
// is compiled with an architecture that supports these features then lld is
// permitted to use them.
static void updateSupportedARMFeatures(const ARMAttributeParser &Attributes) {
  if (!Attributes.hasAttribute(ARMBuildAttrs::CPU_arch))
    return;
  auto Arch = Attributes.getAttributeValue(ARMBuildAttrs::CPU_arch);
  switch (Arch) {
  case ARMBuildAttrs::Pre_v4:
  case ARMBuildAttrs::v4:
  case ARMBuildAttrs::v4T:
    // Architectures prior to v5 do not support BLX instruction
    break;
  case ARMBuildAttrs::v5T:
  case ARMBuildAttrs::v5TE:
  case ARMBuildAttrs::v5TEJ:
  case ARMBuildAttrs::v6:
  case ARMBuildAttrs::v6KZ:
  case ARMBuildAttrs::v6K:
    Config->ARMHasBlx = true;
    // Architectures used in pre-Cortex processors do not support
    // The J1 = 1 J2 = 1 Thumb branch range extension, with the exception
    // of Architecture v6T2 (arm1156t2-s and arm1156t2f-s) that do.
    break;
  default:
    // All other Architectures have BLX and extended branch encoding
    Config->ARMHasBlx = true;
    Config->ARMJ1J2BranchEncoding = true;
    if (Arch != ARMBuildAttrs::v6_M && Arch != ARMBuildAttrs::v6S_M)
      // All Architectures used in Cortex processors with the exception
      // of v6-M and v6S-M have the MOVT and MOVW instructions.
      Config->ARMHasMovtMovw = true;
    break;
  }
}

template <class ELFT>
InputSectionBase *ObjFile<ELFT>::getRelocTarget(const Elf_Shdr &Sec) {
  uint32_t Idx = Sec.sh_info;
  if (Idx >= this->Sections.size())
    fatal(toString(this) + ": invalid relocated section index: " + Twine(Idx));
  InputSectionBase *Target = this->Sections[Idx];

  // Strictly speaking, a relocation section must be included in the
  // group of the section it relocates. However, LLVM 3.3 and earlier
  // would fail to do so, so we gracefully handle that case.
  if (Target == &InputSection::Discarded)
    return nullptr;

  if (!Target)
    fatal(toString(this) + ": unsupported relocation reference");
  return Target;
}

// Create a regular InputSection class that has the same contents
// as a given section.
static InputSection *toRegularSection(MergeInputSection *Sec) {
  return make<InputSection>(Sec->File, Sec->Flags, Sec->Type, Sec->Alignment,
                            Sec->Data, Sec->Name);
}

template <class ELFT>
InputSectionBase *ObjFile<ELFT>::createInputSection(const Elf_Shdr &Sec) {
  StringRef Name = getSectionName(Sec);

  switch (Sec.sh_type) {
  case SHT_ARM_ATTRIBUTES: {
    if (Config->EMachine != EM_ARM)
      break;
    ARMAttributeParser Attributes;
    ArrayRef<uint8_t> Contents = check(this->getObj().getSectionContents(&Sec));
    Attributes.Parse(Contents, /*isLittle*/ Config->EKind == ELF32LEKind);
    updateSupportedARMFeatures(Attributes);
    // FIXME: Retain the first attribute section we see. The eglibc ARM
    // dynamic loaders require the presence of an attribute section for dlopen
    // to work. In a full implementation we would merge all attribute sections.
    if (InX::ARMAttributes == nullptr) {
      InX::ARMAttributes = make<InputSection>(*this, Sec, Name);
      return InX::ARMAttributes;
    }
    return &InputSection::Discarded;
  }
  case SHT_RELA:
  case SHT_REL: {
    // Find a relocation target section and associate this section with that.
    // Target may have been discarded if it is in a different section group
    // and the group is discarded, even though it's a violation of the
    // spec. We handle that situation gracefully by discarding dangling
    // relocation sections.
    InputSectionBase *Target = getRelocTarget(Sec);
    if (!Target)
      return nullptr;

    // This section contains relocation information.
    // If -r is given, we do not interpret or apply relocation
    // but just copy relocation sections to output.
    if (Config->Relocatable)
      return make<InputSection>(*this, Sec, Name);

    if (Target->FirstRelocation)
      fatal(toString(this) +
            ": multiple relocation sections to one section are not supported");

    // ELF spec allows mergeable sections with relocations, but they are
    // rare, and it is in practice hard to merge such sections by contents,
    // because applying relocations at end of linking changes section
    // contents. So, we simply handle such sections as non-mergeable ones.
    // Degrading like this is acceptable because section merging is optional.
    if (auto *MS = dyn_cast<MergeInputSection>(Target)) {
      Target = toRegularSection(MS);
      this->Sections[Sec.sh_info] = Target;
    }

    if (Sec.sh_type == SHT_RELA) {
      ArrayRef<Elf_Rela> Rels = CHECK(this->getObj().relas(&Sec), this);
      Target->FirstRelocation = Rels.begin();
      Target->NumRelocations = Rels.size();
      Target->AreRelocsRela = true;
    } else {
      ArrayRef<Elf_Rel> Rels = CHECK(this->getObj().rels(&Sec), this);
      Target->FirstRelocation = Rels.begin();
      Target->NumRelocations = Rels.size();
      Target->AreRelocsRela = false;
    }
    assert(isUInt<31>(Target->NumRelocations));

    // Relocation sections processed by the linker are usually removed
    // from the output, so returning `nullptr` for the normal case.
    // However, if -emit-relocs is given, we need to leave them in the output.
    // (Some post link analysis tools need this information.)
    if (Config->EmitRelocs) {
      InputSection *RelocSec = make<InputSection>(*this, Sec, Name);
      // We will not emit relocation section if target was discarded.
      Target->DependentSections.push_back(RelocSec);
      return RelocSec;
    }
    return nullptr;
  }
  }

  // The GNU linker uses .note.GNU-stack section as a marker indicating
  // that the code in the object file does not expect that the stack is
  // executable (in terms of NX bit). If all input files have the marker,
  // the GNU linker adds a PT_GNU_STACK segment to tells the loader to
  // make the stack non-executable. Most object files have this section as
  // of 2017.
  //
  // But making the stack non-executable is a norm today for security
  // reasons. Failure to do so may result in a serious security issue.
  // Therefore, we make LLD always add PT_GNU_STACK unless it is
  // explicitly told to do otherwise (by -z execstack). Because the stack
  // executable-ness is controlled solely by command line options,
  // .note.GNU-stack sections are simply ignored.
  if (Name == ".note.GNU-stack")
    return &InputSection::Discarded;

  // Split stacks is a feature to support a discontiguous stack. At least
  // as of 2017, it seems that the feature is not being used widely.
  // Only GNU gold supports that. We don't. For the details about that,
  // see https://gcc.gnu.org/wiki/SplitStacks
  if (Name == ".note.GNU-split-stack") {
    error(toString(this) +
          ": object file compiled with -fsplit-stack is not supported");
    return &InputSection::Discarded;
  }

  // The linkonce feature is a sort of proto-comdat. Some glibc i386 object
  // files contain definitions of symbol "__x86.get_pc_thunk.bx" in linkonce
  // sections. Drop those sections to avoid duplicate symbol errors.
  // FIXME: This is glibc PR20543, we should remove this hack once that has been
  // fixed for a while.
  if (Name.startswith(".gnu.linkonce."))
    return &InputSection::Discarded;

  // If we are creating a new .build-id section, strip existing .build-id
  // sections so that the output won't have more than one .build-id.
  // This is not usually a problem because input object files normally don't
  // have .build-id sections, but you can create such files by
  // "ld.{bfd,gold,lld} -r --build-id", and we want to guard against it.
  if (Name == ".note.gnu.build-id" && Config->BuildId != BuildIdKind::None)
    return &InputSection::Discarded;

  // The linker merges EH (exception handling) frames and creates a
  // .eh_frame_hdr section for runtime. So we handle them with a special
  // class. For relocatable outputs, they are just passed through.
  if (Name == ".eh_frame" && !Config->Relocatable)
    return make<EhInputSection>(*this, Sec, Name);

  if (shouldMerge(Sec))
    return make<MergeInputSection>(*this, Sec, Name);
  return make<InputSection>(*this, Sec, Name);
}

template <class ELFT>
StringRef ObjFile<ELFT>::getSectionName(const Elf_Shdr &Sec) {
  return CHECK(this->getObj().getSectionName(&Sec, SectionStringTable), this);
}

template <class ELFT> void ObjFile<ELFT>::initializeSymbols() {
  this->Symbols.reserve(this->ELFSyms.size());
  for (const Elf_Sym &Sym : this->ELFSyms)
    this->Symbols.push_back(createSymbol(&Sym));
}

template <class ELFT> Symbol *ObjFile<ELFT>::createSymbol(const Elf_Sym *Sym) {
  int Binding = Sym->getBinding();

  uint32_t SecIdx = this->getSectionIndex(*Sym);
  if (SecIdx >= this->Sections.size())
    fatal(toString(this) + ": invalid section index: " + Twine(SecIdx));

  InputSectionBase *Sec = this->Sections[SecIdx];
  uint8_t StOther = Sym->st_other;
  uint8_t Type = Sym->getType();
  uint64_t Value = Sym->st_value;
  uint64_t Size = Sym->st_size;

  if (Binding == STB_LOCAL) {
    if (Sym->getType() == STT_FILE)
      SourceFile = CHECK(Sym->getName(this->StringTable), this);

    if (this->StringTable.size() <= Sym->st_name)
      fatal(toString(this) + ": invalid symbol name offset");

    StringRefZ Name = this->StringTable.data() + Sym->st_name;
    if (Sym->st_shndx == SHN_UNDEF)
      return make<Undefined>(this, Name, Binding, StOther, Type);

    return make<Defined>(this, Name, Binding, StOther, Type, Value, Size, Sec);
  }

  StringRef Name = CHECK(Sym->getName(this->StringTable), this);

  switch (Sym->st_shndx) {
  case SHN_UNDEF:
    return Symtab->addUndefined<ELFT>(Name, Binding, StOther, Type,
                                      /*CanOmitFromDynSym=*/false, this);
  case SHN_COMMON:
    if (Value == 0 || Value >= UINT32_MAX)
      fatal(toString(this) + ": common symbol '" + Name +
            "' has invalid alignment: " + Twine(Value));
    return Symtab->addCommon(Name, Size, Value, Binding, StOther, Type, *this);
  }

  switch (Binding) {
  default:
    fatal(toString(this) + ": unexpected binding: " + Twine(Binding));
  case STB_GLOBAL:
  case STB_WEAK:
  case STB_GNU_UNIQUE:
    if (Sec == &InputSection::Discarded)
      return Symtab->addUndefined<ELFT>(Name, Binding, StOther, Type,
                                        /*CanOmitFromDynSym=*/false, this);
    return Symtab->addRegular(Name, StOther, Type, Value, Size, Binding, Sec,
                              this);
  }
}

ArchiveFile::ArchiveFile(std::unique_ptr<Archive> &&File)
    : InputFile(ArchiveKind, File->getMemoryBufferRef()),
      File(std::move(File)) {}

template <class ELFT> void ArchiveFile::parse() {
  for (const Archive::Symbol &Sym : File->symbols())
    Symtab->addLazyArchive<ELFT>(Sym.getName(), *this, Sym);
}

// Returns a buffer pointing to a member file containing a given symbol.
std::pair<MemoryBufferRef, uint64_t>
ArchiveFile::getMember(const Archive::Symbol *Sym) {
  Archive::Child C =
      CHECK(Sym->getMember(), toString(this) +
                                  ": could not get the member for symbol " +
                                  Sym->getName());

  if (!Seen.insert(C.getChildOffset()).second)
    return {MemoryBufferRef(), 0};

  MemoryBufferRef Ret =
      CHECK(C.getMemoryBufferRef(),
            toString(this) +
                ": could not get the buffer for the member defining symbol " +
                Sym->getName());

  if (C.getParent()->isThin() && Tar)
    Tar->append(relativeToRoot(CHECK(C.getFullName(), this)), Ret.getBuffer());
  if (C.getParent()->isThin())
    return {Ret, 0};
  return {Ret, C.getChildOffset()};
}

template <class ELFT>
SharedFile<ELFT>::SharedFile(MemoryBufferRef M, StringRef DefaultSoName)
    : ELFFileBase<ELFT>(Base::SharedKind, M), SoName(DefaultSoName),
      IsNeeded(!Config->AsNeeded) {}

// Partially parse the shared object file so that we can call
// getSoName on this object.
template <class ELFT> void SharedFile<ELFT>::parseSoName() {
  const Elf_Shdr *DynamicSec = nullptr;
  const ELFFile<ELFT> Obj = this->getObj();
  ArrayRef<Elf_Shdr> Sections = CHECK(Obj.sections(), this);

  // Search for .dynsym, .dynamic, .symtab, .gnu.version and .gnu.version_d.
  for (const Elf_Shdr &Sec : Sections) {
    switch (Sec.sh_type) {
    default:
      continue;
    case SHT_DYNSYM:
      this->initSymtab(Sections, &Sec);
      break;
    case SHT_DYNAMIC:
      DynamicSec = &Sec;
      break;
    case SHT_SYMTAB_SHNDX:
      this->SymtabSHNDX = CHECK(Obj.getSHNDXTable(Sec, Sections), this);
      break;
    case SHT_GNU_versym:
      this->VersymSec = &Sec;
      break;
    case SHT_GNU_verdef:
      this->VerdefSec = &Sec;
      break;
    }
  }

  if (this->VersymSec && this->ELFSyms.empty())
    error("SHT_GNU_versym should be associated with symbol table");

  // Search for a DT_SONAME tag to initialize this->SoName.
  if (!DynamicSec)
    return;
  ArrayRef<Elf_Dyn> Arr =
      CHECK(Obj.template getSectionContentsAsArray<Elf_Dyn>(DynamicSec), this);
  for (const Elf_Dyn &Dyn : Arr) {
    if (Dyn.d_tag == DT_SONAME) {
      uint64_t Val = Dyn.getVal();
      if (Val >= this->StringTable.size())
        fatal(toString(this) + ": invalid DT_SONAME entry");
      SoName = this->StringTable.data() + Val;
      return;
    }
  }
}

// Parses ".gnu.version" section which is a parallel array for the symbol table.
// If a given file doesn't have ".gnu.version" section, returns VER_NDX_GLOBAL.
template <class ELFT> std::vector<uint32_t> SharedFile<ELFT>::parseVersyms() {
  size_t Size = this->ELFSyms.size() - this->FirstNonLocal;
  if (!VersymSec)
    return std::vector<uint32_t>(Size, VER_NDX_GLOBAL);

  const char *Base = this->MB.getBuffer().data();
  const Elf_Versym *Versym =
      reinterpret_cast<const Elf_Versym *>(Base + VersymSec->sh_offset) +
      this->FirstNonLocal;

  std::vector<uint32_t> Ret(Size);
  for (size_t I = 0; I < Size; ++I)
    Ret[I] = Versym[I].vs_index;
  return Ret;
}

// Parse the version definitions in the object file if present. Returns a vector
// whose nth element contains a pointer to the Elf_Verdef for version identifier
// n. Version identifiers that are not definitions map to nullptr.
template <class ELFT>
std::vector<const typename ELFT::Verdef *> SharedFile<ELFT>::parseVerdefs() {
  if (!VerdefSec)
    return {};

  // We cannot determine the largest verdef identifier without inspecting
  // every Elf_Verdef, but both bfd and gold assign verdef identifiers
  // sequentially starting from 1, so we predict that the largest identifier
  // will be VerdefCount.
  unsigned VerdefCount = VerdefSec->sh_info;
  std::vector<const Elf_Verdef *> Verdefs(VerdefCount + 1);

  // Build the Verdefs array by following the chain of Elf_Verdef objects
  // from the start of the .gnu.version_d section.
  const char *Base = this->MB.getBuffer().data();
  const char *Verdef = Base + VerdefSec->sh_offset;
  for (unsigned I = 0; I != VerdefCount; ++I) {
    auto *CurVerdef = reinterpret_cast<const Elf_Verdef *>(Verdef);
    Verdef += CurVerdef->vd_next;
    unsigned VerdefIndex = CurVerdef->vd_ndx;
    if (Verdefs.size() <= VerdefIndex)
      Verdefs.resize(VerdefIndex + 1);
    Verdefs[VerdefIndex] = CurVerdef;
  }

  return Verdefs;
}

// We do not usually care about alignments of data in shared object
// files because the loader takes care of it. However, if we promote a
// DSO symbol to point to .bss due to copy relocation, we need to keep
// the original alignment requirements. We infer it in this function.
template <class ELFT>
uint32_t SharedFile<ELFT>::getAlignment(ArrayRef<Elf_Shdr> Sections,
                                        const Elf_Sym &Sym) {
  uint64_t Ret = 1;
  if (Sym.st_value)
    Ret = 1ULL << countTrailingZeros((uint64_t)Sym.st_value);
  if (0 < Sym.st_shndx && Sym.st_shndx < Sections.size())
    Ret = std::min<uint64_t>(Ret, Sections[Sym.st_shndx].sh_addralign);

  if (Ret > UINT32_MAX)
    error(toString(this) + ": alignment too large: " +
          CHECK(Sym.getName(this->StringTable), this));
  return Ret;
}

// Fully parse the shared object file. This must be called after parseSoName().
//
// This function parses symbol versions. If a DSO has version information,
// the file has a ".gnu.version_d" section which contains symbol version
// definitions. Each symbol is associated to one version through a table in
// ".gnu.version" section. That table is a parallel array for the symbol
// table, and each table entry contains an index in ".gnu.version_d".
//
// The special index 0 is reserved for VERF_NDX_LOCAL and 1 is for
// VER_NDX_GLOBAL. There's no table entry for these special versions in
// ".gnu.version_d".
//
// The file format for symbol versioning is perhaps a bit more complicated
// than necessary, but you can easily understand the code if you wrap your
// head around the data structure described above.
template <class ELFT> void SharedFile<ELFT>::parseRest() {
  Verdefs = parseVerdefs();                       // parse .gnu.version_d
  std::vector<uint32_t> Versyms = parseVersyms(); // parse .gnu.version
  ArrayRef<Elf_Shdr> Sections = CHECK(this->getObj().sections(), this);

  // Add symbols to the symbol table.
  ArrayRef<Elf_Sym> Syms = this->getGlobalELFSyms();
  for (size_t I = 0; I < Syms.size(); ++I) {
    const Elf_Sym &Sym = Syms[I];

    StringRef Name = CHECK(Sym.getName(this->StringTable), this);
    if (Sym.isUndefined()) {
      Symbol *S = Symtab->addUndefined<ELFT>(Name, Sym.getBinding(),
                                             Sym.st_other, Sym.getType(),
                                             /*CanOmitFromDynSym=*/false, this);
      S->ExportDynamic = true;
      continue;
    }

    // ELF spec requires that all local symbols precede weak or global
    // symbols in each symbol table, and the index of first non-local symbol
    // is stored to sh_info. If a local symbol appears after some non-local
    // symbol, that's a violation of the spec.
    if (Sym.getBinding() == STB_LOCAL) {
      warn("found local symbol '" + Name +
           "' in global part of symbol table in file " + toString(this));
      continue;
    }

    // MIPS BFD linker puts _gp_disp symbol into DSO files and incorrectly
    // assigns VER_NDX_LOCAL to this section global symbol. Here is a
    // workaround for this bug.
    uint32_t Idx = Versyms[I] & ~VERSYM_HIDDEN;
    if (Config->EMachine == EM_MIPS && Idx == VER_NDX_LOCAL &&
        Name == "_gp_disp")
      continue;

    uint64_t Alignment = getAlignment(Sections, Sym);
    if (!(Versyms[I] & VERSYM_HIDDEN))
      Symtab->addShared(Name, *this, Sym, Alignment, Idx);

    // Also add the symbol with the versioned name to handle undefined symbols
    // with explicit versions.
    if (Idx == VER_NDX_GLOBAL)
      continue;

    if (Idx >= Verdefs.size() || Idx == VER_NDX_LOCAL) {
      error("corrupt input file: version definition index " + Twine(Idx) +
            " for symbol " + Name + " is out of bounds\n>>> defined in " +
            toString(this));
      continue;
    }

    StringRef VerName =
        this->StringTable.data() + Verdefs[Idx]->getAux()->vda_name;
    Name = Saver.save(Name + "@" + VerName);
    Symtab->addShared(Name, *this, Sym, Alignment, Idx);
  }
}

static ELFKind getBitcodeELFKind(const Triple &T) {
  if (T.isLittleEndian())
    return T.isArch64Bit() ? ELF64LEKind : ELF32LEKind;
  return T.isArch64Bit() ? ELF64BEKind : ELF32BEKind;
}

static uint8_t getBitcodeMachineKind(StringRef Path, const Triple &T) {
  switch (T.getArch()) {
  case Triple::aarch64:
    return EM_AARCH64;
  case Triple::arm:
  case Triple::thumb:
    return EM_ARM;
  case Triple::avr:
    return EM_AVR;
  case Triple::mips:
  case Triple::mipsel:
  case Triple::mips64:
  case Triple::mips64el:
    return EM_MIPS;
  case Triple::ppc:
    return EM_PPC;
  case Triple::ppc64:
    return EM_PPC64;
  case Triple::x86:
    return T.isOSIAMCU() ? EM_IAMCU : EM_386;
  case Triple::x86_64:
    return EM_X86_64;
  default:
    fatal(Path + ": could not infer e_machine from bitcode target triple " +
          T.str());
  }
}

BitcodeFile::BitcodeFile(MemoryBufferRef MB, StringRef ArchiveName,
                         uint64_t OffsetInArchive)
    : InputFile(BitcodeKind, MB) {
  this->ArchiveName = ArchiveName;

  // Here we pass a new MemoryBufferRef which is identified by ArchiveName
  // (the fully resolved path of the archive) + member name + offset of the
  // member in the archive.
  // ThinLTO uses the MemoryBufferRef identifier to access its internal
  // data structures and if two archives define two members with the same name,
  // this causes a collision which result in only one of the objects being
  // taken into consideration at LTO time (which very likely causes undefined
  // symbols later in the link stage).
  MemoryBufferRef MBRef(MB.getBuffer(),
                        Saver.save(ArchiveName + MB.getBufferIdentifier() +
                                   utostr(OffsetInArchive)));
  Obj = CHECK(lto::InputFile::create(MBRef), this);

  Triple T(Obj->getTargetTriple());
  EKind = getBitcodeELFKind(T);
  EMachine = getBitcodeMachineKind(MB.getBufferIdentifier(), T);
}

static uint8_t mapVisibility(GlobalValue::VisibilityTypes GvVisibility) {
  switch (GvVisibility) {
  case GlobalValue::DefaultVisibility:
    return STV_DEFAULT;
  case GlobalValue::HiddenVisibility:
    return STV_HIDDEN;
  case GlobalValue::ProtectedVisibility:
    return STV_PROTECTED;
  }
  llvm_unreachable("unknown visibility");
}

template <class ELFT>
static Symbol *createBitcodeSymbol(const std::vector<bool> &KeptComdats,
                                   const lto::InputFile::Symbol &ObjSym,
                                   BitcodeFile &F) {
  StringRef NameRef = Saver.save(ObjSym.getName());
  uint32_t Binding = ObjSym.isWeak() ? STB_WEAK : STB_GLOBAL;

  uint8_t Type = ObjSym.isTLS() ? STT_TLS : STT_NOTYPE;
  uint8_t Visibility = mapVisibility(ObjSym.getVisibility());
  bool CanOmitFromDynSym = ObjSym.canBeOmittedFromSymbolTable();

  int C = ObjSym.getComdatIndex();
  if (C != -1 && !KeptComdats[C])
    return Symtab->addUndefined<ELFT>(NameRef, Binding, Visibility, Type,
                                      CanOmitFromDynSym, &F);

  if (ObjSym.isUndefined())
    return Symtab->addUndefined<ELFT>(NameRef, Binding, Visibility, Type,
                                      CanOmitFromDynSym, &F);

  if (ObjSym.isCommon())
    return Symtab->addCommon(NameRef, ObjSym.getCommonSize(),
                             ObjSym.getCommonAlignment(), Binding, Visibility,
                             STT_OBJECT, F);

  return Symtab->addBitcode(NameRef, Binding, Visibility, Type,
                            CanOmitFromDynSym, F);
}

template <class ELFT>
void BitcodeFile::parse(DenseSet<CachedHashStringRef> &ComdatGroups) {
  std::vector<bool> KeptComdats;
  for (StringRef S : Obj->getComdatTable())
    KeptComdats.push_back(ComdatGroups.insert(CachedHashStringRef(S)).second);

  for (const lto::InputFile::Symbol &ObjSym : Obj->symbols())
    Symbols.push_back(createBitcodeSymbol<ELFT>(KeptComdats, ObjSym, *this));
}

static ELFKind getELFKind(MemoryBufferRef MB) {
  unsigned char Size;
  unsigned char Endian;
  std::tie(Size, Endian) = getElfArchType(MB.getBuffer());

  if (Endian != ELFDATA2LSB && Endian != ELFDATA2MSB)
    fatal(MB.getBufferIdentifier() + ": invalid data encoding");
  if (Size != ELFCLASS32 && Size != ELFCLASS64)
    fatal(MB.getBufferIdentifier() + ": invalid file class");

  size_t BufSize = MB.getBuffer().size();
  if ((Size == ELFCLASS32 && BufSize < sizeof(Elf32_Ehdr)) ||
      (Size == ELFCLASS64 && BufSize < sizeof(Elf64_Ehdr)))
    fatal(MB.getBufferIdentifier() + ": file is too short");

  if (Size == ELFCLASS32)
    return (Endian == ELFDATA2LSB) ? ELF32LEKind : ELF32BEKind;
  return (Endian == ELFDATA2LSB) ? ELF64LEKind : ELF64BEKind;
}

void BinaryFile::parse() {
  ArrayRef<uint8_t> Data = toArrayRef(MB.getBuffer());
  auto *Section = make<InputSection>(this, SHF_ALLOC | SHF_WRITE, SHT_PROGBITS,
                                     8, Data, ".data");
  Sections.push_back(Section);

  // For each input file foo that is embedded to a result as a binary
  // blob, we define _binary_foo_{start,end,size} symbols, so that
  // user programs can access blobs by name. Non-alphanumeric
  // characters in a filename are replaced with underscore.
  std::string S = "_binary_" + MB.getBufferIdentifier().str();
  for (size_t I = 0; I < S.size(); ++I)
    if (!isAlnum(S[I]))
      S[I] = '_';

  Symtab->addRegular(Saver.save(S + "_start"), STV_DEFAULT, STT_OBJECT, 0, 0,
                     STB_GLOBAL, Section, nullptr);
  Symtab->addRegular(Saver.save(S + "_end"), STV_DEFAULT, STT_OBJECT,
                     Data.size(), 0, STB_GLOBAL, Section, nullptr);
  Symtab->addRegular(Saver.save(S + "_size"), STV_DEFAULT, STT_OBJECT,
                     Data.size(), 0, STB_GLOBAL, nullptr, nullptr);
}

static bool isBitcode(MemoryBufferRef MB) {
  using namespace sys::fs;
  return identify_magic(MB.getBuffer()) == file_magic::bitcode;
}

InputFile *elf::createObjectFile(MemoryBufferRef MB, StringRef ArchiveName,
                                 uint64_t OffsetInArchive) {
  if (isBitcode(MB))
    return make<BitcodeFile>(MB, ArchiveName, OffsetInArchive);

  switch (getELFKind(MB)) {
  case ELF32LEKind:
    return make<ObjFile<ELF32LE>>(MB, ArchiveName);
  case ELF32BEKind:
    return make<ObjFile<ELF32BE>>(MB, ArchiveName);
  case ELF64LEKind:
    return make<ObjFile<ELF64LE>>(MB, ArchiveName);
  case ELF64BEKind:
    return make<ObjFile<ELF64BE>>(MB, ArchiveName);
  default:
    llvm_unreachable("getELFKind");
  }
}

InputFile *elf::createSharedFile(MemoryBufferRef MB, StringRef DefaultSoName) {
  switch (getELFKind(MB)) {
  case ELF32LEKind:
    return make<SharedFile<ELF32LE>>(MB, DefaultSoName);
  case ELF32BEKind:
    return make<SharedFile<ELF32BE>>(MB, DefaultSoName);
  case ELF64LEKind:
    return make<SharedFile<ELF64LE>>(MB, DefaultSoName);
  case ELF64BEKind:
    return make<SharedFile<ELF64BE>>(MB, DefaultSoName);
  default:
    llvm_unreachable("getELFKind");
  }
}

MemoryBufferRef LazyObjFile::getBuffer() {
  if (Seen)
    return MemoryBufferRef();
  Seen = true;
  return MB;
}

InputFile *LazyObjFile::fetch() {
  MemoryBufferRef MBRef = getBuffer();
  if (MBRef.getBuffer().empty())
    return nullptr;
  return createObjectFile(MBRef, ArchiveName, OffsetInArchive);
}

template <class ELFT> void LazyObjFile::parse() {
  // A lazy object file wraps either a bitcode file or an ELF file.
  if (isBitcode(this->MB)) {
    std::unique_ptr<lto::InputFile> Obj =
        CHECK(lto::InputFile::create(this->MB), this);
    for (const lto::InputFile::Symbol &Sym : Obj->symbols())
      if (!Sym.isUndefined())
        Symtab->addLazyObject<ELFT>(Saver.save(Sym.getName()), *this);
    return;
  }

  switch (getELFKind(this->MB)) {
  case ELF32LEKind:
    addElfSymbols<ELF32LE>();
    return;
  case ELF32BEKind:
    addElfSymbols<ELF32BE>();
    return;
  case ELF64LEKind:
    addElfSymbols<ELF64LE>();
    return;
  case ELF64BEKind:
    addElfSymbols<ELF64BE>();
    return;
  default:
    llvm_unreachable("getELFKind");
  }
}

template <class ELFT> void LazyObjFile::addElfSymbols() {
  ELFFile<ELFT> Obj = check(ELFFile<ELFT>::create(MB.getBuffer()));
  ArrayRef<typename ELFT::Shdr> Sections = CHECK(Obj.sections(), this);

  for (const typename ELFT::Shdr &Sec : Sections) {
    if (Sec.sh_type != SHT_SYMTAB)
      continue;

    typename ELFT::SymRange Syms = CHECK(Obj.symbols(&Sec), this);
    uint32_t FirstNonLocal = Sec.sh_info;
    StringRef StringTable =
        CHECK(Obj.getStringTableForSymtab(Sec, Sections), this);

    for (const typename ELFT::Sym &Sym : Syms.slice(FirstNonLocal))
      if (Sym.st_shndx != SHN_UNDEF)
        Symtab->addLazyObject<ELFT>(CHECK(Sym.getName(StringTable), this),
                                    *this);
    return;
  }
}

// This is for --just-symbols.
//
// This option allows you to link your output against other existing
// program, so that if you load both your program and the other program
// into memory, your output can use program's symbols.
//
// What we are doing here is to read defined symbols from a given ELF
// file and add them as absolute symbols.
template <class ELFT> void elf::readJustSymbolsFile(MemoryBufferRef MB) {
  typedef typename ELFT::Shdr Elf_Shdr;
  typedef typename ELFT::Sym Elf_Sym;
  typedef typename ELFT::SymRange Elf_Sym_Range;

  StringRef ObjName = MB.getBufferIdentifier();
  ELFFile<ELFT> Obj = check(ELFFile<ELFT>::create(MB.getBuffer()));
  ArrayRef<Elf_Shdr> Sections = CHECK(Obj.sections(), ObjName);

  for (const Elf_Shdr &Sec : Sections) {
    if (Sec.sh_type != SHT_SYMTAB)
      continue;

    Elf_Sym_Range Syms = CHECK(Obj.symbols(&Sec), ObjName);
    uint32_t FirstNonLocal = Sec.sh_info;
    StringRef StringTable =
        CHECK(Obj.getStringTableForSymtab(Sec, Sections), ObjName);

    for (const Elf_Sym &Sym : Syms.slice(FirstNonLocal))
      if (Sym.st_shndx != SHN_UNDEF)
        Symtab->addRegular(CHECK(Sym.getName(StringTable), ObjName),
                           Sym.st_other, Sym.getType(), Sym.st_value,
                           Sym.st_size, Sym.getBinding(), nullptr, nullptr);
    return;
  }
}

template void ArchiveFile::parse<ELF32LE>();
template void ArchiveFile::parse<ELF32BE>();
template void ArchiveFile::parse<ELF64LE>();
template void ArchiveFile::parse<ELF64BE>();

template void BitcodeFile::parse<ELF32LE>(DenseSet<CachedHashStringRef> &);
template void BitcodeFile::parse<ELF32BE>(DenseSet<CachedHashStringRef> &);
template void BitcodeFile::parse<ELF64LE>(DenseSet<CachedHashStringRef> &);
template void BitcodeFile::parse<ELF64BE>(DenseSet<CachedHashStringRef> &);

template void LazyObjFile::parse<ELF32LE>();
template void LazyObjFile::parse<ELF32BE>();
template void LazyObjFile::parse<ELF64LE>();
template void LazyObjFile::parse<ELF64BE>();

template class elf::ELFFileBase<ELF32LE>;
template class elf::ELFFileBase<ELF32BE>;
template class elf::ELFFileBase<ELF64LE>;
template class elf::ELFFileBase<ELF64BE>;

template class elf::ObjFile<ELF32LE>;
template class elf::ObjFile<ELF32BE>;
template class elf::ObjFile<ELF64LE>;
template class elf::ObjFile<ELF64BE>;

template class elf::SharedFile<ELF32LE>;
template class elf::SharedFile<ELF32BE>;
template class elf::SharedFile<ELF64LE>;
template class elf::SharedFile<ELF64BE>;

template void elf::readJustSymbolsFile<ELF32LE>(MemoryBufferRef);
template void elf::readJustSymbolsFile<ELF32BE>(MemoryBufferRef);
template void elf::readJustSymbolsFile<ELF64LE>(MemoryBufferRef);
template void elf::readJustSymbolsFile<ELF64BE>(MemoryBufferRef);
