// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/memory_pressure_listener.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory/oom_priority_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using content::OpenURLParams;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

namespace memory {
namespace {

class OomPriorityManagerTest : public InProcessBrowserTest {
 public:
  // Tab discarding is enabled by default on CrOS, on other platforms, force it
  // by setting the command line flag.
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if !defined(OS_CHROMEOS)
    command_line->AppendSwitch(switches::kEnableTabDiscarding);
#endif
  }

};

IN_PROC_BROWSER_TEST_F(OomPriorityManagerTest, OomPriorityManagerBasics) {
  using content::WindowedNotificationObserver;
  OomPriorityManager* oom_priority_manager =
      g_browser_process->GetOomPriorityManager();
  ASSERT_TRUE(oom_priority_manager);
  EXPECT_FALSE(oom_priority_manager->recent_tab_discard());

  // Get three tabs open.
  WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();

  WindowedNotificationObserver load3(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open3(GURL(chrome::kChromeUITermsURL), content::Referrer(),
                      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open3);
  load3.Wait();

  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Navigate the current (third) tab to a different URL, so we can test
  // back/forward later.
  WindowedNotificationObserver load4(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open4(GURL(chrome::kChromeUIVersionURL), content::Referrer(),
                      CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open4);
  load4.Wait();

  // Navigate the third tab again, such that we have three navigation entries.
  WindowedNotificationObserver load5(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open5(GURL("chrome://dns"), content::Referrer(), CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open5);
  load5.Wait();

  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Discard a tab.  It should kill the first tab, since it was the oldest
  // and was not selected.
  EXPECT_TRUE(oom_priority_manager->DiscardTab());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabDiscarded(0));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(1));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(2));
  EXPECT_TRUE(oom_priority_manager->recent_tab_discard());

  // Run discard again, make sure it kills the second tab.
  EXPECT_TRUE(oom_priority_manager->DiscardTab());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabDiscarded(0));
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabDiscarded(1));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(2));

  // Kill the third tab. It should not kill the last tab, since it is active
  // tab.
  EXPECT_FALSE(oom_priority_manager->DiscardTab());
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabDiscarded(0));
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabDiscarded(1));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(2));

  // Kill the third tab after making second tab active.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(1));
  browser()->tab_strip_model()->DiscardWebContentsAt(2);
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabDiscarded(2));

  // Force creation of the FindBarController.
  browser()->GetFindBarController();

  // Select the first tab.  It should reload.
  WindowedNotificationObserver reload1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  chrome::SelectNumberedTab(browser(), 0);
  reload1.Wait();
  // Make sure the FindBarController gets the right WebContents.
  EXPECT_EQ(browser()->GetFindBarController()->web_contents(),
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(0));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(1));
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabDiscarded(2));

  // Select the third tab. It should reload.
  WindowedNotificationObserver reload2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  chrome::SelectNumberedTab(browser(), 2);
  reload2.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(0));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(1));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabDiscarded(2));

  // Navigate the third tab back twice.  We used to crash here due to
  // crbug.com/121373.
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  WindowedNotificationObserver back1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  chrome::GoBack(browser(), CURRENT_TAB);
  back1.Wait();
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
  WindowedNotificationObserver back2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  chrome::GoBack(browser(), CURRENT_TAB);
  back2.Wait();
  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
}

// Test that the MemoryPressureListener event is properly triggering a tab
// discard upon |MEMORY_PRESSURE_LEVEL_CRITICAL| event.
IN_PROC_BROWSER_TEST_F(OomPriorityManagerTest, OomPressureListener) {
  OomPriorityManager* oom_priority_manager =
      g_browser_process->GetOomPriorityManager();
  ASSERT_TRUE(oom_priority_manager);

  // Get three tabs open.
  content::WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  content::WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();
  EXPECT_FALSE(oom_priority_manager->recent_tab_discard());

  // Nothing should happen with a moderate memory pressure event.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(oom_priority_manager->recent_tab_discard());

  // A critical memory pressure event should discard a tab.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // Coming here, an asynchronous operation will collect system stats. Once in,
  // a tab should get discarded. As such we need to give it 10s time to discard.
  const int kTimeoutTimeInMS = 10000;
  const int kIntervalTimeInMS = 5;
  int timeout = kTimeoutTimeInMS / kIntervalTimeInMS;
  while (--timeout) {
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kIntervalTimeInMS));
    base::RunLoop().RunUntilIdle();
    if (oom_priority_manager->recent_tab_discard())
      break;
  }
  EXPECT_TRUE(oom_priority_manager->recent_tab_discard());
}

}  // namespace
}  // namespace memory

#endif  // OS_WIN || OS_CHROMEOS
