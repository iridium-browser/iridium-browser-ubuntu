// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_IJS_RUNTIME_H_
#define FXJS_IJS_RUNTIME_H_

#include <memory>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

#ifdef PDF_ENABLE_XFA
#include "fxjs/fxjse.h"
#endif  // PDF_ENABLE_XFA

class CPDFSDK_FormFillEnvironment;
class IJS_EventContext;

// Owns the FJXS objects needed to actually execute JS.
class IJS_Runtime {
 public:
  static void Initialize(unsigned int slot, void* isolate);
  static void Destroy();
  static std::unique_ptr<IJS_Runtime> Create(
      CPDFSDK_FormFillEnvironment* pFormFillEnv);
  virtual ~IJS_Runtime() {}

  virtual IJS_EventContext* NewEventContext() = 0;
  virtual void ReleaseEventContext(IJS_EventContext* pContext) = 0;
  virtual CPDFSDK_FormFillEnvironment* GetFormFillEnv() const = 0;
  virtual int ExecuteScript(const WideString& script, WideString* info) = 0;

#ifdef PDF_ENABLE_XFA
  virtual bool GetValueByNameFromGlobalObject(const ByteStringView& utf8Name,
                                              CFXJSE_Value* pValue) = 0;
  virtual bool SetValueByNameInGlobalObject(const ByteStringView& utf8Name,
                                            CFXJSE_Value* pValue) = 0;
#endif  // PDF_ENABLE_XFA

 protected:
  IJS_Runtime() {}
};

#endif  // FXJS_IJS_RUNTIME_H_
