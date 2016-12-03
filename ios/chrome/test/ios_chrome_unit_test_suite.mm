// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/ios_chrome_unit_test_suite.h"

#include "base/macros.h"
#include "base/path_service.h"
#include "ios/chrome/browser/browser_state/browser_state_keyed_service_factories.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/test/testing_application_context.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/test_chrome_provider_initializer.h"
#include "ios/web/public/web_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "url/url_util.h"

namespace {

class IOSChromeUnitTestSuiteInitializer
    : public testing::EmptyTestEventListener {
 public:
  IOSChromeUnitTestSuiteInitializer() {}
  ~IOSChromeUnitTestSuiteInitializer() override {}

  void OnTestStart(const testing::TestInfo& test_info) override {
    DCHECK(!web_client_);
    web_client_.reset(new web::WebClient);
    web::SetWebClient(web_client_.get());

    DCHECK(!ios::GetChromeBrowserProvider());
    test_ios_chrome_provider_initializer_.reset(
        new ios::TestChromeProviderInitializer());

    DCHECK(!GetApplicationContext());
    application_context_.reset(new TestingApplicationContext);
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    DCHECK_EQ(GetApplicationContext(), application_context_.get());
    application_context_.reset();

    test_ios_chrome_provider_initializer_.reset();
    DCHECK(!ios::GetChromeBrowserProvider());

    DCHECK_EQ(web::GetWebClient(), web_client_.get());
    web::SetWebClient(nullptr);
    web_client_.reset();
  }

 private:
  std::unique_ptr<web::WebClient> web_client_;
  std::unique_ptr<ios::TestChromeProviderInitializer>
      test_ios_chrome_provider_initializer_;
  std::unique_ptr<ApplicationContext> application_context_;
  DISALLOW_COPY_AND_ASSIGN(IOSChromeUnitTestSuiteInitializer);
};

}  // namespace

IOSChromeUnitTestSuite::IOSChromeUnitTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

IOSChromeUnitTestSuite::~IOSChromeUnitTestSuite() {}

void IOSChromeUnitTestSuite::Initialize() {
  // Add an additional listener to do the extra initialization for unit tests.
  // It will be started before the base class listeners and ended after the
  // base class listeners.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new IOSChromeUnitTestSuiteInitializer);

  // Ensure that all BrowserStateKeyedServiceFactories are built before any
  // test is run so that the dependencies are correctly resolved.
  EnsureBrowserStateKeyedServiceFactoriesBuilt();

  ios::RegisterPathProvider();
  ui::RegisterPathProvider();

  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  base::FilePath resources_pack_path;
  PathService::Get(ios::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_100P);

  url::AddStandardScheme(kChromeUIScheme, url::SCHEME_WITHOUT_PORT);

  base::TestSuite::Initialize();
}
