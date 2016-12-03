// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/chrome_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Disables all bookmarks view in bookmark manager.
const char kDisableAllBookmarksView[] = "disable-all-bookmarks-view";

// Disables Contextual Search.
const char kDisableContextualSearch[] = "disable-contextual-search";

// Disables a workaround for fast inset updates for UIWebView.scrollView.
const char kDisableIOSFastWebScrollViewInsets[] =
    "disable-fast-web-scroll-view-insets";

// Lists separated by commas the name of features to disable.
// See base::FeatureList::InitializeFromCommandLine for details.
const char kDisableIOSFeatures[] = "disable-features";

// Disable password generation for iOS.
const char kDisableIOSPasswordGeneration[] = "disable-ios-password-generation";

// Disable showing available password credentials in the keyboard accessory
// view when focused on form fields.
const char kDisableIOSPasswordSuggestions[] =
    "disable-ios-password-suggestions";

// Disable the snapshots lru cache.
const char kDisableLRUSnapshotCache[] = "disable-lru-snapshot-cache";

// Disable auto-reload of error pages if offline.
const char kDisableOfflineAutoReload[] = "disable-offline-auto-reload";

// Disable the Payment Request API.
const char kDisablePaymentRequest[] = "disable-payment-request";

// Disables the tab switcher.
const char kDisableTabSwitcher[] = "disable-tab-switcher";

// Disables Physical Web scanning for nearby URLs.
const char kDisableIOSPhysicalWeb[] = "disable-ios-physical-web";

// Enables all bookmarks view in bookmark manager.
const char kEnableAllBookmarksView[] = "enable-all-bookmarks-view";

// Enables Contextual Search.
const char kEnableContextualSearch[] = "enable-contextual-search";

// Enable the experimental Credential Manager JavaScript API.
const char kEnableCredentialManagerAPI[] = "enable-credential-manager-api";

// Enables a workaround for fast inset updates for UIWebView.scrollView.
const char kEnableIOSFastWebScrollViewInsets[] =
    "enable-fast-web-scroll-view-insets";

// Lists separated by commas the name of features to disable.
// See base::FeatureList::InitializeFromCommandLine for details.
const char kEnableIOSFeatures[] = "enable-features";

// Enables support for Handoff from Chrome on iOS to the default browser of
// other Apple devices.
const char kEnableIOSHandoffToOtherDevices[] =
    "enable-ios-handoff-to-other-devices";

// Enable password generation for iOS.
const char kEnableIOSPasswordGeneration[] = "enable-ios-password-generation";

// Enables the snapshot lru cache.
const char kEnableLRUSnapshotCache[] = "enable-lru-snapshot-cache";

// Enable auto-reload of error pages if offline.
const char kEnableOfflineAutoReload[] = "enable-offline-auto-reload";

// Enable the Payment Request API.
const char kEnablePaymentRequest[] = "enable-payment-request";

// Enables the QR Code scanner.
const char kEnableQRScanner[] = "enable-qr-scanner";

// Enables context-sensitive reader mode button in the toolbar.
const char kEnableReaderModeToolbarIcon[] = "enable-reader-mode-toolbar-icon";

// Enables the tab switcher.
const char kEnableTabSwitcher[] = "enable-tab-switcher";

// Enables Physical Web scanning for nearby URLs.
const char kEnableIOSPhysicalWeb[] = "enable-ios-physical-web";

// Forces additional Chrome Variation Ids that will be sent in X-Client-Data
// header, specified as a 64-bit encoded list of numeric experiment ids. Ids
// prefixed with the character "t" will be treated as Trigger Variation Ids.
const char kIOSForceVariationIds[] = "force-variation-ids";

// Enables the recording of metrics reports but disables reporting. In contrast
// to kDisableMetrics, this executes all the code that a normal client would
// use for reporting, except the report is dropped rather than sent to the
// server. This is useful for finding issues in the metrics code during UI and
// performance tests.
const char kIOSMetricsRecordingOnly[] = "metrics-recording-only";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

// These mappings only apply to the host resolver.
const char kIOSHostResolverRules[] = "host-resolver-rules";

// Ignores certificate-related errors.
const char kIOSIgnoreCertificateErrors[] = "ignore-certificate-errors";

// Allows for forcing socket connections to http/https to use fixed ports.
const char kIOSTestingFixedHttpPort[] = "testing-fixed-http-port";
const char kIOSTestingFixedHttpsPort[] = "testing-fixed-https-port";

// Enables grouping websites by domain and filtering them by period.
const char kHistoryEnableGroupByDomain[] = "enable-grouped-history";

// Use to opt-in to marking HTTP as non-secure.
const char kMarkNonSecureAs[] = "mark-non-secure-as";
const char kMarkNonSecureAsNeutral[] = "neutral";
const char kMarkNonSecureAsNonSecure[] = "non-secure";

}  // namespace switches
