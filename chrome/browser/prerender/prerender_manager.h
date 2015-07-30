// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_

#include <list>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/prerender/prerender_config.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host_observer.h"
#include "net/cookies/cookie_monster.h"
#include "url/gurl.h"

class Profile;
class InstantSearchPrerendererTest;
struct ChromeCookieDetails;

namespace base {
class DictionaryValue;
}

namespace chrome {
struct NavigateParams;
}

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

namespace prerender {

class PrerenderHandle;
class PrerenderHistory;
class PrerenderLocalPredictor;

// PrerenderManager is responsible for initiating and keeping prerendered
// views of web pages. All methods must be called on the UI thread unless
// indicated otherwise.
class PrerenderManager : public base::SupportsWeakPtr<PrerenderManager>,
                         public base::NonThreadSafe,
                         public content::NotificationObserver,
                         public content::RenderProcessHostObserver,
                         public KeyedService,
                         public MediaCaptureDevicesDispatcher::Observer {
 public:
  // NOTE: New values need to be appended, since they are used in histograms.
  enum PrerenderManagerMode {
    PRERENDER_MODE_DISABLED = 0,
    PRERENDER_MODE_ENABLED = 1,
    PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP = 2,
    PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP = 3,
    // Obsolete: PRERENDER_MODE_EXPERIMENT_5MIN_TTL_GROUP = 4,
    PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP = 5,
    PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP = 6,
    PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP = 7,
    PRERENDER_MODE_EXPERIMENT_MATCH_COMPLETE_GROUP = 8,
    PRERENDER_MODE_MAX
  };

  // One or more of these flags must be passed to ClearData() to specify just
  // what data to clear.  See function declaration for more information.
  enum ClearFlags {
    CLEAR_PRERENDER_CONTENTS = 0x1 << 0,
    CLEAR_PRERENDER_HISTORY = 0x1 << 1,
    CLEAR_MAX = 0x1 << 2
  };

  // Owned by a Profile object for the lifetime of the profile.
  explicit PrerenderManager(Profile* profile);

  ~PrerenderManager() override;

  // From KeyedService:
  void Shutdown() override;

  // Entry points for adding prerenders.

  // Adds a prerender for |url| if valid. |process_id| and |route_id| identify
  // the RenderView that the prerender request came from. If |size| is empty, a
  // default from the PrerenderConfig is used. Returns a caller-owned
  // PrerenderHandle* if the URL was added, NULL if it was not. If the launching
  // RenderView is itself prerendering, the prerender is added as a pending
  // prerender.
  PrerenderHandle* AddPrerenderFromLinkRelPrerender(
      int process_id,
      int route_id,
      const GURL& url,
      uint32 rel_types,
      const content::Referrer& referrer,
      const gfx::Size& size);

  // Adds a prerender for |url| if valid. As the prerender request is coming
  // from a source without a RenderFrameHost (i.e., the omnibox) we don't have a
  // child or route id, or a referrer. This method uses sensible values for
  // those. The |session_storage_namespace| matches the namespace of the active
  // tab at the time the prerender is generated from the omnibox. Returns a
  // caller-owned PrerenderHandle*, or NULL.
  PrerenderHandle* AddPrerenderFromOmnibox(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  PrerenderHandle* AddPrerenderFromExternalRequest(
      const GURL& url,
      const content::Referrer& referrer,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  // Adds a prerender for Instant Search |url| if valid. The
  // |session_storage_namespace| matches the namespace of the active tab at the
  // time the prerender is generated. Returns a caller-owned PrerenderHandle* or
  // NULL.
  PrerenderHandle* AddPrerenderForInstant(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  // Cancels all active prerenders.
  void CancelAllPrerenders();

  // If |url| matches a valid prerendered page and |params| are compatible, try
  // to swap it and merge browsing histories. Returns |true| and updates
  // |params->target_contents| if a prerendered page is swapped in, |false|
  // otherwise.
  bool MaybeUsePrerenderedPage(const GURL& url,
                               chrome::NavigateParams* params);

  // Moves a PrerenderContents to the pending delete list from the list of
  // active prerenders when prerendering should be cancelled.
  virtual void MoveEntryToPendingDelete(PrerenderContents* entry,
                                        FinalStatus final_status);

  // Records the page load time for a prerender that wasn't swapped in.
  void RecordPageLoadTimeNotSwappedIn(Origin origin,
                                      base::TimeDelta page_load_time,
                                      const GURL& url);

  // Records the perceived page load time for a page - effectively the time from
  // when the user navigates to a page to when it finishes loading. The actual
  // load may have started prior to navigation due to prerender hints.
  // This must be called on the UI thread.
  // |fraction_plt_elapsed_at_swap_in| must either be in [0.0, 1.0], or a value
  // outside that range indicating that it doesn't apply.
  void RecordPerceivedPageLoadTime(
      Origin origin,
      NavigationType navigation_type,
      base::TimeDelta perceived_page_load_time,
      double fraction_plt_elapsed_at_swap_in,
      const GURL& url);

  static PrerenderManagerMode GetMode();
  static void SetMode(PrerenderManagerMode mode);
  static const char* GetModeString();
  static bool IsPrerenderingPossible();
  static bool ActuallyPrerendering();
  static bool IsControlGroup();
  static bool IsNoUseGroup();

  // Query the list of current prerender pages to see if the given web contents
  // is prerendering a page. The optional parameter |origin| is an output
  // parameter which, if a prerender is found, is set to the Origin of the
  // prerender |web_contents|.
  bool IsWebContentsPrerendering(const content::WebContents* web_contents,
                                 Origin* origin) const;

  // Whether the PrerenderManager has an active prerender with the given url and
  // SessionStorageNamespace associated with the given WebContens.
  bool HasPrerenderedUrl(GURL url, content::WebContents* web_contents) const;

  // Returns the PrerenderContents object for the given web_contents, otherwise
  // returns NULL. Note that the PrerenderContents may have been Destroy()ed,
  // but not yet deleted.
  PrerenderContents* GetPrerenderContents(
      const content::WebContents* web_contents) const;

  // Returns the PrerenderContents object for a given child_id, route_id pair,
  // otherwise returns NULL. Note that the PrerenderContents may have been
  // Destroy()ed, but not yet deleted.
  virtual PrerenderContents* GetPrerenderContentsForRoute(
      int child_id, int route_id) const;

  // Returns a list of all WebContents being prerendered.
  const std::vector<content::WebContents*> GetAllPrerenderingContents() const;

  // Checks whether |url| has been recently navigated to.
  bool HasRecentlyBeenNavigatedTo(Origin origin, const GURL& url);

  // Returns true iff the method given is valid for prerendering.
  static bool IsValidHttpMethod(const std::string& method);

  // Returns true iff the scheme of the URL given is valid for prerendering.
  static bool DoesURLHaveValidScheme(const GURL& url);

  // Returns true iff the scheme of the subresource URL given is valid for
  // prerendering.
  static bool DoesSubresourceURLHaveValidScheme(const GURL& url);

  // Returns a Value object containing the active pages being prerendered, and
  // a history of pages which were prerendered. The caller is responsible for
  // deleting the return value.
  base::DictionaryValue* GetAsValue() const;

  // Clears the data indicated by which bits of clear_flags are set.
  //
  // If the CLEAR_PRERENDER_CONTENTS bit is set, all active prerenders are
  // cancelled and then deleted, and any WebContents queued for destruction are
  // destroyed as well.
  //
  // If the CLEAR_PRERENDER_HISTORY bit is set, the prerender history is
  // cleared, including any entries newly created by destroying them in
  // response to the CLEAR_PRERENDER_CONTENTS flag.
  //
  // Intended to be used when clearing the cache or history.
  void ClearData(int clear_flags);

  // Record a final status of a prerendered page in a histogram.
  // This variation allows specifying whether prerendering had been started
  // (necessary to flag MatchComplete dummies).
  void RecordFinalStatusWithMatchCompleteStatus(
      Origin origin,
      PrerenderContents::MatchCompleteStatus mc_status,
      FinalStatus final_status) const;

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // MediaCaptureDevicesDispatcher::Observer
  void OnCreatingAudioStream(int render_process_id,
                             int render_frame_id) override;

  const Config& config() const { return config_; }
  Config& mutable_config() { return config_; }

  // Records that some visible tab navigated (or was redirected) to the
  // provided URL.
  void RecordNavigation(const GURL& url);

  Profile* profile() const { return profile_; }

  // Classes which will be tested in prerender unit browser tests should use
  // these methods to get times for comparison, so that the test framework can
  // mock advancing/retarding time.
  virtual base::Time GetCurrentTime() const;
  virtual base::TimeTicks GetCurrentTimeTicks() const;

  // Notification that a prerender has completed and its bytes should be
  // recorded.
  void RecordNetworkBytes(Origin origin, bool used, int64 prerender_bytes);

  // Returns whether prerendering is currently enabled for this manager.
  bool IsEnabled() const;

  // Add to the running tally of bytes transferred over the network for this
  // profile if prerendering is currently enabled.
  void AddProfileNetworkBytesIfEnabled(int64 bytes);

  // Registers a new ProcessHost performing a prerender. Called by
  // PrerenderContents.
  void AddPrerenderProcessHost(content::RenderProcessHost* process_host);

  // Returns whether or not |process_host| may be reused for new navigations
  // from a prerendering perspective. Currently, if Prerender Cookie Stores are
  // enabled, prerenders must be in their own processes that may not be shared.
  bool MayReuseProcessHost(content::RenderProcessHost* process_host);

  // content::RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 protected:
  class PrerenderData : public base::SupportsWeakPtr<PrerenderData> {
   public:
    struct OrderByExpiryTime;

    PrerenderData(PrerenderManager* manager,
                  PrerenderContents* contents,
                  base::TimeTicks expiry_time);

    ~PrerenderData();

    // Turn this PrerenderData into a Match Complete replacement for itself,
    // placing the current prerender contents into |to_delete_prerenders_|.
    void MakeIntoMatchCompleteReplacement();

    // A new PrerenderHandle has been created for this PrerenderData.
    void OnHandleCreated(PrerenderHandle* prerender_handle);

    // The launcher associated with a handle is navigating away from the context
    // that launched this prerender. If the prerender is active, it may stay
    // alive briefly though, in case we we going through a redirect chain that
    // will eventually land at it.
    void OnHandleNavigatedAway(PrerenderHandle* prerender_handle);

    // The launcher associated with a handle has taken explicit action to cancel
    // this prerender. We may well destroy the prerender in this case if no
    // other handles continue to track it.
    void OnHandleCanceled(PrerenderHandle* prerender_handle);

    PrerenderContents* contents() { return contents_.get(); }

    PrerenderContents* ReleaseContents();

    int handle_count() const { return handle_count_; }

    base::TimeTicks abandon_time() const { return abandon_time_; }

    base::TimeTicks expiry_time() const { return expiry_time_; }
    void set_expiry_time(base::TimeTicks expiry_time) {
      expiry_time_ = expiry_time;
    }

   private:
    PrerenderManager* manager_;
    scoped_ptr<PrerenderContents> contents_;

    // The number of distinct PrerenderHandles created for |this|, including
    // ones that have called PrerenderData::OnHandleNavigatedAway(), but not
    // counting the ones that have called PrerenderData::OnHandleCanceled(). For
    // pending prerenders, this will always be 1, since the PrerenderManager
    // only merges handles of running prerenders.
    int handle_count_;

    // The time when OnHandleNavigatedAway was called.
    base::TimeTicks abandon_time_;

    // After this time, this prerender is no longer fresh, and should be
    // removed.
    base::TimeTicks expiry_time_;

    DISALLOW_COPY_AND_ASSIGN(PrerenderData);
  };

  void SetPrerenderContentsFactory(
      PrerenderContents::Factory* prerender_contents_factory);

  // Called by a PrerenderData to signal that the launcher has navigated away
  // from the context that launched the prerender. A user may have clicked
  // a link in a page containing a <link rel=prerender> element, or the user
  // might have committed an omnibox navigation. This is used to possibly
  // shorten the TTL of the prerendered page.
  void SourceNavigatedAway(PrerenderData* prerender_data);

 private:
  friend class ::InstantSearchPrerendererTest;
  friend class PrerenderBrowserTest;
  friend class PrerenderContents;
  friend class PrerenderHandle;
  friend class UnitTestPrerenderManager;

  class OnCloseWebContentsDeleter;
  struct NavigationRecord;

  // Time interval before a new prerender is allowed.
  static const int kMinTimeBetweenPrerendersMs = 500;

  // Time window for which we record old navigations, in milliseconds.
  static const int kNavigationRecordWindowMs = 5000;

  void OnCancelPrerenderHandle(PrerenderData* prerender_data);

  // Adds a prerender for |url| from |referrer|. The |origin| specifies how the
  // prerender was added. If |size| is empty, then
  // PrerenderContents::StartPrerendering will instead use a default from
  // PrerenderConfig. Returns a PrerenderHandle*, owned by the caller, or NULL.
  PrerenderHandle* AddPrerender(
      Origin origin,
      const GURL& url,
      const content::Referrer& referrer,
      const gfx::Size& size,
      content::SessionStorageNamespace* session_storage_namespace);

  void StartSchedulingPeriodicCleanups();
  void StopSchedulingPeriodicCleanups();

  void EvictOldestPrerendersIfNecessary();

  // Deletes stale and cancelled prerendered PrerenderContents, as well as
  // WebContents that have been replaced by prerendered WebContents.
  // Also identifies and kills PrerenderContents that use too much
  // resources.
  void PeriodicCleanup();

  // Posts a task to call PeriodicCleanup.  Results in quicker destruction of
  // objects.  If |this| is deleted before the task is run, the task will
  // automatically be cancelled.
  void PostCleanupTask();

  base::TimeTicks GetExpiryTimeForNewPrerender(Origin origin) const;
  base::TimeTicks GetExpiryTimeForNavigatedAwayPrerender() const;

  void DeleteOldEntries();
  virtual PrerenderContents* CreatePrerenderContents(
      const GURL& url,
      const content::Referrer& referrer,
      Origin origin);

  // Insures the |active_prerenders_| are sorted by increasing expiry time. Call
  // after every mutation of active_prerenders_ that can possibly make it
  // unsorted (e.g. an insert, or changing an expiry time).
  void SortActivePrerenders();

  // Finds the active PrerenderData object for a running prerender matching
  // |url| and |session_storage_namespace|.
  PrerenderData* FindPrerenderData(
      const GURL& url,
      const content::SessionStorageNamespace* session_storage_namespace);

  // Given the |prerender_contents|, find the iterator in active_prerenders_
  // correponding to the given prerender.
  ScopedVector<PrerenderData>::iterator
      FindIteratorForPrerenderContents(PrerenderContents* prerender_contents);

  bool DoesRateLimitAllowPrerender(Origin origin) const;

  // Deletes old WebContents that have been replaced by prerendered ones.  This
  // is needed because they're replaced in a callback from the old WebContents,
  // so cannot immediately be deleted.
  void DeleteOldWebContents();

  // Cleans up old NavigationRecord's.
  void CleanUpOldNavigations();

  // Arrange for the given WebContents to be deleted asap. If deleter is not
  // NULL, deletes that as well.
  void ScheduleDeleteOldWebContents(content::WebContents* tab,
                                    OnCloseWebContentsDeleter* deleter);

  // Adds to the history list.
  void AddToHistory(PrerenderContents* contents);

  // Returns a new Value representing the pages currently being prerendered. The
  // caller is responsible for delete'ing the return value.
  base::Value* GetActivePrerendersAsValue() const;

  // Destroys all pending prerenders using FinalStatus.  Also deletes them as
  // well as any swapped out WebContents queued for destruction.
  // Used both on destruction, and when clearing the browsing history.
  void DestroyAllContents(FinalStatus final_status);

  // Helper function to destroy a PrerenderContents with the specified
  // final_status, while at the same time recording that for the MatchComplete
  // case, that this prerender would have been used.
  void DestroyAndMarkMatchCompleteAsUsed(PrerenderContents* prerender_contents,
                                         FinalStatus final_status);

  // Records the final status a prerender in the case that a PrerenderContents
  // was never created, and also adds a PrerenderHistory entry.
  // This is a helper function which will ultimately call
  // RecordFinalStatusWthMatchCompleteStatus, using MATCH_COMPLETE_DEFAULT.
  void RecordFinalStatusWithoutCreatingPrerenderContents(
      const GURL& url, Origin origin, FinalStatus final_status) const;


  // Swaps a prerender |prerender_data| for |url| into the tab, replacing
  // |web_contents|.  Returns the new WebContents that was swapped in, or NULL
  // if a swap-in was not possible.  If |should_replace_current_entry| is true,
  // the current history entry in |web_contents| is replaced.
  content::WebContents* SwapInternal(const GURL& url,
                                     content::WebContents* web_contents,
                                     PrerenderData* prerender_data,
                                     bool should_replace_current_entry);

  // The configuration.
  Config config_;

  // The profile that owns this PrerenderManager.
  Profile* profile_;

  // All running prerenders. Sorted by expiry time, in ascending order.
  ScopedVector<PrerenderData> active_prerenders_;

  // Prerenders awaiting deletion.
  ScopedVector<PrerenderData> to_delete_prerenders_;

  // List of recent navigations in this profile, sorted by ascending
  // navigate_time_.
  std::list<NavigationRecord> navigations_;

  scoped_ptr<PrerenderContents::Factory> prerender_contents_factory_;

  static PrerenderManagerMode mode_;

  // A count of how many prerenders we do per session. Initialized to 0 then
  // incremented and emitted to a histogram on each successful prerender.
  static int prerenders_per_session_count_;

  // RepeatingTimer to perform periodic cleanups of pending prerendered
  // pages.
  base::RepeatingTimer<PrerenderManager> repeating_timer_;

  // Track time of last prerender to limit prerender spam.
  base::TimeTicks last_prerender_start_time_;

  std::list<content::WebContents*> old_web_contents_list_;

  ScopedVector<OnCloseWebContentsDeleter> on_close_web_contents_deleters_;

  scoped_ptr<PrerenderHistory> prerender_history_;

  scoped_ptr<PrerenderHistograms> histograms_;

  content::NotificationRegistrar notification_registrar_;

  // The number of bytes transferred over the network for the profile this
  // PrerenderManager is attached to.
  int64 profile_network_bytes_;

  // The value of profile_network_bytes_ that was last recorded.
  int64 last_recorded_profile_network_bytes_;

  // Set of process hosts being prerendered.
  typedef std::set<content::RenderProcessHost*> PrerenderProcessSet;
  PrerenderProcessSet prerender_process_hosts_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
