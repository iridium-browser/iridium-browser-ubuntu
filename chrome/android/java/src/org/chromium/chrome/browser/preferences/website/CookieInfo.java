// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

/**
 * Cookie information for a given origin.
 */
public class CookieInfo extends PermissionInfo {
    public CookieInfo(String origin, String embedder, boolean isIncognito) {
        super(origin, embedder, isIncognito);
    }

    protected int getNativePreferenceValue(String origin, String embedder, boolean isIncognito) {
        return WebsitePreferenceBridge.nativeGetCookieSettingForOrigin(
                origin, embedder, isIncognito);
    }

    protected void setNativePreferenceValue(
            String origin, String embedder, int value, boolean isIncognito) {
        WebsitePreferenceBridge.nativeSetCookieSettingForOrigin(
                origin, embedder, value, isIncognito);
    }
}
