// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

class OmniboxView;

using testing::Return;

namespace {

class MockSearchIPCRouterDelegate : public SearchIPCRouter::Delegate {
 public:
  virtual ~MockSearchIPCRouterDelegate() {}

  MOCK_METHOD1(OnInstantSupportDetermined, void(bool supports_instant));
  MOCK_METHOD1(OnSetVoiceSearchSupport, void(bool supports_voice_search));
  MOCK_METHOD1(FocusOmnibox, void(OmniboxFocusState state));
  MOCK_METHOD3(NavigateToURL, void(const GURL&, WindowOpenDisposition, bool));
  MOCK_METHOD1(OnDeleteMostVisitedItem, void(const GURL& url));
  MOCK_METHOD1(OnUndoMostVisitedDeletion, void(const GURL& url));
  MOCK_METHOD0(OnUndoAllMostVisitedDeletions, void());
  MOCK_METHOD2(OnLogEvent, void(NTPLoggingEventType event,
                                base::TimeDelta time));
  MOCK_METHOD2(OnLogMostVisitedImpression,
               void(int position, const base::string16& provider));
  MOCK_METHOD2(OnLogMostVisitedNavigation,
               void(int position, const base::string16& provider));
  MOCK_METHOD1(PasteIntoOmnibox, void(const base::string16&));
  MOCK_METHOD1(OnChromeIdentityCheck, void(const base::string16& identity));
  MOCK_METHOD0(OnHistorySyncCheck, void());
};

}  // namespace

class SearchTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SearchTabHelper::CreateForWebContents(web_contents());
  }

  content::BrowserContext* CreateBrowserContext() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              FakeSigninManagerBase::Build);
    builder.AddTestingFactory(
        ProfileSyncServiceFactory::GetInstance(),
        ProfileSyncServiceMock::BuildMockProfileSyncService);
    return builder.Build().release();
  }

  // Creates a sign-in manager for tests.  If |username| is not empty, the
  // testing profile of the WebContents will be connected to the given account.
  void CreateSigninManager(const std::string& username) {
    SigninManagerBase* signin_manager = static_cast<SigninManagerBase*>(
        SigninManagerFactory::GetForProfile(profile()));

    if (!username.empty()) {
      ASSERT_TRUE(signin_manager);
      signin_manager->SetAuthenticatedAccountInfo(username, username);
    }
  }

  // Configure the account to |sync_history| or not.
  void SetHistorySync(bool sync_history) {
    ProfileSyncServiceMock* sync_service = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile()));

    syncer::ModelTypeSet result;
    if (sync_history) {
      result.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    }
    EXPECT_CALL(*sync_service, GetPreferredDataTypes())
        .WillRepeatedly(Return(result));
  }

  bool MessageWasSent(uint32 id) {
    return process()->sink().GetFirstMessageMatching(id) != NULL;
  }

  MockSearchIPCRouterDelegate* mock_delegate() { return &delegate_; }

 private:
  MockSearchIPCRouterDelegate delegate_;
};

TEST_F(SearchTabHelperTest, DetermineIfPageSupportsInstant_Local) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_CALL(*mock_delegate(), OnInstantSupportDetermined(true)).Times(0);

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  search_tab_helper->ipc_router().set_delegate_for_testing(mock_delegate());
  search_tab_helper->DetermineIfPageSupportsInstant();
}

TEST_F(SearchTabHelperTest, DetermineIfPageSupportsInstant_NonLocal) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnInstantSupportDetermined(true)).Times(1);

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  search_tab_helper->ipc_router().set_delegate_for_testing(mock_delegate());
  search_tab_helper->DetermineIfPageSupportsInstant();
  ASSERT_TRUE(MessageWasSent(ChromeViewMsg_DetermineIfPageSupportsInstant::ID));

  scoped_ptr<IPC::Message> response(
      new ChromeViewHostMsg_InstantSupportDetermined(
          web_contents()->GetRoutingID(),
          search_tab_helper->ipc_router().page_seq_no_for_testing(),
          true));
  search_tab_helper->ipc_router().OnMessageReceived(*response);
}

