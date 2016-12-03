// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_GLOBAL_CONFIRM_INFO_BAR_H_
#define CHROME_BROWSER_DEVTOOLS_GLOBAL_CONFIRM_INFO_BAR_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}

// GlobalConfirmInfoBar is shown for every tab in every browser until it
// is dismissed or the close method is called.
// It listens to all tabs in all browsers and adds/removes confirm infobar
// to each of the tabs.
class GlobalConfirmInfoBar : public TabStripModelObserver,
                             public infobars::InfoBarManager::Observer {
 public:
  static base::WeakPtr<GlobalConfirmInfoBar> Show(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);
  void Close();

 private:
  explicit GlobalConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);
  ~GlobalConfirmInfoBar() override;
  class DelegateProxy;

  // TabStripModelObserver:
  void TabInsertedAt(content::WebContents* web_contents,
                     int index,
                     bool foreground) override;
  void TabChangedAt(content::WebContents* web_contents,
                    int index,
                    TabChangeType change_type) override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* info_bar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

  std::unique_ptr<ConfirmInfoBarDelegate> delegate_;
  std::map<infobars::InfoBarManager*, DelegateProxy*> proxies_;
  BrowserTabStripTracker browser_tab_strip_tracker_;

  base::WeakPtrFactory<GlobalConfirmInfoBar> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GlobalConfirmInfoBar);
};

#endif  // CHROME_BROWSER_DEVTOOLS_GLOBAL_CONFIRM_INFO_BAR_H_
