// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_win32_x86.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "courgette/base_test_unittest.h"

class DisassemblerWin32X86Test : public BaseTest {
 public:
  void TestExe() const;
  void TestExe64() const;
  void TestResourceDll() const;
};

void DisassemblerWin32X86Test::TestExe() const {
  std::string file1 = FileContents("setup1.exe");

  scoped_ptr<courgette::DisassemblerWin32X86> disassembler(
      new courgette::DisassemblerWin32X86(file1.c_str(), file1.length()));

  bool can_parse_header = disassembler->ParseHeader();
  EXPECT_TRUE(can_parse_header);

  // The executable is the whole file, not 'embedded' with the file
  EXPECT_EQ(file1.length(), disassembler->length());

  EXPECT_TRUE(disassembler->ok());
  EXPECT_TRUE(disassembler->has_text_section());
  EXPECT_EQ(449536U, disassembler->size_of_code());
  EXPECT_TRUE(disassembler->is_32bit());
  EXPECT_EQ(courgette::DisassemblerWin32X86::SectionName(
      disassembler->RVAToSection(0x00401234 - 0x00400000)),
      std::string(".text"));

  EXPECT_EQ(0, disassembler->RVAToFileOffset(0));
  EXPECT_EQ(1024, disassembler->RVAToFileOffset(4096));
  EXPECT_EQ(46928, disassembler->RVAToFileOffset(50000));

  std::vector<courgette::RVA> relocs;
  bool can_parse_relocs = disassembler->ParseRelocs(&relocs);
  EXPECT_TRUE(can_parse_relocs);
  EXPECT_TRUE(base::STLIsSorted(relocs));

  const uint8* offset_p = disassembler->OffsetToPointer(0);
  EXPECT_EQ(reinterpret_cast<const void*>(file1.c_str()),
            reinterpret_cast<const void*>(offset_p));
  EXPECT_EQ('M', offset_p[0]);
  EXPECT_EQ('Z', offset_p[1]);

  const uint8* rva_p = disassembler->RVAToPointer(0);
  EXPECT_EQ(reinterpret_cast<const void*>(file1.c_str()),
            reinterpret_cast<const void*>(rva_p));
  EXPECT_EQ('M', rva_p[0]);
  EXPECT_EQ('Z', rva_p[1]);
}

void DisassemblerWin32X86Test::TestExe64() const {
  std::string file1 = FileContents("pe-64.exe");

  scoped_ptr<courgette::DisassemblerWin32X86> disassembler(
      new courgette::DisassemblerWin32X86(file1.c_str(), file1.length()));

  bool can_parse_header = disassembler->ParseHeader();
  EXPECT_FALSE(can_parse_header);

  // The executable is the whole file, not 'embedded' with the file
  EXPECT_EQ(file1.length(), disassembler->length());

  EXPECT_FALSE(disassembler->ok());
  EXPECT_TRUE(disassembler->has_text_section());
  EXPECT_EQ(43008U, disassembler->size_of_code());
  EXPECT_FALSE(disassembler->is_32bit());
}

void DisassemblerWin32X86Test::TestResourceDll() const {
  std::string file1 = FileContents("en-US.dll");

  scoped_ptr<courgette::DisassemblerWin32X86> disassembler(
      new courgette::DisassemblerWin32X86(file1.c_str(), file1.length()));

  bool can_parse_header = disassembler->ParseHeader();
  EXPECT_FALSE(can_parse_header);

  // The executable is the whole file, not 'embedded' with the file
  EXPECT_EQ(file1.length(), disassembler->length());

  EXPECT_FALSE(disassembler->ok());
  EXPECT_FALSE(disassembler->has_text_section());
  EXPECT_EQ(0U, disassembler->size_of_code());
  EXPECT_TRUE(disassembler->is_32bit());
}

TEST_F(DisassemblerWin32X86Test, All) {
  TestExe();
  TestExe64();
  TestResourceDll();
}
