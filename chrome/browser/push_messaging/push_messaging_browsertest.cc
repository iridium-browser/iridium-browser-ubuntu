// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/lifetime/keep_alive_registry.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/notifications/message_center_display_service.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/gcm_driver/common/gcm_messages.h"
#include "components/gcm_driver/gcm_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(ENABLE_BACKGROUND)
#include "chrome/browser/background/background_mode_manager.h"
#endif

namespace {

// NIST P-256 public key made available to tests. Must be an uncompressed
// point in accordance with SEC1 2.3.3.
const uint8_t kApplicationServerKey[65] = {
    0x04, 0x55, 0x52, 0x6A, 0xA5, 0x6E, 0x8E, 0xAA, 0x47, 0x97, 0x36,
    0x10, 0xC1, 0x66, 0x3C, 0x1E, 0x65, 0xBF, 0xA1, 0x7B, 0xEE, 0x48,
    0xC9, 0xC6, 0xBB, 0xBF, 0x02, 0x18, 0x53, 0x72, 0x1D, 0x0C, 0x7B,
    0xA9, 0xE3, 0x11, 0xB7, 0x03, 0x52, 0x21, 0xD3, 0x71, 0x90, 0x13,
    0xA8, 0xC1, 0xCF, 0xED, 0x20, 0xF7, 0x1F, 0xD1, 0x7F, 0xF2, 0x76,
    0xB6, 0x01, 0x20, 0xD8, 0x35, 0xA5, 0xD9, 0x3C, 0x43, 0xFD};

// URL-safe base64 encoded version of the |kApplicationServerKey|.
const char kEncodedApplicationServerKey[] =
    "BFVSaqVujqpHlzYQwWY8HmW_oXvuSMnGu78CGFNyHQx7qeMRtwNSIdNxkBOowc_tIPcf0X_ydr"
    "YBINg1pdk8Q_0";

std::string GetTestApplicationServerKey() {
  return std::string(kApplicationServerKey,
                     kApplicationServerKey + arraysize(kApplicationServerKey));
}

// Class to instantiate on the stack that is meant to be used with
// FakeGCMProfileService. The ::Run() method follows the signature of
// FakeGCMProfileService::UnregisterCallback.
class UnregistrationCallback {
 public:
  UnregistrationCallback()
      : message_loop_runner_(new content::MessageLoopRunner) {}

  void Run(const std::string& app_id) {
    app_id_ = app_id;
    message_loop_runner_->Quit();
  }

  void WaitUntilSatisfied() { message_loop_runner_->Run(); }

  const std::string& app_id() { return app_id_; }

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  std::string app_id_;
};

}  // namespace

class PushMessagingBrowserTest : public InProcessBrowserTest {
 public:
  PushMessagingBrowserTest() : gcm_service_(nullptr) {}
  ~PushMessagingBrowserTest() override {}

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_->Start());

#if defined(ENABLE_NOTIFICATIONS)
    notification_manager_.reset(new StubNotificationUIManager);
#endif

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental features for subscription restrictions.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    gcm_service_ = static_cast<gcm::FakeGCMProfileService*>(
        gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            GetBrowser()->profile(), &gcm::FakeGCMProfileService::Build));
    gcm_service_->set_collect(true);
    push_service_ =
        PushMessagingServiceFactory::GetForProfile(GetBrowser()->profile());
#if defined(ENABLE_NOTIFICATIONS)
    display_service_.reset(new MessageCenterDisplayService(
        GetBrowser()->profile(), notification_manager_.get()));
    notification_service()->SetNotificationDisplayServiceForTesting(
        display_service_.get());
