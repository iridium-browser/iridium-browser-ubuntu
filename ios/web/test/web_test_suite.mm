// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/web_test_suite.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/url_schemes.h"
#include "ios/web/web_thread_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace web {

class WebTestSuiteListener : public testing::EmptyTestEventListener {
 public:
  WebTestSuiteListener() {}
  void OnTestEnd(const testing::TestInfo& test_info) override {
    WebThreadImpl::FlushThreadPoolHelperForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTestSuiteListener);
};

WebTestSuite::WebTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv),
      web_client_(base::WrapUnique(new TestWebClient)) {}

WebTestSuite::~WebTestSuite() {}

void WebTestSuite::Initialize() {
  base::TestSuite::Initialize();

  // Initialize the histograms subsystem, so that any histograms hit in tests
  // are correctly registered with the statistics recorder and can be queried
  // by tests.
  base::StatisticsRecorder::Initialize();

  testing::UnitTest::GetInstance()->listeners().Append(
      new WebTestSuiteListener);

  RegisterWebSchemes(false);

  // Force unittests to run using en-US so if testing string output will work
  // regardless of the system language.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  base::FilePath resources_pack_path;
  base::PathService::Get(base::DIR_MODULE, &resources_pack_path);
  resources_pack_path =
      resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);
}

void WebTestSuite::Shutdown() {
  ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace web
