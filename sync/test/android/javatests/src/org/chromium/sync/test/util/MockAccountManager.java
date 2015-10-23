// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.test.util;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorDescription;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.sync.signin.AccountManagerDelegate;
import org.chromium.sync.signin.AccountManagerHelper;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.Callable;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import javax.annotation.Nullable;

/**
 * The MockAccountManager helps out if you want to mock out all calls to the Android AccountManager.
 *
 * You should provide a set of accounts as a constructor argument, or use the more direct approach
 * and provide an array of AccountHolder objects.
 *
 * Currently, this implementation supports adding and removing accounts, handling credentials
 * (including confirming them), and handling of dummy auth tokens.
 *
 * If you want the MockAccountManager to popup an activity for granting/denying access to an
 * authtokentype for a given account, use prepareGrantAppPermission(...).
 *
 * If you want to auto-approve a given authtokentype, use addAccountHolderExplicitly(...) with
 * an AccountHolder you have built with hasBeenAccepted("yourAuthTokenType", true).
 *
 * If you want to auto-approve all auth token types for a given account, use the {@link
 * AccountHolder} builder method alwaysAccept(true).
 */
public class MockAccountManager implements AccountManagerDelegate {

    private static final String TAG = "cr.MockAccountManager";

    private static final long WAIT_TIME_FOR_GRANT_BROADCAST_MS = scaleTimeout(20000);

    static final String MUTEX_WAIT_ACTION =
            "org.chromium.sync.test.util.MockAccountManager.MUTEX_WAIT_ACTION";

    protected final Context mContext;

    private final Context mTestContext;

    private final Set<AccountHolder> mAccounts;

    private final List<AccountAuthTokenPreparation> mAccountPermissionPreparations;

    private final Handler mMainHandler;

    private final SingleThreadedExecutor mExecutor;

    public MockAccountManager(Context context, Context testContext, Account... accounts) {
        mContext = context;
        // The manifest that is backing testContext needs to provide the
        // MockGrantCredentialsPermissionActivity.
        mTestContext = testContext;
        mMainHandler = new Handler(mContext.getMainLooper());
        mExecutor = new SingleThreadedExecutor();
        mAccounts = new HashSet<AccountHolder>();
        mAccountPermissionPreparations = new LinkedList<AccountAuthTokenPreparation>();
        if (accounts != null) {
            for (Account account : accounts) {
                mAccounts.add(AccountHolder.create().account(account).alwaysAccept(true).build());
            }
        }
    }

    private static class SingleThreadedExecutor extends ThreadPoolExecutor {
        public SingleThreadedExecutor() {
            super(1, 1, 1, TimeUnit.SECONDS, new LinkedBlockingDeque<Runnable>());
        }
    }

    @Override
    public Account[] getAccountsByType(@Nullable String type) {
        if (!AccountManagerHelper.GOOGLE_ACCOUNT_TYPE.equals(type)) {
            throw new IllegalArgumentException("Invalid account type: " + type);
        }
        if (mAccounts == null) {
            return new Account[0];
        } else {
            ArrayList<Account> validAccounts = new ArrayList<Account>();
            for (AccountHolder ah : mAccounts) {
                if (TextUtils.equals(ah.getAccount().type, type)) {
                    validAccounts.add(ah.getAccount());
                }
            }

            Account[] accounts = new Account[validAccounts.size()];
            int i = 0;
            for (Account account : validAccounts) {
                accounts[i++] = account;
            }
            return accounts;
        }
    }

    public boolean addAccountHolderExplicitly(AccountHolder accountHolder) {
        return addAccountHolderExplicitly(accountHolder, false);
    }

    /**
     * Add an AccountHolder directly.
     *
     * @param accountHolder the account holder to add
     * @param broadcastEvent whether to broadcast an AccountChangedEvent
     * @return whether the account holder was added successfully
     */
    public boolean addAccountHolderExplicitly(AccountHolder accountHolder,
            boolean broadcastEvent) {
        boolean result = mAccounts.add(accountHolder);
        if (broadcastEvent) {
            postAsyncAccountChangedEvent();
        }
        return result;
    }