#endif

    LoadTestPage();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void RestartPushService() {
    Profile* profile = GetBrowser()->profile();
    PushMessagingServiceFactory::GetInstance()->SetTestingFactory(profile,
                                                                  nullptr);
    ASSERT_EQ(nullptr, PushMessagingServiceFactory::GetForProfile(profile));
    PushMessagingServiceFactory::GetInstance()->RestoreFactoryForTests(profile);
    PushMessagingServiceImpl::InitializeForProfile(profile);
    push_service_ = PushMessagingServiceFactory::GetForProfile(profile);
  }

  // Helper function to test if a Keep Alive is registered while avoiding the
  // platform checks. Returns a boolean so that assertion failures are reported
  // at the right line.
  // Returns true when KeepAlives are not supported by the platform, or when
  // the registration state is equal to the expectation.
  bool IsRegisteredKeepAliveEqualTo(bool expectation) {
#if BUILDFLAG(ENABLE_BACKGROUND)
    return expectation ==
           KeepAliveRegistry::GetInstance()->IsOriginRegistered(
               KeepAliveOrigin::IN_FLIGHT_PUSH_MESSAGE);
#else
    return true;
#endif
  }

  // InProcessBrowserTest:
  void TearDown() override {
#if defined(ENABLE_NOTIFICATIONS)
    notification_service()->SetNotificationDisplayServiceForTesting(nullptr);
#endif

    InProcessBrowserTest::TearDown();
  }

  void LoadTestPage(const std::string& path) {
    ui_test_utils::NavigateToURL(GetBrowser(), https_server_->GetURL(path));
  }

  void LoadTestPage() { LoadTestPage(GetTestURL()); }

  bool RunScript(const std::string& script, std::string* result) {
    return RunScript(script, result, nullptr);
  }

  bool RunScript(const std::string& script, std::string* result,
                 content::WebContents* web_contents) {
    if (!web_contents)
      web_contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
    return content::ExecuteScriptAndExtractString(web_contents->GetMainFrame(),
                                                  script, result);
  }

  gcm::GCMAppHandler* GetAppHandler() {
    return gcm_service()->driver()->GetAppHandler(
        kPushMessagingAppIdentifierPrefix);
  }

  PermissionRequestManager* GetPermissionRequestManager() {
    return PermissionRequestManager::FromWebContents(
        GetBrowser()->tab_strip_model()->GetActiveWebContents());
  }

  void RequestAndAcceptPermission();
  void RequestAndDenyPermission();

  void TryToSubscribeSuccessfully(
      const std::string& expected_push_subscription_info,
      bool use_key = true);

  std::string GetEndpointForSubscriptionId(const std::string& subscription_id,
                                           bool standard_protocol = true) {
    return push_service()->GetEndpoint(standard_protocol).spec() +
           subscription_id;
  }

  PushMessagingAppIdentifier GetAppIdentifierForServiceWorkerRegistration(
      int64_t service_worker_registration_id);

  void SendMessageAndWaitUntilHandled(
      const PushMessagingAppIdentifier& app_identifier,
      const gcm::IncomingMessage& message);

  net::EmbeddedTestServer* https_server() const { return https_server_.get(); }

  gcm::FakeGCMProfileService* gcm_service() const { return gcm_service_; }

#if defined(ENABLE_NOTIFICATIONS)
  // To be called when delivery of a push message has finished. The |run_loop|
  // will be told to quit after |messages_required| messages were received.
  void OnDeliveryFinished(std::vector<size_t>* number_of_notifications_shown,
                          const base::Closure& done_closure) {
    DCHECK(number_of_notifications_shown);

    number_of_notifications_shown->push_back(
        notification_manager_->GetNotificationCount());

    done_closure.Run();
  }

  StubNotificationUIManager* notification_manager() const {
    return notification_manager_.get();
  }

  PlatformNotificationServiceImpl* notification_service() const {
    return PlatformNotificationServiceImpl::GetInstance();
  }
#endif

  PushMessagingServiceImpl* push_service() const { return push_service_; }

  void SetSiteEngagementScore(const GURL& url, double score) {
    SiteEngagementService* service =
        SiteEngagementService::Get(GetBrowser()->profile());
    service->ResetScoreForURL(url, score);
  }

 protected:
  virtual std::string GetTestURL() { return "/push_messaging/test.html"; }

  virtual Browser* GetBrowser() const { return browser(); }

  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  gcm::FakeGCMProfileService* gcm_service_;
  PushMessagingServiceImpl* push_service_;
  base::HistogramTester histogram_tester_;

#if defined(ENABLE_NOTIFICATIONS)
  std::unique_ptr<StubNotificationUIManager> notification_manager_;
  std::unique_ptr<MessageCenterDisplayService> display_service_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PushMessagingBrowserTest);
};

class PushMessagingBrowserTestEmptySubscriptionOptions
    : public PushMessagingBrowserTest {
  std::string GetTestURL() override {
    return "/push_messaging/test_no_subscription_options.html";
  }
};

void PushMessagingBrowserTest::RequestAndAcceptPermission() {
  std::string script_result;
  GetPermissionRequestManager()->set_auto_response_for_test(
      PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(RunScript("requestNotificationPermission();", &script_result));
  EXPECT_EQ("permission status - granted", script_result);
}

void PushMessagingBrowserTest::RequestAndDenyPermission() {
  std::string script_result;
  GetPermissionRequestManager()->set_auto_response_for_test(
      PermissionRequestManager::DENY_ALL);
  EXPECT_TRUE(RunScript("requestNotificationPermission();", &script_result));
  EXPECT_EQ("permission status - denied", script_result);
}

void PushMessagingBrowserTest::TryToSubscribeSuccessfully(
    const std::string& expected_push_subscription_info,
    bool use_key) {
  std::string script_result;

  EXPECT_TRUE(RunScript("registerServiceWorker()", &script_result));
  EXPECT_EQ("ok - service worker registered", script_result);

  RequestAndAcceptPermission();

  if (use_key) {
    ASSERT_TRUE(RunScript("removeManifest()", &script_result));
    ASSERT_EQ("manifest removed", script_result);

    EXPECT_TRUE(RunScript("documentSubscribePush()", &script_result));
  } else {
    // Test backwards compatibility with old ID based subscriptions.
    EXPECT_TRUE(RunScript("documentSubscribePushWithoutKey()", &script_result));
  }

  EXPECT_EQ(
      GetEndpointForSubscriptionId(expected_push_subscription_info, use_key),
      script_result);
}

PushMessagingAppIdentifier
PushMessagingBrowserTest::GetAppIdentifierForServiceWorkerRegistration(
    int64_t service_worker_registration_id) {
  GURL origin = https_server()->GetURL("/").GetOrigin();
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(), origin, service_worker_registration_id);
  EXPECT_FALSE(app_identifier.is_null());
  return app_identifier;
}

