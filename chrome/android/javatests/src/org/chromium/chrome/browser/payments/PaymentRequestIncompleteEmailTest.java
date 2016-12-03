// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests email address from a user that has
 * incomplete email address stored on disk.
 */
public class PaymentRequestIncompleteEmailTest extends PaymentRequestTestBase {
    public PaymentRequestIncompleteEmailTest() {
        // This merchant requests an email address.
        super("payment_request_email_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has an invalid email address on disk.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "jon.doe" /* invalid email address */, "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** Attempt to update the email with invalid data and cancel the transaction. */
    @MediumTest
    public void testEditIncompleteEmailAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_first_radio_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"gmail.com"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Update the email with valid data and provide that to the merchant. */
    @MediumTest
    public void testEditIncompleteEmailAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_first_radio_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"jon.doe@google.com"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"jon.doe@google.com"});
    }
}
