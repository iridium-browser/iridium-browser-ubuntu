// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/javascript/JavaScript.h"
#include "../../include/javascript/IJavaScript.h"
#include "../../include/javascript/JS_Define.h"
#include "../../include/javascript/JS_Object.h"
#include "../../include/javascript/JS_Context.h"

int FXJS_MsgBox(CPDFDoc_Environment* pApp,
                CPDFSDK_PageView* pPageView,
                const FX_WCHAR* swMsg,
                const FX_WCHAR* swTitle,
                FX_UINT nType,
                FX_UINT nIcon) {
  if (!pApp)
    return 0;

  if (CPDFSDK_Document* pDoc = pApp->GetSDKDocument())
    pDoc->KillFocusAnnot();

  return pApp->JS_appAlert(swMsg, swTitle, nType, nIcon);
}

CPDFSDK_PageView* FXJS_GetPageView(IFXJS_Context* cc) {
  if (CJS_Context* pContext = (CJS_Context*)cc) {
    if (pContext->GetReaderDocument())
      return NULL;
  }
  return NULL;
}

CJS_EmbedObj::CJS_EmbedObj(CJS_Object* pJSObject) : m_pJSObject(pJSObject) {}

CJS_EmbedObj::~CJS_EmbedObj() {
  m_pJSObject = NULL;
}

CPDFSDK_PageView* CJS_EmbedObj::JSGetPageView(IFXJS_Context* cc) {
  return FXJS_GetPageView(cc);
}

int CJS_EmbedObj::MsgBox(CPDFDoc_Environment* pApp,
                         CPDFSDK_PageView* pPageView,
                         const FX_WCHAR* swMsg,
                         const FX_WCHAR* swTitle,
                         FX_UINT nType,
                         FX_UINT nIcon) {
  return FXJS_MsgBox(pApp, pPageView, swMsg, swTitle, nType, nIcon);
}

void CJS_EmbedObj::Alert(CJS_Context* pContext, const FX_WCHAR* swMsg) {
  CJS_Object::Alert(pContext, swMsg);
}

void FreeObject(const v8::WeakCallbackInfo<CJS_Object>& data) {
  CJS_Object* pJSObj = data.GetParameter();
  pJSObj->ExitInstance();
  delete pJSObj;
  JS_FreePrivate(data.GetInternalField(0));
}

void DisposeObject(const v8::WeakCallbackInfo<CJS_Object>& data) {
  CJS_Object* pJSObj = data.GetParameter();
  pJSObj->Dispose();
  data.SetSecondPassCallback(FreeObject);
}

CJS_Object::CJS_Object(JSFXObject pObject) : m_pEmbedObj(NULL) {
  v8::Local<v8::Context> context = pObject->CreationContext();
  m_pIsolate = context->GetIsolate();
  m_pObject.Reset(m_pIsolate, pObject);
};

CJS_Object::~CJS_Object() {
  m_pObject.Reset();
};

void CJS_Object::MakeWeak() {
  m_pObject.SetWeak(this, DisposeObject, v8::WeakCallbackType::kInternalFields);
}

void CJS_Object::Dispose() {
  m_pObject.Reset();
}

CPDFSDK_PageView* CJS_Object::JSGetPageView(IFXJS_Context* cc) {
  return FXJS_GetPageView(cc);
}

int CJS_Object::MsgBox(CPDFDoc_Environment* pApp,
                       CPDFSDK_PageView* pPageView,
                       const FX_WCHAR* swMsg,
                       const FX_WCHAR* swTitle,
                       FX_UINT nType,
                       FX_UINT nIcon) {
  return FXJS_MsgBox(pApp, pPageView, swMsg, swTitle, nType, nIcon);
}

void CJS_Object::Alert(CJS_Context* pContext, const FX_WCHAR* swMsg) {
  ASSERT(pContext != NULL);

  if (pContext->IsMsgBoxEnabled()) {
    CPDFDoc_Environment* pApp = pContext->GetReaderApp();
    if (pApp)
      pApp->JS_appAlert(swMsg, NULL, 0, 3);
  }
}

CJS_Timer::CJS_Timer(CJS_EmbedObj* pObj,
                     CPDFDoc_Environment* pApp,
                     CJS_Runtime* pRuntime,
                     int nType,
                     const CFX_WideString& script,
                     FX_DWORD dwElapse,
                     FX_DWORD dwTimeOut)
    : m_nTimerID(0),
      m_pEmbedObj(pObj),
      m_bProcessing(false),
      m_bValid(true),
      m_nType(nType),
      m_dwTimeOut(dwTimeOut),
      m_pRuntime(pRuntime),
      m_pApp(pApp) {
  IFX_SystemHandler* pHandler = m_pApp->GetSysHandler();
  m_nTimerID = pHandler->SetTimer(dwElapse, TimerProc);
  (*GetGlobalTimerMap())[m_nTimerID] = this;
  m_pRuntime->AddObserver(this);
}

CJS_Timer::~CJS_Timer() {
  CJS_Runtime* pRuntime = GetRuntime();
  if (pRuntime)
    pRuntime->RemoveObserver(this);
  KillJSTimer();
}

void CJS_Timer::KillJSTimer() {
  if (m_nTimerID) {
    if (m_bValid) {
      IFX_SystemHandler* pHandler = m_pApp->GetSysHandler();
      pHandler->KillTimer(m_nTimerID);
    }
    GetGlobalTimerMap()->erase(m_nTimerID);
    m_nTimerID = 0;
  }
}

// static
void CJS_Timer::TimerProc(int idEvent) {
  const auto it = GetGlobalTimerMap()->find(idEvent);
  if (it != GetGlobalTimerMap()->end()) {
    CJS_Timer* pTimer = it->second;
    if (!pTimer->m_bProcessing) {
      CFX_AutoRestorer<bool> scoped_processing(&pTimer->m_bProcessing);
      pTimer->m_bProcessing = true;
      if (pTimer->m_pEmbedObj)
        pTimer->m_pEmbedObj->TimerProc(pTimer);
    }
  }
}

// static
CJS_Timer::TimerMap* CJS_Timer::GetGlobalTimerMap() {
  // Leak the timer array at shutdown.
  static auto* s_TimerMap = new TimerMap;
  return s_TimerMap;
}

void CJS_Timer::OnDestroyed() {
  m_bValid = false;
}