void PushMessagingBrowserTest::SendMessageAndWaitUntilHandled(
    const PushMessagingAppIdentifier& app_identifier,
    const gcm::IncomingMessage& message) {
  base::RunLoop run_loop;
  push_service()->SetMessageCallbackForTesting(run_loop.QuitClosure());
  push_service()->OnMessage(app_identifier.app_id(), message);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeWithoutKeySuccessNotificationsGranted) {
  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */, false);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ("1234567890", gcm_service()->last_registered_sender_ids()[0]);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeSuccessNotificationsGranted) {
  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeSuccessNotificationsPrompt) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  GetPermissionRequestManager()->set_auto_response_for_test(
      PermissionRequestManager::ACCEPT_ALL);
  ASSERT_TRUE(RunScript("documentSubscribePush()", &script_result));
  EXPECT_EQ(GetEndpointForSubscriptionId("1-0"), script_result);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeFailureBadKey) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndAcceptPermission();

  ASSERT_TRUE(RunScript("documentSubscribePushBadKey()", &script_result));
  EXPECT_EQ(
      "InvalidAccessError - Failed to execute 'subscribe' on 'PushManager': "
      "The provided applicationServerKey is not valid.",
      script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeFailureNotificationsBlocked) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndDenyPermission();

  ASSERT_TRUE(RunScript("documentSubscribePush()", &script_result));
  EXPECT_EQ("NotAllowedError - Registration failed - permission denied",
            script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeFailureNoManifest) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndAcceptPermission();

  ASSERT_TRUE(RunScript("removeManifest()", &script_result));
  ASSERT_EQ("manifest removed", script_result);

  ASSERT_TRUE(RunScript("documentSubscribePushWithoutKey()", &script_result));
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "manifest empty or missing",
      script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeFailureNoSenderId) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndAcceptPermission();

  ASSERT_TRUE(RunScript("swapManifestNoSenderId()", &script_result));
  ASSERT_EQ("sender id removed from manifest", script_result);

  ASSERT_TRUE(RunScript("documentSubscribePushWithoutKey()", &script_result));
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "gcm_sender_id not found in manifest",
      script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTestEmptySubscriptionOptions,
                       RegisterFailureEmptyPushSubscriptionOptions) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndAcceptPermission();

  ASSERT_TRUE(RunScript("documentSubscribePush()", &script_result));
  EXPECT_EQ("NotAllowedError - Registration failed - permission denied",
            script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeWorker) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndAcceptPermission();

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  // Try to subscribe from a worker without a key. This should fail.
  ASSERT_TRUE(RunScript("workerSubscribePushNoKey()", &script_result));
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "gcm_sender_id not found in manifest",
      script_result);
  // Now run the subscribe from the service worker with a key. This
  // should succeed, and write the key to the datastore.
  ASSERT_TRUE(RunScript("workerSubscribePush()", &script_result));
  EXPECT_EQ(GetEndpointForSubscriptionId("1-0"), script_result);

  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);
  EXPECT_NE(push_service(), GetAppHandler());

  // Now run the subscribe from the service worker without a key.
  // In this case, the key will be read from the datastore.
  ASSERT_TRUE(RunScript("workerSubscribePushNoKey()", &script_result));
  EXPECT_EQ(GetEndpointForSubscriptionId("1-1"), script_result);

  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);
  EXPECT_NE(push_service(), GetAppHandler());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeWorkerUsingManifest) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndAcceptPermission();

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  // Try to subscribe from a worker without a key. This should fail.
  ASSERT_TRUE(RunScript("workerSubscribePushNoKey()", &script_result));
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "gcm_sender_id not found in manifest",
      script_result);
  EXPECT_NE(push_service(), GetAppHandler());

  // Run the subscription from the document without a key, this will trigger
  // the code to read sender id from the manifest and will write it to the
  // datastore.
  ASSERT_TRUE(RunScript("documentSubscribePushWithoutKey()", &script_result));
  EXPECT_EQ(GetEndpointForSubscriptionId("1-0", false), script_result);

  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);
  EXPECT_NE(push_service(), GetAppHandler());

  // Now run the subscribe from the service worker without a key.
  // In this case, the sender id will be read from the datastore.
  ASSERT_TRUE(RunScript("workerSubscribePushNoKey()", &script_result));
  EXPECT_EQ(GetEndpointForSubscriptionId("1-1", false), script_result);

  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);
  EXPECT_NE(push_service(), GetAppHandler());
}

