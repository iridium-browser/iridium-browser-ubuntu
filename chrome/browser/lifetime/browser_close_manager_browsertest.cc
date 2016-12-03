// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/browser_close_manager.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/test/url_request/url_request_slow_download_job.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

namespace {

app_modal::NativeAppModalDialog* GetNextDialog() {
  app_modal::AppModalDialog* dialog = ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(dialog->IsJavaScriptModalDialog());
  app_modal::JavaScriptAppModalDialog* js_dialog =
      static_cast<app_modal::JavaScriptAppModalDialog*>(dialog);
  CHECK(js_dialog->native_dialog());
  return js_dialog->native_dialog();
}

// Note: call |DisableHangMonitor| on the relevant WebContents or Browser before
// trying to close it, to avoid flakiness. https://crbug.com/519646
void AcceptClose() {
  GetNextDialog()->AcceptAppModalDialog();
}

// Note: call |DisableHangMonitor| on the relevant WebContents or Browser before
// trying to close it, to avoid flakiness. https://crbug.com/519646
void CancelClose() {
  GetNextDialog()->CancelAppModalDialog();
}

class RepeatedNotificationObserver : public content::NotificationObserver {
 public:
  explicit RepeatedNotificationObserver(int type, int count)
      : num_outstanding_(count), running_(false) {
    registrar_.Add(this, type, content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ASSERT_GT(num_outstanding_, 0);
    if (!--num_outstanding_ && running_) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE, run_loop_.QuitClosure());
    }
  }

  void Wait() {
    if (num_outstanding_ <= 0)
      return;

    running_ = true;
    run_loop_.Run();
    running_ = false;
  }

 private:
  int num_outstanding_;
  content::NotificationRegistrar registrar_;
  bool running_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RepeatedNotificationObserver);
};

class TestBrowserCloseManager : public BrowserCloseManager {
 public:
  enum UserChoice {
    USER_CHOICE_USER_CANCELS_CLOSE,
    USER_CHOICE_USER_ALLOWS_CLOSE,
    NO_USER_CHOICE
  };

  static void AttemptClose(UserChoice user_choice) {
    scoped_refptr<BrowserCloseManager> browser_close_manager =
        new TestBrowserCloseManager(user_choice);
    browser_shutdown::SetTryingToQuit(true);
    browser_close_manager->StartClosingBrowsers();
  }

 protected:
  ~TestBrowserCloseManager() override {}

  void ConfirmCloseWithPendingDownloads(
      int download_count,
      const base::Callback<void(bool)>& callback) override {
    EXPECT_NE(NO_USER_CHOICE, user_choice_);
    switch (user_choice_) {
      case NO_USER_CHOICE:
      case USER_CHOICE_USER_CANCELS_CLOSE: {
        callback.Run(false);
        break;
      }
      case USER_CHOICE_USER_ALLOWS_CLOSE: {
        callback.Run(true);
        break;
      }
    }
  }

 private:
  explicit TestBrowserCloseManager(UserChoice user_choice)
      : user_choice_(user_choice) {}

  UserChoice user_choice_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserCloseManager);
};

class TestDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
    GetDownloadIdReceiverCallback().Run(content::DownloadItem::kInvalidId + 1);
  }
  ~TestDownloadManagerDelegate() override {}

  bool DetermineDownloadTarget(
      content::DownloadItem* item,
      const content::DownloadTargetCallback& callback) override {
    content::DownloadTargetCallback dangerous_callback =
        base::Bind(&TestDownloadManagerDelegate::SetDangerous, callback);
    return ChromeDownloadManagerDelegate::DetermineDownloadTarget(
        item, dangerous_callback);
  }

  static void SetDangerous(
      const content::DownloadTargetCallback& callback,
      const base::FilePath& target_path,
      content::DownloadItem::TargetDisposition disp,
      content::DownloadDangerType danger_type,
      const base::FilePath& intermediate_path) {
    callback.Run(target_path,
                 disp,
                 content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                 intermediate_path);
  }
};

class FakeBackgroundModeManager : public BackgroundModeManager {
 public:
  FakeBackgroundModeManager()
      : BackgroundModeManager(*base::CommandLine::ForCurrentProcess(),
                              &g_browser_process->profile_manager()
                                  ->GetProfileAttributesStorage()),
        suspended_(false) {}

  void SuspendBackgroundMode() override {
    BackgroundModeManager::SuspendBackgroundMode();
    suspended_ = true;
  }

