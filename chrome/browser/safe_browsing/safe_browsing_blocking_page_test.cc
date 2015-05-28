// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a fake safebrowsing service, where we can inject known-
// threat urls.  It then uses a real browser to go to these urls, and sends
// "goback" or "proceed" commands and verifies they work.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"

using chrome_browser_interstitials::SecurityInterstitialIDNTest;
using content::BrowserThread;
using content::InterstitialPage;
using content::NavigationController;
using content::WebContents;

namespace {

const char kEmptyPage[] = "files/empty.html";
const char kMalwarePage[] = "files/safe_browsing/malware.html";
const char kMalwareIframe[] = "files/safe_browsing/malware_iframe.html";

// A SafeBrowsingDatabaseManager class that allows us to inject the malicious
// URLs.
class FakeSafeBrowsingDatabaseManager :  public SafeBrowsingDatabaseManager {
 public:
  explicit FakeSafeBrowsingDatabaseManager(SafeBrowsingService* service)
      : SafeBrowsingDatabaseManager(service) { }

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Overrides SafeBrowsingDatabaseManager::CheckBrowseUrl.
  bool CheckBrowseUrl(const GURL& gurl, Client* client) override {
    if (badurls[gurl.spec()] == SB_THREAT_TYPE_SAFE)
      return true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                   this, gurl, client));
    return false;
  }

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    std::vector<SBThreatType> expected_threats;
    expected_threats.push_back(SB_THREAT_TYPE_URL_MALWARE);
    expected_threats.push_back(SB_THREAT_TYPE_URL_PHISHING);
    expected_threats.push_back(SB_THREAT_TYPE_URL_UNWANTED);
    SafeBrowsingDatabaseManager::SafeBrowsingCheck sb_check(
        std::vector<GURL>(1, gurl),
        std::vector<SBFullHash>(),
        client,
        safe_browsing_util::MALWARE,
        expected_threats);
    sb_check.url_results[0] = badurls[gurl.spec()];
    client->OnSafeBrowsingResult(sb_check);
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    badurls[url.spec()] = threat_type;
  }

 private:
  ~FakeSafeBrowsingDatabaseManager() override {}

  base::hash_map<std::string, SBThreatType> badurls;
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

// A SafeBrowingUIManager class that allows intercepting malware details.
class FakeSafeBrowsingUIManager :  public SafeBrowsingUIManager {
 public:
  explicit FakeSafeBrowsingUIManager(SafeBrowsingService* service) :
      SafeBrowsingUIManager(service) { }

  // Overrides SafeBrowsingUIManager
  void SendSerializedMalwareDetails(const std::string& serialized) override {
    // Notify the UI thread that we got a report.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&FakeSafeBrowsingUIManager::OnMalwareDetailsDone,
                   this,
                   serialized));
  }

  void OnMalwareDetailsDone(const std::string& serialized) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    report_ = serialized;

    EXPECT_FALSE(malware_details_done_callback_.is_null());
    if (!malware_details_done_callback_.is_null()) {
      malware_details_done_callback_.Run();
      malware_details_done_callback_ = base::Closure();
    }
  }

  void set_malware_details_done_callback(const base::Closure& callback) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(malware_details_done_callback_.is_null());
    malware_details_done_callback_ = callback;
  }

  std::string GetReport() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return report_;
  }

 protected:
  ~FakeSafeBrowsingUIManager() override {}

 private:
  std::string report_;
  base::Closure malware_details_done_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingUIManager);
};

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  FakeSafeBrowsingService()
      : fake_database_manager_(),
        fake_ui_manager_() { }

  // Returned pointer has the same lifespan as the database_manager_ refcounted
  // object.
  FakeSafeBrowsingDatabaseManager* fake_database_manager() {
    return fake_database_manager_;
  }
  // Returned pointer has the same lifespan as the ui_manager_ refcounted
  // object.
  FakeSafeBrowsingUIManager* fake_ui_manager() {
    return fake_ui_manager_;
  }

 protected:
  ~FakeSafeBrowsingService() override {}

  SafeBrowsingDatabaseManager* CreateDatabaseManager() override {
    fake_database_manager_ = new FakeSafeBrowsingDatabaseManager(this);
    return fake_database_manager_;
  }

  SafeBrowsingUIManager* CreateUIManager() override {
    fake_ui_manager_ = new FakeSafeBrowsingUIManager(this);
    return fake_ui_manager_;
  }

 private:
  FakeSafeBrowsingDatabaseManager* fake_database_manager_;
  FakeSafeBrowsingUIManager* fake_ui_manager_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory() :
      most_recent_service_(NULL) { }
  ~TestSafeBrowsingServiceFactory() override {}

  SafeBrowsingService* CreateSafeBrowsingService() override {
    most_recent_service_ =  new FakeSafeBrowsingService();
    return most_recent_service_;
  }

  FakeSafeBrowsingService* most_recent_service() const {
    return most_recent_service_;
  }

 private:
  FakeSafeBrowsingService* most_recent_service_;
};