// Disabled on Windows and Linux due to flakiness (http://crbug.com/554003).
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_SubscribePersisted DISABLED_SubscribePersisted
#else
#define MAYBE_SubscribePersisted SubscribePersisted
#endif
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, MAYBE_SubscribePersisted) {
  std::string script_result;

  // First, test that Service Worker registration IDs are assigned in order of
  // registering the Service Workers, and the (fake) push subscription ids are
  // assigned in order of push subscription (even when these orders are
  // different).

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);
  PushMessagingAppIdentifier sw0_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(sw0_identifier.app_id(), gcm_service()->last_registered_app_id());

  LoadTestPage("/push_messaging/subscope1/test.html");
  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  LoadTestPage("/push_messaging/subscope2/test.html");
  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  // Note that we need to reload the page after registering, otherwise
  // navigator.serviceWorker.ready is going to be resolved with the parent
  // Service Worker which still controls the page.
  LoadTestPage("/push_messaging/subscope2/test.html");
  TryToSubscribeSuccessfully("1-1" /* expected_push_subscription_id */);
  PushMessagingAppIdentifier sw2_identifier =
      GetAppIdentifierForServiceWorkerRegistration(2LL);
  EXPECT_EQ(sw2_identifier.app_id(), gcm_service()->last_registered_app_id());

  LoadTestPage("/push_messaging/subscope1/test.html");
  TryToSubscribeSuccessfully("1-2" /* expected_push_subscription_id */);
  PushMessagingAppIdentifier sw1_identifier =
      GetAppIdentifierForServiceWorkerRegistration(1LL);
  EXPECT_EQ(sw1_identifier.app_id(), gcm_service()->last_registered_app_id());

  // Now test that the Service Worker registration IDs and push subscription IDs
  // generated above were persisted to SW storage, by checking that they are
  // unchanged despite requesting them in a different order.
  // TODO(johnme): Ideally we would restart the browser at this point to check
  // they were persisted to disk, but that's not currently possible since the
  // test server uses random port numbers for each test (even PRE_Foo and Foo),
  // so we wouldn't be able to load the test pages with the same origin.

  LoadTestPage("/push_messaging/subscope1/test.html");
  TryToSubscribeSuccessfully("1-2" /* expected_push_subscription_id */);
  EXPECT_EQ(sw1_identifier.app_id(), gcm_service()->last_registered_app_id());

  LoadTestPage("/push_messaging/subscope2/test.html");
  TryToSubscribeSuccessfully("1-1" /* expected_push_subscription_id */);
  EXPECT_EQ(sw1_identifier.app_id(), gcm_service()->last_registered_app_id());

  LoadTestPage();
  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);
  EXPECT_EQ(sw1_identifier.app_id(), gcm_service()->last_registered_app_id());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, AppHandlerOnlyIfSubscribed) {
  // This test restarts the push service to simulate restarting the browser.

  EXPECT_NE(push_service(), GetAppHandler());
  ASSERT_NO_FATAL_FAILURE(RestartPushService());
  EXPECT_NE(push_service(), GetAppHandler());

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  EXPECT_EQ(push_service(), GetAppHandler());
  ASSERT_NO_FATAL_FAILURE(RestartPushService());
  EXPECT_EQ(push_service(), GetAppHandler());

  // Unsubscribe.
  std::string script_result;
  gcm_service()->AddExpectedUnregisterResponse(gcm::GCMClient::SUCCESS);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);

  EXPECT_NE(push_service(), GetAppHandler());
  ASSERT_NO_FATAL_FAILURE(RestartPushService());
  EXPECT_NE(push_service(), GetAppHandler());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventSuccess) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("false - is not controlled", script_result);

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;
  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(true));
  ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result));
  EXPECT_EQ("testdata", script_result);

  // Check that we record this case in UMA.
  GetHistogramTester()->ExpectUniqueSample(
      "PushMessaging.DeliveryStatus.FindServiceWorker",
      0 /* SERVICE_WORKER_OK */, 1);
  GetHistogramTester()->ExpectUniqueSample(
      "PushMessaging.DeliveryStatus.ServiceWorkerEvent",
      0 /* SERVICE_WORKER_OK */, 1);
  GetHistogramTester()->ExpectUniqueSample(
      "PushMessaging.DeliveryStatus",
       content::PUSH_DELIVERY_STATUS_SUCCESS, 1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventWithoutPayload) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("false - is not controlled", script_result);

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.decrypted = false;

  push_service()->OnMessage(app_identifier.app_id(), message);
  ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result));
  EXPECT_EQ("[NULL]", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventNoServiceWorker) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("false - is not controlled", script_result);

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  // Unregister service worker. Sending a message should now fail.
  ASSERT_TRUE(RunScript("unregisterServiceWorker()", &script_result));
  ASSERT_EQ("service worker unregistration status: true", script_result);

  // When the push service will receive it next message, given that there is no
  // SW available, it should unregister |app_identifier.app_id()|.
  UnregistrationCallback callback;
  gcm_service()->SetUnregisterCallback(
      base::Bind(&UnregistrationCallback::Run, base::Unretained(&callback)));

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(true));
  callback.WaitUntilSatisfied();
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  EXPECT_EQ(app_identifier.app_id(), callback.app_id());

  // Check that we record this case in UMA.
  GetHistogramTester()->ExpectUniqueSample(
      "PushMessaging.DeliveryStatus.FindServiceWorker",
      5 /* SERVICE_WORKER_ERROR_NOT_FOUND */, 1);
  GetHistogramTester()->ExpectTotalCount(
      "PushMessaging.DeliveryStatus.ServiceWorkerEvent", 0);
  GetHistogramTester()->ExpectUniqueSample(
      "PushMessaging.DeliveryStatus",
      content::PUSH_DELIVERY_STATUS_NO_SERVICE_WORKER, 1);

  // No push data should have been received.
  ASSERT_TRUE(RunScript("resultQueue.popImmediately()", &script_result));
  EXPECT_EQ("null", script_result);
}

