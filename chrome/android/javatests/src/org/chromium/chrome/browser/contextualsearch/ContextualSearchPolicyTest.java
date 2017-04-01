// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;

import java.util.ArrayList;

/**
 * Tests for the ContextualSearchPolicy class.
 */
public class ContextualSearchPolicyTest extends ChromeTabbedActivityTestBase {
    ContextualSearchPolicy mPolicy;

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Context context = getActivity().getApplicationContext();
                mPolicy = new ContextualSearchPolicy(context, null, null);
            }
        });
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testBestTargetLanguageFromMultiple() {
        ArrayList<String> list = new ArrayList<String>();
        list.add("br");
        list.add("de");
        assertEquals("br", mPolicy.bestTargetLanguage(list));
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testBestTargetLanguageSkipsEnglish() {
        ArrayList<String> list = new ArrayList<String>();
        list.add("en");
        list.add("de");
        assertEquals("de", mPolicy.bestTargetLanguage(list));
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    @CommandLineFlags.Add(ContextualSearchFieldTrial.ENABLE_ENGLISH_TARGET_TRANSLATION + "=true")
    public void testBestTargetLanguageReturnsEnglishWhenEnabled() {
        ArrayList<String> list = new ArrayList<String>();
        list.add("en");
        list.add("de");
        assertEquals("en", mPolicy.bestTargetLanguage(list));
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testBestTargetLanguageUsesEnglishWhenOnlyChoice() {
        ArrayList<String> list = new ArrayList<String>();
        list.add("en");
        assertEquals("en", mPolicy.bestTargetLanguage(list));
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testBestTargetLanguageReturnsEmptyWhenNoChoice() {
        ArrayList<String> list = new ArrayList<String>();
        assertEquals("", mPolicy.bestTargetLanguage(list));
    }

    // TODO(donnd): This set of tests is not complete, add more tests.
}
