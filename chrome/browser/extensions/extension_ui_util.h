// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UI_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UI_UTIL_H_

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

namespace ui_util {

// Returns true if the extension should be displayed in the app launcher.
// Checks whether the extension is an ephemeral app or should be hidden due to
// policy.
bool ShouldDisplayInAppLauncher(const Extension* extension,
                                content::BrowserContext* context);

// Returns true if the extension can be displayed in the app launcher.
// Checks whether the extension should be hidden due to policy, but does not
// exclude ephemeral apps.
bool CanDisplayInAppLauncher(const Extension* extension,
                             content::BrowserContext* context);

// Returns true if the extension should be displayed in the browser NTP.
// Checks whether the extension is an ephemeral app or should be hidden due to
// policy.
bool ShouldDisplayInNewTabPage(const Extension* extension,
                               content::BrowserContext* context);

// Returns true if the extension should be displayed in the extension
// settings page (i.e. chrome://extensions). Checks whether the extension is
// an ephemeral app.
bool ShouldDisplayInExtensionSettings(const Extension* extension,
                                      content::BrowserContext* context);

// Returns true if the extension should not be shown anywhere. This is
// mostly the same as the extension being a component extension, but also
// includes non-component apps that are hidden from the app launcher and NTP,
// as well as ephemeral apps.
bool ShouldNotBeVisible(const Extension* extension,
                        content::BrowserContext* context);

}  // namespace ui_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UI_UTIL_H_
