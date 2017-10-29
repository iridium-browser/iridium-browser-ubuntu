// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTShaderTextureLOD_h
#define EXTShaderTextureLOD_h

#include "modules/webgl/WebGLExtension.h"

namespace blink {

class EXTShaderTextureLOD final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static EXTShaderTextureLOD* Create(WebGLRenderingContextBase*);
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  WebGLExtensionName GetName() const override;

 private:
  explicit EXTShaderTextureLOD(WebGLRenderingContextBase*);
};

}  // namespace blink

#endif  // EXTShaderTextureLOD_h
