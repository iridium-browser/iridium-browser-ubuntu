// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class FilePath;
}

namespace infobars {
class InfoBarDelegate;
}

// Manages per-tab state with regard to hung plugins. This only handles
// Pepper plugins which we know are windowless. Hung NPAPI plugins (which
// may have native windows) can not be handled with infobars and have a
// separate OS-specific hang monitoring.
//
// Our job is:
// - Pop up an infobar when a plugin is hung.
// - Terminate the plugin process if the user so chooses.
// - Periodically re-show the hung plugin infobar if the user closes it without
//   terminating the plugin.
// - Hide the infobar if the plugin starts responding again.
// - Keep track of all of this for any number of plugins.
class HungPluginTabHelper
    : public content::WebContentsObserver,
      public content::NotificationObserver,
      public content::WebContentsUserData<HungPluginTabHelper> {
 public:
  ~HungPluginTabHelper() override;

  // content::WebContentsObserver:
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void PluginHungStatusChanged(int plugin_child_id,
                               const base::FilePath& plugin_path,
                               bool is_hung) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called by an infobar when the user selects to kill the plugin.
  void KillPlugin(int child_id);

 private:
  friend class content::WebContentsUserData<HungPluginTabHelper>;

  struct PluginState;
  typedef std::map<int, linked_ptr<PluginState> > PluginStateMap;

  explicit HungPluginTabHelper(content::WebContents* contents);

  // Called on a timer for a hung plugin to re-show the bar.
  void OnReshowTimer(int child_id);

  // Shows the bar for the plugin identified by the given state, updating the
  // state accordingly. The plugin must not have an infobar already.
  void ShowBar(int child_id, PluginState* state);

  // Closes the infobar associated with the given state. Note that this can
  // be called even if the bar is not opened, in which case it will do nothing.
  void CloseBar(PluginState* state);

  content::NotificationRegistrar registrar_;

  // All currently hung plugins.
  PluginStateMap hung_plugins_;

  DISALLOW_COPY_AND_ASSIGN(HungPluginTabHelper);
};

#endif  // CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_
