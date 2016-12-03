// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/cjs_runtime.h"

#include <algorithm>

#include "fpdfsdk/include/fsdk_mgr.h"
#include "fpdfsdk/javascript/Annot.h"
#include "fpdfsdk/javascript/Consts.h"
#include "fpdfsdk/javascript/Document.h"
#include "fpdfsdk/javascript/Field.h"
#include "fpdfsdk/javascript/Icon.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_GlobalData.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/PublicMethods.h"
#include "fpdfsdk/javascript/app.h"
#include "fpdfsdk/javascript/cjs_context.h"
#include "fpdfsdk/javascript/color.h"
#include "fpdfsdk/javascript/console.h"
#include "fpdfsdk/javascript/event.h"
#include "fpdfsdk/javascript/global.h"
#include "fpdfsdk/javascript/report.h"
#include "fpdfsdk/javascript/util.h"
#include "third_party/base/stl_util.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/include/fpdfxfa_app.h"
#include "fxjs/include/cfxjse_value.h"
#endif  // PDF_ENABLE_XFA

// static
void IJS_Runtime::Initialize(unsigned int slot, void* isolate) {
  FXJS_Initialize(slot, reinterpret_cast<v8::Isolate*>(isolate));
}

// static
void IJS_Runtime::Destroy() {
  FXJS_Release();
}

// static
IJS_Runtime* IJS_Runtime::Create(CPDFDoc_Environment* pEnv) {
  return new CJS_Runtime(pEnv);
}

// static
CJS_Runtime* CJS_Runtime::FromContext(const IJS_Context* cc) {
  const CJS_Context* pContext = static_cast<const CJS_Context*>(cc);
  return pContext->GetJSRuntime();
}

// static
CJS_Runtime* CJS_Runtime::CurrentRuntimeFromIsolate(v8::Isolate* pIsolate) {
  return static_cast<CJS_Runtime*>(
      CFXJS_Engine::CurrentEngineFromIsolate(pIsolate));
}

CJS_Runtime::CJS_Runtime(CPDFDoc_Environment* pApp)
    : m_pApp(pApp),
      m_pDocument(nullptr),
      m_bBlocking(false),
      m_isolateManaged(false) {
  v8::Isolate* pIsolate = nullptr;
#ifndef PDF_ENABLE_XFA
  IPDF_JSPLATFORM* pPlatform = m_pApp->GetFormFillInfo()->m_pJsPlatform;
  if (pPlatform->version <= 2) {
    unsigned int embedderDataSlot = 0;
    v8::Isolate* pExternalIsolate = nullptr;
    if (pPlatform->version == 2) {
      pExternalIsolate = reinterpret_cast<v8::Isolate*>(pPlatform->m_isolate);
      embedderDataSlot = pPlatform->m_v8EmbedderSlot;
    }
    FXJS_Initialize(embedderDataSlot, pExternalIsolate);
  }
  m_isolateManaged = FXJS_GetIsolate(&pIsolate);
  SetIsolate(pIsolate);
#else
  if (CPDFXFA_App::GetInstance()->GetJSERuntime()) {
    // TODO(tsepez): CPDFXFA_App should also use the embedder provided isolate.
    pIsolate = CPDFXFA_App::GetInstance()->GetJSERuntime();
    SetIsolate(pIsolate);
  } else {
    IPDF_JSPLATFORM* pPlatform = m_pApp->GetFormFillInfo()->m_pJsPlatform;
    if (pPlatform->version <= 2) {
      unsigned int embedderDataSlot = 0;
      v8::Isolate* pExternalIsolate = nullptr;
      if (pPlatform->version == 2) {
        pExternalIsolate = reinterpret_cast<v8::Isolate*>(pPlatform->m_isolate);
        embedderDataSlot = pPlatform->m_v8EmbedderSlot;
      }
      FXJS_Initialize(embedderDataSlot, pExternalIsolate);
    }
    m_isolateManaged = FXJS_GetIsolate(&pIsolate);
    SetIsolate(pIsolate);
  }

  v8::Isolate::Scope isolate_scope(pIsolate);
  v8::HandleScope handle_scope(pIsolate);
  if (CPDFXFA_App::GetInstance()->IsJavaScriptInitialized()) {
    CJS_Context* pContext = (CJS_Context*)NewContext();
    InitializeEngine();
    ReleaseContext(pContext);
    return;
  }
#endif

  if (m_isolateManaged || FXJS_GlobalIsolateRefCount() == 0)
    DefineJSObjects();

#ifdef PDF_ENABLE_XFA
  CPDFXFA_App::GetInstance()->SetJavaScriptInitialized(TRUE);
#endif

  CJS_Context* pContext = (CJS_Context*)NewContext();
  InitializeEngine();
  ReleaseContext(pContext);
}

