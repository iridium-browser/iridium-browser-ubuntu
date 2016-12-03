// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

namespace features {

// All features in alphabetical order.

#if defined(OS_CHROMEOS)
// Whether to handle low memory kill of ARC apps by Chrome.
const base::Feature kArcMemoryManagement{
    "ArcMemoryManagement", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX)
// Enables automatic tab discarding, when the system is in low memory state.
const base::Feature kAutomaticTabDiscarding{"AutomaticTabDiscarding",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_LINUX)
// Enables the Restart background mode optimization. When all Chrome UI is
// closed and it goes in the background, allows to restart the browser to
// discard memory.
const base::Feature kBackgroundModeAllowRestart{
    "BackgroundModeAllowRestart", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_LINUX)

// Enables the Backspace key to navigate back in the browser, as well as
// Shift+Backspace to navigate forward.
const base::Feature kBackspaceGoesBackFeature {
  "BackspaceGoesBack", base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables or disables whether permission prompts are automatically blocked
// after the user has explicitly dismissed them too many times.
const base::Feature kBlockPromptsIfDismissedOften{
    "BlockPromptsIfDismissedOften", base::FEATURE_DISABLED_BY_DEFAULT};

// Experiment to disable small cross-origin content. (http://crbug.com/608886)
const base::Feature kBlockSmallContent{"BlockSmallPluginContent",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Fixes for browser hang bugs are deployed in a field trial in order to measure
// their impact. See crbug.com/478209.
const base::Feature kBrowserHangFixesExperiment{
    "BrowserHangFixesExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Experiment to display a toggle allowing users to opt-out of persisting a
// Grant or Deny decision in a permission prompt.
const base::Feature kDisplayPersistenceToggleInPermissionPrompts{
    "DisplayPersistenceToggleInPermissionPrompts",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Expect CT reporting, which sends reports for opted-in sites
// that don't serve sufficient Certificate Transparency information.
const base::Feature kExpectCTReporting{"ExpectCTReporting",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// An experimental fullscreen prototype that allows pages to map browser and
// system-reserved keyboard shortcuts.
const base::Feature kExperimentalKeyboardLockUI{
    "ExperimentalKeyboardLockUI", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined (OS_CHROMEOS)
// Enables or disables the Happiness Tracking System for the device.
const base::Feature kHappinessTrackingSystem {
    "HappinessTrackingSystem", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Enables showing the "This computer will no longer receive Google Chrome
// updates" infobar instead of the "will soon stop receiving" infobar on
// deprecated systems.
const base::Feature kLinuxObsoleteSystemIsEndOfTheLine{
    "LinuxObsoleteSystemIsEndOfTheLine", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(ENABLE_EXTENSIONS)
// Enabled or disabled the Material Design version of chrome://extensions.
const base::Feature kMaterialDesignExtensions{
    "MaterialDesignExtensions", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables YouTube Flash videos to be overridden.
const base::Feature kOverrideYouTubeFlashEmbed{
    "OverrideYouTubeFlashEmbed", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables push subscriptions keeping Chrome running in the
// background when closed.
const base::Feature kPushMessagingBackgroundMode{
    "PushMessagingBackgroundMode", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Material Design version of chrome://history.
const base::Feature kMaterialDesignHistory{
    "MaterialDesignHistory", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Material Design version of chrome://settings.
// Also affects chrome://help.
const base::Feature kMaterialDesignSettings{
    "MaterialDesignSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Prefer HTML content by hiding Flash from the list of plugins.
// https://crbug.com/626728
const base::Feature kPreferHtmlOverPlugins{"PreferHtmlOverPlugins",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Runtime flag that indicates whether this leak detector should be enabled in
// the current instance of Chrome.
const base::Feature kRuntimeMemoryLeakDetector{
    "RuntimeMemoryLeakDetector", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

const base::Feature kSafeSearchUrlReporting{"SafeSearchUrlReporting",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// A new user experience for transitioning into fullscreen and mouse pointer
// lock states.
const base::Feature kSimplifiedFullscreenUI{"ViewsSimplifiedFullscreenUI",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(SYZYASAN)
// Enable the deferred free mechanism in the syzyasan module, which helps the
// performance by deferring some work on the critical path to a background
// thread.
const base::Feature kSyzyasanDeferredFree{"SyzyasanDeferredFree",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// Enables or disables the opt-in IME menu in the language settings page.
const base::Feature kOptInImeMenu{"OptInImeMenu",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables PIN quick unlock settings integration.
const base::Feature kQuickUnlockPin{"QuickUnlockPin",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

}  // namespace features
