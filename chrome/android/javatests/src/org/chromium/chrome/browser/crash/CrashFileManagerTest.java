// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.util.regex.Pattern;

/**
 * Unittests for {@link CrashFileManager}.
 */
public class CrashFileManagerTest extends CrashTestCase {
    private static final int TEST_PID = 23;

    private File mTmpFile1;
    private File mTmpFile2;

    private File mDmpFile1;
    private File mDmpFile2;

    private File mUpFile1;
    private File mUpFile2;

    @SuppressFBWarnings("RV_RETURN_VALUE_IGNORED_BAD_PRACTICE")
    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // The following files will be deleted in CrashTestCase#tearDown().
        mTmpFile1 = new File(mCrashDir, "12345ABCDE" + CrashFileManager.TMP_SUFFIX);
        mTmpFile1.createNewFile();

        mTmpFile2 = new File(mCrashDir, "abcde12345" + CrashFileManager.TMP_SUFFIX);
        mTmpFile2.createNewFile();

        mDmpFile1 = new File(mCrashDir, "123_abc.dmp0");
        mDmpFile1.createNewFile();

        mDmpFile2 = new File(mCrashDir, "chromium-renderer_abc.dmp" + TEST_PID);
        mDmpFile2.createNewFile();

        mUpFile1 = new File(mCrashDir, "123_abcd.up0");
        mUpFile1.createNewFile();

        mUpFile2 = new File(mCrashDir, "chromium-renderer_abcd.up" + TEST_PID);
        mUpFile2.createNewFile();
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCrashFileManagerWithNull() {
        try {
            new CrashFileManager(null);
            fail("Constructor should throw NullPointerException with null context.");
        } catch (NullPointerException npe) {
            return;
        }
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMatchingFiles() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        // Three files begin with 123.
        File[] expectedFiles = new File[] { mTmpFile1, mDmpFile1, mUpFile1 };
        Pattern testPattern = Pattern.compile("^123");
        File[] actualFiles = crashFileManager.getMatchingFiles(testPattern);
        assertNotNull(actualFiles);
        assertEquals(expectedFiles.length, actualFiles.length);
        for (int i = 0; i < expectedFiles.length; i++) {
            // Test to see if files are properly ordered.
            assertEquals(expectedFiles[i], actualFiles[i]);
        }
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllMinidumpFilesSorted() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File[] expectedFiles = new File[] {mDmpFile1, mDmpFile2};
        File[] actualFiles = crashFileManager.getAllMinidumpFilesSorted();
        assertNotNull(actualFiles);
        assertEquals(expectedFiles.length, actualFiles.length);
        for (int i = 0; i < expectedFiles.length; i++) {
            // Test to see if files are properly ordered.
            assertEquals(expectedFiles[i], actualFiles[i]);
        }
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashDirectory() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File actualFile = crashFileManager.getCrashDirectory();
        assertTrue(actualFile.isDirectory());
        assertEquals(mCrashDir, actualFile);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testDeleteFile() {
        assertTrue(CrashFileManager.deleteFile(mTmpFile1));
        assertFalse(mTmpFile1.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllMinidumpFiles() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File[] expectedFiles = new File[] { mDmpFile1, mDmpFile2 };
        File[] actualFiles = crashFileManager.getAllMinidumpFiles();
        assertNotNull(actualFiles);
        assertEquals(expectedFiles.length, actualFiles.length);
        for (int i = 0; i < expectedFiles.length; i++) {
            // Test to see if files are properly ordered.
            assertEquals(expectedFiles[i], actualFiles[i]);
        }
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllUploadedFiles() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File[] expectedFiles = new File[] { mUpFile1, mUpFile2 };
        File[] actualFiles = crashFileManager.getAllUploadedFiles();
        assertNotNull(actualFiles);
        assertEquals(expectedFiles.length, actualFiles.length);
        for (int i = 0; i < expectedFiles.length; i++) {
            // Test to see if files are properly ordered.
            assertEquals(expectedFiles[i], actualFiles[i]);
        }
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAttemptNumber() {
        assertEquals(0, CrashFileManager.readAttemptNumber("file.dmp"));
        assertEquals(0, CrashFileManager.readAttemptNumber(".try"));
        assertEquals(0, CrashFileManager.readAttemptNumber("try1"));
        assertEquals(1, CrashFileManager.readAttemptNumber("file.try1.dmp"));
        assertEquals(1, CrashFileManager.readAttemptNumber("file.dmp.try1"));
        assertEquals(2, CrashFileManager.readAttemptNumber(".try2"));
        assertEquals(2, CrashFileManager.readAttemptNumber("file.try2.dmp"));
        assertEquals(2, CrashFileManager.readAttemptNumber("file.dmp.try2"));
        assertEquals(2, CrashFileManager.readAttemptNumber(".try2"));
        assertEquals(0, CrashFileManager.readAttemptNumber("file.tryN.dmp"));
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAttemptNumberRename() {
        assertEquals("file.dmp.try1", CrashFileManager.incrementAttemptNumber("file.dmp"));
        assertEquals("f.dmp.try2", CrashFileManager.incrementAttemptNumber("f.dmp.try1"));
        assertEquals("f.dmp.try20", CrashFileManager.incrementAttemptNumber("f.dmp.try10"));
        assertEquals("f.try2.dmp", CrashFileManager.incrementAttemptNumber("f.try1.dmp"));
        assertEquals("f.tryN.dmp.try1", CrashFileManager.incrementAttemptNumber("f.tryN.dmp"));
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCleanAllMiniDumps() {
        assertEquals(6, mCrashDir.listFiles().length);

        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        crashFileManager.cleanAllMiniDumps();

        assertEquals(0, mCrashDir.listFiles().length);
    }
}
