// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOMATIC_SETTINGS_RESET_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOMATIC_SETTINGS_RESET_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

// Handler for the banner that displays a settings reset event at the top of the
// settings page.
class AutomaticSettingsResetHandler : public OptionsPageUIHandler {
 public:
  AutomaticSettingsResetHandler();
  ~AutomaticSettingsResetHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializePage() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomaticSettingsResetHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOMATIC_SETTINGS_RESET_HANDLER_H_