// A MalwareDetails class lets us intercept calls from the renderer.
class FakeMalwareDetails : public MalwareDetails {
 public:
  FakeMalwareDetails(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource)
      : MalwareDetails(delegate, web_contents, unsafe_resource),
        got_dom_(false),
        waiting_(false) { }

  void AddDOMDetails(
      const std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node>& params)
      override {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    MalwareDetails::AddDOMDetails(params);

    // Notify the UI thread that we got the dom details.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&FakeMalwareDetails::OnDOMDetailsDone,
                                       this));
  }

  void WaitForDOM() {
    if (got_dom_) {
      return;
    }
    // This condition might not trigger normally, but if you add a
    // sleep(1) in malware_dom_details it triggers :).
    waiting_ = true;
    content::RunMessageLoop();
    EXPECT_TRUE(got_dom_);
  }

 private:
  ~FakeMalwareDetails() override {}

  void OnDOMDetailsDone() {
    got_dom_ = true;
    if (waiting_) {
      base::MessageLoopForUI::current()->Quit();
    }
  }

  // Some logic to figure out if we should wait for the dom details or not.
  // These variables should only be accessed in the UI thread.
  bool got_dom_;
  bool waiting_;
};

class TestMalwareDetailsFactory : public MalwareDetailsFactory {
 public:
  TestMalwareDetailsFactory() : details_() { }
  ~TestMalwareDetailsFactory() override {}

  MalwareDetails* CreateMalwareDetails(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource) override {
    details_ = new FakeMalwareDetails(delegate, web_contents,
                                      unsafe_resource);
    return details_;
  }

  FakeMalwareDetails* get_details() {
    return details_;
  }

 private:
  FakeMalwareDetails* details_;
};