    public boolean removeAccountHolderExplicitly(AccountHolder accountHolder) {
        return removeAccountHolderExplicitly(accountHolder, false);
    }

    /**
     * Remove an AccountHolder directly.
     *
     * @param accountHolder the account holder to remove
     * @param broadcastEvent whether to broadcast an AccountChangedEvent
     * @return whether the account holder was removed successfully
     */
    public boolean removeAccountHolderExplicitly(AccountHolder accountHolder,
            boolean broadcastEvent) {
        boolean result = mAccounts.remove(accountHolder);
        if (broadcastEvent) {
            postAsyncAccountChangedEvent();
        }
        return result;
    }

    @Override
    public AccountManagerFuture<Bundle> getAuthToken(Account account, String authTokenType,
            boolean notifyAuthFailure, AccountManagerCallback<Bundle> callback, Handler handler) {
        return getAuthTokenFuture(account, authTokenType, null, callback, handler);
    }

    private AccountManagerFuture<Bundle> getAuthTokenFuture(Account account, String authTokenType,
            Activity activity, AccountManagerCallback<Bundle> callback, Handler handler) {
        final AccountHolder ah = getAccountHolder(account);
        if (ah.hasBeenAccepted(authTokenType)) {
            final String authToken = internalGenerateAndStoreAuthToken(ah, authTokenType);
            return runTask(mExecutor,
                    new AccountManagerAuthTokenTask(activity, handler, callback,
                            account, authTokenType,
                            new Callable<Bundle>() {
                        @Override
                        public Bundle call() throws Exception {
                            return getAuthTokenBundle(ah.getAccount(), authToken);
                        }
                    }));
        } else {
            Log.d(TAG, "getAuthTokenFuture: Account " + ah.getAccount()
                    + " is asking for permission for " + authTokenType);
            final Intent intent = newGrantCredentialsPermissionIntent(
                    activity != null, account, authTokenType);
            return runTask(mExecutor,
                    new AccountManagerAuthTokenTask(activity, handler, callback,
                            account, authTokenType,
                            new Callable<Bundle>() {
                        @Override
                        public Bundle call() throws Exception {
                            Bundle result = new Bundle();
                            result.putParcelable(AccountManager.KEY_INTENT, intent);
                            return result;
                        }
                    }));
        }
    }