CJS_Runtime::~CJS_Runtime() {
  for (auto* obs : m_observers)
    obs->OnDestroyed();

  ReleaseEngine();
  if (m_isolateManaged) {
    GetIsolate()->Dispose();
    SetIsolate(nullptr);
  }
}

void CJS_Runtime::DefineJSObjects() {
  v8::Isolate::Scope isolate_scope(GetIsolate());
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> context = v8::Context::New(GetIsolate());
  v8::Context::Scope context_scope(context);

  // The call order determines the "ObjDefID" assigned to each class.
  // ObjDefIDs 0 - 2
  CJS_Border::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Display::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Font::DefineJSObjects(this, FXJSOBJTYPE_STATIC);

  // ObjDefIDs 3 - 5
  CJS_Highlight::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Position::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_ScaleHow::DefineJSObjects(this, FXJSOBJTYPE_STATIC);

  // ObjDefIDs 6 - 8
  CJS_ScaleWhen::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Style::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Zoomtype::DefineJSObjects(this, FXJSOBJTYPE_STATIC);

  // ObjDefIDs 9 - 11
  CJS_App::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Color::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Console::DefineJSObjects(this, FXJSOBJTYPE_STATIC);

  // ObjDefIDs 12 - 14
  CJS_Document::DefineJSObjects(this, FXJSOBJTYPE_GLOBAL);
  CJS_Event::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Field::DefineJSObjects(this, FXJSOBJTYPE_DYNAMIC);

  // ObjDefIDs 15 - 17
  CJS_Global::DefineJSObjects(this, FXJSOBJTYPE_STATIC);
  CJS_Icon::DefineJSObjects(this, FXJSOBJTYPE_DYNAMIC);
  CJS_Util::DefineJSObjects(this, FXJSOBJTYPE_STATIC);

  // ObjDefIDs 18 - 20 (these can't fail, return void).
  CJS_PublicMethods::DefineJSObjects(this);
  CJS_GlobalConsts::DefineJSObjects(this);
  CJS_GlobalArrays::DefineJSObjects(this);

  // ObjDefIDs 21 - 23.
  CJS_TimerObj::DefineJSObjects(this, FXJSOBJTYPE_DYNAMIC);
  CJS_PrintParamsObj::DefineJSObjects(this, FXJSOBJTYPE_DYNAMIC);
  CJS_Annot::DefineJSObjects(this, FXJSOBJTYPE_DYNAMIC);
}

IJS_Context* CJS_Runtime::NewContext() {
  m_ContextArray.push_back(std::unique_ptr<CJS_Context>(new CJS_Context(this)));
  return m_ContextArray.back().get();
}

void CJS_Runtime::ReleaseContext(IJS_Context* pContext) {
  for (auto it = m_ContextArray.begin(); it != m_ContextArray.end(); ++it) {
    if (it->get() == static_cast<CJS_Context*>(pContext)) {
      m_ContextArray.erase(it);
      return;
    }
  }
}

IJS_Context* CJS_Runtime::GetCurrentContext() {
  return m_ContextArray.empty() ? nullptr : m_ContextArray.back().get();
}

