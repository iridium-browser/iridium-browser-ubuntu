// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/after_startup_task_utils.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_info.h"
#include "base/rand_util.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/task_runner.h"
#include "base/tracked_objects.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::BrowserThread;
using content::WebContents;
using content::WebContentsObserver;
using StartupCompleteFlag = base::CancellationFlag;

namespace {

struct AfterStartupTask {
  AfterStartupTask(const tracked_objects::Location& from_here,
                   const scoped_refptr<base::TaskRunner>& task_runner,
                   const base::Closure& task)
      : from_here(from_here), task_runner(task_runner), task(task) {}
  ~AfterStartupTask() {}

  const tracked_objects::Location from_here;
  const scoped_refptr<base::TaskRunner> task_runner;
  const base::Closure task;
};

// The flag may be read on any thread, but must only be set on the UI thread.
base::LazyInstance<StartupCompleteFlag>::Leaky g_startup_complete_flag;

// The queue may only be accessed on the UI thread.
base::LazyInstance<std::deque<AfterStartupTask*>>::Leaky g_after_startup_tasks;

bool IsBrowserStartupComplete() {
  // Be sure to initialize the LazyInstance on the main thread since the flag
  // may only be set on it's initializing thread.
  if (g_startup_complete_flag == nullptr)
    return false;
  return g_startup_complete_flag.Get().IsSet();
}

void RunTask(scoped_ptr<AfterStartupTask> queued_task) {
  // We're careful to delete the caller's |task| on the target runner's thread.
  DCHECK(queued_task->task_runner->RunsTasksOnCurrentThread());
  queued_task->task.Run();
}

void ScheduleTask(scoped_ptr<AfterStartupTask> queued_task) {
  // Spread their execution over a brief time.
  const int kMinDelaySec = 0;
  const int kMaxDelaySec = 10;
  scoped_refptr<base::TaskRunner> target_runner = queued_task->task_runner;
  tracked_objects::Location from_here = queued_task->from_here;
  target_runner->PostDelayedTask(
      from_here, base::Bind(&RunTask, base::Passed(queued_task.Pass())),
      base::TimeDelta::FromSeconds(base::RandInt(kMinDelaySec, kMaxDelaySec)));
}

void QueueTask(scoped_ptr<AfterStartupTask> queued_task) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(QueueTask, base::Passed(queued_task.Pass())));
    return;
  }

  // The flag may have been set while the task to invoke this method
  // on the UI thread was inflight.
  if (IsBrowserStartupComplete()) {
    ScheduleTask(queued_task.Pass());
    return;
  }
  g_after_startup_tasks.Get().push_back(queued_task.release());
}

void SetBrowserStartupIsComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  // CurrentProcessInfo::CreationTime() is not available on all platforms.
  const base::Time process_creation_time =
      base::CurrentProcessInfo::CreationTime();
  if (!process_creation_time.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Startup.AfterStartupTaskDelayedUntilTime",
                             base::Time::Now() - process_creation_time);
  }
#endif  // defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  UMA_HISTOGRAM_COUNTS_10000("Startup.AfterStartupTaskCount",
                             g_after_startup_tasks.Get().size());
  g_startup_complete_flag.Get().Set();
  for (AfterStartupTask* queued_task : g_after_startup_tasks.Get())
    ScheduleTask(make_scoped_ptr(queued_task));
  g_after_startup_tasks.Get().clear();

  // The shrink_to_fit() method is not available for all of our build targets.
  std::deque<AfterStartupTask*>(g_after_startup_tasks.Get())
      .swap(g_after_startup_tasks.Get());
}

// Observes the first visible page load and sets the startup complete
// flag accordingly.
class StartupObserver : public WebContentsObserver, public base::NonThreadSafe {
 public:
  StartupObserver() : weak_factory_(this) {}
  ~StartupObserver() override { DCHECK(IsBrowserStartupComplete()); }

  void Start();

 private:
  void OnStartupComplete() {
    DCHECK(CalledOnValidThread());
    SetBrowserStartupIsComplete();
    delete this;
  }

  void OnFailsafeTimeout() { OnStartupComplete(); }

  // WebContentsObserver overrides
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host->GetParent())
      OnStartupComplete();
  }

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description,
                   bool was_ignored_by_handler) override {
    if (!render_frame_host->GetParent())
      OnStartupComplete();
  }

  void WebContentsDestroyed() override { OnStartupComplete(); }

  base::WeakPtrFactory<StartupObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StartupObserver);
};

void StartupObserver::Start() {
  // Signal completion quickly when there is no first page to load.
  const int kShortDelaySecs = 3;
  base::TimeDelta delay = base::TimeDelta::FromSeconds(kShortDelaySecs);

#if !defined(OS_ANDROID)
  WebContents* contents = nullptr;
  for (chrome::BrowserIterator iter; !iter.done(); iter.Next()) {
    contents = (*iter)->tab_strip_model()->GetActiveWebContents();
    if (contents && contents->GetMainFrame() &&
        contents->GetMainFrame()->GetVisibilityState() ==
            blink::WebPageVisibilityStateVisible) {
      break;
    }
  }

  if (contents) {
    // Give the page time to finish loading.
    const int kLongerDelayMins = 3;
    Observe(contents);
    delay = base::TimeDelta::FromMinutes(kLongerDelayMins);
  }
#else
  // Startup completion is signaled via AfterStartupTaskUtils.java,
  // this is just a failsafe timeout.
  const int kLongerDelayMins = 3;
  delay = base::TimeDelta::FromMinutes(kLongerDelayMins);
#endif  // !defined(OS_ANDROID)

  BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
                                 base::Bind(&StartupObserver::OnFailsafeTimeout,
                                            weak_factory_.GetWeakPtr()),
                                 delay);
}

}  // namespace

void AfterStartupTaskUtils::StartMonitoringStartup() {
  // The observer is self-deleting.
  (new StartupObserver)->Start();
}

void AfterStartupTaskUtils::PostTask(
    const tracked_objects::Location& from_here,
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Closure& task) {
  if (IsBrowserStartupComplete()) {
    task_runner->PostTask(from_here, task);
    return;
  }

  scoped_ptr<AfterStartupTask> queued_task(
      new AfterStartupTask(from_here, task_runner, task));
  QueueTask(queued_task.Pass());
}

void AfterStartupTaskUtils::SetBrowserStartupIsComplete() {
  ::SetBrowserStartupIsComplete();
}

bool AfterStartupTaskUtils::IsBrowserStartupComplete() {
  return ::IsBrowserStartupComplete();
}

void AfterStartupTaskUtils::UnsafeResetForTesting() {
  DCHECK(g_after_startup_tasks.Get().empty());
  if (!IsBrowserStartupComplete())
    return;
  g_startup_complete_flag.Get().UnsafeResetForTesting();
  DCHECK(!IsBrowserStartupComplete());
}