  void ResumeBackgroundMode() override {
    BackgroundModeManager::ResumeBackgroundMode();
    suspended_ = false;
  }

  bool IsBackgroundModeSuspended() {
    return suspended_;
  }

 private:
  bool suspended_;

  DISALLOW_COPY_AND_ASSIGN(FakeBackgroundModeManager);
};

}  // namespace

class BrowserCloseManagerBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
    browsers_.push_back(browser());
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam())
      command_line->AppendSwitch(switches::kEnableFastUnload);
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void CreateStalledDownload(Browser* browser) {
    content::DownloadTestObserverInProgress observer(
        content::BrowserContext::GetDownloadManager(browser->profile()), 1);
    ui_test_utils::NavigateToURLWithDisposition(
        browser,
        GURL(net::URLRequestSlowDownloadJob::kKnownSizeUrl),
        NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    observer.WaitForFinished();
    EXPECT_EQ(
        1UL,
        observer.NumDownloadsSeenInState(content::DownloadItem::IN_PROGRESS));
  }

  // Makes sure that hang monitor will not trigger RendererUnresponsive
  // for that web content or browser. That must be called before close action
  // when using |AcceptClose| or |CancelClose|, to ensure the timeout does not
  // prevent the dialog from appearing. https://crbug.com/519646
  void DisableHangMonitor(content::WebContents* web_contents) {
    web_contents->GetRenderViewHost()
        ->GetWidget()
        ->DisableHangMonitorForTesting();
  }

  void DisableHangMonitor(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); i++)
      DisableHangMonitor(browser->tab_strip_model()->GetWebContentsAt(i));
  }

  std::vector<Browser*> browsers_;
};

IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest, TestSingleTabShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browser());

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 1);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestShutdownMoreThanOnce) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browser());

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 1);
  chrome::CloseAllBrowsersAndQuit();
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);
  chrome::CloseAllBrowsersAndQuit();
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       PRE_TestSessionRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/beforeunload.html")));
  AddBlankTabAndShow(browser());
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL)));
  DisableHangMonitor(browser());

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 1);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());

  browser()->tab_strip_model()
      ->CloseWebContentsAt(1, TabStripModel::CLOSE_USER_GESTURE);
  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  ASSERT_NO_FATAL_FAILURE(
      NavigateToURLWithDisposition(browser(),
                                   GURL(chrome::kChromeUIVersionURL),
                                   CURRENT_TAB,
                                   ui_test_utils::BROWSER_TEST_NONE));
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  navigation_observer.Wait();

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);
  chrome::CloseAllBrowsersAndQuit();
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that the tab closed after the aborted shutdown attempt is not re-opened
// when restoring the session.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestSessionRestore) {
  // The testing framework launches Chrome with about:blank as args.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUIVersionURL),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GURL("about:blank"),
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

// Test that browser windows are only closed if all browsers are ready to close
// and that all beforeunload dialogs are shown again after a cancel.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest, TestMultipleWindows) {
  ASSERT_TRUE(embedded_test_server()->Start());
  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);
  DisableHangMonitor(browsers_[1]);

  // Cancel shutdown on the first beforeunload event.
  {
    RepeatedNotificationObserver cancel_observer(
        chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 1);
    chrome::CloseAllBrowsersAndQuit();
    ASSERT_NO_FATAL_FAILURE(CancelClose());
    cancel_observer.Wait();
  }
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(1, browsers_[0]->tab_strip_model()->count());
  EXPECT_EQ(1, browsers_[1]->tab_strip_model()->count());

  // Cancel shutdown on the second beforeunload event.
  {
    RepeatedNotificationObserver cancel_observer(
        chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 2);
    chrome::CloseAllBrowsersAndQuit();
    ASSERT_NO_FATAL_FAILURE(AcceptClose());
    ASSERT_NO_FATAL_FAILURE(CancelClose());
    cancel_observer.Wait();
  }
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(1, browsers_[0]->tab_strip_model()->count());
  EXPECT_EQ(1, browsers_[1]->tab_strip_model()->count());

  // Allow shutdown for both beforeunload events.
  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that tabs in the same window with a beforeunload event that hangs are
// treated the same as the user accepting the close, but do not close the tab
// early.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestHangInBeforeUnloadMultipleTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload_hang.html")));
  AddBlankTabAndShow(browsers_[0]);
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  AddBlankTabAndShow(browsers_[0]);
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload_hang.html")));
  // Disable the hang monitor in the tab that is not expected to hang, so that
  // the dialog is guaranteed to show.
  DisableHangMonitor(browsers_[0]->tab_strip_model()->GetWebContentsAt(1));

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 1);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  // All tabs should still be open.
  EXPECT_EQ(3, browsers_[0]->tab_strip_model()->count());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that tabs in different windows with a beforeunload event that hangs are
