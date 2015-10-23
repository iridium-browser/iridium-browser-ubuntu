// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace settings {

// The base class handler of Javascript messages of settings pages.
class SettingsPageUIHandler : public content::WebUIMessageHandler {
 public:
  SettingsPageUIHandler();
  ~SettingsPageUIHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsPageUIHandler);
};

// The WebUI handler for chrome://md-settings.
class MdSettingsUI : public content::WebUIController {
 public:
  explicit MdSettingsUI(content::WebUI* web_ui);
  ~MdSettingsUI() override;

 private:
  // Adds SettingsPageUiHandler to the handlers list if handler is enabled.
  void AddSettingsPageUIHandler(SettingsPageUIHandler* handler);

  DISALLOW_COPY_AND_ASSIGN(MdSettingsUI);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_
