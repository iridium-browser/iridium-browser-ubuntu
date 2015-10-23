// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::NetworkChangeNotifier;

namespace content {

namespace {

const char kDefaultTestURL[] = "files/background_sync/test.html";

const char kSuccessfulOperationPrefix[] = "ok - ";

std::string BuildScriptString(const std::string& function,
                              const std::string& argument) {
  return base::StringPrintf("%s('%s');", function.c_str(), argument.c_str());
}

std::string BuildExpectedResult(const std::string& tag,
                                const std::string& action) {
  return base::StringPrintf("%s%s %s", kSuccessfulOperationPrefix, tag.c_str(),
                            action.c_str());
}

void OneShotPendingCallback(
    const base::Closure& quit,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    bool* result_out,
    bool result) {
  *result_out = result;
  task_runner->PostTask(FROM_HERE, quit);
}

void OneShotPendingDidGetSyncRegistration(
    const base::Callback<void(bool)>& callback,
    BackgroundSyncStatus error_type,
    const BackgroundSyncRegistration& registration) {
  ASSERT_EQ(BACKGROUND_SYNC_STATUS_OK, error_type);
  callback.Run(registration.sync_state() == SYNC_STATE_PENDING);
}

void OneShotPendingDidGetSWRegistration(
    const scoped_refptr<BackgroundSyncContext> sync_context,
    const std::string& tag,
    const base::Callback<void(bool)>& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  ASSERT_EQ(SERVICE_WORKER_OK, status);
  int64 service_worker_id = registration->id();
  BackgroundSyncManager* sync_manager = sync_context->background_sync_manager();
  sync_manager->GetRegistration(
      service_worker_id, tag, SYNC_ONE_SHOT,
      base::Bind(&OneShotPendingDidGetSyncRegistration, callback));
}

void OneShotPendingOnIOThread(
    const scoped_refptr<BackgroundSyncContext> sync_context,
    const scoped_refptr<ServiceWorkerContextWrapper> sw_context,
    const std::string& tag,
    const GURL& url,
    const base::Callback<void(bool)>& callback) {
  sw_context->FindRegistrationForDocument(
      url, base::Bind(&OneShotPendingDidGetSWRegistration, sync_context, tag,
                      callback));
}

class BackgroundSyncBrowserTest : public ContentBrowserTest {
 public:
  BackgroundSyncBrowserTest() {}
  ~BackgroundSyncBrowserTest() override {}

  void SetUp() override {
    NetworkChangeNotifier::SetTestNotificationsOnly(true);

#if defined(OS_CHROMEOS)
    // ChromeOS's NetworkChangeNotifier doesn't get created in
    // content_browsertests, so make one now.
    net::NetworkChangeNotifier::CreateMock();
#endif

    ContentBrowserTest::SetUp();
  }

  void SetIncognitoMode(bool incognito) {
    shell_ = incognito ? CreateOffTheRecordBrowser() : shell();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(jkarlin): Remove this once background sync is no longer
    // experimental.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_.reset(new net::SpawnedTestServer(
        net::SpawnedTestServer::TYPE_HTTPS,
        net::BaseTestServer::SSLOptions(
            net::BaseTestServer::SSLOptions::CERT_OK),
        base::FilePath(FILE_PATH_LITERAL("content/test/data/"))));
    ASSERT_TRUE(https_server_->Start());

    SetOnline(true);

    SetIncognitoMode(false);

    ASSERT_TRUE(LoadTestPage(kDefaultTestURL));

    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { https_server_.reset(); }

  bool LoadTestPage(const std::string& path) {
    return NavigateToURL(shell_, https_server_->GetURL(path));
  }

  bool RunScript(const std::string& script, std::string* result) {
    return content::ExecuteScriptAndExtractString(shell_->web_contents(),
                                                  script, result);
  }

  void SetOnline(bool online);

  // Returns true if the one-shot sync with tag is currently pending. Fails
  // (assertion failure) if the tag isn't registered.
  bool OneShotPending(const std::string& tag);

  bool PopConsole(const std::string& expected_msg);
  bool RegisterServiceWorker();
  bool RegisterOneShot(const std::string& tag);
  bool GetRegistrationOneShot(const std::string& tag);
  bool GetRegistrationsOneShot(const std::vector<std::string>& expected_tags);
  bool CompleteDelayedOneShot();
  bool RejectDelayedOneShot();

 private:
  scoped_ptr<net::SpawnedTestServer> https_server_;
  Shell* shell_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncBrowserTest);
};

void BackgroundSyncBrowserTest::SetOnline(bool online) {
  if (online) {
    NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        NetworkChangeNotifier::CONNECTION_WIFI);
  } else {
    NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        NetworkChangeNotifier::CONNECTION_NONE);
  }
  base::RunLoop().RunUntilIdle();
}

bool BackgroundSyncBrowserTest::OneShotPending(const std::string& tag) {
  bool is_pending;
  base::RunLoop run_loop;

  StoragePartition* storage = BrowserContext::GetDefaultStoragePartition(
      shell_->web_contents()->GetBrowserContext());
  BackgroundSyncContext* sync_context = storage->GetBackgroundSyncContext();
  ServiceWorkerContextWrapper* service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          storage->GetServiceWorkerContext());

  base::Callback<void(bool)> callback =
      base::Bind(&OneShotPendingCallback, run_loop.QuitClosure(),
                 base::ThreadTaskRunnerHandle::Get(), &is_pending);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OneShotPendingOnIOThread, make_scoped_refptr(sync_context),
                 make_scoped_refptr(service_worker_context), tag,
                 https_server_->GetURL(kDefaultTestURL), callback));

  run_loop.Run();

  return is_pending;
}

