// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_listener.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "extensions/browser/extension_registry.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::ResourceController;
using content::ResourceThrottle;
using content::ResourceType;

namespace extensions {

namespace {

const char kMatchingUrl[] = "http://google.com/";
const char kNotMatchingUrl[] = "http://example.com/";
const char kTestData[] = "Hello, World!";

class ThrottleController : public base::SupportsUserData::Data,
                           public ResourceController {
 public:
  ThrottleController(net::URLRequest* request, ResourceThrottle* throttle)
      : request_(request),
        throttle_(throttle) {
    throttle_->set_controller_for_testing(this);
  }

  // ResourceController implementation:
  void Resume() override { request_->Start(); }
  void Cancel() override { NOTREACHED(); }
  void CancelAndIgnore() override { NOTREACHED(); }
  void CancelWithError(int error_code) override { NOTREACHED(); }

 private:
  net::URLRequest* request_;
  std::unique_ptr<ResourceThrottle> throttle_;
};

// A simple test net::URLRequestJob. We don't care what it does, only that
// whether it starts and finishes.
class SimpleTestJob : public net::URLRequestTestJob {
 public:
  SimpleTestJob(net::URLRequest* request,
                net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               test_headers(),
                               kTestData,
                               true) {}
 private:
  ~SimpleTestJob() override {}
};

// Yoinked from extension_manifest_unittest.cc.
std::unique_ptr<base::DictionaryValue> LoadManifestFile(
    const base::FilePath path,
    std::string* error) {
  EXPECT_TRUE(base::PathExists(path));
  JSONFileValueDeserializer deserializer(path);
  return base::DictionaryValue::From(deserializer.Deserialize(NULL, error));
}

scoped_refptr<Extension> LoadExtension(const std::string& filename,
                                       std::string* error) {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.
      AppendASCII("extensions").
      AppendASCII("manifest_tests").
      AppendASCII(filename.c_str());
  std::unique_ptr<base::DictionaryValue> value = LoadManifestFile(path, error);
  if (!value)
    return NULL;
  return Extension::Create(path.DirName(), Manifest::UNPACKED, *value,
                           Extension::NO_FLAGS, error);
}

class SimpleTestJobURLRequestInterceptor
    : public net::URLRequestInterceptor {
 public:
  SimpleTestJobURLRequestInterceptor() {}
  ~SimpleTestJobURLRequestInterceptor() override {}

  // net::URLRequestJobFactory::ProtocolHandler
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new SimpleTestJob(request, network_delegate);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleTestJobURLRequestInterceptor);
};

}  // namespace

class UserScriptListenerTest : public ExtensionServiceTestBase {
 public:
  UserScriptListenerTest() {
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", "google.com", std::unique_ptr<net::URLRequestInterceptor>(
                                  new SimpleTestJobURLRequestInterceptor()));
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", "example.com", std::unique_ptr<net::URLRequestInterceptor>(
                                   new SimpleTestJobURLRequestInterceptor()));
  }

  ~UserScriptListenerTest() override {
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler("http",
                                                                "google.com");
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler("http",
                                                                "example.com");
  }

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    service_->Init();
    base::RunLoop().RunUntilIdle();

    listener_ = new UserScriptListener();
  }

  void TearDown() override {
    listener_ = NULL;
    base::RunLoop().RunUntilIdle();
    ExtensionServiceTestBase::TearDown();
  }

 protected:
  std::unique_ptr<net::URLRequest> StartTestRequest(
      net::URLRequest::Delegate* delegate,
      const std::string& url_string,
      net::TestURLRequestContext* context) {
    GURL url(url_string);
    std::unique_ptr<net::URLRequest> request(
        context->CreateRequest(url, net::DEFAULT_PRIORITY, delegate));

    ResourceThrottle* throttle = listener_->CreateResourceThrottle(
        url, content::RESOURCE_TYPE_MAIN_FRAME);

    bool defer = false;
    if (throttle) {
      request->SetUserData(NULL,
                           new ThrottleController(request.get(), throttle));

      throttle->WillStartRequest(&defer);
    }

    if (!defer)
      request->Start();

    return request;
  }

  void LoadTestExtension() {
    base::FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    base::FilePath extension_path = test_dir
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
    UnpackedInstaller::Create(service_)->Load(extension_path);
  }

  void UnloadTestExtension() {
    const extensions::ExtensionSet& extensions =
        registry()->enabled_extensions();
    ASSERT_FALSE(extensions.is_empty());
    service_->UnloadExtension((*extensions.begin())->id(),
                              UnloadedExtensionInfo::REASON_DISABLE);
  }

  scoped_refptr<UserScriptListener> listener_;
};

