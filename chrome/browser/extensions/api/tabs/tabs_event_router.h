// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/ui/zoom/zoom_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/event_router.h"

namespace content {
class WebContents;
}

namespace favicon {
class FaviconDriver;
}

namespace extensions {

// The TabsEventRouter listens to tab events and routes them to listeners inside
// extension process renderers.
// TabsEventRouter will only route events from windows/tabs within a profile to
// extension processes in the same profile.
class TabsEventRouter : public TabStripModelObserver,
                        public chrome::BrowserListObserver,
                        public content::NotificationObserver,
                        public favicon::FaviconDriverObserver,
                        public ui_zoom::ZoomObserver {
 public:
  explicit TabsEventRouter(Profile* profile);
  ~TabsEventRouter() override;

  // chrome::BrowserListObserver
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;

  // TabStripModelObserver
  void TabInsertedAt(content::WebContents* contents,
                     int index,
                     bool active) override;
  void TabClosingAt(TabStripModel* tab_strip_model,
                    content::WebContents* contents,
                    int index) override;
  void TabDetachedAt(content::WebContents* contents, int index) override;
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override;
  void TabSelectionChanged(TabStripModel* tab_strip_model,
                           const ui::ListSelectionModel& old_model) override;
  void TabMoved(content::WebContents* contents,
                int from_index,
                int to_index) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents,
                     int index) override;
  void TabPinnedStateChanged(content::WebContents* contents,
                             int index) override;

  // content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ZoomObserver.
  void OnZoomChanged(
      const ui_zoom::ZoomController::ZoomChangedEventData& data) override;

  // favicon::FaviconDriverObserver.
  void OnFaviconAvailable(const gfx::Image& image) override;
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        bool icon_url_changed) override;

 private:
  // "Synthetic" event. Called from TabInsertedAt if new tab is detected.
  void TabCreatedAt(content::WebContents* contents, int index, bool active);

  // Internal processing of tab updated events. Is called by both TabChangedAt
  // and Observe/NAV_ENTRY_COMMITTED.
  class TabEntry;
  void TabUpdated(linked_ptr<TabEntry> entry,
                  scoped_ptr<base::DictionaryValue> changed_properties);

  // Triggers a tab updated event if the favicon URL changes.
  void FaviconUrlUpdated(content::WebContents* contents);

  // The DispatchEvent methods forward events to the |profile|'s event router.
  // The TabsEventRouter listens to events for all profiles,
  // so we avoid duplication by dropping events destined for other profiles.
  void DispatchEvent(Profile* profile,
                     events::HistogramValue histogram_value,
                     const std::string& event_name,
                     scoped_ptr<base::ListValue> args,
                     EventRouter::UserGestureState user_gesture);

  void DispatchEventsAcrossIncognito(
      Profile* profile,
      const std::string& event_name,
      scoped_ptr<base::ListValue> event_args,
      scoped_ptr<base::ListValue> cross_incognito_args);

  // Packages |changed_properties| as a tab updated event for the tab |contents|
  // and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(
      content::WebContents* contents,
      scoped_ptr<base::DictionaryValue> changed_properties);

  // Register ourselves to receive the various notifications we are interested
  // in for a browser.
  void RegisterForBrowserNotifications(Browser* browser);

  // Register ourselves to receive the various notifications we are interested
  // in for a tab.
  void RegisterForTabNotifications(content::WebContents* contents);

  // Removes notifications added in RegisterForTabNotifications.
  void UnregisterForTabNotifications(content::WebContents* contents);

  content::NotificationRegistrar registrar_;

  // Maintain some information about known tabs, so we can:
  //
  //  - distinguish between tab creation and tab insertion
  //  - not send tab-detached after tab-removed
  //  - reduce the "noise" of TabChangedAt() when sending events to extensions
  //  - remember last muted and audible states to know if there was a change
  class TabEntry {
   public:
    // Create a TabEntry associated with, and tracking state changes to,
    // |contents|.
    explicit TabEntry(content::WebContents* contents);

    // Indicate via a list of key/value pairs if a tab is loading based on its
    // WebContents. Whether the state has changed or not is used to determine
    // if events needs to be sent to extensions during processing of
    // TabChangedAt(). If this method indicates that a tab should "hold" a
    // state-change to "loading", the DidNavigate() method should eventually
    // send a similar message to undo it. If false, the returned key/value
    // pairs list is empty.
    scoped_ptr<base::DictionaryValue> UpdateLoadState();

    // Indicate via a list of key/value pairs that a tab load has resulted in a
    // navigation and the destination url is available for inspection. The list
    // is empty if no updates should be sent.
    scoped_ptr<base::DictionaryValue> DidNavigate();

    // Update the audible and muted states and return whether they were changed
    bool SetAudible(bool new_val);
    bool SetMuted(bool new_val);

    content::WebContents* web_contents() { return contents_; }

   private:
    content::WebContents* contents_;

    // Whether we are waiting to fire the 'complete' status change. This will
    // occur the first time the WebContents stops loading after the
    // NAV_ENTRY_COMMITTED was fired. The tab may go back into and out of the
    // loading state subsequently, but we will ignore those changes.
    bool complete_waiting_on_load_;

    // Previous audible and muted states
    bool was_audible_;
    bool was_muted_;

    GURL url_;
  };

  // Gets the TabEntry for the given |contents|. Returns linked_ptr<TabEntry>
  // if found, NULL if not.
  linked_ptr<TabEntry> GetTabEntry(content::WebContents* contents);

  using TabEntryMap = std::map<int, linked_ptr<TabEntry>>;
  TabEntryMap tab_entries_;

  // The main profile that owns this event router.
  Profile* profile_;

  ScopedObserver<favicon::FaviconDriver, TabsEventRouter>
      favicon_scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(TabsEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