// treated the same as the user accepting the close, but do not close the tab
// early.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestHangInBeforeUnloadMultipleWindows) {
  ASSERT_TRUE(embedded_test_server()->Start());
  browsers_.push_back(CreateBrowser(browser()->profile()));
  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload_hang.html")));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[2], embedded_test_server()->GetURL("/beforeunload_hang.html")));
  // Disable the hang monitor in the tab that is not expected to hang, so that
  // the dialog is guaranteed to show.
  DisableHangMonitor(browsers_[1]);

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 2);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  // All windows should still be open.
  EXPECT_EQ(1, browsers_[0]->tab_strip_model()->count());
  EXPECT_EQ(1, browsers_[1]->tab_strip_model()->count());
  EXPECT_EQ(1, browsers_[2]->tab_strip_model()->count());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 3);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that tabs that are slow to respond are not closed prematurely.
// Regression for crbug.com/365052 caused some of tabs to be closed even if
// user chose to cancel browser close.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestUnloadMultipleSlowTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const int kTabCount = 5;
  const int kResposiveTabIndex = 2;
  // Create tab strip with all tabs except one responding after
  // RenderViewHostImpl::kUnloadTimeoutMS.
  // Minimum configuration is two slow tabs and then responsive tab.
  // But we also want to check how slow tabs behave in tail.
  for (int i = 0; i < kTabCount; i++) {
    if (i)
      AddBlankTabAndShow(browsers_[0]);
    ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
        browsers_[0],
        embedded_test_server()->GetURL((i == kResposiveTabIndex)
                                           ? "/beforeunload.html"
                                           : "/beforeunload_slow.html")));
  }
  // Disable the hang monitor in the tab that is not expected to hang, so that
  // the dialog is guaranteed to show.
  DisableHangMonitor(
      browsers_[0]->tab_strip_model()->GetWebContentsAt(kResposiveTabIndex));

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 1);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());

  // All tabs should still be open.
  EXPECT_EQ(kTabCount, browsers_[0]->tab_strip_model()->count());
  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);

  // Quit, this time accepting close confirmation dialog.
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that tabs in different windows with a slow beforeunload event response
// are treated the same as the user accepting the close, but do not close the
// tab early.
// Regression for crbug.com/365052 caused CHECK in tabstrip.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestBeforeUnloadMultipleSlowWindows) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const int kBrowserCount = 5;
  const int kResposiveBrowserIndex = 2;
  // Create multiple browsers with all tabs except one responding after
  // RenderViewHostImpl::kUnloadTimeoutMS .
  // Minimum configuration is just one browser with slow tab and then
  // browser with responsive tab.
  // But we also want to check how slow tabs behave in tail and make test
  // more robust.
  for (int i = 0; i < kBrowserCount; i++) {
    if (i)
      browsers_.push_back(CreateBrowser(browser()->profile()));
    ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
        browsers_[i],
        embedded_test_server()->GetURL((i == kResposiveBrowserIndex)
                                           ? "/beforeunload.html"
                                           : "/beforeunload_slow.html")));
  }
  // Disable the hang monitor in the tab that is not expected to hang, so that
  // the dialog is guaranteed to show.
  DisableHangMonitor(browsers_[kResposiveBrowserIndex]);

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, kResposiveBrowserIndex + 1);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());

  // All windows should still be open.
  for (int i = 0; i < kBrowserCount; i++)
    EXPECT_EQ(1, browsers_[i]->tab_strip_model()->count());

  // Quit, this time accepting close confirmation dialog.
  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, kBrowserCount);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that a window created during shutdown is closed.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestAddWindowDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  chrome::CloseAllBrowsersAndQuit();
  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that a window created during shutdown with a beforeunload handler can
// cancel the shutdown.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestAddWindowWithBeforeUnloadDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 2);
  chrome::CloseAllBrowsersAndQuit();
  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[1]);
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(1, browsers_[0]->tab_strip_model()->count());
  EXPECT_EQ(1, browsers_[1]->tab_strip_model()->count());

  // Allow shutdown for both beforeunload dialogs.
  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that tabs added during shutdown are closed.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestAddTabDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);
  DisableHangMonitor(browsers_[1]);

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  AddBlankTabAndShow(browsers_[0]);
  AddBlankTabAndShow(browsers_[1]);
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test that tabs created during shutdown with beforeunload handlers can cancel
// the shutdown.

IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestAddTabWithBeforeUnloadDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);
  DisableHangMonitor(browsers_[1]);

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 2);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  AddBlankTabAndShow(browsers_[0]);
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  AddBlankTabAndShow(browsers_[1]);
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);
  DisableHangMonitor(browsers_[1]);
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(2, browsers_[0]->tab_strip_model()->count());
  EXPECT_EQ(2, browsers_[1]->tab_strip_model()->count());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());

  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestCloseTabDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 1);
  chrome::CloseAllBrowsersAndQuit();

  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[1]);
  browsers_[1]->tab_strip_model()->CloseAllTabs();
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(1, browsers_[0]->tab_strip_model()->count());
  EXPECT_EQ(1, browsers_[1]->tab_strip_model()->count());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  chrome::CloseAllBrowsersAndQuit();
  browsers_[1]->tab_strip_model()->CloseAllTabs();
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());

  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestOpenAndCloseWindowDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 2);
  chrome::CloseAllBrowsersAndQuit();

  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[1]);
  ASSERT_FALSE(browsers_[1]->ShouldCloseWindow());
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(1, browsers_[0]->tab_strip_model()->count());
  EXPECT_EQ(1, browsers_[1]->tab_strip_model()->count());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_FALSE(browsers_[1]->ShouldCloseWindow());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());

  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

IN_PROC_BROWSER_TEST_P(BrowserCloseManagerBrowserTest,
                       TestCloseWindowDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[0], embedded_test_server()->GetURL("/beforeunload.html")));
  browsers_.push_back(CreateBrowser(browser()->profile()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browsers_[1], embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browsers_[0]);
  DisableHangMonitor(browsers_[1]);

  RepeatedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED, 1);
  chrome::CloseAllBrowsersAndQuit();

  ASSERT_FALSE(browsers_[0]->ShouldCloseWindow());
  ASSERT_NO_FATAL_FAILURE(CancelClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_EQ(1, browsers_[0]->tab_strip_model()->count());
  EXPECT_EQ(1, browsers_[1]->tab_strip_model()->count());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  chrome::CloseAllBrowsersAndQuit();
  ASSERT_FALSE(browsers_[0]->ShouldCloseWindow());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  ASSERT_NO_FATAL_FAILURE(AcceptClose());

  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

INSTANTIATE_TEST_CASE_P(BrowserCloseManagerBrowserTest,
                        BrowserCloseManagerBrowserTest,
                        testing::Bool());

class BrowserCloseManagerWithDownloadsBrowserTest :
  public BrowserCloseManagerBrowserTest {
 public:
  BrowserCloseManagerWithDownloadsBrowserTest() {}
  virtual ~BrowserCloseManagerWithDownloadsBrowserTest() {}

  void SetUpOnMainThread() override {
    BrowserCloseManagerBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(scoped_download_directory_.CreateUniqueTempDir());
  }

  void SetDownloadPathForProfile(Profile* profile) {
    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(profile);
    download_prefs->SetDownloadPath(download_path());
  }

  const base::FilePath& download_path() const {
    return scoped_download_directory_.path();
  }

 private:
  base::ScopedTempDir scoped_download_directory_;
};

// Mac has its own in-progress download prompt in app_controller_mac.mm, so
// BrowserCloseManager should simply close all browsers. If there are no
// browsers, it should not crash.
#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithDownloadsBrowserTest,
                       TestWithDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetDownloadPathForProfile(browser()->profile());
  ASSERT_NO_FATAL_FAILURE(CreateStalledDownload(browser()));

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);

  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::NO_USER_CHOICE);
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_EQ(1, DownloadService::NonMaliciousDownloadCountAllProfiles());

  // Attempting to close again should not crash.
  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::NO_USER_CHOICE);
}
#else  // defined(OS_MACOSX)

