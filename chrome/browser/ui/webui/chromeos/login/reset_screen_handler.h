// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/reset_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

class PrefRegistrySimple;

namespace chromeos {

// WebUI implementation of ResetScreenActor.
class ResetScreenHandler : public ResetView,
                           public BaseScreenHandler {
 public:
  ResetScreenHandler();
  ~ResetScreenHandler() override;

  // ResetView implementation:
  void Bind(ResetModel& model) override;
  void Unbind() override;
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:

  ResetModel* model_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(ResetScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_