#if defined(ENABLE_NOTIFICATIONS)
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       PushEventEnforcesUserVisibleNotification) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("false - is not controlled", script_result);

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  notification_manager()->CancelAll();
  ASSERT_EQ(0u, notification_manager()->GetNotificationCount());

  // We'll need to specify the web_contents in which to eval script, since we're
  // going to run script in a background tab.
  content::WebContents* web_contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Set the site engagement score for the site. Setting it to 4 means it should
  // have enough budget for two non-shown notification, which cost 2 each.
  SetSiteEngagementScore(web_contents->GetURL(), 4.0);

  // If the site is visible in an active tab, we should not force a notification
  // to be shown. Try it twice, since we allow one mistake per 10 push events.
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.decrypted = true;
  for (int n = 0; n < 2; n++) {
    message.raw_data = "testdata";
    SendMessageAndWaitUntilHandled(app_identifier, message);
    ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result));
    EXPECT_EQ("testdata", script_result);
    EXPECT_EQ(0u, notification_manager()->GetNotificationCount());
  }

  // Open a blank foreground tab so site is no longer visible.
  ui_test_utils::NavigateToURLWithDisposition(
      GetBrowser(), GURL("about:blank"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // If the Service Worker push event handler shows a notification, we
  // should not show a forced one.
  message.raw_data = "shownotification";
  SendMessageAndWaitUntilHandled(app_identifier, message);
  ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result, web_contents));
  EXPECT_EQ("shownotification", script_result);
  EXPECT_EQ(1u, notification_manager()->GetNotificationCount());
  EXPECT_EQ("push_test_tag",
            notification_manager()->GetNotificationAt(0).tag());
  notification_manager()->CancelAll();

  // If the Service Worker push event handler does not show a notification, we
  // should show a forced one, but only once the origin is out of budget.
  message.raw_data = "testdata";
  for (int n = 0; n < 2; n++) {
    // First two missed notifications shouldn't force a default one.
    SendMessageAndWaitUntilHandled(app_identifier, message);
    ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result, web_contents));
    EXPECT_EQ("testdata", script_result);
    EXPECT_EQ(0u, notification_manager()->GetNotificationCount());
  }

  // Third missed notification should trigger a default notification, since the
  // origin will be out of budget.
  message.raw_data = "testdata";
  SendMessageAndWaitUntilHandled(app_identifier, message);
  ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result, web_contents));
  EXPECT_EQ("testdata", script_result);

  ASSERT_EQ(1u, notification_manager()->GetNotificationCount());
  {
    const Notification& forced_notification =
        notification_manager()->GetNotificationAt(0);

    EXPECT_EQ(kPushMessagingForcedNotificationTag, forced_notification.tag());
    EXPECT_TRUE(forced_notification.silent());
  }

  // The notification will be automatically dismissed when the developer shows
  // a new notification themselves at a later point in time.
  message.raw_data = "shownotification";
  SendMessageAndWaitUntilHandled(app_identifier, message);
  ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result, web_contents));
  EXPECT_EQ("shownotification", script_result);

  ASSERT_EQ(1u, notification_manager()->GetNotificationCount());
  {
    const Notification& first_notification =
        notification_manager()->GetNotificationAt(0);

    EXPECT_NE(kPushMessagingForcedNotificationTag, first_notification.tag());
  }

  // Check that the UMA has been recorded correctly.
  // There should be a total of 7 budget samples, spread across 3 buckets. The
  // first four notifications (before any budget is consumed) have budget of 4,
  // which is the starting SES. The next one has 2 (one hidden notification) and
  // the final two have 0 (two hidden notifications.
  std::vector<base::Bucket> buckets =
      GetHistogramTester()->GetAllSamples("PushMessaging.BackgroundBudget");
  ASSERT_EQ(3.0, buckets.size());
  // First bucket is for 0 budget, which has 2 samples.
  EXPECT_EQ(0, buckets[0].min);
  EXPECT_EQ(2, buckets[0].count);
  // Second bucket is for 2 budget, which has 1 sample.
  EXPECT_EQ(2, buckets[1].min);
  EXPECT_EQ(1, buckets[1].count);
  // Final bucket is for 4 budget, which has 4 samples.
  EXPECT_EQ(4, buckets[2].min);
  EXPECT_EQ(4, buckets[2].count);

  std::vector<base::Bucket> no_budget_buckets =
      GetHistogramTester()->GetAllSamples("PushMessaging.SESForNoBudgetOrigin");
  ASSERT_EQ(1.0, no_budget_buckets.size());
  EXPECT_EQ(4, no_budget_buckets[0].min);
  EXPECT_EQ(2, no_budget_buckets[0].count);

  std::vector<base::Bucket> low_budget_buckets =
      GetHistogramTester()->GetAllSamples(
          "PushMessaging.SESForLowBudgetOrigin");
  ASSERT_EQ(1.0, low_budget_buckets.size());
  EXPECT_EQ(4, low_budget_buckets[0].min);
  EXPECT_EQ(1, low_budget_buckets[0].count);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       PushEventAllowSilentPushCommandLineFlag) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("false - is not controlled", script_result);

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  notification_manager()->CancelAll();
  ASSERT_EQ(0u, notification_manager()->GetNotificationCount());

  // We'll need to specify the web_contents in which to eval script, since we're
  // going to run script in a background tab.
  content::WebContents* web_contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURLWithDisposition(
      GetBrowser(), GURL("about:blank"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  SetSiteEngagementScore(web_contents->GetURL(), 0.0);

  // If the Service Worker push event handler does not show a notification, we
  // should show a forced one providing there is no foreground tab and the
  // origin ran out of budget.
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;

  SendMessageAndWaitUntilHandled(app_identifier, message);
  ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result, web_contents));
  EXPECT_EQ("testdata", script_result);

  // Because the --allow-silent-push command line flag has not been passed,
  // this should have shown a default notification.
  ASSERT_EQ(1u, notification_manager()->GetNotificationCount());
  {
    const Notification& forced_notification =
        notification_manager()->GetNotificationAt(0);

    EXPECT_EQ(kPushMessagingForcedNotificationTag, forced_notification.tag());
    EXPECT_TRUE(forced_notification.silent());
  }

  notification_manager()->CancelAll();

  // Send the message again, but this time with the -allow-silent-push command
  // line flag set. The default notification should *not* be shown.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowSilentPush);

  SendMessageAndWaitUntilHandled(app_identifier, message);
  ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result, web_contents));
  EXPECT_EQ("testdata", script_result);

  ASSERT_EQ(0u, notification_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       PushEventEnforcesUserVisibleNotificationAfterQueue) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("false - is not controlled", script_result);

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  // Fire off two push messages in sequence, only the second one of which will
  // display a notification. The additional round-trip and I/O required by the
  // second message, which shows a notification, should give us a reasonable
  // confidence that the ordering will be maintained.

  std::vector<size_t> number_of_notifications_shown;

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.decrypted = true;

  {
    base::RunLoop run_loop;
    push_service()->SetMessageCallbackForTesting(base::Bind(
        &PushMessagingBrowserTest::OnDeliveryFinished, base::Unretained(this),
        &number_of_notifications_shown,
        base::BarrierClosure(2 /* num_closures */, run_loop.QuitClosure())));

    message.raw_data = "testdata";
    push_service()->OnMessage(app_identifier.app_id(), message);

    message.raw_data = "shownotification";
    push_service()->OnMessage(app_identifier.app_id(), message);

    run_loop.Run();
  }

  ASSERT_EQ(2u, number_of_notifications_shown.size());
  EXPECT_EQ(0u, number_of_notifications_shown[0]);
  EXPECT_EQ(1u, number_of_notifications_shown[1]);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       PushEventNotificationWithoutEventWaitUntil) {
  std::string script_result;
  content::WebContents* web_contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_service()->last_registered_sender_ids()[0]);

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("false - is not controlled", script_result);

  LoadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  notification_manager()->SetNotificationAddedCallback(
      message_loop_runner->QuitClosure());

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "shownotification-without-waituntil";
  message.decrypted = true;
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(true));
  ASSERT_TRUE(RunScript("resultQueue.pop()", &script_result, web_contents));
  EXPECT_EQ("immediate:shownotification-without-waituntil", script_result);

  message_loop_runner->Run();

  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  ASSERT_EQ(1u, notification_manager()->GetNotificationCount());
  EXPECT_EQ("push_test_tag",
            notification_manager()->GetNotificationAt(0).tag());

  // Verify that the renderer process hasn't crashed.
  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);
}
#endif

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PermissionStateSaysPrompt) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  ASSERT_EQ("permission status - prompt", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PermissionStateSaysGranted) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndAcceptPermission();

  ASSERT_TRUE(RunScript("documentSubscribePush()", &script_result));
  EXPECT_EQ(GetEndpointForSubscriptionId("1-0"), script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PermissionStateSaysDenied) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  RequestAndDenyPermission();

  ASSERT_TRUE(RunScript("documentSubscribePush()", &script_result));
  EXPECT_EQ("NotAllowedError - Registration failed - permission denied",
            script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - denied", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, UnsubscribeSuccess) {
  std::string script_result;

  EXPECT_TRUE(RunScript("registerServiceWorker()", &script_result));
  EXPECT_EQ("ok - service worker registered", script_result);

  // Resolves true if there was a subscription.
  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */, false);
  gcm_service()->AddExpectedUnregisterResponse(gcm::GCMClient::SUCCESS);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);

  // Resolves false if there was no longer a subscription.
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: false", script_result);

  // Doesn't reject if there was a network error (deactivates subscription
  // locally anyway).
  TryToSubscribeSuccessfully("1-1" /* expected_push_subscription_id */, false);
  gcm_service()->AddExpectedUnregisterResponse(gcm::GCMClient::NETWORK_ERROR);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);
  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("false - not subscribed", script_result);

  // Doesn't reject if there were other push service errors (deactivates
  // subscription locally anyway).
  TryToSubscribeSuccessfully("1-2" /* expected_push_subscription_id */, false);
  gcm_service()->AddExpectedUnregisterResponse(
      gcm::GCMClient::INVALID_PARAMETER);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);

  // Unsubscribing (with an existing reference to a PushSubscription), after
  // unregistering the Service Worker, just means push subscription isn't found.
  TryToSubscribeSuccessfully("1-3" /* expected_push_subscription_id */, false);
  ASSERT_TRUE(RunScript("unregisterServiceWorker()", &script_result));
  ASSERT_EQ("service worker unregistration status: true", script_result);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: false", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       GlobalResetPushPermissionUnsubscribes) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      message_loop_runner->QuitClosure());

  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);

  message_loop_runner->Run();

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - prompt", script_result);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("false - not subscribed", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       LocalResetPushPermissionUnsubscribes) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      message_loop_runner->QuitClosure());

  GURL origin = https_server()->GetURL("/").GetOrigin();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, origin,
                                      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), CONTENT_SETTING_DEFAULT);

  message_loop_runner->Run();

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - prompt", script_result);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("false - not subscribed", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       DenyPushPermissionUnsubscribes) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      message_loop_runner->QuitClosure());

  GURL origin = https_server()->GetURL("/").GetOrigin();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, origin,
                                      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), CONTENT_SETTING_BLOCK);

  message_loop_runner->Run();

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - denied", script_result);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("false - not subscribed", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       GlobalResetNotificationsPermissionUnsubscribes) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      message_loop_runner->QuitClosure());

  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);

  message_loop_runner->Run();

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - prompt", script_result);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("false - not subscribed", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       LocalResetNotificationsPermissionUnsubscribes) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      message_loop_runner->QuitClosure());

  GURL origin = https_server()->GetURL("/").GetOrigin();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), CONTENT_SETTING_DEFAULT);

  message_loop_runner->Run();

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - prompt", script_result);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("false - not subscribed", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       DenyNotificationsPermissionUnsubscribes) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      message_loop_runner->QuitClosure());

  GURL origin = https_server()->GetURL("/").GetOrigin();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), CONTENT_SETTING_BLOCK);

  message_loop_runner->Run();

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - denied", script_result);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("false - not subscribed", script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       GrantAlreadyGrantedPermissionDoesNotUnsubscribe) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(1, message_loop_runner->QuitClosure()));

  GURL origin = https_server()->GetURL("/").GetOrigin();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), CONTENT_SETTING_ALLOW);

  message_loop_runner->Run();

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);
}

