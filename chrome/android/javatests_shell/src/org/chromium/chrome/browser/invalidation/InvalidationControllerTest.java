// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CollectionUtil;
import org.chromium.base.test.util.Feature;
import org.chromium.components.invalidation.InvalidationClientService;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationIntentProtocol;
import org.chromium.sync.notifier.InvalidationPreferences;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the {@link InvalidationController}.
 */
public class InvalidationControllerTest extends InstrumentationTestCase {
    private IntentSavingContext mContext;
    private InvalidationController mController;

    @Override
    protected void setUp() throws Exception {
        mContext = new IntentSavingContext(getInstrumentation().getTargetContext());
        mController = InvalidationController.get(mContext);
        // We don't want to use the system content resolver, so we override it.
        MockSyncContentResolverDelegate delegate = new MockSyncContentResolverDelegate();
        // Android master sync can safely always be on.
        delegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, delegate);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testStart() throws Exception {
        mController.start();
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testStop() throws Exception {
        mController.stop();
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(1, intent.getExtras().size());
        assertTrue(intent.hasExtra(InvalidationIntentProtocol.EXTRA_STOP));
        assertTrue(intent.getBooleanExtra(InvalidationIntentProtocol.EXTRA_STOP, false));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testResumingMainActivity() throws Exception {
        // Resuming main activity should trigger a start if sync is enabled.
        setupSync(true);
        mController.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testResumingMainActivityWithSyncDisabled() throws Exception {
        // Resuming main activity should NOT trigger a start if sync is disabled.
        setupSync(false);
        mController.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        assertEquals(0, mContext.getNumStartedIntents());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPausingMainActivity() throws Exception {
        // Resuming main activity should trigger a stop if sync is enabled.
        setupSync(true);
        mController.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(1, intent.getExtras().size());
        assertTrue(intent.hasExtra(InvalidationIntentProtocol.EXTRA_STOP));
        assertTrue(intent.getBooleanExtra(InvalidationIntentProtocol.EXTRA_STOP, false));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPausingMainActivityWithSyncDisabled() throws Exception {
        // Resuming main activity should NOT trigger a stop if sync is disabled.
        setupSync(false);
        mController.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        assertEquals(0, mContext.getNumStartedIntents());
    }

    private void setupSync(boolean syncEnabled) {
        Account account = AccountManagerHelper.createAccountFromName("test@gmail.com");
        ChromeSigninController chromeSigninController = ChromeSigninController.get(mContext);
        chromeSigninController.setSignedInAccountName(account.name);
        AndroidSyncSettings androidSyncSettings = AndroidSyncSettings.get(mContext);
        if (syncEnabled) {
            androidSyncSettings.enableChromeSync();
        } else {
            androidSyncSettings.disableChromeSync();
        }
    }

    @SmallTest
    @Feature({"Sync"})
    public void testEnsureConstructorRegistersListener() throws Exception {
        final AtomicBoolean listenerCallbackCalled = new AtomicBoolean();

        // Create instance.
        new InvalidationController(mContext) {
            @Override
            public void onApplicationStateChange(int newState) {
                listenerCallbackCalled.set(true);
            }
        };

        // Ensure initial state is correct.
        assertFalse(listenerCallbackCalled.get());

        // Ensure we get a callback, which means we have registered for them.
        ApplicationStatus.onStateChangeForTesting(new Activity(), ActivityState.CREATED);
        assertTrue(listenerCallbackCalled.get());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegisterForSpecificTypes() {
        InvalidationController controller = new InvalidationController(mContext);
        Account account = new Account("test@example.com", "bogus");
        controller.setRegisteredTypes(account, false,
                CollectionUtil.newHashSet(ModelType.BOOKMARK, ModelType.SESSION));
        assertEquals(1, mContext.getNumStartedIntents());

        // Validate destination.
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(InvalidationIntentProtocol.ACTION_REGISTER, intent.getAction());

        // Validate account.
        Account intentAccount =
                intent.getParcelableExtra(InvalidationIntentProtocol.EXTRA_ACCOUNT);
        assertEquals(account, intentAccount);

        // Validate registered types.
        Set<String> expectedTypes = CollectionUtil.newHashSet(ModelType.BOOKMARK.name(),
                ModelType.SESSION.name());
        Set<String> actualTypes = new HashSet<String>();
        actualTypes.addAll(intent.getStringArrayListExtra(
                                InvalidationIntentProtocol.EXTRA_REGISTERED_TYPES));
        assertEquals(expectedTypes, actualTypes);
        assertNull(InvalidationIntentProtocol.getRegisteredObjectIds(intent));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegisterForAllTypes() {
        Account account = new Account("test@example.com", "bogus");
        mController.setRegisteredTypes(account, true,
                CollectionUtil.newHashSet(ModelType.BOOKMARK, ModelType.SESSION));
        assertEquals(1, mContext.getNumStartedIntents());

        // Validate destination.
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(InvalidationIntentProtocol.ACTION_REGISTER, intent.getAction());

        // Validate account.
        Account intentAccount =
                intent.getParcelableExtra(InvalidationIntentProtocol.EXTRA_ACCOUNT);
        assertEquals(account, intentAccount);

        // Validate registered types.
        Set<String> expectedTypes = CollectionUtil.newHashSet(ModelType.ALL_TYPES_TYPE);
        Set<String> actualTypes = new HashSet<String>();
        actualTypes.addAll(intent.getStringArrayListExtra(
                                InvalidationIntentProtocol.EXTRA_REGISTERED_TYPES));
        assertEquals(expectedTypes, actualTypes);
        assertNull(InvalidationIntentProtocol.getRegisteredObjectIds(intent));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRefreshShouldReadValuesFromDiskWithSpecificTypes() {
        // Store some preferences for ModelTypes and account. We are using the helper class
        // for this, so we don't have to deal with low-level details such as preference keys.
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        InvalidationPreferences.EditContext edit = invalidationPreferences.edit();
        Set<String> storedModelTypes = new HashSet<String>();
        storedModelTypes.add(ModelType.BOOKMARK.name());
        storedModelTypes.add(ModelType.TYPED_URL.name());
        Set<ModelType> refreshedTypes = new HashSet<ModelType>();
        refreshedTypes.add(ModelType.BOOKMARK);
        refreshedTypes.add(ModelType.TYPED_URL);
        invalidationPreferences.setSyncTypes(edit, storedModelTypes);
        Account storedAccount = AccountManagerHelper.createAccountFromName("test@gmail.com");
        invalidationPreferences.setAccount(edit, storedAccount);
        invalidationPreferences.commit(edit);

        // Ensure all calls to {@link InvalidationController#setRegisteredTypes} store values
        // we can inspect in the test.
        final AtomicReference<Account> resultAccount = new AtomicReference<Account>();
        final AtomicBoolean resultAllTypes = new AtomicBoolean();
        final AtomicReference<Set<ModelType>> resultTypes = new AtomicReference<Set<ModelType>>();
        InvalidationController controller = new InvalidationController(mContext) {
            @Override
            public void setRegisteredTypes(
                    Account account, boolean allTypes, Set<ModelType> types) {
                resultAccount.set(account);
                resultAllTypes.set(allTypes);
                resultTypes.set(types);
            }
        };

        // Execute the test.
        controller.refreshRegisteredTypes(refreshedTypes);

        // Validate the values.
        assertEquals(storedAccount, resultAccount.get());
        assertEquals(false, resultAllTypes.get());
        assertEquals(ModelType.syncTypesToModelTypes(storedModelTypes), resultTypes.get());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRefreshShouldReadValuesFromDiskWithAllTypes() {
        // Store preferences for the ModelType.ALL_TYPES_TYPE and account. We
        // are using the helper class for this, so we don't have to deal with
        // low-level details such as preference keys.
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        InvalidationPreferences.EditContext edit = invalidationPreferences.edit();
        List<String> storedModelTypes = new ArrayList<String>();
        storedModelTypes.add(ModelType.ALL_TYPES_TYPE);
        invalidationPreferences.setSyncTypes(edit, storedModelTypes);
        Account storedAccount = AccountManagerHelper.createAccountFromName("test@gmail.com");
        invalidationPreferences.setAccount(edit, storedAccount);
        invalidationPreferences.commit(edit);

        // Ensure all calls to {@link InvalidationController#setRegisteredTypes} store values
        // we can inspect in the test.
        final AtomicReference<Account> resultAccount = new AtomicReference<Account>();
        final AtomicBoolean resultAllTypes = new AtomicBoolean();
        final AtomicReference<Set<ModelType>> resultTypes = new AtomicReference<Set<ModelType>>();
        InvalidationController controller = new InvalidationController(mContext) {
            @Override
            public void setRegisteredTypes(
                    Account account, boolean allTypes, Set<ModelType> types) {
                resultAccount.set(account);
                resultAllTypes.set(allTypes);
                resultTypes.set(types);
            }
        };

        // Execute the test.
        controller.refreshRegisteredTypes(new HashSet<ModelType>());

        // Validate the values.
        assertEquals(storedAccount, resultAccount.get());
        assertEquals(true, resultAllTypes.get());
    }

    /**
     * Asserts that {@code intent} is destined for the correct component.
     */
    private static void validateIntentComponent(Intent intent) {
        assertNotNull(intent.getComponent());
        assertEquals(InvalidationClientService.class.getName(),
                intent.getComponent().getClassName());
    }

}
