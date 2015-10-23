// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_INCLUDE_JAVASCRIPT_APP_H_
#define FPDFSDK_INCLUDE_JAVASCRIPT_APP_H_

#include "JS_Define.h"

class CJS_Runtime;

/* ---------------------------- TimerObj ---------------------------- */

class CJS_Timer;

class TimerObj : public CJS_EmbedObj {
 public:
  TimerObj(CJS_Object* pJSObject);
  ~TimerObj() override;

 public:
  void SetTimer(CJS_Timer* pTimer);
  CJS_Timer* GetTimer() const;

 private:
  CJS_Timer* m_pTimer;
};

class CJS_TimerObj : public CJS_Object {
 public:
  CJS_TimerObj(JSFXObject pObject) : CJS_Object(pObject) {}
  ~CJS_TimerObj() override {}

  DECLARE_JS_CLASS(CJS_TimerObj);
};

class app : public CJS_EmbedObj {
 public:
  app(CJS_Object* pJSObject);
  ~app() override;

 public:
  FX_BOOL activeDocs(IFXJS_Context* cc,
                     CJS_PropValue& vp,
                     CFX_WideString& sError);
  FX_BOOL calculate(IFXJS_Context* cc,
                    CJS_PropValue& vp,
                    CFX_WideString& sError);
  FX_BOOL formsVersion(IFXJS_Context* cc,
                       CJS_PropValue& vp,
                       CFX_WideString& sError);
  FX_BOOL fs(IFXJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  FX_BOOL fullscreen(IFXJS_Context* cc,
                     CJS_PropValue& vp,
                     CFX_WideString& sError);
  FX_BOOL language(IFXJS_Context* cc,
                   CJS_PropValue& vp,
                   CFX_WideString& sError);
  FX_BOOL media(IFXJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  FX_BOOL platform(IFXJS_Context* cc,
                   CJS_PropValue& vp,
                   CFX_WideString& sError);
  FX_BOOL runtimeHighlight(IFXJS_Context* cc,
                           CJS_PropValue& vp,
                           CFX_WideString& sError);
  FX_BOOL viewerType(IFXJS_Context* cc,
                     CJS_PropValue& vp,
                     CFX_WideString& sError);
  FX_BOOL viewerVariation(IFXJS_Context* cc,
                          CJS_PropValue& vp,
                          CFX_WideString& sError);
  FX_BOOL viewerVersion(IFXJS_Context* cc,
                        CJS_PropValue& vp,
                        CFX_WideString& sError);

  FX_BOOL alert(IFXJS_Context* cc,
                const CJS_Parameters& params,
                CJS_Value& vRet,
                CFX_WideString& sError);
  FX_BOOL beep(IFXJS_Context* cc,
               const CJS_Parameters& params,
               CJS_Value& vRet,
               CFX_WideString& sError);
  FX_BOOL browseForDoc(IFXJS_Context* cc,
                       const CJS_Parameters& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError);
  FX_BOOL clearInterval(IFXJS_Context* cc,
                        const CJS_Parameters& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError);
  FX_BOOL clearTimeOut(IFXJS_Context* cc,
                       const CJS_Parameters& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError);
  FX_BOOL execDialog(IFXJS_Context* cc,
                     const CJS_Parameters& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError);
  FX_BOOL execMenuItem(IFXJS_Context* cc,
                       const CJS_Parameters& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError);
  FX_BOOL findComponent(IFXJS_Context* cc,
                        const CJS_Parameters& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError);
  FX_BOOL goBack(IFXJS_Context* cc,
                 const CJS_Parameters& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError);
  FX_BOOL goForward(IFXJS_Context* cc,
                    const CJS_Parameters& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError);
  FX_BOOL launchURL(IFXJS_Context* cc,
                    const CJS_Parameters& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError);
  FX_BOOL mailMsg(IFXJS_Context* cc,
                  const CJS_Parameters& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError);
  FX_BOOL newFDF(IFXJS_Context* cc,
                 const CJS_Parameters& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError);
  FX_BOOL newDoc(IFXJS_Context* cc,
                 const CJS_Parameters& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError);
  FX_BOOL openDoc(IFXJS_Context* cc,
                  const CJS_Parameters& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError);
  FX_BOOL openFDF(IFXJS_Context* cc,
                  const CJS_Parameters& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError);
  FX_BOOL popUpMenuEx(IFXJS_Context* cc,
                      const CJS_Parameters& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError);
  FX_BOOL popUpMenu(IFXJS_Context* cc,
                    const CJS_Parameters& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError);
  FX_BOOL response(IFXJS_Context* cc,
                   const CJS_Parameters& params,
                   CJS_Value& vRet,
                   CFX_WideString& sError);
  FX_BOOL setInterval(IFXJS_Context* cc,
                      const CJS_Parameters& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError);
  FX_BOOL setTimeOut(IFXJS_Context* cc,
                     const CJS_Parameters& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError);

  static CFX_WideString SysPathToPDFPath(const CFX_WideString& sOldPath);

 private:
  // CJS_EmbedObj
  void TimerProc(CJS_Timer* pTimer) override;
  void RunJsScript(CJS_Runtime* pRuntime, const CFX_WideString& wsScript);

  bool m_bCalculate;
  bool m_bRuntimeHighLight;
  CFX_ArrayTemplate<CJS_Timer*> m_aTimer;
};

class CJS_App : public CJS_Object {
 public:
  explicit CJS_App(JSFXObject pObject) : CJS_Object(pObject) {}
  ~CJS_App() override {}

  DECLARE_JS_CLASS(CJS_App);

  JS_STATIC_PROP(activeDocs, app);
  JS_STATIC_PROP(calculate, app);
  JS_STATIC_PROP(formsVersion, app);
  JS_STATIC_PROP(fs, app);
  JS_STATIC_PROP(fullscreen, app);
  JS_STATIC_PROP(language, app);
  JS_STATIC_PROP(media, app);
  JS_STATIC_PROP(platform, app);
  JS_STATIC_PROP(runtimeHighlight, app);
  JS_STATIC_PROP(viewerType, app);
  JS_STATIC_PROP(viewerVariation, app);
  JS_STATIC_PROP(viewerVersion, app);

  JS_STATIC_METHOD(alert, app);
  JS_STATIC_METHOD(beep, app);
  JS_STATIC_METHOD(browseForDoc, app);
  JS_STATIC_METHOD(clearInterval, app);
  JS_STATIC_METHOD(clearTimeOut, app);
  JS_STATIC_METHOD(execDialog, app);
  JS_STATIC_METHOD(execMenuItem, app);
  JS_STATIC_METHOD(findComponent, app);
  JS_STATIC_METHOD(goBack, app);
  JS_STATIC_METHOD(goForward, app);
  JS_STATIC_METHOD(launchURL, app);
  JS_STATIC_METHOD(mailMsg, app);
  JS_STATIC_METHOD(newFDF, app);
  JS_STATIC_METHOD(newDoc, app);
  JS_STATIC_METHOD(openDoc, app);
  JS_STATIC_METHOD(openFDF, app);
  JS_STATIC_METHOD(popUpMenuEx, app);
  JS_STATIC_METHOD(popUpMenu, app);
  JS_STATIC_METHOD(response, app);
  JS_STATIC_METHOD(setInterval, app);
  JS_STATIC_METHOD(setTimeOut, app);
};

#endif  // FPDFSDK_INCLUDE_JAVASCRIPT_APP_H_
