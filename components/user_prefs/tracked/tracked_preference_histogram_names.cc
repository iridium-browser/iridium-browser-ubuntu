// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/tracked_preference_histogram_names.h"

namespace user_prefs {
namespace tracked {

// Tracked pref histogram names.
const char kTrackedPrefHistogramUnchanged[] =
    "Settings.TrackedPreferenceUnchanged";
const char kTrackedPrefHistogramCleared[] = "Settings.TrackedPreferenceCleared";
const char kTrackedPrefHistogramMigratedLegacyDeviceId[] =
    "Settings.TrackedPreferenceMigratedLegacyDeviceId";
const char kTrackedPrefHistogramChanged[] = "Settings.TrackedPreferenceChanged";
const char kTrackedPrefHistogramInitialized[] =
    "Settings.TrackedPreferenceInitialized";
const char kTrackedPrefHistogramTrustedInitialized[] =
    "Settings.TrackedPreferenceTrustedInitialized";
const char kTrackedPrefHistogramNullInitialized[] =
    "Settings.TrackedPreferenceNullInitialized";
const char kTrackedPrefHistogramWantedReset[] =
    "Settings.TrackedPreferenceWantedReset";
const char kTrackedPrefHistogramReset[] = "Settings.TrackedPreferenceReset";
const char kTrackedSplitPrefHistogramChanged[] =
    "Settings.TrackedSplitPreferenceChanged.";

}  // namespace tracked
}  // namespace user_prefs