// This test is testing some non-trivial content settings rules and make sure
// that they are respected with regards to automatic unsubscription. In other
// words, it checks that the push service does not end up unsubscribing origins
// that have push permission with some non-common rules.
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       AutomaticUnsubscriptionFollowsContentSettingRules) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(2, message_loop_runner->QuitClosure()));

  GURL origin = https_server()->GetURL("/").GetOrigin();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_ALLOW);
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), CONTENT_SETTING_DEFAULT);

  message_loop_runner->Run();

  // The two first rules should give |origin| the permission to use Push even
  // if the rules it used to have have been reset.
  // The Push service should not unsubcribe |origin| because at no point it was
  // left without permission to use Push.

  ASSERT_TRUE(RunScript("permissionState()", &script_result));
  EXPECT_EQ("permission status - granted", script_result);

  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  EXPECT_EQ("true - subscribed", script_result);
}

// Checks that automatically unsubscribing due to a revoked permission is
// handled well if the sender ID needed to unsubscribe was already deleted.
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       ResetPushPermissionAfterClearingSiteData) {
  std::string script_result;

  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_service()->last_registered_app_id());
  PushMessagingAppIdentifier stored_app_identifier =
      PushMessagingAppIdentifier::FindByAppId(GetBrowser()->profile(),
                                              app_identifier.app_id());
  EXPECT_FALSE(stored_app_identifier.is_null());

  // Simulate a user clearing site data (including Service Workers, crucially).
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(GetBrowser()->profile());
  BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(BrowsingDataRemover::Unbounded(),
                          BrowsingDataRemover::REMOVE_SITE_DATA,
                          BrowsingDataHelper::UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();

  base::RunLoop run_loop;
  push_service()->SetContentSettingChangedCallbackForTesting(
      run_loop.QuitClosure());

  // This shouldn't (asynchronously) cause a DCHECK.
  // TODO(johnme): Get this test running on Android, which has a different
  // codepath due to sender_id being required for unsubscribing there.
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);

  run_loop.Run();

  // |app_identifier| should no longer be stored in prefs.
  PushMessagingAppIdentifier stored_app_identifier2 =
      PushMessagingAppIdentifier::FindByAppId(GetBrowser()->profile(),
                                              app_identifier.app_id());
  EXPECT_TRUE(stored_app_identifier2.is_null());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, EncryptionKeyUniqueness) {
  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */, false);

  std::string first_public_key;
  ASSERT_TRUE(RunScript("GetP256dh()", &first_public_key));
  EXPECT_GE(first_public_key.size(), 32u);

  std::string script_result;
  gcm_service()->AddExpectedUnregisterResponse(gcm::GCMClient::SUCCESS);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);

  TryToSubscribeSuccessfully("1-1" /* expected_push_subscription_id */);

  std::string second_public_key;
  ASSERT_TRUE(RunScript("GetP256dh()", &second_public_key));
  EXPECT_GE(second_public_key.size(), 32u);

  EXPECT_NE(first_public_key, second_public_key);
}

