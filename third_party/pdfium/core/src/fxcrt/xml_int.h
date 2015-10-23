// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_SRC_FXCRT_XML_INT_H_
#define CORE_SRC_FXCRT_XML_INT_H_

#include "../../include/fxcrt/fx_stream.h"

class CXML_DataBufAcc : public IFX_BufferRead {
 public:
  CXML_DataBufAcc(const uint8_t* pBuffer, size_t size)
      : m_pBuffer(pBuffer), m_dwSize(size), m_dwCurPos(0) {}
  ~CXML_DataBufAcc() override {}

  // IFX_BufferRead
  void Release() override { delete this; }
  FX_BOOL IsEOF() override { return m_dwCurPos >= m_dwSize; }
  FX_FILESIZE GetPosition() override { return (FX_FILESIZE)m_dwCurPos; }
  size_t ReadBlock(void* buffer, size_t size) override { return 0; }
  FX_BOOL ReadNextBlock(FX_BOOL bRestart = FALSE) override {
    if (bRestart) {
      m_dwCurPos = 0;
    }
    if (m_dwCurPos < m_dwSize) {
      m_dwCurPos = m_dwSize;
      return TRUE;
    }
    return FALSE;
  }
  const uint8_t* GetBlockBuffer() override { return m_pBuffer; }
  size_t GetBlockSize() override { return m_dwSize; }
  FX_FILESIZE GetBlockOffset() override { return 0; }

 protected:
  const uint8_t* m_pBuffer;
  size_t m_dwSize;
  size_t m_dwCurPos;
};

#define FX_XMLDATASTREAM_BufferSize (32 * 1024)
class CXML_DataStmAcc : public IFX_BufferRead {
 public:
  CXML_DataStmAcc(IFX_FileRead* pFileRead)
      : m_pFileRead(pFileRead), m_pBuffer(NULL), m_nStart(0), m_dwSize(0) {
    FXSYS_assert(m_pFileRead != NULL);
  }
  ~CXML_DataStmAcc() override { FX_Free(m_pBuffer); }

  void Release() override { delete this; }
  FX_BOOL IsEOF() override {
    return m_nStart + (FX_FILESIZE)m_dwSize >= m_pFileRead->GetSize();
  }
  FX_FILESIZE GetPosition() override {
    return m_nStart + (FX_FILESIZE)m_dwSize;
  }
  size_t ReadBlock(void* buffer, size_t size) override { return 0; }
  FX_BOOL ReadNextBlock(FX_BOOL bRestart = FALSE) override {
    if (bRestart) {
      m_nStart = 0;
    }
    FX_FILESIZE nLength = m_pFileRead->GetSize();
    m_nStart += (FX_FILESIZE)m_dwSize;
    if (m_nStart >= nLength) {
      return FALSE;
    }
    m_dwSize = (size_t)FX_MIN(FX_XMLDATASTREAM_BufferSize, nLength - m_nStart);
    if (!m_pBuffer) {
      m_pBuffer = FX_Alloc(uint8_t, m_dwSize);
    }
    return m_pFileRead->ReadBlock(m_pBuffer, m_nStart, m_dwSize);
  }
  const uint8_t* GetBlockBuffer() override { return (const uint8_t*)m_pBuffer; }
  size_t GetBlockSize() override { return m_dwSize; }
  FX_FILESIZE GetBlockOffset() override { return m_nStart; }

 protected:
  IFX_FileRead* m_pFileRead;
  uint8_t* m_pBuffer;
  FX_FILESIZE m_nStart;
  size_t m_dwSize;
};

class CXML_Parser {
 public:
  ~CXML_Parser();
  IFX_BufferRead* m_pDataAcc;
  FX_BOOL m_bOwnedStream;
  FX_FILESIZE m_nOffset;
  FX_BOOL m_bSaveSpaceChars;
  const uint8_t* m_pBuffer;
  size_t m_dwBufferSize;
  FX_FILESIZE m_nBufferOffset;
  size_t m_dwIndex;
  FX_BOOL Init(uint8_t* pBuffer, size_t size);
  FX_BOOL Init(IFX_FileRead* pFileRead);
  FX_BOOL Init(IFX_BufferRead* pBuffer);
  FX_BOOL Init(FX_BOOL bOwndedStream);
  FX_BOOL ReadNextBlock();
  FX_BOOL IsEOF();
  FX_BOOL HaveAvailData();
  void SkipWhiteSpaces();
  void GetName(CFX_ByteString& space, CFX_ByteString& name);
  void GetAttrValue(CFX_WideString& value);
  FX_DWORD GetCharRef();
  void GetTagName(CFX_ByteString& space,
                  CFX_ByteString& name,
                  FX_BOOL& bEndTag,
                  FX_BOOL bStartTag = FALSE);
  void SkipLiterals(const CFX_ByteStringC& str);
  CXML_Element* ParseElement(CXML_Element* pParent, FX_BOOL bStartTag = FALSE);
  void InsertContentSegment(FX_BOOL bCDATA,
                            const CFX_WideStringC& content,
                            CXML_Element* pElement);
  void InsertCDATASegment(CFX_UTF8Decoder& decoder, CXML_Element* pElement);
};

void FX_XML_SplitQualifiedName(const CFX_ByteStringC& bsFullName,
                               CFX_ByteStringC& bsSpace,
                               CFX_ByteStringC& bsName);

#endif  // CORE_SRC_FXCRT_XML_INT_H_
