// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;


import android.Manifest;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorDescription;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.net.NetworkChangeNotifier;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Pattern;

import javax.annotation.Nullable;

/**
 * AccountManagerHelper wraps our access of AccountManager in Android.
 *
 * Use the AccountManagerHelper.get(someContext) to instantiate it
 */
public class AccountManagerHelper {

    private static final String TAG = "AccountManagerHelper";

    private static final Pattern AT_SYMBOL = Pattern.compile("@");

    private static final String GMAIL_COM = "gmail.com";

    private static final String GOOGLEMAIL_COM = "googlemail.com";

    public static final String GOOGLE_ACCOUNT_TYPE = "com.google";

    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a child account.
     */
    @VisibleForTesting public static final String FEATURE_IS_CHILD_ACCOUNT_KEY = "service_uca";

    private static final Object sLock = new Object();

    private static final int MAX_TRIES = 3;

    private static AccountManagerHelper sAccountManagerHelper;

    private final AccountManagerDelegate mAccountManager;

    private Context mApplicationContext;

    /**
     * A simple callback for getAuthToken.
     */
    public interface GetAuthTokenCallback {
        /**
         * Invoked on the UI thread once a token has been provided by the AccountManager.
         * @param token Auth token, or null if no token is available (bad credentials,
         *      permission denied, etc).
         * @param isTransientError If the token is null, then this parameter indicates
         *      if the error is transient or persistent.  If token is non-null, this
         *      parameter is not used.
         */
        void tokenAvailable(String token, boolean isTransientError);
    }

    /**
     * @param context the Android context
     * @param accountManager the account manager to use as a backend service
     */
    private AccountManagerHelper(Context context, AccountManagerDelegate accountManager) {
        mApplicationContext = context.getApplicationContext();
        mAccountManager = accountManager;
    }

    /**
     * Initialize AccountManagerHelper with a custom AccountManagerDelegate.
     * Ensures that the singleton AccountManagerHelper hasn't been created yet.
     * This can be overriden in tests using the overrideAccountManagerHelperForTests method.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @param delegate the custom AccountManagerDelegate to use.
     */
    public static void initializeAccountManagerHelper(
            Context context, AccountManagerDelegate delegate) {
        synchronized (sLock) {
            assert sAccountManagerHelper == null;
            sAccountManagerHelper = new AccountManagerHelper(context, delegate);
        }
    }

    /**
     * A getter method for AccountManagerHelper singleton which also initializes it if not wasn't
     * already initialized.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the AccountManagerHelper
     */
    public static AccountManagerHelper get(Context context) {
        synchronized (sLock) {
            if (sAccountManagerHelper == null) {
                sAccountManagerHelper = new AccountManagerHelper(
                        context, new SystemAccountManagerDelegate(context));
            }
        }
        return sAccountManagerHelper;
    }

    /**
     * Override AccountManagerHelper with a custom AccountManagerDelegate in tests.
     * Unlike initializeAccountManagerHelper, this will override the existing instance of
     * AccountManagerHelper if any. Only for use in Tests.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @param delegate the custom AccountManagerDelegate to use.
     */
    @VisibleForTesting
    public static void overrideAccountManagerHelperForTests(
            Context context, AccountManagerDelegate delegate) {
        synchronized (sLock) {
            sAccountManagerHelper = new AccountManagerHelper(context, delegate);
        }
    }

    /**
     * Creates an Account object for the given name.
     */
    public static Account createAccountFromName(String name) {
        return new Account(name, GOOGLE_ACCOUNT_TYPE);
    }

    public List<String> getGoogleAccountNames() {
        List<String> accountNames = new ArrayList<String>();
        for (Account account : getGoogleAccounts()) {
            accountNames.add(account.name);
        }
        return accountNames;
    }

    /**
     * Returns all Google accounts on the device.
     * @return an array of accounts.
     */
    public Account[] getGoogleAccounts() {
        return mAccountManager.getAccountsByType(GOOGLE_ACCOUNT_TYPE);
    }