class PushMessagingIncognitoBrowserTest : public PushMessagingBrowserTest {
 public:
  ~PushMessagingIncognitoBrowserTest() override {}

  // PushMessagingBrowserTest:
  void SetUpOnMainThread() override {
    incognito_browser_ = CreateIncognitoBrowser();
    PushMessagingBrowserTest::SetUpOnMainThread();
  }

  Browser* GetBrowser() const override { return incognito_browser_; }

 private:
  Browser* incognito_browser_ = nullptr;
};

// Regression test for https://crbug.com/476474
IN_PROC_BROWSER_TEST_F(PushMessagingIncognitoBrowserTest,
                       IncognitoGetSubscriptionDoesNotHang) {
  ASSERT_TRUE(GetBrowser()->profile()->IsOffTheRecord());

  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  // In Incognito mode the promise returned by getSubscription should not hang,
  // it should just fulfill with null.
  ASSERT_TRUE(RunScript("hasSubscription()", &script_result));
  ASSERT_EQ("false - not subscribed", script_result);
}

// None of the following should matter on ChromeOS: crbug.com/527045
#if BUILDFLAG(ENABLE_BACKGROUND) && !defined(OS_CHROMEOS)
// Push background mode is disabled by default.
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       BackgroundModeDisabledByDefault) {
  // Initially background mode is inactive.
  BackgroundModeManager* background_mode_manager =
      g_browser_process->background_mode_manager();
  ASSERT_FALSE(background_mode_manager->IsBackgroundModeActive());

  // Once there is a push subscription background mode is still inactive.
  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);
  ASSERT_FALSE(background_mode_manager->IsBackgroundModeActive());

  // After dropping the last subscription it is still inactive.
  std::string script_result;
  gcm_service()->AddExpectedUnregisterResponse(gcm::GCMClient::SUCCESS);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);
  ASSERT_FALSE(background_mode_manager->IsBackgroundModeActive());
}

