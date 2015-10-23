// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_OOM_PRIORITY_MANAGER_H_
#define CHROME_BROWSER_MEMORY_OOM_PRIORITY_MANAGER_H_

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "chrome/browser/memory/tab_stats.h"

class BrowserList;
class GURL;

namespace memory {

#if defined(OS_CHROMEOS)
class OomPriorityManagerDelegate;
#endif

// The OomPriorityManager periodically updates (see
// |kAdjustmentIntervalSeconds| in the source) the status of renderers
// which are then used by the algorithm embedded here for priority in being
// killed upon OOM conditions.
//
// The algorithm used favors killing tabs that are not selected, not pinned,
// and have been idle for longest, in that order of priority.
//
// On Chrome OS (via the delegate), the kernel (via /proc/<pid>/oom_score_adj)
// will be informed of each renderer's score, which is based on the status, so
// in case Chrome is not able to relieve the pressure quickly enough and the
// kernel is forced to kill processes, it will be able to do so using the same
// algorithm as the one used here.
//
// Note that the browser tests are only active for platforms that use
// OomPriorityManager (CrOS only for now) and need to be adjusted accordingly if
// support for new platforms is added.
class OomPriorityManager {
 public:
  OomPriorityManager();
  ~OomPriorityManager();

  // Number of discard events since Chrome started.
  int discard_count() const { return discard_count_; }

  // See member comment.
  bool recent_tab_discard() const { return recent_tab_discard_; }

  void Start();
  void Stop();

  // Returns the list of the stats for all renderers. Must be called on the UI
  // thread.
  TabStatsList GetTabStats();

  // Discards a tab to free the memory occupied by its renderer. The tab still
  // exists in the tab-strip; clicking on it will reload it. Returns true if it
  // successfully found a tab and discarded it.
  bool DiscardTab();

  // Discards a tab with the given unique ID. The tab still exists in the
  // tab-strip; clicking on it will reload it. Returns true if it successfully
  // found a tab and discarded it.
  bool DiscardTabById(int64 target_web_contents_id);

  // Log memory statistics for the running processes, then discards a tab.
  // Tab discard happens sometime later, as collecting the statistics touches
  // multiple threads and takes time.
  void LogMemoryAndDiscardTab();

  // Log memory statistics for the running processes, then call the callback.
  void LogMemory(const std::string& title, const base::Closure& callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(OomPriorityManagerTest, Comparator);
  FRIEND_TEST_ALL_PREFIXES(OomPriorityManagerTest, IsInternalPage);

  static void PurgeMemoryAndDiscardTab();

  // Returns true if the |url| represents an internal Chrome web UI page that
  // can be easily reloaded and hence makes a good choice to discard.
  static bool IsInternalPage(const GURL& url);

  // Records UMA histogram statistics for a tab discard. We record statistics
  // for user triggered discards via chrome://discards/ because that allows us
  // to manually test the system.
  void RecordDiscardStatistics();

  // Record whether we ran out of memory during a recent time interval.
  // This allows us to normalize low memory statistics versus usage.
  void RecordRecentTabDiscard();

  // Purges data structures in the browser that can be easily recomputed.
  void PurgeBrowserMemory();

  // Returns the number of tabs open in all browser instances.
  int GetTabCount() const;

  // Adds all the stats of the tabs in |browser_list| into |stats_list|. If
  // |active_desktop| is true, we consider its first window as being active.
  void AddTabStats(BrowserList* browser_list,
                   bool active_desktop,
                   TabStatsList* stats_list);

  // Callback for when |update_timer_| fires. Takes care of executing the tasks
  // that need to be run periodically (see comment in implementation).
  void UpdateTimerCallback();

  static bool CompareTabStats(TabStats first, TabStats second);

  // Called by the memory pressure listener when the memory pressure rises.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Timer to periodically update the stats of the renderers.
  base::RepeatingTimer<OomPriorityManager> update_timer_;

  // Timer to periodically report whether a tab has been discarded since the
  // last time the timer has fired.
  base::RepeatingTimer<OomPriorityManager> recent_tab_discard_timer_;

  // A listener to global memory pressure events.
  scoped_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Wall-clock time when the priority manager started running.
  base::TimeTicks start_time_;

  // Wall-clock time of last tab discard during this browsing session, or 0 if
  // no discard has happened yet.
  base::TimeTicks last_discard_time_;

  // Wall-clock time of last priority adjustment, used to correct the above
  // times for discontinuities caused by suspend/resume.
  base::TimeTicks last_adjust_time_;

  // Number of times we have discarded a tab, for statistics.
  int discard_count_;

  // Whether a tab discard event has occurred during the last time interval,
  // used for statistics normalized by usage.
  bool recent_tab_discard_;

#if defined(OS_CHROMEOS)
  scoped_ptr<OomPriorityManagerDelegate> delegate_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OomPriorityManager);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_OOM_PRIORITY_MANAGER_H_