void CJS_Runtime::SetReaderDocument(CPDFSDK_Document* pReaderDoc) {
  if (m_pDocument == pReaderDoc)
    return;

  v8::Isolate::Scope isolate_scope(GetIsolate());
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> context = NewLocalContext();
  v8::Context::Scope context_scope(context);

  m_pDocument = pReaderDoc;
  if (!pReaderDoc)
    return;

  v8::Local<v8::Object> pThis = GetThisObj();
  if (pThis.IsEmpty())
    return;

  if (CFXJS_Engine::GetObjDefnID(pThis) != CJS_Document::g_nObjDefnID)
    return;

  CJS_Document* pJSDocument =
      static_cast<CJS_Document*>(GetObjectPrivate(pThis));
  if (!pJSDocument)
    return;

  Document* pDocument = static_cast<Document*>(pJSDocument->GetEmbedObject());
  if (!pDocument)
    return;

  pDocument->AttachDoc(pReaderDoc);
}

CPDFSDK_Document* CJS_Runtime::GetReaderDocument() {
  return m_pDocument;
}

int CJS_Runtime::ExecuteScript(const CFX_WideString& script,
                               CFX_WideString* info) {
  FXJSErr error = {};
  int nRet = Execute(script, &error);
  if (nRet < 0) {
    info->Format(L"[ Line: %05d { %s } ] : %s", error.linnum - 1, error.srcline,
                 error.message);
  }
  return nRet;
}

bool CJS_Runtime::AddEventToSet(const FieldEvent& event) {
  return m_FieldEventSet.insert(event).second;
}

void CJS_Runtime::RemoveEventFromSet(const FieldEvent& event) {
  m_FieldEventSet.erase(event);
}

void CJS_Runtime::AddObserver(Observer* observer) {
  ASSERT(!pdfium::ContainsKey(m_observers, observer));
  m_observers.insert(observer);
}

void CJS_Runtime::RemoveObserver(Observer* observer) {
  ASSERT(pdfium::ContainsKey(m_observers, observer));
  m_observers.erase(observer);
}

#ifdef PDF_ENABLE_XFA
CFX_WideString ChangeObjName(const CFX_WideString& str) {
  CFX_WideString sRet = str;
  sRet.Replace(L"_", L".");
  return sRet;
}
FX_BOOL CJS_Runtime::GetValueByName(const CFX_ByteStringC& utf8Name,
                                    CFXJSE_Value* pValue) {
  const FX_CHAR* name = utf8Name.c_str();

  v8::Isolate::Scope isolate_scope(GetIsolate());
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> old_context = GetIsolate()->GetCurrentContext();
  v8::Local<v8::Context> context = NewLocalContext();
  v8::Context::Scope context_scope(context);

  // Caution: We're about to hand to XFA an object that in order to invoke
  // methods will require that the current v8::Context always has a pointer
  // to a CJS_Runtime in its embedder data slot. Unfortunately, XFA creates
  // its own v8::Context which has not initialized the embedder data slot.
  // Do so now.
  // TODO(tsepez): redesign PDF-side objects to not rely on v8::Context's
  // embedder data slots, and/or to always use the right context.
  CFXJS_Engine::SetForV8Context(old_context, this);

  v8::Local<v8::Value> propvalue =
      context->Global()->Get(v8::String::NewFromUtf8(
          GetIsolate(), name, v8::String::kNormalString, utf8Name.GetLength()));

  if (propvalue.IsEmpty()) {
    pValue->SetUndefined();
    return FALSE;
  }
  pValue->ForceSetValue(propvalue);
  return TRUE;
}
FX_BOOL CJS_Runtime::SetValueByName(const CFX_ByteStringC& utf8Name,
                                    CFXJSE_Value* pValue) {
  if (utf8Name.IsEmpty() || !pValue)
    return FALSE;
  const FX_CHAR* name = utf8Name.c_str();
  v8::Isolate* pIsolate = GetIsolate();
  v8::Isolate::Scope isolate_scope(pIsolate);
  v8::HandleScope handle_scope(pIsolate);
  v8::Local<v8::Context> context = NewLocalContext();
  v8::Context::Scope context_scope(context);

  // v8::Local<v8::Context> tmpCotext =
  // v8::Local<v8::Context>::New(GetIsolate(), m_context);
  v8::Local<v8::Value> propvalue =
      v8::Local<v8::Value>::New(GetIsolate(), pValue->DirectGetValue());
  context->Global()->Set(
      v8::String::NewFromUtf8(pIsolate, name, v8::String::kNormalString,
                              utf8Name.GetLength()),
      propvalue);
  return TRUE;
}
#endif