    /**
     * Convenience method to get the single Google account on the device. Should only be
     * called if it has been determined that there is exactly one account.
     *
     * @return The single account to sign into.
     */
    public Account getSingleGoogleAccount() {
        Account[] googleAccounts = getGoogleAccounts();
        assert googleAccounts.length == 1;
        return googleAccounts[0];
    }

    public boolean hasGoogleAccounts() {
        return getGoogleAccounts().length > 0;
    }

    private String canonicalizeName(String name) {
        String[] parts = AT_SYMBOL.split(name);
        if (parts.length != 2) return name;

        if (GOOGLEMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[1] = GMAIL_COM;
        }
        if (GMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[0] = parts[0].replace(".", "");
        }
        return (parts[0] + "@" + parts[1]).toLowerCase(Locale.US);
    }

    /**
     * Returns the account if it exists, null otherwise.
     */
    public Account getAccountFromName(String accountName) {
        String canonicalName = canonicalizeName(accountName);
        Account[] accounts = getGoogleAccounts();
        for (Account account : accounts) {
            if (canonicalizeName(account.name).equals(canonicalName)) {
                return account;
            }
        }
        return null;
    }

    /**
     * Returns whether the accounts exists.
     */
    public boolean hasAccountForName(String accountName) {
        return getAccountFromName(accountName) != null;
    }

    /**
     * @return Whether or not there is an account authenticator for Google accounts.
     */
    public boolean hasGoogleAccountAuthenticator() {
        AuthenticatorDescription[] descs = mAccountManager.getAuthenticatorTypes();
        for (AuthenticatorDescription desc : descs) {
            if (GOOGLE_ACCOUNT_TYPE.equals(desc.type)) return true;
        }
        return false;
    }

    /**
     * Gets the auth token synchronously.
     *
     * - Assumes that the account is a valid account.
     * - Should not be called on the main thread.
     */
    @Deprecated
    public String getAuthTokenFromBackground(Account account, String authTokenType) {
        AccountManagerFuture<Bundle> future = mAccountManager.getAuthToken(
                account, authTokenType, true, null, null);
        AtomicBoolean isTransientError = new AtomicBoolean(false);
        return getAuthTokenInner(future, isTransientError);
    }

    /**
     * Gets the auth token and returns the response asynchronously.
     * This should be called when we have a foreground activity that needs an auth token.
     * If encountered an IO error, it will attempt to retry when the network is back.
     *
     * - Assumes that the account is a valid account.
     */
    public void getAuthTokenFromForeground(Activity activity, Account account, String authTokenType,
                GetAuthTokenCallback callback) {
        AtomicInteger numTries = new AtomicInteger(0);
        AtomicBoolean isTransientError = new AtomicBoolean(false);
        getAuthTokenAsynchronously(activity, account, authTokenType, callback, numTries,
                isTransientError, null);
    }

    private class ConnectionRetry implements NetworkChangeNotifier.ConnectionTypeObserver {
        private final Account mAccount;
        private final String mAuthTokenType;
        private final GetAuthTokenCallback mCallback;
        private final AtomicInteger mNumTries;
        private final AtomicBoolean mIsTransientError;

        ConnectionRetry(Account account, String authTokenType, GetAuthTokenCallback callback,
                AtomicInteger numTries, AtomicBoolean isTransientError) {
            mAccount = account;
            mAuthTokenType = authTokenType;
            mCallback = callback;
            mNumTries = numTries;
            mIsTransientError = isTransientError;
        }

        @Override
        public void onConnectionTypeChanged(int connectionType) {
            assert mNumTries.get() <= MAX_TRIES;
            if (mNumTries.get() == MAX_TRIES) {
                NetworkChangeNotifier.removeConnectionTypeObserver(this);
                return;
            }
            if (NetworkChangeNotifier.isOnline()) {
                NetworkChangeNotifier.removeConnectionTypeObserver(this);
                getAuthTokenAsynchronously(null, mAccount, mAuthTokenType, mCallback, mNumTries,
                        mIsTransientError, this);
            }
        }
    }

    private boolean hasUseCredentialsPermission() {
        return BuildInfo.isMncOrLater()
                || mApplicationContext.checkPermission("android.permission.USE_CREDENTIALS",
                Process.myPid(), Process.myUid()) == PackageManager.PERMISSION_GRANTED;
    }