TEST_F(SearchTabHelperTest, PageURLDoesntBelongToInstantRenderer) {
  // Navigate to a page URL that doesn't belong to Instant renderer.
  // SearchTabHelper::DeterminerIfPageSupportsInstant() should return
  // immediately without dispatching any message to the renderer.
  NavigateAndCommit(GURL("http://www.example.com"));
  process()->sink().ClearMessages();
  EXPECT_CALL(*mock_delegate(), OnInstantSupportDetermined(false)).Times(0);

  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  search_tab_helper->ipc_router().set_delegate_for_testing(mock_delegate());
  search_tab_helper->DetermineIfPageSupportsInstant();
  ASSERT_FALSE(MessageWasSent(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID));
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckMatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  CreateSigninManager(std::string("foo@bar.com"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("foo@bar.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, get<0>(params));
  ASSERT_TRUE(get<1>(params));
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckMatchSlightlyDifferentGmail) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  CreateSigninManager(std::string("foobar123@gmail.com"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  // For gmail, canonicalization is done so that email addresses have a
  // standard form.
  const base::string16 test_identity =
      base::ASCIIToUTF16("Foo.Bar.123@gmail.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, get<0>(params));
  ASSERT_TRUE(get<1>(params));
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckMatchSlightlyDifferentGmail2) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  //
  CreateSigninManager(std::string("chrome.guy.7FOREVER"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  // For gmail/googlemail, canonicalization is done so that email addresses have
  // a standard form.
  const base::string16 test_identity =
      base::ASCIIToUTF16("chromeguy7forever@googlemail.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, get<0>(params));
  ASSERT_TRUE(get<1>(params));
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckMismatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  CreateSigninManager(std::string("foo@bar.com"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("bar@foo.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, get<0>(params));
  ASSERT_FALSE(get<1>(params));
}

TEST_F(SearchTabHelperTest, OnChromeIdentityCheckSignedOutMismatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  // This test does not sign in.
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("bar@foo.com");
  search_tab_helper->OnChromeIdentityCheck(test_identity);

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_ChromeIdentityCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_ChromeIdentityCheckResult::Param params;
  ChromeViewMsg_ChromeIdentityCheckResult::Read(message, &params);
  EXPECT_EQ(test_identity, get<0>(params));
  ASSERT_FALSE(get<1>(params));
}

TEST_F(SearchTabHelperTest, OnHistorySyncCheckSyncing) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetHistorySync(true);
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  search_tab_helper->OnHistorySyncCheck();

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_HistorySyncCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_HistorySyncCheckResult::Param params;
  ChromeViewMsg_HistorySyncCheckResult::Read(message, &params);
  ASSERT_TRUE(get<0>(params));
}

TEST_F(SearchTabHelperTest, OnHistorySyncCheckNotSyncing) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetHistorySync(false);
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);

  search_tab_helper->OnHistorySyncCheck();

  const IPC::Message* message = process()->sink().GetUniqueMessageMatching(
      ChromeViewMsg_HistorySyncCheckResult::ID);
  ASSERT_TRUE(message != NULL);

  ChromeViewMsg_HistorySyncCheckResult::Param params;
  ChromeViewMsg_HistorySyncCheckResult::Read(message, &params);
  ASSERT_FALSE(get<0>(params));
}

class TabTitleObserver : public content::WebContentsObserver {
 public:
  explicit TabTitleObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}

  base::string16 title_on_start() { return title_on_start_; }
  base::string16 title_on_commit() { return title_on_commit_; }

 private:
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* /* render_frame_host */,
      const GURL& /* validated_url */,
      bool /* is_error_page */,
      bool /* is_iframe_srcdoc */) override {
    title_on_start_ = web_contents()->GetTitle();
  }

  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& /* details */,
      const content::FrameNavigateParams& /* params */) override {
    title_on_commit_ = web_contents()->GetTitle();
  }

  base::string16 title_on_start_;
  base::string16 title_on_commit_;
};

TEST_F(SearchTabHelperTest, TitleIsSetForNTP) {
  TabTitleObserver title_observer(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  const base::string16 title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  EXPECT_EQ(title, title_observer.title_on_start());
  EXPECT_EQ(title, title_observer.title_on_commit());
  EXPECT_EQ(title, web_contents()->GetTitle());
}

class SearchTabHelperWindowTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?"
        "{google:omniboxStartMarginParameter}{google:forceInstantResults}"
        "foo=foo#foo=foo&strk";
    data.new_tab_url = std::string("https://foo.com/newtab?strk");
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(data);
    template_url_service->Add(template_url);
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }
};

class SearchTabHelperPrerenderTest : public InstantUnitTestBase {
 public:
  ~SearchTabHelperPrerenderTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "EmbeddedSearch",
        "Group1 espv:89 prefetch_results:1 "
        "prerender_instant_url_on_omnibox_focus:1"));
    InstantUnitTestBase::SetUp();

    AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
    SearchTabHelper::FromWebContents(web_contents())->set_omnibox_has_focus_fn(
        omnibox_has_focus);
    SearchTabHelperPrerenderTest::omnibox_has_focus_ = true;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool IsInstantURLMarkedForPrerendering() {
    GURL instant_url(chrome::GetSearchResultPrefetchBaseURL(profile()));
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profile());
    return prerender_manager->HasPrerenderedUrl(instant_url, web_contents());
  }

  static bool omnibox_has_focus(OmniboxView* omnibox) {
    return omnibox_has_focus_;
  }

  static bool omnibox_has_focus_;
};

bool SearchTabHelperPrerenderTest::omnibox_has_focus_ = true;

TEST_F(SearchTabHelperPrerenderTest, OnOmniboxFocusPrerenderInstantURL) {
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  search_tab_helper->OmniboxFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                                         OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  ASSERT_TRUE(IsInstantURLMarkedForPrerendering());
  search_tab_helper->OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                         OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  ASSERT_FALSE(IsInstantURLMarkedForPrerendering());
}

TEST_F(SearchTabHelperPrerenderTest, OnTabActivatedPrerenderInstantURL) {
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  search_tab_helper->OnTabActivated();
  ASSERT_TRUE(IsInstantURLMarkedForPrerendering());
}

TEST_F(SearchTabHelperPrerenderTest,
    OnTabActivatedNoPrerenderIfOmniboxBlurred) {
  SearchTabHelperPrerenderTest::omnibox_has_focus_ = false;
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  search_tab_helper->OnTabActivated();
  ASSERT_FALSE(IsInstantURLMarkedForPrerendering());
}