bool BackgroundSyncBrowserTest::PopConsole(const std::string& expected_msg) {
  std::string script_result;
  EXPECT_TRUE(RunScript("resultQueue.pop()", &script_result));
  return script_result == expected_msg;
}

bool BackgroundSyncBrowserTest::RegisterServiceWorker() {
  std::string script_result;
  EXPECT_TRUE(RunScript("registerServiceWorker()", &script_result));
  return script_result == BuildExpectedResult("service worker", "registered");
}

bool BackgroundSyncBrowserTest::RegisterOneShot(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("registerOneShot", tag), &script_result));
  return script_result == BuildExpectedResult(tag, "registered");
}

bool BackgroundSyncBrowserTest::GetRegistrationOneShot(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(BuildScriptString("getRegistrationOneShot", tag),
                        &script_result));
  return script_result == BuildExpectedResult(tag, "found");
}

bool BackgroundSyncBrowserTest::GetRegistrationsOneShot(
    const std::vector<std::string>& expected_tags) {
  std::string script_result;
  EXPECT_TRUE(RunScript("getRegistrationsOneShot()", &script_result));

  EXPECT_TRUE(base::StartsWith(script_result, kSuccessfulOperationPrefix,
                               base::CompareCase::INSENSITIVE_ASCII));
  script_result = script_result.substr(strlen(kSuccessfulOperationPrefix));
  std::vector<std::string> result_tags = base::SplitString(
      script_result, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  return std::set<std::string>(expected_tags.begin(), expected_tags.end()) ==
         std::set<std::string>(result_tags.begin(), result_tags.end());
}

bool BackgroundSyncBrowserTest::CompleteDelayedOneShot() {
  std::string script_result;
  EXPECT_TRUE(RunScript("completeDelayedOneShot()", &script_result));
  return script_result == BuildExpectedResult("delay", "completing");
}

bool BackgroundSyncBrowserTest::RejectDelayedOneShot() {
  std::string script_result;
  EXPECT_TRUE(RunScript("rejectDelayedOneShot()", &script_result));
  return script_result == BuildExpectedResult("delay", "rejecting");
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, OneShotFires) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(RegisterOneShot("foo"));
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(GetRegistrationOneShot("foo"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, OneShotDelaysForNetwork) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  // Prevent firing by going offline.
  SetOnline(false);
  EXPECT_TRUE(RegisterOneShot("foo"));
  EXPECT_TRUE(GetRegistrationOneShot("foo"));
  EXPECT_TRUE(OneShotPending("foo"));

  // Resume firing by going online.
  SetOnline(true);
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(GetRegistrationOneShot("foo"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, WaitUntil) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  SetOnline(true);
  EXPECT_TRUE(RegisterOneShot("delay"));

  // Verify that it is firing.
  EXPECT_TRUE(GetRegistrationOneShot("delay"));
  EXPECT_FALSE(OneShotPending("delay"));

  // Complete the task.
  EXPECT_TRUE(CompleteDelayedOneShot());
  EXPECT_TRUE(PopConsole("ok - delay completed"));

  // Verify that it finished firing.
  // TODO(jkarlin): Use registration.done to verify that the event actually
  // completed successfully.
  EXPECT_FALSE(GetRegistrationOneShot("delay"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, WaitUntilReject) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  SetOnline(true);
  EXPECT_TRUE(RegisterOneShot("delay"));

  // Verify that it is firing.
  EXPECT_TRUE(GetRegistrationOneShot("delay"));
  EXPECT_FALSE(OneShotPending("delay"));

  // Complete the task.
  EXPECT_TRUE(RejectDelayedOneShot());
  EXPECT_TRUE(PopConsole("ok - delay rejected"));

  // Since the event failed the registration should still be there.
  // TODO(jkarlin): Use registration.done to verify that the event actually
  // failed.
  EXPECT_TRUE(GetRegistrationOneShot("delay"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, Incognito) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  SetOnline(false);
  EXPECT_TRUE(RegisterOneShot("normal"));
  EXPECT_TRUE(OneShotPending("normal"));

  // Go incognito and verify that incognito doesn't see the registration.
  SetIncognitoMode(true);

  // Tell the new network observer that we're offline (it initializes from
  // NetworkChangeNotifier::GetCurrentConnectionType() which is not mocked out
  // in this test).
  SetOnline(false);

  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_FALSE(GetRegistrationOneShot("normal"));

  EXPECT_TRUE(RegisterOneShot("incognito"));
  EXPECT_TRUE(OneShotPending("incognito"));

  // Switch back and make sure the registration is still there.
  SetIncognitoMode(false);
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Should be controlled.

  EXPECT_TRUE(GetRegistrationOneShot("normal"));
  EXPECT_FALSE(GetRegistrationOneShot("incognito"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, GetRegistrations) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));

  SetOnline(false);
  registered_tags.push_back("foo");
  registered_tags.push_back("bar");

  for (const std::string& tag : registered_tags)
    EXPECT_TRUE(RegisterOneShot(tag));

  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));
}

}  // namespace

}  // namespace content
