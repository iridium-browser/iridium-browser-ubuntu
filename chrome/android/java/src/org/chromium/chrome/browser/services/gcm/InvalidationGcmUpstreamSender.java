// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.accounts.Account;
import android.os.Bundle;
import android.util.Log;

import com.google.android.gms.gcm.GoogleCloudMessaging;
import com.google.ipc.invalidation.ticl.android2.channel.GcmUpstreamSenderService;

import org.chromium.chrome.browser.signin.OAuth2TokenService;
import org.chromium.sync.SyncConstants;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;

import java.io.IOException;
import java.util.UUID;

import javax.annotation.Nullable;

/**
 * Sends Upstream messages for Invalidations using GCM.
 */
public class InvalidationGcmUpstreamSender extends GcmUpstreamSenderService {
    private static final String TAG = "InvalidationGcmUpstream";

    // GCM Payload Limit in bytes.
    private static final int GCM_PAYLOAD_LIMIT = 4000;

    @Override
    public void deliverMessage(final String to, final Bundle data) {
        @Nullable
        Account account = ChromeSigninController.get(this).getSignedInUser();
        if (account == null) {
            // This should never happen, because this code should only be run if a user is
            // signed-in.
            Log.w(TAG, "No signed-in user; cannot send message to data center");
            return;
        }

        // Attempt to retrieve a token for the user.
        OAuth2TokenService.getOAuth2AccessToken(this, null, account,
                SyncConstants.CHROME_SYNC_OAUTH2_SCOPE,
                new AccountManagerHelper.GetAuthTokenCallback() {
                    @Override
                    public void tokenAvailable(String token, boolean isTransientError) {
                        sendUpstreamMessage(to, data, token);
                    }
                });
    }

    private void sendUpstreamMessage(String to, Bundle data, String token) {
        if (token == null) {
            GcmUpstreamUma.recordHistogram(
                    getApplicationContext(), GcmUpstreamUma.UMA_TOKEN_REQUEST_FAILED);
        }
        // Add the OAuth2 token to the bundle. The token should have the prefix Bearer added to it.
        data.putString("Authorization", "Bearer " + token);
        if (!isMessageWithinLimit(data)) {
            GcmUpstreamUma.recordHistogram(
                    getApplicationContext(), GcmUpstreamUma.UMA_SIZE_LIMIT_EXCEEDED);
            return;
        }
        String msgId = UUID.randomUUID().toString();
        try {
            GoogleCloudMessaging.getInstance(getApplicationContext()).send(to, msgId, 1, data);
        } catch (IOException | IllegalArgumentException exception) {
            Log.w(TAG, "Send message failed");
            GcmUpstreamUma.recordHistogram(getApplicationContext(), GcmUpstreamUma.UMA_SEND_FAILED);
        }
    }

    private boolean isMessageWithinLimit(Bundle data) {
        int size = 0;
        for (String key : data.keySet()) {
            size += key.length() + data.getString(key).length();
        }
        if (size > GCM_PAYLOAD_LIMIT) {
            return false;
        }
        return true;
    }
}