namespace {

TEST_F(UserScriptListenerTest, DelayAndUpdate) {
  LoadTestExtension();
  base::RunLoop().RunUntilIdle();

  net::TestDelegate delegate;
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(
      StartTestRequest(&delegate, kMatchingUrl, &context));
  ASSERT_FALSE(request->is_pending());

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestData, delegate.data_received());
}

TEST_F(UserScriptListenerTest, DelayAndUnload) {
  LoadTestExtension();
  base::RunLoop().RunUntilIdle();

  net::TestDelegate delegate;
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(
      StartTestRequest(&delegate, kMatchingUrl, &context));
  ASSERT_FALSE(request->is_pending());

  UnloadTestExtension();
  base::RunLoop().RunUntilIdle();

  // This is still not enough to start delayed requests. We have to notify the
  // listener that the user scripts have been updated.
  ASSERT_FALSE(request->is_pending());

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestData, delegate.data_received());
}

TEST_F(UserScriptListenerTest, NoDelayNoExtension) {
  net::TestDelegate delegate;
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(
      StartTestRequest(&delegate, kMatchingUrl, &context));

  // The request should be started immediately.
  ASSERT_TRUE(request->is_pending());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestData, delegate.data_received());
}

TEST_F(UserScriptListenerTest, NoDelayNotMatching) {
  LoadTestExtension();
  base::RunLoop().RunUntilIdle();

  net::TestDelegate delegate;
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(
      StartTestRequest(&delegate, kNotMatchingUrl, &context));

  // The request should be started immediately.
  ASSERT_TRUE(request->is_pending());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestData, delegate.data_received());
}

TEST_F(UserScriptListenerTest, MultiProfile) {
  LoadTestExtension();
  base::RunLoop().RunUntilIdle();

  // Fire up a second profile and have it load an extension with a content
  // script.
  TestingProfile profile2;
  std::string error;
  scoped_refptr<Extension> extension = LoadExtension(
      "content_script_yahoo.json", &error);
  ASSERT_TRUE(extension.get());

  extensions::ExtensionRegistry::Get(&profile2)->AddEnabled(extension);

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
      content::Source<Profile>(&profile2),
      content::Details<Extension>(extension.get()));

  net::TestDelegate delegate;
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(
      StartTestRequest(&delegate, kMatchingUrl, &context));
  ASSERT_FALSE(request->is_pending());

  // When the first profile's user scripts are ready, the request should still
  // be blocked waiting for profile2.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(request->is_pending());
  EXPECT_TRUE(delegate.data_received().empty());

  // After profile2 is ready, the request should proceed.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(&profile2),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestData, delegate.data_received());
}

// Test when the script updated notification occurs before the throttle's
// WillStartRequest function is called.  This can occur when there are multiple
// throttles.
TEST_F(UserScriptListenerTest, ResumeBeforeStart) {
  LoadTestExtension();
  base::RunLoop().RunUntilIdle();
  net::TestDelegate delegate;
  net::TestURLRequestContext context;
  GURL url(kMatchingUrl);
  std::unique_ptr<net::URLRequest> request(
      context.CreateRequest(url, net::DEFAULT_PRIORITY, &delegate));

  ResourceThrottle* throttle =
      listener_->CreateResourceThrottle(url, content::RESOURCE_TYPE_MAIN_FRAME);
  ASSERT_TRUE(throttle);
  request->SetUserData(NULL, new ThrottleController(request.get(), throttle));

  ASSERT_FALSE(request->is_pending());

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();

  bool defer = false;
  throttle->WillStartRequest(&defer);
  ASSERT_FALSE(defer);
}

}  // namespace

}  // namespace extensions
