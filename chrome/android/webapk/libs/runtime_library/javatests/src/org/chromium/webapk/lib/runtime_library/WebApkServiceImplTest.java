// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.runtime_library;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.content.browser.test.util.CallbackHelper;

/**
 * Instrumentation tests for {@link org.chromium.webapk.WebApkServiceImpl}.
 */
public class WebApkServiceImplTest extends InstrumentationTestCase {
    private static final String APK_WITH_WEBAPK_SERVICE_PACKAGE =
            "org.chromium.webapk.lib.runtime_library.test.apk_with_webapk_service";
    private static final String WEBAPK_SERVICE_IMPL_WRAPPER_CLASS_NAME =
            "org.chromium.webapk.lib.runtime_library.test.TestWebApkServiceImplWrapper";

    private static final int SMALL_ICON_ID = 1229;

    private Context mContext;
    private Context mTargetContext;

    /**
     * The target app's uid.
     */
    private int mTargetUid;

    /**
     * CallbackHelper which blocks till the service is connected.
     */
    private static class ServiceConnectionWaiter
            extends CallbackHelper implements ServiceConnection {
        private IWebApkApi mApi;

        public IWebApkApi api() {
            return mApi;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mApi = IWebApkApi.Stub.asInterface(service);
            notifyCalled();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    @Override
    public void setUp() {
        mContext = getInstrumentation().getContext();
        mTargetContext = getInstrumentation().getTargetContext();
        mTargetUid = getUid(mTargetContext);
    }

    /**
     * Test that an application which is not allowed to use the WebAPK service actually cannot.
     */
    @SmallTest
    public void testApiFailsIfNoPermission() throws Exception {
        IWebApkApi api = bindService(mContext, mTargetUid + 1, SMALL_ICON_ID);
        try {
            // Check that the api either throws an exception or returns a default small icon id.
            int actualSmallIconId = api.getSmallIconId();
            assertTrue(actualSmallIconId != SMALL_ICON_ID);
        } catch (Exception e) {
        }
    }

    /**
     * Test that an application which is allowed to use the WebAPK service actually can.
     */
    @SmallTest
    public void testApiWorksIfHasPermission() throws Exception {
        IWebApkApi api = bindService(mContext, mTargetUid, SMALL_ICON_ID);
        try {
            // Check that the api returns the real small icon id.
            int actualSmallIconId = api.getSmallIconId();
            assertEquals(SMALL_ICON_ID, actualSmallIconId);
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown an exception when permission is granted.");
        }
    }

    /**
     * Returns the uid for {@link context}.
     */
    private static int getUid(Context context) {
        PackageManager packageManager = context.getPackageManager();
        ApplicationInfo appInfo;
        try {
            appInfo = packageManager.getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
            return appInfo.uid;
        } catch (Exception e) {
            fail();
        }
        return -1;
    }

    /**
     * Binds to the WebAPK service and blocks till the service is connected.
     * @param context The context for the application containing the WebAPK service to bind to.
     * @param authorizedUid The uid of the only application allowed to use the WebAPK service's
     *        methods.
     * @param smallIconId The real small icon id.
     * @return IWebApkApi to use to communicate with the service.
     */
    private static IWebApkApi bindService(Context context, int authorizedUid, int smallIconId)
            throws Exception {
        Intent intent = new Intent();
        intent.setComponent(new ComponentName(
                APK_WITH_WEBAPK_SERVICE_PACKAGE, WEBAPK_SERVICE_IMPL_WRAPPER_CLASS_NAME));
        intent.putExtra(WebApkServiceImpl.KEY_SMALL_ICON_ID, smallIconId);
        intent.putExtra(WebApkServiceImpl.KEY_HOST_BROWSER_UID, authorizedUid);

        ServiceConnectionWaiter waiter = new ServiceConnectionWaiter();
        context.bindService(intent, waiter, Context.BIND_AUTO_CREATE);
        waiter.waitForCallback(0);

        IWebApkApi api = waiter.api();
        assertNotNull(api);
        return api;
    }
}
