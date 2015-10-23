// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.invalidation;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;

import com.google.protos.ipc.invalidation.Types;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.sync.notifier.InvalidationClientNameProvider;
import org.chromium.sync.notifier.InvalidationIntentProtocol;
import org.chromium.sync.notifier.InvalidationPreferences;

/**
 * Wrapper for invalidations::InvalidationServiceAndroid.
 *
 * Serves as the bridge between Java and C++ for the invalidations component.
 */
@JNINamespace("invalidation")
public class InvalidationService {
    private final Context mContext;

    private final long mNativeInvalidationServiceAndroid;

    private InvalidationService(Context context, long nativeInvalidationServiceAndroid) {
        mContext = context.getApplicationContext();
        if (mContext == null) {
            throw new NullPointerException("mContext is null.");
        }
        mNativeInvalidationServiceAndroid = nativeInvalidationServiceAndroid;
    }

    public void notifyInvalidationToNativeChrome(
            int objectSource, String objectId, long version, String payload) {
        ThreadUtils.assertOnUiThread();
        nativeInvalidate(
                mNativeInvalidationServiceAndroid, objectSource, objectId, version, payload);
    }

    public void requestSyncFromNativeChromeForAllTypes() {
        notifyInvalidationToNativeChrome(Types.ObjectSource.CHROME_SYNC, null, 0L, null);
    }

    @CalledByNative
    private static InvalidationService create(
            Context context, long nativeInvalidationServiceAndroid) {
        ThreadUtils.assertOnUiThread();
        return new InvalidationService(context, nativeInvalidationServiceAndroid);
    }

    /**
     * Sets object ids for which the client should register for notification. This is intended for
     * registering non-Sync types; Sync types are registered with {@code setRegisteredTypes}.
     *
     * @param objectSources The sources of the objects.
     * @param objectNames   The names of the objects.
     */
    @VisibleForTesting
    @CalledByNative
    public void setRegisteredObjectIds(int[] objectSources, String[] objectNames) {
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        Account account = invalidationPreferences.getSavedSyncedAccount();
        Intent registerIntent = InvalidationIntentProtocol.createRegisterIntent(
                account, objectSources, objectNames);
        registerIntent.setClass(mContext, InvalidationClientService.class);
        mContext.startService(registerIntent);
    }

    /**
     * Fetches the Invalidator client name.
     *
     * Note that there is a naming discrepancy here.  In C++, we refer to the invalidation client
     * identifier that is unique for every invalidation client instance in an account as the client
     * ID.  In Java, we call it the client name.
     */
    @CalledByNative
    private byte[] getInvalidatorClientId() {
        return InvalidationClientNameProvider.get().getInvalidatorClientName();
    }

    private native void nativeInvalidate(long nativeInvalidationServiceAndroid, int objectSource,
            String objectId, long version, String payload);
}