// A SafeBrowingBlockingPage class that lets us wait until it's hidden.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(SafeBrowsingUIManager* manager,
                                 WebContents* web_contents,
                                 const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPage(manager, web_contents, unsafe_resources),
        wait_for_delete_(false) {
    // Don't wait the whole 3 seconds for the browser test.
    malware_details_proceed_delay_ms_ = 100;
  }

  ~TestSafeBrowsingBlockingPage() override {
    if (!wait_for_delete_)
      return;

    // Notify that we are gone
    base::MessageLoopForUI::current()->Quit();
    wait_for_delete_ = false;
  }

  void WaitForDelete() {
    wait_for_delete_ = true;
    content::RunMessageLoop();
  }

  // InterstitialPageDelegate methods:
  void CommandReceived(const std::string& command) override {
    SafeBrowsingBlockingPage::CommandReceived(command);
  }
  void OnProceed() override { SafeBrowsingBlockingPage::OnProceed(); }
  void OnDontProceed() override { SafeBrowsingBlockingPage::OnDontProceed(); }

 private:
  bool wait_for_delete_;
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() { }
  ~TestSafeBrowsingBlockingPageFactory() override {}

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    return new TestSafeBrowsingBlockingPage(delegate, web_contents,
                                              unsafe_resources);
  }
};

}  // namespace

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingBlockingPageBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<SBThreatType> {
 public:
  enum Visibility {
    VISIBILITY_ERROR = -1,
    HIDDEN = 0,
    VISIBLE = 1
  };

  SafeBrowsingBlockingPageBrowserTest() {}

  void SetUp() override {
    SafeBrowsingService::RegisterFactory(&factory_);
    SafeBrowsingBlockingPage::RegisterFactory(&blocking_page_factory_);
    MalwareDetails::RegisterFactory(&details_factory_);
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials, "UwSInterstitialStatus/On/");
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    SafeBrowsingBlockingPage::RegisterFactory(NULL);
    SafeBrowsingService::RegisterFactory(NULL);
    MalwareDetails::RegisterFactory(NULL);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(test_server()->Start());
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    FakeSafeBrowsingService* service =
        static_cast<FakeSafeBrowsingService*>(
            g_browser_process->safe_browsing_service());

    ASSERT_TRUE(service);
    service->fake_database_manager()->SetURLThreatType(url, threat_type);
  }

  // Adds a safebrowsing result of the current test threat to the fake
  // safebrowsing service, navigates to that page, and returns the url.
  GURL SetupWarningAndNavigate() {
    GURL url = test_server()->GetURL(kEmptyPage);
    SetURLThreatType(url, GetParam());

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady());
    return url;
  }

  // Adds a safebrowsing threat result to the fake safebrowsing service,
  // navigates to a page with an iframe containing the threat site, and returns
  // the url of the parent page.
  GURL SetupThreatIframeWarningAndNavigate() {
    GURL url = test_server()->GetURL(kMalwarePage);
    GURL iframe_url = test_server()->GetURL(kMalwareIframe);
    SetURLThreatType(iframe_url, GetParam());

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady());
    return url;
  }

  void SendCommand(
      SecurityInterstitialPage::SecurityInterstitialCommands command) {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // We use InterstitialPage::GetInterstitialPage(tab) instead of
    // tab->GetInterstitialPage() because the tab doesn't have a pointer
    // to its interstital page until it gets a command from the renderer
    // that it has indeed displayed it -- and this sometimes happens after
    // NavigateToURL returns.
    SafeBrowsingBlockingPage* interstitial_page =
        static_cast<SafeBrowsingBlockingPage*>(
            InterstitialPage::GetInterstitialPage(contents)->
                GetDelegateForTesting());
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(SafeBrowsingBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
    interstitial_page->CommandReceived(base::IntToString(command));
  }

  void DontProceedThroughInterstitial() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InterstitialPage* interstitial_page = InterstitialPage::GetInterstitialPage(
        contents);
    ASSERT_TRUE(interstitial_page);
    interstitial_page->DontProceed();
  }

  void ProceedThroughInterstitial() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InterstitialPage* interstitial_page = InterstitialPage::GetInterstitialPage(
        contents);
    ASSERT_TRUE(interstitial_page);
    interstitial_page->Proceed();
  }

  void AssertNoInterstitial(bool wait_for_delete) {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    if (contents->ShowingInterstitialPage() && wait_for_delete) {
      // We'll get notified when the interstitial is deleted.
      TestSafeBrowsingBlockingPage* page =
          static_cast<TestSafeBrowsingBlockingPage*>(
              contents->GetInterstitialPage()->GetDelegateForTesting());
      ASSERT_EQ(SafeBrowsingBlockingPage::kTypeForTesting,
                page->GetTypeForTesting());
      page->WaitForDelete();
    }

    // Can't use InterstitialPage::GetInterstitialPage() because that
    // gets updated after the TestSafeBrowsingBlockingPage destructor
    ASSERT_FALSE(contents->ShowingInterstitialPage());
  }

  bool YesInterstitial() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InterstitialPage* interstitial_page = InterstitialPage::GetInterstitialPage(
        contents);
    return interstitial_page != NULL;
  }

  void SetReportSentCallback(const base::Closure& callback) {
    factory_.most_recent_service()
        ->fake_ui_manager()
        ->set_malware_details_done_callback(callback);
  }

  std::string GetReportSent() {
    return factory_.most_recent_service()->fake_ui_manager()->GetReport();
  }

  void MalwareRedirectCancelAndProceed(const std::string& open_function) {
    GURL load_url = test_server()->GetURL(
        "files/safe_browsing/interstitial_cancel.html");
    GURL malware_url("http://localhost/files/safe_browsing/malware.html");
    SetURLThreatType(malware_url, GetParam());

    // Load the test page.
    ui_test_utils::NavigateToURL(browser(), load_url);
    // Trigger the safe browsing interstitial page via a redirect in
    // "openWin()".
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        GURL("javascript:" + open_function + "()"),
        CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WaitForInterstitialAttach(contents);
    // Cancel the redirect request while interstitial page is open.
    browser()->tab_strip_model()->ActivateTabAt(0, true);
    ui_test_utils::NavigateToURL(browser(), GURL("javascript:stopWin()"));
    browser()->tab_strip_model()->ActivateTabAt(1, true);
    // Simulate the user clicking "proceed", there should be no crash.  Since
    // clicking proceed may do nothing (see comment in RedirectCanceled
    // below, and crbug.com/76460), we use SendCommand to trigger the callback
    // directly rather than using ClickAndWaitForDetach since there might not
    // be a notification to wait for.
    SendCommand(SecurityInterstitialPage::CMD_PROCEED);
  }

  content::RenderViewHost* GetRenderViewHost() {
    InterstitialPage* interstitial = InterstitialPage::GetInterstitialPage(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (!interstitial)
      return NULL;
    return interstitial->GetMainFrame()->GetRenderViewHost();
  }

  bool WaitForReady() {
    InterstitialPage* interstitial = InterstitialPage::GetInterstitialPage(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (!interstitial)
      return false;
    return content::WaitForRenderFrameReady(interstitial->GetMainFrame());
  }

  Visibility GetVisibility(const std::string& node_id) {
    content::RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return VISIBILITY_ERROR;
    scoped_ptr<base::Value> value = content::ExecuteScriptAndGetValue(
        rvh->GetMainFrame(),
        "var node = document.getElementById('" + node_id + "');\n"
        "if (node)\n"
        "   node.offsetWidth > 0 && node.offsetHeight > 0;"
        "else\n"
        "  'node not found';\n");
    if (!value.get())
      return VISIBILITY_ERROR;
    bool result = false;
    if (!value->GetAsBoolean(&result))
      return VISIBILITY_ERROR;
    return result ? VISIBLE : HIDDEN;
  }

  bool Click(const std::string& node_id) {
    content::RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return false;
    // We don't use ExecuteScriptAndGetValue for this one, since clicking
    // the button/link may navigate away before the injected javascript can
    // reply, hanging the test.
    rvh->GetMainFrame()->ExecuteJavaScript(
        base::ASCIIToUTF16(
            "document.getElementById('" + node_id + "').click();\n"));
    return true;
  }

  bool ClickAndWaitForDetach(const std::string& node_id) {
    // We wait for interstitial_detached rather than nav_entry_committed, as
    // going back from a main-frame malware interstitial page will not cause a
    // nav entry committed event.
    if (!Click(node_id))
      return false;
    content::WaitForInterstitialDetach(
        browser()->tab_strip_model()->GetActiveWebContents());
    return true;
  }

 protected:
  TestMalwareDetailsFactory details_factory_;

 private:
  TestSafeBrowsingServiceFactory factory_;
  TestSafeBrowsingBlockingPageFactory blocking_page_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageBrowserTest);
};

