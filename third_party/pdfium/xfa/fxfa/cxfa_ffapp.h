// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_CXFA_FFAPP_H_
#define XFA_FXFA_CXFA_FFAPP_H_

#include <memory>
#include <vector>

#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/unowned_ptr.h"
#include "xfa/fwl/cfwl_app.h"
#include "xfa/fxfa/fxfa.h"

class CFGAS_DefaultFontManager;
class CFGAS_FontMgr;
class CFWL_WidgetMgr;
class CPDF_Document;
class CXFA_FFDocHandler;
class CXFA_FontMgr;
class CXFA_FWLAdapterWidgetMgr;
class CXFA_FWLTheme;
class IFWL_AdapterTimerMgr;

class CXFA_FFApp {
 public:
  static void SkipFontLoadForTesting(bool skip);

  explicit CXFA_FFApp(IXFA_AppProvider* pProvider);
  ~CXFA_FFApp();

  std::unique_ptr<CXFA_FFDoc> CreateDoc(IXFA_DocEnvironment* pDocEnvironment,
                                        CPDF_Document* pPDFDoc);
  void SetDefaultFontMgr(std::unique_ptr<CFGAS_DefaultFontManager> pFontMgr);

  CXFA_FWLAdapterWidgetMgr* GetFWLAdapterWidgetMgr();
  CFWL_WidgetMgr* GetFWLWidgetMgr() const { return m_pFWLApp->GetWidgetMgr(); }

  CFGAS_FontMgr* GetFDEFontMgr();
  CXFA_FWLTheme* GetFWLTheme();

  IXFA_AppProvider* GetAppProvider() const { return m_pProvider.Get(); }
  const CFWL_App* GetFWLApp() const { return m_pFWLApp.get(); }
  IFWL_AdapterTimerMgr* GetTimerMgr() const;
  CXFA_FontMgr* GetXFAFontMgr() const;

  void ClearEventTargets();

 private:
  UnownedPtr<IXFA_AppProvider> const m_pProvider;

  // The fonts stored in the font manager may have been created by the default
  // font manager. The GEFont::LoadFont call takes the manager as a param and
  // stores it internally. When you destroy the GEFont it tries to unregister
  // from the font manager and if the default font manager was destroyed first
  // get get a use-after-free. The m_pFWLTheme can try to cleanup a GEFont
  // when it frees, so make sure it gets cleaned up first. That requires
  // m_pFWLApp to be cleaned up as well.
  //
  // TODO(dsinclair): The GEFont should have the FontMgr as the pointer instead
  // of the DEFFontMgr so this goes away. Bug 561.
  std::unique_ptr<CFGAS_FontMgr> m_pFDEFontMgr;
  std::unique_ptr<CXFA_FontMgr> m_pFontMgr;

  std::unique_ptr<CXFA_FWLAdapterWidgetMgr> m_pAdapterWidgetMgr;

  // |m_pFWLApp| has to be released first, then |m_pFWLTheme| since the former
  // may refers to theme manager and the latter refers to font manager.
  std::unique_ptr<CXFA_FWLTheme> m_pFWLTheme;
  std::unique_ptr<CFWL_App> m_pFWLApp;
};

#endif  // XFA_FXFA_CXFA_FFAPP_H_
