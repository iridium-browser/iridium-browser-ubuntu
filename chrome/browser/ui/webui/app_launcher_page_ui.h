// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_

#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

class Profile;

namespace base {
class RefCountedMemory;
}

// The WebUIController used for the app launcher page UI.
class AppLauncherPageUI : public content::WebUIController {
 public:
  explicit AppLauncherPageUI(content::WebUI* web_ui);
  ~AppLauncherPageUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

  // content::WebUIController:
  bool OverrideHandleWebUIMessage(const GURL& source_url,
                                  const std::string& message,
                                  const base::ListValue& args) override;

 private:
  class HTMLSource : public content::URLDataSource {
   public:
    explicit HTMLSource(Profile* profile);
    ~HTMLSource() override;

    // content::URLDataSource implementation.
    std::string GetSource() const override;
    void StartDataRequest(
        const std::string& path,
        int render_process_id,
        int render_frame_id,
        const content::URLDataSource::GotDataCallback& callback) override;
    std::string GetMimeType(const std::string&) const override;
    bool ShouldReplaceExistingSource() const override;
    bool ShouldAddContentSecurityPolicy() const override;

   private:

    // Pointer back to the original profile.
    Profile* profile_;

    DISALLOW_COPY_AND_ASSIGN(HTMLSource);
  };

  Profile* GetProfile() const;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherPageUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_