    private static Bundle getAuthTokenBundle(Account account, String authToken) {
        Bundle result = new Bundle();
        result.putString(AccountManager.KEY_AUTHTOKEN, authToken);
        result.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);
        result.putString(AccountManager.KEY_ACCOUNT_TYPE, account.type);
        return result;
    }

    private String internalGenerateAndStoreAuthToken(AccountHolder ah, String authTokenType) {
        synchronized (mAccounts) {
            // Some tests register auth tokens with value null, and those should be preserved.
            if (!ah.hasAuthTokenRegistered(authTokenType)
                    && ah.getAuthToken(authTokenType) == null) {
                // No authtoken registered. Need to create one.
                String authToken = UUID.randomUUID().toString();
                Log.d(TAG, "Created new auth token for " + ah.getAccount()
                        + ": autTokenType = " + authTokenType + ", authToken = " + authToken);
                ah = ah.withAuthToken(authTokenType, authToken);
                mAccounts.add(ah);
            }
        }
        return ah.getAuthToken(authTokenType);
    }

    @Override
    public void invalidateAuthToken(String accountType, String authToken) {
        if (!AccountManagerHelper.GOOGLE_ACCOUNT_TYPE.equals(accountType)) {
            throw new IllegalArgumentException("Invalid account type: " + accountType);
        }
        if (authToken == null) {
            throw new IllegalArgumentException("AuthToken can not be null");
        }
        for (AccountHolder ah : mAccounts) {
            if (ah.removeAuthToken(authToken)) {
                break;
            }
        }
    }

    @Override
    public AuthenticatorDescription[] getAuthenticatorTypes() {
        AuthenticatorDescription googleAuthenticator = new AuthenticatorDescription(
                AccountManagerHelper.GOOGLE_ACCOUNT_TYPE, "p1", 0, 0, 0, 0);

        return new AuthenticatorDescription[] { googleAuthenticator };
    }

    @Override
    public AccountManagerFuture<Boolean> hasFeatures(Account account, final String[] features,
            AccountManagerCallback<Boolean> callback, Handler handler) {
        final AccountHolder accountHolder = getAccountHolder(account);
        AccountManagerTask<Boolean> accountManagerTask =
                new AccountManagerTask<Boolean>(handler, callback, new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        Set<String> accountFeatures = accountHolder.getFeatures();
                        for (String feature : features) {
                            if (!accountFeatures.contains(feature)) {
                                Log.d(TAG, accountFeatures + " does not contain " + feature);
                                return false;
                            }
                        }
                        return true;
                    }
                });
        accountHolder.addFeaturesCallback(accountManagerTask);
        return accountManagerTask;
    }

    public void notifyFeaturesFetched(Account account, Set<String> features) {
        getAccountHolder(account).didFetchFeatures(features);
    }

    public void prepareAllowAppPermission(Account account, String authTokenType) {
        addPreparedAppPermission(new AccountAuthTokenPreparation(account, authTokenType, true));
    }

    public void prepareDenyAppPermission(Account account, String authTokenType) {
        addPreparedAppPermission(new AccountAuthTokenPreparation(account, authTokenType, false));
    }

    private void addPreparedAppPermission(AccountAuthTokenPreparation accountAuthTokenPreparation) {
        Log.d(TAG, "Adding " + accountAuthTokenPreparation);
        mAccountPermissionPreparations.add(accountAuthTokenPreparation);
    }

    private AccountAuthTokenPreparation getPreparedPermission(Account account,
            String authTokenType) {
        for (AccountAuthTokenPreparation accountPrep : mAccountPermissionPreparations) {
            if (accountPrep.getAccount().equals(account)
                    && accountPrep.getAuthTokenType().equals(authTokenType)) {
                return accountPrep;
            }
        }
        return null;
    }

    private void applyPreparedPermission(AccountAuthTokenPreparation prep) {
        if (prep != null) {
            Log.d(TAG, "Applying " + prep);
            mAccountPermissionPreparations.remove(prep);
            mAccounts.add(getAccountHolder(prep.getAccount()).withHasBeenAccepted(
                    prep.getAuthTokenType(), prep.isAllowed()));
        }
    }

    private Intent newGrantCredentialsPermissionIntent(boolean hasActivity, Account account,
            String authTokenType) {
        ComponentName component = new ComponentName(mTestContext,
                MockGrantCredentialsPermissionActivity.class.getCanonicalName());

        // Make sure we can start the activity.
        ActivityInfo ai = null;
        try {
            ai = mContext.getPackageManager().getActivityInfo(component, 0);
        } catch (PackageManager.NameNotFoundException e) {
            throw new IllegalStateException(
                    "Unable to find " + component.getClassName());
        }
        if (ai.applicationInfo != mContext.getApplicationInfo() && !ai.exported) {
            throw new IllegalStateException(
                    "Unable to start " + ai.name + ". "
                    + "The accounts you added to MockAccountManager may not be "
                    + "configured correctly.");
        }

        Intent intent = new Intent();
        intent.setComponent(component);
        intent.putExtra(MockGrantCredentialsPermissionActivity.ACCOUNT, account);
        intent.putExtra(MockGrantCredentialsPermissionActivity.AUTH_TOKEN_TYPE, authTokenType);
        if (!hasActivity) {
            // No activity provided, so we help the caller by adding the new task flag
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        }
        return intent;
    }

    private AccountHolder getAccountHolder(Account account) {
        if (account == null) {
            throw new IllegalArgumentException("Account can not be null");
        }
        for (AccountHolder accountHolder : mAccounts) {
            if (account.equals(accountHolder.getAccount())) {
                return accountHolder;
            }
        }
        throw new IllegalArgumentException("Can not find AccountHolder for account " + account);
    }

    private static <T> AccountManagerFuture<T> runTask(Executor executorService,
            AccountManagerTask<T> accountManagerBundleTask) {
        executorService.execute(accountManagerBundleTask);
        return accountManagerBundleTask;
    }

    private class AccountManagerTask<T> extends FutureTask<T> implements AccountManagerFuture<T> {

        protected final Handler mHandler;

        protected final AccountManagerCallback<T> mCallback;

        protected final Callable<T> mCallable;

        public AccountManagerTask(Handler handler,
                AccountManagerCallback<T> callback, Callable<T> callable) {
            super(new Callable<T>() {
                @Override
                public T call() throws Exception {
                    throw new IllegalStateException("this should never be called, "
                            + "but call must be overridden.");
                }
            });
            mHandler = handler;
            mCallback = callback;
            mCallable = callable;
        }

        private T internalGetResult(long timeout, TimeUnit unit)
                throws OperationCanceledException, IOException, AuthenticatorException {
            try {
                if (timeout == -1) {
                    return get();
                } else {
                    return get(timeout, unit);
                }
            } catch (CancellationException e) {
                throw new OperationCanceledException();
            } catch (TimeoutException e) {
                // Fall through and cancel.
            } catch (InterruptedException e) {
                // Fall through and cancel.
            } catch (ExecutionException e) {
                final Throwable cause = e.getCause();
                if (cause instanceof IOException) {
                    throw (IOException) cause;
                } else if (cause instanceof UnsupportedOperationException) {
                    throw new AuthenticatorException(cause);
                } else if (cause instanceof AuthenticatorException) {
                    throw (AuthenticatorException) cause;
                } else if (cause instanceof RuntimeException) {
                    throw (RuntimeException) cause;
                } else if (cause instanceof Error) {
                    throw (Error) cause;
                } else {
                    throw new IllegalStateException(cause);
                }
            } finally {
                cancel(true /* Interrupt if running. */);
            }
            throw new OperationCanceledException();
        }

        @Override
        public T getResult()
                throws OperationCanceledException, IOException, AuthenticatorException {
            return internalGetResult(-1, null);
        }

        @Override
        public T getResult(long timeout, TimeUnit unit)
                throws OperationCanceledException, IOException, AuthenticatorException {
            return internalGetResult(timeout, unit);
        }

        @Override
        public void run() {
            try {
                set(mCallable.call());
            } catch (Exception e) {
                setException(e);
            }
        }

        @Override
        protected void done() {
            if (mCallback != null) {
                postToHandler(getHandler(), mCallback, this);
            }
        }

        private Handler getHandler() {
            return mHandler == null ? mMainHandler : mHandler;
        }

    }

    private static <T> void postToHandler(Handler handler, final AccountManagerCallback<T> callback,
            final AccountManagerFuture<T> future) {
        handler.post(new Runnable() {
            @Override
            public void run() {
                callback.run(future);
            }
        });
    }

    private class AccountManagerAuthTokenTask extends AccountManagerTask<Bundle> {

        private final Activity mActivity;

        private final AccountAuthTokenPreparation mAccountAuthTokenPreparation;

        private final Account mAccount;

        private final String mAuthTokenType;

        public AccountManagerAuthTokenTask(Activity activity, Handler handler,
                AccountManagerCallback<Bundle> callback,
                Account account, String authTokenType,
                Callable<Bundle> callable) {
            super(handler, callback, callable);
            mActivity = activity;
            mAccountAuthTokenPreparation = getPreparedPermission(account, authTokenType);
            mAccount = account;
            mAuthTokenType = authTokenType;
        }

        @Override
        public void run() {
            try {
                Bundle bundle = mCallable.call();
                Intent intent = bundle.getParcelable(AccountManager.KEY_INTENT);
                if (intent != null) {
                    // Start the intent activity and wait for it to finish.
                    if (mActivity != null) {
                        waitForActivity(mActivity, intent);
                    } else {
                        waitForActivity(mContext, intent);
                    }
                    if (mAccountAuthTokenPreparation == null) {
                        throw new IllegalStateException("No account preparation ready for "
                                + mAccount + ", authTokenType = " + mAuthTokenType
                                + ". Add a call to either prepareGrantAppPermission(...) or "
                                + "prepareRevokeAppPermission(...) in your test before asking for "
                                + "an auth token");
                    } else {
                        // We have shown the Allow/Deny activity, and it has gone away. We can now
                        // apply the pre-stored permission.
                        applyPreparedPermission(mAccountAuthTokenPreparation);
                        generateResult(getAccountHolder(mAccount), mAuthTokenType);
                    }
                } else {
                    set(bundle);
                }
            } catch (Exception e) {
                setException(e);
            }
        }

        private void generateResult(AccountHolder accountHolder, String authTokenType)
                throws OperationCanceledException {
            if (accountHolder.hasBeenAccepted(authTokenType)) {
                String authToken = internalGenerateAndStoreAuthToken(accountHolder, authTokenType);
                // Return a valid auth token.
                set(getAuthTokenBundle(accountHolder.getAccount(), authToken));
            } else {
                // Throw same exception as when user clicks "Deny".
                throw new OperationCanceledException("User denied request");
            }
        }
    }

    /**
     * This method starts {@link MockGrantCredentialsPermissionActivity} and waits for it
     * to be started before it returns.
     *
     * @param context the context to start the intent in
     * @param intent the intent to use to start MockGrantCredentialsPermissionActivity
     */
    @SuppressWarnings("WaitNotInLoop")
    private void waitForActivity(Context context, Intent intent) {
        final Object mutex = new Object();
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                synchronized (mutex) {
                    mutex.notifyAll();
                }
            }
        };
        if (!MockGrantCredentialsPermissionActivity.class.getCanonicalName()
                .equals(intent.getComponent().getClassName())) {
            throw new IllegalArgumentException("Can only wait for "
                    + "MockGrantCredentialsPermissionActivity");
        }
        mContext.registerReceiver(receiver, new IntentFilter(MUTEX_WAIT_ACTION));
        context.startActivity(intent);
        try {
            Log.d(TAG, "Waiting for broadcast of " + MUTEX_WAIT_ACTION);
            synchronized (mutex) {
                mutex.wait(WAIT_TIME_FOR_GRANT_BROADCAST_MS);
            }
        } catch (InterruptedException e) {
            throw new IllegalStateException("Got unexpected InterruptedException");
        }
        Log.d(TAG, "Got broadcast of " + MUTEX_WAIT_ACTION);
        mContext.unregisterReceiver(receiver);
    }

    private void postAsyncAccountChangedEvent() {
        // Mimic that this does not happen on the main thread.
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                mContext.sendBroadcast(new Intent(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION));
                return null;
            }
        }.execute();
    }

    /**
     * Internal class for storage of prepared account auth token permissions.
     *
     * This is used internally by {@link MockAccountManager} to mock the same behavior as clicking
     * Allow/Deny in the Android {@link GrantCredentialsPermissionActivity}.
     */
    private static class AccountAuthTokenPreparation {

        private final Account mAccount;

        private final String mAuthTokenType;

        private final boolean mAllowed;

        private AccountAuthTokenPreparation(Account account, String authTokenType,
                boolean allowed) {
            mAccount = account;
            mAuthTokenType = authTokenType;
            mAllowed = allowed;
        }

        public Account getAccount() {
            return mAccount;
        }

        public String getAuthTokenType() {
            return mAuthTokenType;
        }

        public boolean isAllowed() {
            return mAllowed;
        }

        @Override
        public String toString() {
            return "AccountAuthTokenPreparation{"
                    + "mAccount=" + mAccount
                    + ", mAuthTokenType='" + mAuthTokenType + '\''
                    + ", mAllowed=" + mAllowed
                    + '}';
        }
    }
}