// TODO(linux_aura) http://crbug.com/163931
// TODO(win_aura) http://crbug.com/154081
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#define MAYBE_RedirectInIFrameCanceled DISABLED_RedirectInIFrameCanceled
#else
#define MAYBE_RedirectInIFrameCanceled RedirectInIFrameCanceled
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_RedirectInIFrameCanceled) {
  // 1. Test the case that redirect is a subresource.
  MalwareRedirectCancelAndProceed("openWinIFrame");
  // If the redirect was from subresource but canceled, "proceed" will continue
  // with the rest of resources.
  AssertNoInterstitial(true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, RedirectCanceled) {
  // 2. Test the case that redirect is the only resource.
  MalwareRedirectCancelAndProceed("openWin");
  // Clicking proceed won't do anything if the main request is cancelled
  // already.  See crbug.com/76460.
  EXPECT_TRUE(YesInterstitial());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, DontProceed) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  SetupWarningAndNavigate();

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  AssertNoInterstitial(false);   // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, Proceed) {
  GURL url = SetupWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, IframeDontProceed) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  SetupThreatIframeWarningAndNavigate();

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  AssertNoInterstitial(false);  // Assert the interstitial is gone

  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, IframeProceed) {
  GURL url = SetupThreatIframeWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       IframeOptInAndReportMalwareDetails) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats. It however only results in uploading further
  // details about the immediate threat when facing malware threats.
  const bool expect_malware_details = GetParam() == SB_THREAT_TYPE_URL_MALWARE;

  scoped_refptr<content::MessageLoopRunner> malware_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_malware_details)
    SetReportSentCallback(malware_report_sent_runner->QuitClosure());

  GURL url = SetupThreatIframeWarningAndNavigate();

  FakeMalwareDetails* fake_malware_details = details_factory_.get_details();
  EXPECT_EQ(expect_malware_details, fake_malware_details != nullptr);

  // If the DOM details from renderer did not already return when they are
  // expected, wait for them.
  if (expect_malware_details)
    fake_malware_details->WaitForDOM();

  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
              prefs::kSafeBrowsingExtendedReportingEnabled));
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  if (expect_malware_details) {
    malware_report_sent_runner->Run();
    std::string serialized = GetReportSent();
    safe_browsing::ClientMalwareReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
  }
}