// Test shutdown with a DANGEROUS_URL download undecided.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithDownloadsBrowserTest,
    TestWithDangerousUrlDownload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetDownloadPathForProfile(browser()->profile());

  // Set up the fake delegate that forces the download to be malicious.
  std::unique_ptr<TestDownloadManagerDelegate> test_delegate(
      new TestDownloadManagerDelegate(browser()->profile()));
  DownloadServiceFactory::GetForBrowserContext(browser()->profile())
      ->SetDownloadManagerDelegateForTesting(std::move(test_delegate));

  // Run a dangerous download, but the user doesn't make a decision.
  // This .swf normally would be categorized as DANGEROUS_FILE, but
  // TestDownloadManagerDelegate turns it into DANGEROUS_URL.
  GURL download_url(net::URLRequestMockHTTPJob::GetMockUrl(
      "downloads/dangerous/dangerous.swf"));
  content::DownloadTestObserverInterrupted observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(download_url),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  observer.WaitForFinished();

  // Check that the download manager has the expected state.
  EXPECT_EQ(1, content::BrowserContext::GetDownloadManager(
      browser()->profile())->InProgressCount());
  EXPECT_EQ(0, content::BrowserContext::GetDownloadManager(
      browser()->profile())->NonMaliciousInProgressCount());

  // Close the browser with no user action.
  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);
  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::NO_USER_CHOICE);
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

// Test shutdown with a download in progress.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithDownloadsBrowserTest,
                       TestWithDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetDownloadPathForProfile(browser()->profile());
  ASSERT_NO_FATAL_FAILURE(CreateStalledDownload(browser()));
  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::USER_CHOICE_USER_CANCELS_CLOSE);
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  navigation_observer.Wait();
  EXPECT_EQ(GURL(chrome::kChromeUIDownloadsURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);

  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::USER_CHOICE_USER_ALLOWS_CLOSE);
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  if (browser_defaults::kBrowserAliveWithNoWindows)
    EXPECT_EQ(1, DownloadService::NonMaliciousDownloadCountAllProfiles());
  else
    EXPECT_EQ(0, DownloadService::NonMaliciousDownloadCountAllProfiles());
}

// Test shutdown with a download in progress in an off-the-record profile.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithDownloadsBrowserTest,
                       TestWithOffTheRecordDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());
  Profile* otr_profile = browser()->profile()->GetOffTheRecordProfile();
  SetDownloadPathForProfile(otr_profile);
  Browser* otr_browser = CreateBrowser(otr_profile);
  {
    RepeatedNotificationObserver close_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED, 1);
    browser()->window()->Close();
    close_observer.Wait();
  }
  ASSERT_NO_FATAL_FAILURE(CreateStalledDownload(otr_browser));
  content::TestNavigationObserver navigation_observer(
      otr_browser->tab_strip_model()->GetActiveWebContents(), 1);
  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::USER_CHOICE_USER_CANCELS_CLOSE);
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  navigation_observer.Wait();
  EXPECT_EQ(GURL(chrome::kChromeUIDownloadsURL),
            otr_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);

  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::USER_CHOICE_USER_ALLOWS_CLOSE);
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_EQ(0, DownloadService::NonMaliciousDownloadCountAllProfiles());
}

// Test shutdown with a download in progress from one profile, where the only
// open windows are for another profile.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithDownloadsBrowserTest,
                       TestWithDownloadsFromDifferentProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath path =
      profile_manager->user_data_dir().AppendASCII("test_profile");
  if (!base::PathExists(path))
    ASSERT_TRUE(base::CreateDirectory(path));
  Profile* other_profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  profile_manager->RegisterTestingProfile(other_profile, true, false);
  Browser* other_profile_browser = CreateBrowser(other_profile);

  ASSERT_TRUE(embedded_test_server()->Start());
  SetDownloadPathForProfile(browser()->profile());
  SetDownloadPathForProfile(other_profile);
  ASSERT_NO_FATAL_FAILURE(CreateStalledDownload(browser()));
  {
    RepeatedNotificationObserver close_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED, 1);
    browser()->window()->Close();
    close_observer.Wait();
  }

  // When the shutdown is cancelled, the downloads page should be opened in a
  // browser for that profile. Because there are no browsers for that profile, a
  // new browser should be opened.
  ui_test_utils::BrowserAddedObserver new_browser_observer;
  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::USER_CHOICE_USER_CANCELS_CLOSE);
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  Browser* opened_browser = new_browser_observer.WaitForSingleNewBrowser();
  EXPECT_EQ(
      GURL(chrome::kChromeUIDownloadsURL),
      opened_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  EXPECT_EQ(GURL("about:blank"),
            other_profile_browser->tab_strip_model()->GetActiveWebContents()
                ->GetURL());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 2);
  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::USER_CHOICE_USER_ALLOWS_CLOSE);
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  if (browser_defaults::kBrowserAliveWithNoWindows)
    EXPECT_EQ(1, DownloadService::NonMaliciousDownloadCountAllProfiles());
  else
    EXPECT_EQ(0, DownloadService::NonMaliciousDownloadCountAllProfiles());
}

