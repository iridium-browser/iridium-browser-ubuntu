// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.chromium.base.Log;

/**
 * Activity for displaying WebView OSS licenses.
 */
public class LicenseActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final String packageName = getPackageName();
        final Intent intent = new Intent(Intent.ACTION_VIEW);
        final String licenseUri = String.format("content://%s.%s",
                packageName, LicenseContentProvider.LICENSES_URI_SUFFIX);
        intent.setDataAndType(
                Uri.parse(licenseUri), LicenseContentProvider.LICENSES_CONTENT_TYPE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        final int titleId = getResources().getIdentifier(
                "license_activity_title", "string", packageName);
        if (titleId != 0) {
            intent.putExtra(Intent.EXTRA_TITLE, getString(titleId));
        }
        intent.setPackage("com.android.htmlviewer");

        try {
            startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.e("WebView", "Failed to find viewer", e);
        }
        finish();
    }
}
