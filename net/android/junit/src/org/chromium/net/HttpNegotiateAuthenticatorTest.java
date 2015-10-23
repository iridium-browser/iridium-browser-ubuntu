// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowAccountManager;

import java.io.IOException;

/**
 * Robolectric tests for HttpNegotiateAuthenticator
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = HttpNegotiateAuthenticatorTest.ExtendedShadowAccountManager.class,
        application = BaseChromiumApplication.class)
public class HttpNegotiateAuthenticatorTest {
    // Since the account manager is an SDK singleton (it is fetched using AccountManager.get()) we
    // can't validate its method calls with Mockito, so do so using our shadow method. Since the
    // shadow class has to be static the variables it uses also have to be static.
    static int sCallCount;
    static String sAccountTypeReceived;
    static String sAuthTokenTypeReceived;
    static String sFeaturesReceived[];
    static Bundle sAddAccountOptionsReceived;
    static Bundle sAuthTokenOptionsReceived;
    static AccountManagerCallback<Bundle> sCallbackReceived;
    static Handler sHandlerReceived;

    /**
     * Robolectic's ShadowAccountManager doesn't implement getAccountsByTypeAndFeature so extend it.
     * We simply check the call is correct, and don't try to emulate it. This also allows us to do
     * more checking than we could using a vanilla shadow.
     *
     * Note: Shadow classes need to be public and static.
     */
    @Implements(AccountManager.class)
    public static class ExtendedShadowAccountManager extends ShadowAccountManager {
        @Implementation
        public AccountManagerFuture<Bundle> getAuthTokenByFeatures(String accountType,
                String authTokenType, String[] features, Activity activity,
                Bundle addAccountOptions, Bundle getAuthTokenOptions,
                AccountManagerCallback<Bundle> callback, Handler handler) {
            sCallCount++;
            sAccountTypeReceived = accountType;
            sAuthTokenTypeReceived = authTokenType;
            sFeaturesReceived = features;
            sAddAccountOptionsReceived = addAccountOptions;
            sAuthTokenOptionsReceived = getAuthTokenOptions;
            sCallbackReceived = callback;
            sHandlerReceived = handler;

            return null;
        }
    }

    @Before
    public void setUp() {
        sCallCount = 0;
    }

    /**
     * Test of {@link HttpNegotiateAuthenticator#getNextAuthToken}
     */
    @Test
    public void testGetNextAuthToken() {
        HttpNegotiateAuthenticator authenticator =
                HttpNegotiateAuthenticator.create("Dummy_Account");
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        authenticator.getNextAuthToken(0, "test_principal", "", true);
        assertThat("getAuthTokenByFeatures called precisely once", sCallCount, equalTo(1));
        assertThat("Received account type matches input", sAccountTypeReceived,
                equalTo("Dummy_Account"));
        assertThat("AuthTokenType is \"SPNEGO:HOSTBASED:test_principal\"", sAuthTokenTypeReceived,
                equalTo("SPNEGO:HOSTBASED:test_principal"));
        assertThat("Features are precisely {\"SPNEGO\"}", sFeaturesReceived,
                equalTo(new String[] {"SPNEGO"}));
        assertThat("No account options requested", sAddAccountOptionsReceived, nullValue());
        assertThat("There is no existing context",
                sAuthTokenOptionsReceived.get(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT),
                nullValue());
        assertThat("The existing token is empty",
                sAuthTokenOptionsReceived.getString(HttpNegotiateConstants.KEY_INCOMING_AUTH_TOKEN),
                equalTo(""));
        assertThat("Delegation is allowed",
                sAuthTokenOptionsReceived.getBoolean(HttpNegotiateConstants.KEY_CAN_DELEGATE),
                equalTo(true));
        assertThat("getAuthTokenByFeatures was called with a callback", sCallbackReceived,
                notNullValue());
        assertThat("getAuthTokenByFeatures was called with a handler", sHandlerReceived,
                notNullValue());
    }

    /**
     * Test of callback called when getting the auth token completes.
     */
    @Test
    public void testAccountManagerCallbackRun() {
        // Spy on the authenticator so that we can override and intercept the native method call.
        HttpNegotiateAuthenticator authenticator =
                spy(HttpNegotiateAuthenticator.create("Dummy_Account"));
        doNothing().when(authenticator).nativeSetResult(anyLong(), anyInt(), anyString());

        Robolectric.buildActivity(Activity.class).create().start().resume().visible();

        // Call getNextAuthToken to get the callback
        authenticator.getNextAuthToken(1234, "test_principal", "", true);

        // Avoid warning when creating mock accountManagerFuture, can't take .class of an
        // instantiated generic type, yet compiler complains if I leave it uninstantiated.
        @SuppressWarnings("unchecked")
        AccountManagerFuture<Bundle> accountManagerFuture = mock(AccountManagerFuture.class);
        Bundle resultBundle = new Bundle();
        Bundle context = new Bundle();
        context.putString("String", "test_context");
        resultBundle.putInt(HttpNegotiateConstants.KEY_SPNEGO_RESULT, HttpNegotiateConstants.OK);
        resultBundle.putBundle(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT, context);
        resultBundle.putString(AccountManager.KEY_AUTHTOKEN, "output_token");
        try {
            when(accountManagerFuture.getResult()).thenReturn(resultBundle);
        } catch (OperationCanceledException | AuthenticatorException | IOException e) {
            // Can never happen - artifact of Mockito.
            fail();
        }
        sCallbackReceived.run(accountManagerFuture);
        verify(authenticator).nativeSetResult(1234, 0, "output_token");

        // Check that the next call to getNextAuthToken uses the correct context
        authenticator.getNextAuthToken(5678, "test_principal", "", true);
        assertThat("The spnego context is preserved between calls",
                sAuthTokenOptionsReceived.getBundle(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT),
                equalTo(context));

        // Test exception path
        try {
            when(accountManagerFuture.getResult()).thenThrow(new OperationCanceledException());
        } catch (OperationCanceledException | AuthenticatorException | IOException e) {
            // Can never happen - artifact of Mockito.
            fail();
        }
        sCallbackReceived.run(accountManagerFuture);
        verify(authenticator).nativeSetResult(5678, NetError.ERR_ABORTED, null);
    }

    private void checkErrorReturn(Integer spnegoError, int expectedError) {
        // Spy on the authenticator so that we can override and intercept the native method call.
        HttpNegotiateAuthenticator authenticator =
                spy(HttpNegotiateAuthenticator.create("Dummy_Account"));
        doNothing().when(authenticator).nativeSetResult(anyLong(), anyInt(), anyString());

        Robolectric.buildActivity(Activity.class).create().start().resume().visible();

        // Call getNextAuthToken to get the callback
        authenticator.getNextAuthToken(1234, "test_principal", "", true);

        // Avoid warning when creating mock accountManagerFuture, can't take .class of an
        // instantiated generic type, yet compiler complains if I leave it uninstantiated.
        @SuppressWarnings("unchecked")
        AccountManagerFuture<Bundle> accountManagerFuture = mock(AccountManagerFuture.class);
        Bundle resultBundle = new Bundle();
        if (spnegoError != null) {
            resultBundle.putInt(HttpNegotiateConstants.KEY_SPNEGO_RESULT, spnegoError);
        }
        try {
            when(accountManagerFuture.getResult()).thenReturn(resultBundle);
        } catch (OperationCanceledException | AuthenticatorException | IOException e) {
            // Can never happen - artifact of Mockito.
            fail();
        }
        sCallbackReceived.run(accountManagerFuture);
        verify(authenticator).nativeSetResult(anyLong(), eq(expectedError), anyString());
    }

    /**
     * Test of callback error returns when getting the auth token completes.
     */
    @Test
    public void testAccountManagerCallbackErrorReturns() {
        checkErrorReturn(null, NetError.ERR_UNEXPECTED);
        checkErrorReturn(HttpNegotiateConstants.ERR_UNEXPECTED, NetError.ERR_UNEXPECTED);
        checkErrorReturn(HttpNegotiateConstants.ERR_ABORTED, NetError.ERR_ABORTED);
        checkErrorReturn(HttpNegotiateConstants.ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS,
                NetError.ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS);
        checkErrorReturn(
                HttpNegotiateConstants.ERR_INVALID_RESPONSE, NetError.ERR_INVALID_RESPONSE);
        checkErrorReturn(HttpNegotiateConstants.ERR_INVALID_AUTH_CREDENTIALS,
                NetError.ERR_INVALID_AUTH_CREDENTIALS);
        checkErrorReturn(HttpNegotiateConstants.ERR_UNSUPPORTED_AUTH_SCHEME,
                NetError.ERR_UNSUPPORTED_AUTH_SCHEME);
        checkErrorReturn(HttpNegotiateConstants.ERR_MISSING_AUTH_CREDENTIALS,
                NetError.ERR_MISSING_AUTH_CREDENTIALS);
        checkErrorReturn(HttpNegotiateConstants.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS,
                NetError.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS);
        checkErrorReturn(
                HttpNegotiateConstants.ERR_MALFORMED_IDENTITY, NetError.ERR_MALFORMED_IDENTITY);
        // 9999 is not a valid return value
        checkErrorReturn(9999, NetError.ERR_UNEXPECTED);
    }
}
