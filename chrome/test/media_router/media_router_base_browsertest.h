// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_BASE_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_BASE_BROWSERTEST_H_

#include <string>

#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/browser/process_manager_observer.h"

namespace media_router {

class MediaRouter;

/**
 * Base class for media router browser test.
 *
 * It provides the basic functions for integration and e2e browser tests,
 * including install unpacked or packed extension at beginning of the test,
 * uninstall the extension at the end of the test.
 *
 * This class accepts two flags to specify the location of MR extension:
 * 1. "--extension-crx" flag to specify the packed extension location
 * 2. "--extension-unpacked" flag to specify the unpacked extension location
 * Only one of them should be passed when run browser tests.
 */
class MediaRouterBaseBrowserTest : public ExtensionBrowserTest,
                                   public extensions::ProcessManagerObserver {
 public:
  MediaRouterBaseBrowserTest();
  ~MediaRouterBaseBrowserTest() override;

  // InProcessBrowserTest Overrides
  void SetUp() override;
  void TearDown() override;

 protected:
  // InProcessBrowserTest Overrides
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void InstallAndEnableMRExtension();
  void UninstallMRExtension();

  virtual void ParseCommandLine();

  // extensions::ProcessManagerObserver Overrides
  void OnBackgroundHostCreated(extensions::ExtensionHost* host) override;

  // Wait until get the successful callback or timeout.
  // TODO(leilei): Replace this method with WaitableEvent class.
  void ConditionalWait(base::TimeDelta timeout,
                       base::TimeDelta interval,
                       const base::Callback<bool(void)>& callback);

  // Wait for a specific time.
  void Wait(base::TimeDelta timeout);

  bool is_unpacked() const { return !extension_unpacked_.empty(); }

  bool is_extension_host_created() const { return extension_host_created_; }

  // These values are initialized via flags.
  base::FilePath extension_crx_;
  base::FilePath extension_unpacked_;

  base::WaitableEvent extension_load_event_;
  std::string extension_id_;
  bool extension_host_created_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRouterBaseBrowserTest);
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_BASE_BROWSERTEST_H_