    public boolean hasGetAccountsPermission() {
        return mApplicationContext.checkPermission(Manifest.permission.GET_ACCOUNTS,
                Process.myPid(), Process.myUid()) == PackageManager.PERMISSION_GRANTED;
    }

    // Gets the auth token synchronously
    private String getAuthTokenInner(AccountManagerFuture<Bundle> future,
            AtomicBoolean isTransientError) {
        try {
            Bundle result = future.getResult();
            if (result != null) {
                return result.getString(AccountManager.KEY_AUTHTOKEN);
            } else {
                Log.w(TAG, "Auth token - getAuthToken returned null");
            }
        } catch (OperationCanceledException e) {
            Log.w(TAG, "Auth token - operation cancelled", e);
        } catch (AuthenticatorException e) {
            Log.w(TAG, "Auth token - authenticator exception", e);
        } catch (IOException e) {
            Log.w(TAG, "Auth token - IO exception", e);
            isTransientError.set(true);
        }
        return null;
    }

    private void getAuthTokenAsynchronously(@Nullable Activity activity, final Account account,
            final String authTokenType, final GetAuthTokenCallback callback,
            final AtomicInteger numTries, final AtomicBoolean isTransientError,
            final ConnectionRetry retry) {
        // Return null token for no USE_CREDENTIALS permission.
        if (!hasUseCredentialsPermission()) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    callback.tokenAvailable(null, false);
                }
            });
            return;
        }
        final AccountManagerFuture<Bundle> future = mAccountManager.getAuthToken(
                account, authTokenType, true, null, null);
        isTransientError.set(false);

        new AsyncTask<Void, Void, String>() {
            @Override
            public String doInBackground(Void... params) {
                return getAuthTokenInner(future, isTransientError);
            }
            @Override
            public void onPostExecute(String authToken) {
                onGotAuthTokenResult(account, authTokenType, authToken, callback, numTries,
                        isTransientError, retry);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void onGotAuthTokenResult(Account account, String authTokenType, String authToken,
            GetAuthTokenCallback callback, AtomicInteger numTries, AtomicBoolean isTransientError,
            ConnectionRetry retry) {
        if (authToken != null || !isTransientError.get()
                || numTries.incrementAndGet() == MAX_TRIES
                || !NetworkChangeNotifier.isInitialized()) {
            callback.tokenAvailable(authToken, isTransientError.get());
            return;
        }
        if (retry == null) {
            ConnectionRetry newRetry = new ConnectionRetry(account, authTokenType, callback,
                    numTries, isTransientError);
            NetworkChangeNotifier.addConnectionTypeObserver(newRetry);
        } else {
            NetworkChangeNotifier.addConnectionTypeObserver(retry);
        }
    }

    /**
     * Invalidates the old token (if non-null/non-empty) and asynchronously generates a new one.
     *
     * - Assumes that the account is a valid account.
     */
    public void getNewAuthTokenFromForeground(Account account, String authToken,
                String authTokenType, GetAuthTokenCallback callback) {
        invalidateAuthToken(authToken);
        AtomicInteger numTries = new AtomicInteger(0);
        AtomicBoolean isTransientError = new AtomicBoolean(false);
        getAuthTokenAsynchronously(
                null, account, authTokenType, callback, numTries, isTransientError, null);
    }

    /**
     * Removes an auth token from the AccountManager's cache.
     */
    public void invalidateAuthToken(String authToken) {
        // Cancel operation for no USE_CREDENTIALS permission.
        if (!hasUseCredentialsPermission()) {
            return;
        }
        if (authToken != null && !authToken.isEmpty()) {
            mAccountManager.invalidateAuthToken(GOOGLE_ACCOUNT_TYPE, authToken);
        }
    }

    public AccountManagerFuture<Boolean> checkChildAccount(
            Account account, AccountManagerCallback<Boolean> callback) {
        String[] features = {FEATURE_IS_CHILD_ACCOUNT_KEY};
        return mAccountManager.hasFeatures(account, features, callback, null /* handler */);
    }
}
