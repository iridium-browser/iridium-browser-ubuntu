//===- OutputSections.cpp -------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "OutputSections.h"
#include "InputChunks.h"
#include "InputFiles.h"
#include "OutputSegment.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Threads.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/LEB128.h"

#define DEBUG_TYPE "lld"

using namespace llvm;
using namespace llvm::wasm;
using namespace lld;
using namespace lld::wasm;

static StringRef sectionTypeToString(uint32_t SectionType) {
  switch (SectionType) {
  case WASM_SEC_CUSTOM:
    return "CUSTOM";
  case WASM_SEC_TYPE:
    return "TYPE";
  case WASM_SEC_IMPORT:
    return "IMPORT";
  case WASM_SEC_FUNCTION:
    return "FUNCTION";
  case WASM_SEC_TABLE:
    return "TABLE";
  case WASM_SEC_MEMORY:
    return "MEMORY";
  case WASM_SEC_GLOBAL:
    return "GLOBAL";
  case WASM_SEC_EXPORT:
    return "EXPORT";
  case WASM_SEC_START:
    return "START";
  case WASM_SEC_ELEM:
    return "ELEM";
  case WASM_SEC_CODE:
    return "CODE";
  case WASM_SEC_DATA:
    return "DATA";
  default:
    fatal("invalid section type");
  }
}

std::string lld::toString(const OutputSection &Section) {
  std::string rtn = Section.getSectionName();
  if (!Section.Name.empty())
    rtn += "(" + Section.Name + ")";
  return rtn;
}

std::string OutputSection::getSectionName() const {
  return sectionTypeToString(Type);
}

std::string SubSection::getSectionName() const {
  return std::string("subsection <type=") + std::to_string(Type) + ">";
}

void OutputSection::createHeader(size_t BodySize) {
  raw_string_ostream OS(Header);
  debugWrite(OS.tell(), "section type [" + Twine(getSectionName()) + "]");
  encodeULEB128(Type, OS);
  writeUleb128(OS, BodySize, "section size");
  OS.flush();
  log("createHeader: " + toString(*this) + " body=" + Twine(BodySize) +
      " total=" + Twine(getSize()));
}

CodeSection::CodeSection(ArrayRef<InputFunction *> Functions)
    : OutputSection(WASM_SEC_CODE), Functions(Functions) {
  assert(Functions.size() > 0);

  raw_string_ostream OS(CodeSectionHeader);
  writeUleb128(OS, Functions.size(), "function count");
  OS.flush();
  BodySize = CodeSectionHeader.size();

  for (InputChunk *Func : Functions) {
    Func->OutputOffset = BodySize;
    BodySize += Func->getSize();
  }

  createHeader(BodySize);
}

void CodeSection::writeTo(uint8_t *Buf) {
  log("writing " + toString(*this));
  log(" size=" + Twine(getSize()));
  log(" headersize=" + Twine(Header.size()));
  log(" codeheadersize=" + Twine(CodeSectionHeader.size()));
  Buf += Offset;

  // Write section header
  memcpy(Buf, Header.data(), Header.size());
  Buf += Header.size();

  uint8_t *ContentsStart = Buf;

  // Write code section headers
  memcpy(Buf, CodeSectionHeader.data(), CodeSectionHeader.size());
  Buf += CodeSectionHeader.size();

  // Write code section bodies
  parallelForEach(Functions, [ContentsStart](const InputChunk *Chunk) {
    Chunk->writeTo(ContentsStart);
  });
}

uint32_t CodeSection::numRelocations() const {
  uint32_t Count = 0;
  for (const InputChunk *Func : Functions)
    Count += Func->NumRelocations();
  return Count;
}

void CodeSection::writeRelocations(raw_ostream &OS) const {
  for (const InputChunk *C : Functions)
    C->writeRelocations(OS);
}

DataSection::DataSection(ArrayRef<OutputSegment *> Segments)
    : OutputSection(WASM_SEC_DATA), Segments(Segments) {
  raw_string_ostream OS(DataSectionHeader);

  writeUleb128(OS, Segments.size(), "data segment count");
  OS.flush();
  BodySize = DataSectionHeader.size();

  for (OutputSegment *Segment : Segments) {
    raw_string_ostream OS(Segment->Header);
    writeUleb128(OS, 0, "memory index");
    writeUleb128(OS, WASM_OPCODE_I32_CONST, "opcode:i32const");
    writeSleb128(OS, Segment->StartVA, "memory offset");
    writeUleb128(OS, WASM_OPCODE_END, "opcode:end");
    writeUleb128(OS, Segment->Size, "segment size");
    OS.flush();
    Segment->setSectionOffset(BodySize);
    BodySize += Segment->Header.size() + Segment->Size;
    log("Data segment: size=" + Twine(Segment->Size));
    for (InputSegment *InputSeg : Segment->InputSegments)
      InputSeg->OutputOffset = Segment->getSectionOffset() +
                               Segment->Header.size() +
                               InputSeg->OutputSegmentOffset;
  }

  createHeader(BodySize);
}

void DataSection::writeTo(uint8_t *Buf) {
  log("writing " + toString(*this) + " size=" + Twine(getSize()) +
      " body=" + Twine(BodySize));
  Buf += Offset;

  // Write section header
  memcpy(Buf, Header.data(), Header.size());
  Buf += Header.size();

  uint8_t *ContentsStart = Buf;

  // Write data section headers
  memcpy(Buf, DataSectionHeader.data(), DataSectionHeader.size());

  parallelForEach(Segments, [ContentsStart](const OutputSegment *Segment) {
    // Write data segment header
    uint8_t *SegStart = ContentsStart + Segment->getSectionOffset();
    memcpy(SegStart, Segment->Header.data(), Segment->Header.size());

    // Write segment data payload
    for (const InputChunk *Chunk : Segment->InputSegments)
      Chunk->writeTo(ContentsStart);
  });
}

uint32_t DataSection::numRelocations() const {
  uint32_t Count = 0;
  for (const OutputSegment *Seg : Segments)
    for (const InputChunk *InputSeg : Seg->InputSegments)
      Count += InputSeg->NumRelocations();
  return Count;
}

void DataSection::writeRelocations(raw_ostream &OS) const {
  for (const OutputSegment *Seg : Segments)
    for (const InputChunk *C : Seg->InputSegments)
      C->writeRelocations(OS);
}
