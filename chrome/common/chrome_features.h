// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the chrome
// module.

#ifndef CHROME_COMMON_CHROME_FEATURES_H_
#define CHROME_COMMON_CHROME_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if defined(OS_CHROMEOS)
extern const base::Feature kArcMemoryManagement;
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX)
extern const base::Feature kAutomaticTabDiscarding;
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_LINUX)
extern const base::Feature kBackgroundModeAllowRestart;
#endif  // defined(OS_WIN) || defined(OS_LINUX)

extern const base::Feature kBackspaceGoesBackFeature;

extern const base::Feature kBlockPromptsIfDismissedOften;

extern const base::Feature kBlockSmallContent;

extern const base::Feature kBrowserHangFixesExperiment;

extern const base::Feature kDisplayPersistenceToggleInPermissionPrompts;

extern const base::Feature kExpectCTReporting;

extern const base::Feature kExperimentalKeyboardLockUI;

#if defined(OS_CHROMEOS)
extern const base::Feature kHappinessTrackingSystem;
#endif

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const base::Feature kLinuxObsoleteSystemIsEndOfTheLine;
#endif

#if defined(ENABLE_EXTENSIONS)
extern const base::Feature kMaterialDesignExtensions;
#endif

extern const base::Feature kMaterialDesignHistory;

extern const base::Feature kMaterialDesignSettings;

extern const base::Feature kPreferHtmlOverPlugins;

extern const base::Feature kOverrideYouTubeFlashEmbed;

extern const base::Feature kPushMessagingBackgroundMode;

#if defined(OS_CHROMEOS)
extern const base::Feature kRuntimeMemoryLeakDetector;
#endif  // defined(OS_CHROMEOS)

extern const base::Feature kSafeSearchUrlReporting;

extern const base::Feature kSimplifiedFullscreenUI;

#if defined(SYZYASAN)
extern const base::Feature kSyzyasanDeferredFree;
#endif

#if defined(OS_CHROMEOS)
extern const base::Feature kOptInImeMenu;

extern const base::Feature kQuickUnlockPin;
#endif  // defined(OS_CHROMEOS)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