// Verifies that the "proceed anyway" link isn't available when it is disabled
// by the corresponding policy. Also verifies that sending the "proceed"
// command anyway doesn't advance to the malware site.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ProceedDisabled) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  // Simulate a policy disabling the "proceed anyway" link.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingProceedAnywayDisabled, true);

  SetupWarningAndNavigate();

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
  SendCommand(SecurityInterstitialPage::CMD_PROCEED);

  // The "proceed" command should go back instead, if proceeding is disabled.
  AssertNoInterstitial(true);
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Verifies that the reporting checkbox is hidden on non-HTTP pages.
// TODO(mattm): Should also verify that no report is sent, but there isn't a
// good way to do that in the current design.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ReportingDisabled) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL url = https_server.GetURL(kEmptyPage);
  SetURLThreatType(url, GetParam());
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(WaitForReady());

  EXPECT_EQ(HIDDEN, GetVisibility("extended-reporting-opt-in"));
  EXPECT_EQ(HIDDEN, GetVisibility("opt-in-checkbox"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("help-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));

  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial(false);   // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, LearnMore) {
  SetupWarningAndNavigate();
  EXPECT_TRUE(ClickAndWaitForDetach("help-link"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  // We are in the help page.
  EXPECT_EQ(
      GetParam() == SB_THREAT_TYPE_URL_PHISHING
          ? "/transparencyreport/safebrowsing/"
          : "/safebrowsing/diagnostic",
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().path());
}

INSTANTIATE_TEST_CASE_P(SafeBrowsingBlockingPageBrowserTestWithThreatType,
                        SafeBrowsingBlockingPageBrowserTest,
                        testing::Values(SB_THREAT_TYPE_URL_MALWARE,
                                        SB_THREAT_TYPE_URL_PHISHING,
                                        SB_THREAT_TYPE_URL_UNWANTED));

// Test that SafeBrowsingBlockingPage properly decodes IDN URLs that are
// displayed.
class SafeBrowsingBlockingPageIDNTest
    : public SecurityInterstitialIDNTest,
      public testing::WithParamInterface<SBThreatType> {
 protected:
  // SecurityInterstitialIDNTest implementation
  SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    SafeBrowsingService* sb_service =
        g_browser_process->safe_browsing_service();
    SafeBrowsingBlockingPage::UnsafeResource resource;

    resource.url = request_url;
    resource.is_subresource = false;
    resource.threat_type = GetParam();
    resource.render_process_host_id = contents->GetRenderProcessHost()->GetID();
    resource.render_view_id = contents->GetRenderViewHost()->GetRoutingID();

    return SafeBrowsingBlockingPage::CreateBlockingPage(
        sb_service->ui_manager().get(), contents, resource);
  }
};

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageIDNTest,
                       SafeBrowsingBlockingPageDecodesIDN) {
  EXPECT_TRUE(VerifyIDNDecoded());
}

INSTANTIATE_TEST_CASE_P(SafeBrowsingBlockingPageIDNTestWithThreatType,
                        SafeBrowsingBlockingPageIDNTest,
                        testing::Values(SB_THREAT_TYPE_URL_MALWARE,
                                        SB_THREAT_TYPE_URL_PHISHING,
                                        SB_THREAT_TYPE_URL_UNWANTED));
