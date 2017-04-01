// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Tests logic in the SelectFileDialog class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectFileDialogTest {
    /**
     * Returns the determined scope for the accepted |fileTypes|.
     */
    private int scopeForFileTypes(String... fileTypes) {
        SelectFileDialog instance = SelectFileDialog.create((long) 0 /* nativeSelectFileDialog */);
        instance.setFileTypesForTests(new ArrayList<String>(Arrays.asList(fileTypes)));

        return instance.determineSelectFileDialogScope();
    }

    @Test
    public void testDetermineSelectFileDialogScope() {
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC, scopeForFileTypes());
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC, scopeForFileTypes("*/*"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC, scopeForFileTypes("text/plain"));

        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES, scopeForFileTypes("image/*"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES, scopeForFileTypes("image/png"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("image/*", "test/plain"));

        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS, scopeForFileTypes("video/*"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS, scopeForFileTypes("video/ogg"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("video/*", "test/plain"));

        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS,
                scopeForFileTypes("video/*", "image/*"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS,
                scopeForFileTypes("image/jpeg", "video/ogg"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("video/*", "image/*", "text/plain"));
    }
}