class PushMessagingBackgroundModeEnabledBrowserTest
    : public PushMessagingBrowserTest {
 public:
  ~PushMessagingBackgroundModeEnabledBrowserTest() override {}

  // PushMessagingBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePushApiBackgroundMode);
    PushMessagingBrowserTest::SetUpCommandLine(command_line);
  }
};

// In this test the command line enables push background mode.
IN_PROC_BROWSER_TEST_F(PushMessagingBackgroundModeEnabledBrowserTest,
                       BackgroundModeEnabledWithCommandLine) {
  // Initially background mode is inactive.
  BackgroundModeManager* background_mode_manager =
      g_browser_process->background_mode_manager();
  ASSERT_FALSE(background_mode_manager->IsBackgroundModeActive());

  // Once there is a push subscription background mode is active.
  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);
  ASSERT_TRUE(background_mode_manager->IsBackgroundModeActive());

  // Dropping the last subscription deactivates background mode.
  std::string script_result;
  gcm_service()->AddExpectedUnregisterResponse(gcm::GCMClient::SUCCESS);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);
  ASSERT_FALSE(background_mode_manager->IsBackgroundModeActive());
}

class PushMessagingBackgroundModeDisabledBrowserTest
    : public PushMessagingBrowserTest {
 public:
  ~PushMessagingBackgroundModeDisabledBrowserTest() override {}

  // PushMessagingBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisablePushApiBackgroundMode);
    PushMessagingBrowserTest::SetUpCommandLine(command_line);
  }
};

// In this test the command line disables push background mode.
IN_PROC_BROWSER_TEST_F(PushMessagingBackgroundModeDisabledBrowserTest,
                       BackgroundModeDisabledWithCommandLine) {
  // Initially background mode is inactive.
  BackgroundModeManager* background_mode_manager =
      g_browser_process->background_mode_manager();
  ASSERT_FALSE(background_mode_manager->IsBackgroundModeActive());

  // Once there is a push subscription background mode is still inactive.
  TryToSubscribeSuccessfully("1-0" /* expected_push_subscription_id */);
  ASSERT_FALSE(background_mode_manager->IsBackgroundModeActive());

  // After dropping the last subscription background mode is still inactive.
  std::string script_result;
  gcm_service()->AddExpectedUnregisterResponse(gcm::GCMClient::SUCCESS);
  ASSERT_TRUE(RunScript("unsubscribePush()", &script_result));
  EXPECT_EQ("unsubscribe result: true", script_result);
  ASSERT_FALSE(background_mode_manager->IsBackgroundModeActive());
}
#endif  // BUILDFLAG(ENABLE_BACKGROUND) && !defined(OS_CHROMEOS)
