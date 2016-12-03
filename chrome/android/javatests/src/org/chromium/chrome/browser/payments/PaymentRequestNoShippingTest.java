// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that does not require shipping address.
 */
public class PaymentRequestNoShippingTest extends PaymentRequestTestBase {
    private static final int DECEMBER = 11;
    private static final int NEXT_YEAR = 1;
    private static final int FIRST_BILLING_ADDRESS = 1;

    public PaymentRequestNoShippingTest() {
        super("payment_request_no_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** Click [X] to cancel payment. */
    @MediumTest
    public void testCloseDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Click [EDIT] to expand the dialog, then click [X] to cancel payment. */
    @MediumTest
    public void testEditAndCloseDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickAndWait(R.id.button_secondary, mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Click [EDIT] to expand the dialog, then click [CANCEL] to cancel payment. */
    @MediumTest
    public void testEditAndCancelDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickAndWait(R.id.button_secondary, mReadyForInput);
        clickAndWait(R.id.button_secondary, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Click [PAY] and dismiss the card unmask dialog. */
    @MediumTest
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123"});
    }

    /** Click [PAY], type in "123" into the CVC dialog, then submit the payment. */
    @MediumTest
    public void testCancelUnmaskAndRetry()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_NEGATIVE, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123"});
    }

    /**
     * Attempt to add an invalid credit card number and cancel payment.
     * @MediumTest
     */
    @FlakyTest(message = "crbug.com/626289")
    public void testAddInvalidCardNumberAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        fillNewCardForm("123", "Bob", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    private void fillNewCardForm(String cardNumber, String nameOnCard, int month, int year,
            int billingAddress) throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInCardEditorAndWait(new String[] {cardNumber, nameOnCard}, mEditorTextUpdate);
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {month, year, billingAddress}, mBillingAddressChangeProcessed);
    }

    /**
     * Attempt to add a credit card with an empty name on card and cancel payment.
     * @MediumTest
     */
    @FlakyTest(message = "crbug.com/626289")
    public void testAddEmptyNameOnCardAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        fillNewCardForm("5454-5454-5454-5454", "", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Save a new card on disk and pay. */
    @MediumTest
    public void testSaveNewCardAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        fillNewCardForm("5454-5454-5454-5454", "Bob", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"5454545454545454", "12", "Bob"});
    }

    /** Use a temporary credit card to complete payment. */
    @MediumTest
    public void testAddTemporaryCardAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        fillNewCardForm("5454-5454-5454-5454", "Bob", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);

        // Uncheck the "Save this card on this device" checkbox, so the card is temporary.
        selectCheckboxAndWait(R.id.payments_edit_checkbox, false, mReadyToEdit);

        clickInCardEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"5454545454545454", "12", "Bob"});
    }

    /** Add a new card together with a new billing address and pay. */
    @MediumTest
    public void testSaveNewCardAndNewBillingAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInCardEditorAndWait(new String[] {"5454 5454 5454 5454", "Bob"}, mEditorTextUpdate);

        // Select December of next year for expiration and [Add address] in the billing address
        // dropdown.
        int december = 11;
        int nextYear = 1;
        int addBillingAddress = 2;
        setSpinnerSelectionsInCardEditorAndWait(new int[] {december, nextYear, addBillingAddress},
                mReadyToEdit);

        setTextInEditorAndWait(new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy",
                "Mountain View", "CA", "94043", "999-999-9999"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToEdit);

        clickInCardEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"5454545454545454", "12", "Bob", "Google",
                "1600 Amphitheatre Pkwy", "Mountain View", "CA", "94043", "999-999-9999"});
    }

    /** Quickly pressing on "add card" and then [X] should not crash. */
    @MediumTest
    public void testQuickAddCardAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "add card" and then [X].
        int callCount = mReadyToEdit.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getPaymentMethodSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
                mUI.getDialogForTest().findViewById(R.id.close_button).performClick();
            }
        });
        mReadyToEdit.waitForCallback(callCount);

        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on [X] and then "add card" should not crash. */
    @MediumTest
    public void testQuickCloseAndAddCardShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on [X] and then "add card."
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(R.id.close_button).performClick();
                mUI.getPaymentMethodSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "add card" and then "cancel" should not crash. */
    @MediumTest
    public void testQuickAddCardAndCancelShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "add card" and then "cancel."
        int callCount = mReadyToEdit.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getPaymentMethodSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
                mUI.getDialogForTest().findViewById(R.id.button_secondary).performClick();
            }
        });
        mReadyToEdit.waitForCallback(callCount);

        clickInCardEditorAndWait(R.id.payments_edit_cancel_button, mReadyForInput);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Quickly pressing on "cancel" and then "add card" should not crash. */
    @MediumTest
    public void testQuickCancelAndAddCardShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Quickly press on "cancel" and then "add card."
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(R.id.button_secondary).performClick();
                mUI.getPaymentMethodSectionForTest().findViewById(
                        R.id.payments_add_option_button).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * "pay" should not crash.
     */
    @MediumTest
    public void testQuickDismissAndPayShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Quickly dismiss and then press on "pay."
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().onBackPressed();
                mUI.getDialogForTest().findViewById(R.id.button_primary).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * [X] should not crash.
     */
    @MediumTest
    public void testQuickDismissAndCloseShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Quickly dismiss and then press on [X].
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().onBackPressed();
                mUI.getDialogForTest().findViewById(R.id.close_button).performClick();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Quickly pressing on [X] and then dismissing the dialog (via Android's back button, for
     * example) should not crash.
     */
    @MediumTest
    public void testQuickCloseAndDismissShouldNotCrash()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Quickly press on [X] and then dismiss.
        int callCount = mDismissed.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(R.id.close_button).performClick();
                mUI.getDialogForTest().onBackPressed();
            }
        });
        mDismissed.waitForCallback(callCount);

        expectResultContains(new String[] {"Request cancelled"});
    }

    /**
     * Test that starting a payment request that requires user information except for the payment
     * results in the appropriate metric being logged in the PaymentRequest.RequestedInformation
     * histogram.
     */
    @MediumTest
    public void testRequestedInformationMetric() throws InterruptedException, ExecutionException,
            TimeoutException {
        // Start the Payment Request.
        triggerUIAndWait(mReadyToPay);

        // Make sure that only the appropriate enum value was logged.
        for (int i = 0; i < PaymentRequestMetrics.REQUESTED_INFORMATION_MAX; ++i) {
            assertEquals((i == PaymentRequestMetrics.REQUESTED_INFORMATION_NONE ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.RequestedInformation", i));
        }
    }

    /** Verifies the format of the billing address suggestions when adding a new credit card. */
    @MediumTest
    public void testNewCardBillingAddressFormat()
            throws InterruptedException, ExecutionException, TimeoutException {
        fillNewCardForm("5454-5454-5454-5454", "Bob", DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS);
        assertTrue(getSpinnerSelectionTextInCardEditor(2).equals(
                "Jon Doe, Google, 340 Main St, Los Angeles, CA 90291, United States"));
    }
}
