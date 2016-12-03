// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/include/xfa_ffapp.h"

#include <algorithm>
#include <utility>

#include "xfa/fgas/font/fgas_stdfontmgr.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fxfa/app/xfa_fwladapter.h"
#include "xfa/fxfa/app/xfa_fwltheme.h"
#include "xfa/fxfa/include/xfa_ffdoc.h"
#include "xfa/fxfa/include/xfa_ffdochandler.h"
#include "xfa/fxfa/include/xfa_ffwidgethandler.h"
#include "xfa/fxfa/include/xfa_fontmgr.h"

CXFA_FileRead::CXFA_FileRead(const CFX_ArrayTemplate<CPDF_Stream*>& streams) {
  int32_t iCount = streams.GetSize();
  for (int32_t i = 0; i < iCount; i++) {
    CPDF_StreamAcc& acc = m_Data.Add();
    acc.LoadAllData(streams[i]);
  }
}

CXFA_FileRead::~CXFA_FileRead() {}

FX_FILESIZE CXFA_FileRead::GetSize() {
  uint32_t dwSize = 0;
  int32_t iCount = m_Data.GetSize();
  for (int32_t i = 0; i < iCount; i++) {
    CPDF_StreamAcc& acc = m_Data[i];
    dwSize += acc.GetSize();
  }
  return dwSize;
}

FX_BOOL CXFA_FileRead::ReadBlock(void* buffer,
                                 FX_FILESIZE offset,
                                 size_t size) {
  int32_t iCount = m_Data.GetSize();
  int32_t index = 0;
  while (index < iCount) {
    CPDF_StreamAcc& acc = m_Data[index];
    FX_FILESIZE dwSize = acc.GetSize();
    if (offset < dwSize) {
      break;
    }
    offset -= dwSize;
    index++;
  }
  while (index < iCount) {
    CPDF_StreamAcc& acc = m_Data[index];
    uint32_t dwSize = acc.GetSize();
    size_t dwRead = std::min(size, static_cast<size_t>(dwSize - offset));
    FXSYS_memcpy(buffer, acc.GetData() + offset, dwRead);
    size -= dwRead;
    if (size == 0) {
      return TRUE;
    }
    buffer = (uint8_t*)buffer + dwRead;
    offset = 0;
    index++;
  }
  return FALSE;
}

void CXFA_FileRead::Release() {
  delete this;
}

CXFA_FFApp::CXFA_FFApp(IXFA_AppProvider* pProvider)
    : m_pProvider(pProvider),
      m_pWidgetMgrDelegate(nullptr),
      m_pFWLApp(IFWL_App::Create(this)) {
  FWL_SetApp(m_pFWLApp.get());
  m_pFWLApp->Initialize();
  CXFA_TimeZoneProvider::Create();
}

CXFA_FFApp::~CXFA_FFApp() {
  if (m_pFWLApp) {
    m_pFWLApp->Finalize();
    m_pFWLApp->Release();
  }

  CXFA_TimeZoneProvider::Destroy();
}

CXFA_FFDocHandler* CXFA_FFApp::GetDocHandler() {
  if (!m_pDocHandler)
    m_pDocHandler.reset(new CXFA_FFDocHandler);
  return m_pDocHandler.get();
}

CXFA_FFDoc* CXFA_FFApp::CreateDoc(IXFA_DocProvider* pProvider,
                                  IFX_FileRead* pStream,
                                  FX_BOOL bTakeOverFile) {
  std::unique_ptr<CXFA_FFDoc> pDoc(new CXFA_FFDoc(this, pProvider));
  FX_BOOL bSuccess = pDoc->OpenDoc(pStream, bTakeOverFile);
  return bSuccess ? pDoc.release() : nullptr;
}

CXFA_FFDoc* CXFA_FFApp::CreateDoc(IXFA_DocProvider* pProvider,
                                  CPDF_Document* pPDFDoc) {
  if (!pPDFDoc)
    return nullptr;

  std::unique_ptr<CXFA_FFDoc> pDoc(new CXFA_FFDoc(this, pProvider));
  FX_BOOL bSuccess = pDoc->OpenDoc(pPDFDoc);
  return bSuccess ? pDoc.release() : nullptr;
}

void CXFA_FFApp::SetDefaultFontMgr(std::unique_ptr<CXFA_DefFontMgr> pFontMgr) {
  if (!m_pFontMgr)
    m_pFontMgr.reset(new CXFA_FontMgr());
  m_pFontMgr->SetDefFontMgr(std::move(pFontMgr));
}

CXFA_FontMgr* CXFA_FFApp::GetXFAFontMgr() const {
  return m_pFontMgr.get();
}

IFGAS_FontMgr* CXFA_FFApp::GetFDEFontMgr() {
  if (!m_pFDEFontMgr) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
    m_pFDEFontMgr = IFGAS_FontMgr::Create(FX_GetDefFontEnumerator());
#else
    m_pFontSource.reset(new CFX_FontSourceEnum_File);
    m_pFDEFontMgr = IFGAS_FontMgr::Create(m_pFontSource.get());
#endif
  }
  return m_pFDEFontMgr.get();
}

CXFA_FWLTheme* CXFA_FFApp::GetFWLTheme() {
  if (!m_pFWLTheme)
    m_pFWLTheme.reset(new CXFA_FWLTheme(this));
  return m_pFWLTheme.get();
}

CXFA_FWLAdapterWidgetMgr* CXFA_FFApp::GetWidgetMgr(
    CFWL_WidgetMgrDelegate* pDelegate) {
  if (!m_pAdapterWidgetMgr) {
    m_pAdapterWidgetMgr.reset(new CXFA_FWLAdapterWidgetMgr);
    pDelegate->OnSetCapability(FWL_WGTMGR_DisableThread |
                               FWL_WGTMGR_DisableForm);
    m_pWidgetMgrDelegate = pDelegate;
  }
  return m_pAdapterWidgetMgr.get();
}

IFWL_AdapterTimerMgr* CXFA_FFApp::GetTimerMgr() const {
  return m_pProvider->GetTimerMgr();
}