// Test shutdown with downloads in progress and beforeunload handlers.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithDownloadsBrowserTest,
                       TestBeforeUnloadAndDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetDownloadPathForProfile(browser()->profile());
  ASSERT_NO_FATAL_FAILURE(CreateStalledDownload(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/beforeunload.html")));
  DisableHangMonitor(browser());

  content::WindowedNotificationObserver cancel_observer(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
      content::NotificationService::AllSources());
  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::USER_CHOICE_USER_CANCELS_CLOSE);
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  cancel_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());

  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);
  TestBrowserCloseManager::AttemptClose(
      TestBrowserCloseManager::USER_CHOICE_USER_ALLOWS_CLOSE);
  ASSERT_NO_FATAL_FAILURE(AcceptClose());
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

#endif  // defined(OS_MACOSX)

INSTANTIATE_TEST_CASE_P(BrowserCloseManagerWithDownloadsBrowserTest,
                        BrowserCloseManagerWithDownloadsBrowserTest,
                        testing::Bool());

class BrowserCloseManagerWithBackgroundModeBrowserTest
    : public BrowserCloseManagerBrowserTest {
 public:
  BrowserCloseManagerWithBackgroundModeBrowserTest() {}

  void SetUpOnMainThread() override {
    BrowserCloseManagerBrowserTest::SetUpOnMainThread();
    g_browser_process->set_background_mode_manager_for_test(
        std::unique_ptr<BackgroundModeManager>(new FakeBackgroundModeManager));
  }

  bool IsBackgroundModeSuspended() {
    return static_cast<FakeBackgroundModeManager*>(
        g_browser_process->background_mode_manager())
        ->IsBackgroundModeSuspended();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCloseManagerWithBackgroundModeBrowserTest);
};

// Check that background mode is suspended when closing all browsers unless we
// are quitting and that background mode is resumed when a new browser window is
// opened.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithBackgroundModeBrowserTest,
                       CloseAllBrowsersWithBackgroundMode) {
  EXPECT_FALSE(IsBackgroundModeSuspended());
  std::unique_ptr<ScopedKeepAlive> tmp_keep_alive;
  Profile* profile = browser()->profile();
  {
    RepeatedNotificationObserver close_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED, 1);
    tmp_keep_alive.reset(new ScopedKeepAlive(KeepAliveOrigin::PANEL_VIEW,
                                             KeepAliveRestartOption::DISABLED));
    chrome::CloseAllBrowsers();
    close_observer.Wait();
  }
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_TRUE(IsBackgroundModeSuspended());

  // Background mode should be resumed when a new browser window is opened.
  ui_test_utils::BrowserAddedObserver new_browser_observer;
  chrome::NewEmptyWindow(profile);
  new_browser_observer.WaitForSingleNewBrowser();
  tmp_keep_alive.reset();
  EXPECT_FALSE(IsBackgroundModeSuspended());
  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);

  // Background mode should not be suspended when quitting.
  chrome::CloseAllBrowsersAndQuit();
  close_observer.Wait();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_FALSE(IsBackgroundModeSuspended());
}

// Check that closing the last browser window individually does not affect
// background mode.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithBackgroundModeBrowserTest,
                       DISABLED_CloseSingleBrowserWithBackgroundMode) {
  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);
  EXPECT_FALSE(IsBackgroundModeSuspended());
  browser()->window()->Close();
  close_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_FALSE(IsBackgroundModeSuspended());
}

// Check that closing all browsers with no browser windows open suspends
// background mode but does not cause Chrome to quit.
IN_PROC_BROWSER_TEST_P(BrowserCloseManagerWithBackgroundModeBrowserTest,
                       DISABLED_CloseAllBrowsersWithNoOpenBrowsersWithBackgroundMode) {
  RepeatedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, 1);
  EXPECT_FALSE(IsBackgroundModeSuspended());
  ScopedKeepAlive tmp_keep_alive(KeepAliveOrigin::PANEL_VIEW,
                                 KeepAliveRestartOption::DISABLED);
  browser()->window()->Close();
  close_observer.Wait();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_FALSE(IsBackgroundModeSuspended());

  chrome::CloseAllBrowsers();
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_TRUE(IsBackgroundModeSuspended());
}

INSTANTIATE_TEST_CASE_P(BrowserCloseManagerWithBackgroundModeBrowserTest,
                        BrowserCloseManagerWithBackgroundModeBrowserTest,
                        testing::Bool());
