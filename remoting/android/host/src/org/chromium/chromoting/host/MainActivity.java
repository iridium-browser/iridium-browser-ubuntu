// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.host;

import android.accounts.AccountManager;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.chromoting.base.OAuthTokenFetcher;
import org.chromium.chromoting.host.jni.Host;
import org.chromium.chromoting.host.jni.It2MeHostObserver;

/**
 * Main screen of the Chromoting Host application.
 */
public class MainActivity extends AppCompatActivity implements It2MeHostObserver {
    private static final String TAG = "host";

    /** Scope to use when fetching the OAuth token. */
    private static final String TOKEN_SCOPE = "oauth2:https://www.googleapis.com/auth/googletalk";

    private static final int REQUEST_CODE_CHOOSE_ACCOUNT = 0;

    private Host mHost;
    private String mAccountName;

    private Button mShareButton;
    private Button mDisconnectButton;
    private TextView mStatusText;
    private TextView mAccessCode;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        mShareButton = (Button) findViewById(R.id.share_button);
        mDisconnectButton = (Button) findViewById(R.id.disconnect_button);
        mDisconnectButton.setEnabled(false);

        mStatusText = (TextView) findViewById(R.id.status_text);
        mAccessCode = (TextView) findViewById(R.id.access_code);

        if (mHost == null) {
            mHost = new Host();
        }
    }

    @SuppressWarnings("deprecation")
    public void onShareClicked(View view) {
        Intent intent = AccountManager.newChooseAccountIntent(
                null, null, new String[] {"com.google"}, false, null, null, null, null);
        startActivityForResult(intent, REQUEST_CODE_CHOOSE_ACCOUNT);
    }

    public void onDisconnectClicked(View view) {
        mHost.disconnect();
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (resultCode != RESULT_OK) {
            return;
        }

        if (requestCode == REQUEST_CODE_CHOOSE_ACCOUNT) {
            mAccountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            requestAuthToken();
        } else if (requestCode == OAuthTokenFetcher.REQUEST_CODE_RECOVER_FROM_OAUTH_ERROR) {
            // User gave OAuth permission to this app (or recovered from any OAuth failure),
            // so retry fetching the token.
            requestAuthToken();
        }
    }

    private void requestAuthToken() {
        new OAuthTokenFetcher(this, mAccountName, TOKEN_SCOPE, new OAuthTokenFetcher.Callback() {
            @Override
            public void onTokenFetched(String token) {
                mHost.connect(mAccountName, token, MainActivity.this);
            }

            @Override
            public void onError(OAuthTokenFetcher.Error error) {
                Log.e(TAG, "Error fetching token: %s", error.name());
            }
        }).fetch();
    }

    @Override
    public void onAccessCodeReceived(String accessCode, int lifetimeSeconds) {
        mAccessCode.setText(accessCode);
    }

    @Override
    public void onStateChanged(State state, String errorMessage) {
        mStatusText.setText(state.name());
        mShareButton.setEnabled(state == It2MeHostObserver.State.DISCONNECTED);
        mDisconnectButton.setEnabled(state != It2MeHostObserver.State.DISCONNECTED);
        if (state == It2MeHostObserver.State.DISCONNECTED) {
            mAccessCode.setText("");
        }
    }
}
