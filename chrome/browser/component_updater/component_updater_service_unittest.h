// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_UNITTEST_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
class LocalHostTestURLRequestInterceptor;
}

namespace update_client {
struct CrxComponent;
class InterceptorFactory;
class TestConfigurator;
class TestInstaller;
class URLRequestPostInterceptor;
}

namespace component_updater {

// Intercepts HTTP GET requests sent to "localhost".
typedef net::LocalHostTestURLRequestInterceptor GetInterceptor;

class ComponentUpdaterTest : public testing::Test {
 public:
  enum TestComponents {
    kTestComponent_abag,
    kTestComponent_jebg,
    kTestComponent_ihfo,
  };

  ComponentUpdaterTest();

  ~ComponentUpdaterTest() override;

  void SetUp() override;

  void TearDown() override;

  ComponentUpdateService* component_updater();

  // Makes the full path to a component updater test file.
  const base::FilePath test_file(const char* file);

  scoped_refptr<update_client::TestConfigurator> test_configurator();

  ComponentUpdateService::Status RegisterComponent(
      update_client::CrxComponent* com,
      TestComponents component,
      const Version& version,
      const scoped_refptr<update_client::TestInstaller>& installer);

 protected:
  void RunThreads();
  void RunThreadsUntilIdle();

  scoped_ptr<update_client::InterceptorFactory> interceptor_factory_;

  // Owned by the factory.
  update_client::URLRequestPostInterceptor* post_interceptor_;

  scoped_ptr<GetInterceptor> get_interceptor_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<update_client::TestConfigurator> test_config_;
  scoped_ptr<ComponentUpdateService> component_updater_;
};

const char expected_crx_url[] =
    "http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx";

class MockServiceObserver : public ServiceObserver {
 public:
  MockServiceObserver();
  ~MockServiceObserver();
  MOCK_METHOD2(OnEvent, void(Events event, const std::string&));
};

class OnDemandTester {
 public:
  static ComponentUpdateService::Status OnDemand(
      ComponentUpdateService* cus,
      const std::string& component_id);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_UNITTEST_H_
