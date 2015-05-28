/*
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_MODULE_PPAPI_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_MODULE_PPAPI_H_

#include "components/nacl/renderer/ppb_nacl_private.h"
#include "ppapi/cpp/module.h"

namespace plugin {

class ModulePpapi : public pp::Module {
 public:
  ModulePpapi();

  ~ModulePpapi() override;

  bool Init() override;

  pp::Instance* CreateInstance(PP_Instance pp_instance) override;

 private:
  bool init_was_successful_;
  const PPB_NaCl_Private* private_interface_;
};

}  // namespace plugin


namespace pp {

Module* CreateModule();

}  // namespace pp

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_MODULE_PPAPI_H_
