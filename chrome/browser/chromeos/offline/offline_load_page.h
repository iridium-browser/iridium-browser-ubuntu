// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
#define CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "net/base/network_change_notifier.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace content {
class InterstitialPage;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace chromeos {

// OfflineLoadPage class shows the interstitial page that is shown
// when no network is available and hides when some network (either
// one of wifi, 3g or ethernet) becomes available.
// It deletes itself when the interstitial page is closed.
class OfflineLoadPage
    : public content::InterstitialPageDelegate,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Passed a boolean indicating whether or not it is OK to proceed with the
  // page load.
  typedef base::Callback<void(bool /*proceed*/)> CompletionCallback;

  // Create a offline load page for the |web_contents|.  The callback will be
  // run on the IO thread.
  OfflineLoadPage(content::WebContents* web_contents, const GURL& url,
                  const CompletionCallback& callback);

  void Show();

 protected:
  ~OfflineLoadPage() override;

  // Overridden by tests.
  virtual void NotifyBlockingPageComplete(bool proceed);

 private:
  friend class TestOfflineLoadPage;

  // InterstitialPageDelegate implementation.
  std::string GetHTMLContents() override;
  void CommandReceived(const std::string& command) override;
  void OverrideRendererPrefs(content::RendererPreferences* prefs) override;
  void OnProceed() override;
  void OnDontProceed() override;

  // net::NetworkChangeNotifier::ConnectionTypeObserver overrides.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  CompletionCallback callback_;

  // True if the proceed is chosen.
  bool proceeded_;

  content::WebContents* web_contents_;
  GURL url_;
  content::InterstitialPage* interstitial_page_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(OfflineLoadPage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
