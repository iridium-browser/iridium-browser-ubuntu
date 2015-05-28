// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
struct LoadCommittedDetails;
class WebContents;
}

namespace infobars {
class InfoBar;
}

// Associates a Tab to a InfoBarManager and manages its lifetime.
// It manages the infobar notifications and responds to navigation events.
class InfoBarService : public infobars::InfoBarManager,
                       public content::WebContentsObserver,
                       public content::WebContentsUserData<InfoBarService> {
 public:
  static infobars::InfoBarDelegate::NavigationDetails
      NavigationDetailsFromLoadCommittedDetails(
          const content::LoadCommittedDetails& details);

  // This function must only be called on infobars that are owned by an
  // InfoBarService instance (or not owned at all, in which case this returns
  // NULL).
  static content::WebContents* WebContentsFromInfoBar(
      infobars::InfoBar* infobar);

  // Makes it so the next reload is ignored. That is, if the next commit is a
  // reload then it is treated as if nothing happened and no infobars are
  // attempted to be closed.
  // This is useful for non-user triggered reloads that should not dismiss
  // infobars. For example, instant may trigger a reload when the google URL
  // changes.
  void set_ignore_next_reload() { ignore_next_reload_ = true; }

  // InfoBarManager:
  // TODO(sdefresne): Change clients to invoke this on infobars::InfoBarManager
  // and turn the method override private.
  scoped_ptr<infobars::InfoBar> CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate> delegate) override;

 private:
  friend class content::WebContentsUserData<InfoBarService>;

  explicit InfoBarService(content::WebContents* web_contents);
  ~InfoBarService() override;

  // InfoBarManager:
  int GetActiveEntryID() override;
  // TODO(droger): Remove these functions once infobar notifications are
  // removed. See http://crbug.com/354380
  void NotifyInfoBarAdded(infobars::InfoBar* infobar) override;
  void NotifyInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void WebContentsDestroyed() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers.
  void OnDidBlockDisplayingInsecureContent();

  // See description in set_ignore_next_reload().
  bool ignore_next_reload_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarService);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
