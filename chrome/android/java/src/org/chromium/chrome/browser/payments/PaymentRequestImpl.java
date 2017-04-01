// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Handler;
import android.support.v4.util.ArrayMap;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.payments.ui.Completable;
import org.chromium.chrome.browser.payments.ui.ContactDetailsSection;
import org.chromium.chrome.browser.payments.ui.LineItem;
import org.chromium.chrome.browser.payments.ui.PaymentInformation;
import org.chromium.chrome.browser.payments.ui.PaymentOption;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection.FocusChangedObserver;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.browser.payments.ui.ShoppingCart;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.autofill.AutofillPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.CanMakePaymentQueryResult;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentShippingType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * Android implementation of the PaymentRequest service defined in
 * components/payments/payment_request.mojom.
 */
public class PaymentRequestImpl
        implements PaymentRequest, PaymentRequestUI.Client, PaymentApp.InstrumentsCallback,
                   PaymentInstrument.InstrumentDetailsCallback,
                   PaymentAppFactory.PaymentAppCreatedCallback,
                   PaymentResponseHelper.PaymentResponseRequesterDelegate, FocusChangedObserver {
    /**
     * A test-only observer for the PaymentRequest service implementation.
     */
    public interface PaymentRequestServiceObserverForTest {
        /**
         * Called when an abort request was denied.
         */
        void onPaymentRequestServiceUnableToAbort();

        /**
         * Called when the controller is notified of billing address change, but does not alter the
         * editor UI.
         */
        void onPaymentRequestServiceBillingAddressChangeProcessed();

        /**
         * Called when the controller is notified of an expiration month change.
         */
        void onPaymentRequestServiceExpirationMonthChange();

        /**
         * Called when a show request failed. This can happen when:
         * <ul>
         *   <li>The merchant requests only unsupported payment methods.</li>
         *   <li>The merchant requests only payment methods that don't have instruments and are not
         *       able to add instruments from PaymentRequest UI.</li>
         * </ul>
         */
        void onPaymentRequestServiceShowFailed();

        /**
         * Called when the canMakePayment() request has been responded.
         */
        void onPaymentRequestServiceCanMakePaymentQueryResponded();
    }

    /** The object to keep track of cached payment query results. */
    private static class CanMakePaymentQuery {
        private final Set<PaymentRequestImpl> mObservers = new HashSet<>();
        private final Set<String> mMethods;
        private Boolean mResponse;

        /**
         * Keeps track of a payment query.
         *
         * @param methods The payment methods that are being queried.
         */
        public CanMakePaymentQuery(Set<String> methods) {
            assert methods != null;
            mMethods = methods;
        }

        /**
         * Checks whether the given payment methods matches the previously queried payment methods.
         *
         * @param methods The payment methods that are being queried.
         * @return True if the given methods match the previously queried payment methods.
         */
        public boolean matchesPaymentMethods(Set<String> methods) {
            return mMethods.equals(methods);
        }

        /** @return Whether payment can be made, or null if response is not known yet. */
        public Boolean getPreviousResponse() {
            return mResponse;
        }

        /** @param response Whether payment can be made. */
        public void setResponse(boolean response) {
            if (mResponse == null) mResponse = response;
            for (PaymentRequestImpl observer : mObservers) {
                observer.respondCanMakePaymentQuery(mResponse.booleanValue());
            }
            mObservers.clear();
        }

        /** @param observer The observer to notify when the query response is known. */
        public void addObserver(PaymentRequestImpl observer) {
            mObservers.add(observer);
        }
    };

    /** Limit in the number of suggested items in a section. */
    public static final int SUGGESTIONS_LIMIT = 4;

    private static final String TAG = "cr_PaymentRequest";
    private static final String ANDROID_PAY_METHOD_NAME = "https://android.com/pay";
    private static final String PAYMENT_COMPLETE_ONCE = "payment_complete_once";
    private static final Comparator<Completable> COMPLETENESS_COMPARATOR =
            new Comparator<Completable>() {
                @Override
                public int compare(Completable a, Completable b) {
                    return (b.isComplete() ? 1 : 0) - (a.isComplete() ? 1 : 0);
                }
            };

    /** Every origin can call canMakePayment() every 30 minutes. */
    private static final int CAN_MAKE_PAYMENT_QUERY_PERIOD_MS = 30 * 60 * 1000;

    private static PaymentRequestServiceObserverForTest sObserverForTest;

    /** True if show() was called in any PaymentRequestImpl object. */
    private static boolean sIsShowing;

    /**
     * In-memory mapping of the origins of websites that have recently called canMakePayment()
     * to the list of the payment methods that were being queried. Used for throttling the usage of
     * this call. The mapping is shared among all instances of PaymentRequestImpl in the browser
     * process on UI thread. The user can reset the throttling mechanism by restarting the browser.
     */
    private static Map<String, CanMakePaymentQuery> sCanMakePaymentQueries;

    /** Monitors changes in the TabModelSelector. */
    private final TabModelSelectorObserver mSelectorObserver = new EmptyTabModelSelectorObserver() {
        @Override
        public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
            onDismiss();
        }
    };

    /** Monitors changes in the current TabModel. */
    private final TabModelObserver mTabModelObserver = new EmptyTabModelObserver() {
        @Override
        public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
            if (tab == null || tab.getId() != lastId) onDismiss();
        }
    };

    private final Handler mHandler = new Handler();
    private final WebContents mWebContents;
    private final String mMerchantName;
    private final String mOrigin;
    private final AddressEditor mAddressEditor;
    private final CardEditor mCardEditor;
    private final PaymentRequestJourneyLogger mJourneyLogger = new PaymentRequestJourneyLogger();

    private PaymentRequestClient mClient;

    /**
     * The raw total amount being charged, as it was received from the website. This data is passed
     * to the payment app.
     */
    private PaymentItem mRawTotal;

    /**
     * The raw items in the shopping cart, as they were received from the website. This data is
     * passed to the payment app.
     */
    private List<PaymentItem> mRawLineItems;

    /**
     * A mapping from method names to modifiers, which include modified totals and additional line
     * items. Used to display modified totals for each payment instrument, modified total in order
     * summary, and additional line items in order summary.
     */
    private Map<String, PaymentDetailsModifier> mModifiers;

    /**
     * The UI model of the shopping cart, including the total. Each item includes a label and a
     * price string. This data is passed to the UI.
     */
    private ShoppingCart mUiShoppingCart;

    /**
     * The UI model for the shipping options. Includes the label and sublabel for each shipping
     * option. Also keeps track of the selected shipping option. This data is passed to the UI.
     */
    private SectionInformation mUiShippingOptions;

    private Map<String, PaymentMethodData> mMethodData;
    private boolean mRequestShipping;
    private boolean mRequestPayerName;
    private boolean mRequestPayerPhone;
    private boolean mRequestPayerEmail;
    private int mShippingType;
    private SectionInformation mShippingAddressesSection;
    private ContactDetailsSection mContactSection;
    private List<PaymentApp> mApps;
    private List<PaymentApp> mPendingApps;
    private List<PaymentInstrument> mPendingInstruments;
    private List<PaymentInstrument> mPendingAutofillInstruments;
    private SectionInformation mPaymentMethodsSection;
    private PaymentRequestUI mUI;
    private Callback<PaymentInformation> mPaymentInformationCallback;
    private boolean mPaymentAppRunning;
    private boolean mMerchantSupportsAutofillPaymentInstruments;
    private ContactEditor mContactEditor;
    private boolean mHasRecordedAbortReason;
    private boolean mQueriedCanMakePayment;
    private CurrencyFormatter mCurrencyFormatter;
    private TabModelSelector mObservedTabModelSelector;
    private TabModel mObservedTabModel;

    /** True if any of the requested payment methods are supported. */
    private boolean mArePaymentMethodsSupported;

    /**
     * True after at least one usable payment instrument has been found. Should be read only after
     * all payment apps have been queried.
     */
    private boolean mCanMakePayment;

    /** The helper to create and fill the response to send to the merchant. */
    private PaymentResponseHelper mPaymentResponseHelper;

    /**
     * Builds the PaymentRequest service implementation.
     *
     * @param webContents The web contents that have invoked the PaymentRequest API.
     */
    public PaymentRequestImpl(WebContents webContents) {
        assert webContents != null;

        mWebContents = webContents;

        mMerchantName = webContents.getTitle();
        // The feature is available only in secure context, so it's OK to not show HTTPS.
        mOrigin = UrlFormatter.formatUrlForSecurityDisplay(
                mWebContents.getLastCommittedUrl(), false);

        mApps = new ArrayList<>();

        mAddressEditor = new AddressEditor();
        mCardEditor = new CardEditor(mWebContents, mAddressEditor, sObserverForTest);

        if (sCanMakePaymentQueries == null) sCanMakePaymentQueries = new ArrayMap<>();

        recordSuccessFunnelHistograms("Initiated");
    }

    protected void finalize() throws Throwable {
        super.finalize();
        if (mCurrencyFormatter != null) {
            // Ensures the native implementation of currency formatter does not leak.
            mCurrencyFormatter.destroy();
        }
    }

    /**
     * Called by the merchant website to initialize the payment request data.
     */
    @Override
    public void init(PaymentRequestClient client, PaymentMethodData[] methodData,
            PaymentDetails details, PaymentOptions options) {
        if (mClient != null || client == null) return;
        mClient = client;

        if (mMethodData != null) {
            disconnectFromClientWithDebugMessage("PaymentRequest.show() called more than once.");
            recordAbortReasonHistogram(
                    PaymentRequestMetrics.ABORT_REASON_INVALID_DATA_FROM_RENDERER);
            return;
        }

        mMethodData = getValidatedMethodData(methodData, mCardEditor);
        if (mMethodData == null) {
            disconnectFromClientWithDebugMessage("Invalid payment methods or data");
            recordAbortReasonHistogram(
                    PaymentRequestMetrics.ABORT_REASON_INVALID_DATA_FROM_RENDERER);
            return;
        }

        if (!parseAndValidateDetailsOrDisconnectFromClient(details)) return;

        PaymentAppFactory.getInstance().create(
                mWebContents, Collections.unmodifiableSet(mMethodData.keySet()), this);

        mRequestShipping = options != null && options.requestShipping;
        mRequestPayerName = options != null && options.requestPayerName;
        mRequestPayerPhone = options != null && options.requestPayerPhone;
        mRequestPayerEmail = options != null && options.requestPayerEmail;
        mShippingType = options == null ? PaymentShippingType.SHIPPING : options.shippingType;

        PaymentRequestMetrics.recordRequestedInformationHistogram(mRequestPayerEmail,
                mRequestPayerPhone, mRequestShipping, mRequestPayerName);
    }

    private void buildUI(Activity activity) {
        assert activity != null;

        List<AutofillProfile> profiles = null;
        if (mRequestShipping || mRequestPayerName || mRequestPayerPhone || mRequestPayerEmail) {
            profiles = PersonalDataManager.getInstance().getProfilesToSuggest(
                    false /* includeNameInLabel */);
        }

        if (mRequestShipping) {
            createShippingSection(activity, Collections.unmodifiableList(profiles));
        }

        if (mRequestPayerName || mRequestPayerPhone || mRequestPayerEmail) {
            mContactEditor =
                    new ContactEditor(mRequestPayerName, mRequestPayerPhone, mRequestPayerEmail);
            mContactSection = new ContactDetailsSection(
                    activity, Collections.unmodifiableList(profiles), mContactEditor);
        }

        mUI = new PaymentRequestUI(activity, this, mRequestShipping,
                mRequestPayerName || mRequestPayerPhone || mRequestPayerEmail,
                mMerchantSupportsAutofillPaymentInstruments,
                !isPaymentCompleteOnce(), mMerchantName, mOrigin,
                new ShippingStrings(mShippingType));

        final FaviconHelper faviconHelper = new FaviconHelper();
        faviconHelper.getLocalFaviconImageForURL(Profile.getLastUsedProfile(),
                mWebContents.getLastCommittedUrl(),
                activity.getResources().getDimensionPixelSize(R.dimen.payments_favicon_size),
                new FaviconHelper.FaviconImageCallback() {
                    @Override
                    public void onFaviconAvailable(Bitmap bitmap, String iconUrl) {
                        if (bitmap != null) mUI.setTitleBitmap(bitmap);
                        faviconHelper.destroy();
                    }
                });

        // Add the callback to change the label of shipping addresses depending on the focus.
        if (mRequestShipping) mUI.setShippingAddressSectionFocusChangedObserver(this);

        mAddressEditor.setEditorView(mUI.getEditorView());
        mCardEditor.setEditorView(mUI.getCardEditorView());
        if (mContactEditor != null) mContactEditor.setEditorView(mUI.getEditorView());
    }

    private void createShippingSection(
            Context context, List<AutofillProfile> unmodifiableProfiles) {
        List<AutofillAddress> addresses = new ArrayList<>();

        for (int i = 0; i < unmodifiableProfiles.size(); i++) {
            AutofillProfile profile = unmodifiableProfiles.get(i);
            mAddressEditor.addPhoneNumberIfValid(profile.getPhoneNumber());

            // Only suggest addresses that have a street address.
            if (!TextUtils.isEmpty(profile.getStreetAddress())) {
                addresses.add(new AutofillAddress(context, profile));
            }
        }

        // Suggest complete addresses first.
        Collections.sort(addresses, COMPLETENESS_COMPARATOR);

        // Limit the number of suggestions.
        addresses = addresses.subList(0, Math.min(addresses.size(), SUGGESTIONS_LIMIT));

        // Load the validation rules for each unique region code.
        Set<String> uniqueCountryCodes = new HashSet<>();
        for (int i = 0; i < addresses.size(); ++i) {
            String countryCode = AutofillAddress.getCountryCode(addresses.get(i).getProfile());
            if (!uniqueCountryCodes.contains(countryCode)) {
                uniqueCountryCodes.add(countryCode);
                PersonalDataManager.getInstance().loadRulesForRegion(countryCode);
            }
        }

        // Log the number of suggested shipping addresses.
        mJourneyLogger.setNumberOfSuggestionsShown(
                PaymentRequestJourneyLogger.SECTION_SHIPPING_ADDRESS, addresses.size());

        // Automatically select the first address if one is complete and if the merchant does
        // not require a shipping address to calculate shipping costs.
        int firstCompleteAddressIndex = SectionInformation.NO_SELECTION;
        if (mUiShippingOptions.getSelectedItem() != null && !addresses.isEmpty()
                && addresses.get(0).isComplete()) {
            firstCompleteAddressIndex = 0;

            // The initial label for the selected shipping address should not include the
            // country.
            addresses.get(firstCompleteAddressIndex).setShippingAddressLabelWithoutCountry();
        }

        mShippingAddressesSection = new SectionInformation(
                PaymentRequestUI.TYPE_SHIPPING_ADDRESSES, firstCompleteAddressIndex, addresses);
    }

    /**
     * Called by the merchant website to show the payment request to the user.
     */
    @Override
    public void show() {
        if (mClient == null) return;

        if (getIsShowing()) {
            disconnectFromClientWithDebugMessage("A PaymentRequest UI is already showing");
            recordAbortReasonHistogram(
                    PaymentRequestMetrics.ABORT_REASON_INVALID_DATA_FROM_RENDERER);
            return;
        }

        setIsShowing(true);
        if (disconnectIfNoPaymentMethodsSupported()) return;

        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) {
            disconnectFromClientWithDebugMessage("Unable to find Chrome activity");
            recordAbortReasonHistogram(PaymentRequestMetrics.ABORT_REASON_OTHER);
            return;
        }

        // Catch any time the user switches tabs. Because the dialog is modal, a user shouldn't be
        // allowed to switch tabs, which can happen if the user receives an external Intent.
        mObservedTabModelSelector = chromeActivity.getTabModelSelector();
        mObservedTabModel = chromeActivity.getCurrentTabModel();
        mObservedTabModelSelector.addObserver(mSelectorObserver);
        mObservedTabModel.addObserver(mTabModelObserver);

        buildUI(chromeActivity);
        mUI.show();
        recordSuccessFunnelHistograms("Shown");
        mJourneyLogger.setShowCalled();
    }

    private static Map<String, PaymentMethodData> getValidatedMethodData(
            PaymentMethodData[] methodData, CardEditor paymentMethodsCollector) {
        // Payment methodData are required.
        if (methodData == null || methodData.length == 0) return null;
        Map<String, PaymentMethodData> result = new ArrayMap<>();
        for (int i = 0; i < methodData.length; i++) {
            String[] methods = methodData[i].supportedMethods;

            // Payment methods are required.
            if (methods == null || methods.length == 0) return null;

            for (int j = 0; j < methods.length; j++) {
                // Payment methods should be non-empty.
                if (TextUtils.isEmpty(methods[j])) return null;
                result.put(methods[j], methodData[i]);
            }

            paymentMethodsCollector.addAcceptedPaymentMethodsIfRecognized(methodData[i]);
        }

        return Collections.unmodifiableMap(result);
    }

    @Override
    public void onPaymentAppCreated(PaymentApp paymentApp) {
        mApps.add(paymentApp);
    }

    @Override
    public void onAllPaymentAppsCreated() {
        if (mClient == null) return;

        assert mPendingApps == null;

        mPendingApps = new ArrayList<>(mApps);
        mPendingInstruments = new ArrayList<>();
        mPendingAutofillInstruments = new ArrayList<>();

        Map<PaymentApp, Map<String, PaymentMethodData>> queryApps = new ArrayMap<>();
        for (int i = 0; i < mApps.size(); i++) {
            PaymentApp app = mApps.get(i);
            Map<String, PaymentMethodData> appMethods = filterMerchantMethodData(mMethodData,
                    app.getAppMethodNames());
            if (appMethods == null || !app.supportsMethodsAndData(appMethods)) {
                mPendingApps.remove(app);
            } else {
                mArePaymentMethodsSupported = true;
                mMerchantSupportsAutofillPaymentInstruments |= app instanceof AutofillPaymentApp;
                queryApps.put(app, appMethods);
            }
        }

        // Query instruments after mMerchantSupportsAutofillPaymentInstruments has been initialized,
        // so a fast response from a non-autofill payment app at the front of the app list does not
        // cause NOT_SUPPORTED payment rejection.
        for (Map.Entry<PaymentApp, Map<String, PaymentMethodData>> q : queryApps.entrySet()) {
            q.getKey().getInstruments(q.getValue(), mOrigin, this);
        }
    }

    /** Filter out merchant method data that's not relevant to a payment app. Can return null. */
    private static Map<String, PaymentMethodData> filterMerchantMethodData(
            Map<String, PaymentMethodData> merchantMethodData, Set<String> appMethods) {
        Map<String, PaymentMethodData> result = null;
        for (String method : appMethods) {
            if (merchantMethodData.containsKey(method)) {
                if (result == null) result = new ArrayMap<>();
                result.put(method, merchantMethodData.get(method));
            }
        }
        return result == null ? null : Collections.unmodifiableMap(result);
    }

    /**
     * Called by merchant to update the shipping options and line items after the user has selected
     * their shipping address or shipping option.
     */
    @Override
    public void updateWith(PaymentDetails details) {
        if (mClient == null) return;

        if (mUI == null) {
            disconnectFromClientWithDebugMessage(
                    "PaymentRequestUpdateEvent.updateWith() called without PaymentRequest.show()");
            recordAbortReasonHistogram(
                    PaymentRequestMetrics.ABORT_REASON_INVALID_DATA_FROM_RENDERER);
            return;
        }

        if (!parseAndValidateDetailsOrDisconnectFromClient(details)) return;

        if (mUiShippingOptions.isEmpty() && mShippingAddressesSection.getSelectedItem() != null) {
            mShippingAddressesSection.getSelectedItem().setInvalid();
            mShippingAddressesSection.setSelectedItemIndex(SectionInformation.INVALID_SELECTION);
            mShippingAddressesSection.setErrorMessage(details.error);
        }

        if (mPaymentInformationCallback != null) {
            providePaymentInformation();
        } else {
            mUI.updateOrderSummarySection(mUiShoppingCart);
            mUI.updateSection(PaymentRequestUI.TYPE_SHIPPING_OPTIONS, mUiShippingOptions);
        }
    }

    /**
     * Sets the total, display line items, and shipping options based on input and returns the
     * status boolean. That status is true for valid data, false for invalid data. If the input is
     * invalid, disconnects from the client. Both raw and UI versions of data are updated.
     *
     * @param details The total, line items, and shipping options to parse, validate, and save in
     *                member variables.
     * @return True if the data is valid. False if the data is invalid.
     */
    private boolean parseAndValidateDetailsOrDisconnectFromClient(PaymentDetails details) {
        if (!PaymentValidator.validatePaymentDetails(details)) {
            disconnectFromClientWithDebugMessage("Invalid payment details");
            recordAbortReasonHistogram(
                    PaymentRequestMetrics.ABORT_REASON_INVALID_DATA_FROM_RENDERER);
            return false;
        }

        if (mCurrencyFormatter == null) {
            mCurrencyFormatter = new CurrencyFormatter(details.total.amount.currency,
                    details.total.amount.currencySystem, Locale.getDefault());
        }

        // Total is never pending.
        LineItem uiTotal = new LineItem(details.total.label,
                mCurrencyFormatter.getFormattedCurrencyCode(),
                mCurrencyFormatter.format(details.total.amount.value), /* isPending */ false);

        List<LineItem> uiLineItems = getLineItems(details.displayItems, mCurrencyFormatter);

        mUiShoppingCart = new ShoppingCart(uiTotal, uiLineItems);
        mRawTotal = details.total;
        mRawLineItems = Collections.unmodifiableList(Arrays.asList(details.displayItems));

        mUiShippingOptions = getShippingOptions(details.shippingOptions, mCurrencyFormatter);

        for (int i = 0; i < details.modifiers.length; i++) {
            PaymentDetailsModifier modifier = details.modifiers[i];
            String[] methods = modifier.methodData.supportedMethods;
            for (int j = 0; j < methods.length; j++) {
                if (mModifiers == null) mModifiers = new ArrayMap<>();
                mModifiers.put(methods[j], modifier);
            }
        }

        updateInstrumentModifiedTotals();

        return true;
    }

    /** Updates the modifiers for payment instruments and order summary. */
    private void updateInstrumentModifiedTotals() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_PAYMENTS_MODIFIERS)) return;
        if (mModifiers == null) return;
        if (mPaymentMethodsSection == null) return;

        for (int i = 0; i < mPaymentMethodsSection.getSize(); i++) {
            PaymentInstrument instrument = (PaymentInstrument) mPaymentMethodsSection.getItem(i);
            PaymentDetailsModifier modifier = getModifier(instrument);
            instrument.setModifiedTotal(modifier == null || modifier.total == null
                            ? null
                            : mCurrencyFormatter.format(modifier.total.amount.value));
        }

        updateOrderSummary((PaymentInstrument) mPaymentMethodsSection.getSelectedItem());
    }

    /** Sets the modifier for the order summary based on the given instrument, if any. */
    private void updateOrderSummary(@Nullable PaymentInstrument instrument) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_PAYMENTS_MODIFIERS)) return;

        PaymentDetailsModifier modifier = getModifier(instrument);
        PaymentItem total = modifier == null ? null : modifier.total;
        if (total == null) total = mRawTotal;

        mUiShoppingCart.setTotal(
                new LineItem(total.label, mCurrencyFormatter.getFormattedCurrencyCode(),
                        mCurrencyFormatter.format(total.amount.value), false /* isPending */));
        mUiShoppingCart.setAdditionalContents(modifier == null
                        ? null
                        : getLineItems(modifier.additionalDisplayItems, mCurrencyFormatter));
        mUI.updateOrderSummarySection(mUiShoppingCart);
    }

    /** @return The first modifier that matches the given instrument, or null. */
    @Nullable private PaymentDetailsModifier getModifier(@Nullable PaymentInstrument instrument) {
        if (instrument == null) return null;
        Set<String> methodNames = instrument.getInstrumentMethodNames();
        methodNames.retainAll(mModifiers.keySet());
        return methodNames.isEmpty() ? null : mModifiers.get(methodNames.iterator().next());
    }

    /**
     * Converts a list of payment items and returns their parsed representation.
     *
     * @param items     The payment items to parse. Can be null.
     * @param formatter A formatter for the currency amount value.
     * @return A list of valid line items.
     */
    private static List<LineItem> getLineItems(
            @Nullable PaymentItem[] items, CurrencyFormatter formatter) {
        // Line items are optional.
        if (items == null) return new ArrayList<>();

        List<LineItem> result = new ArrayList<>(items.length);
        for (int i = 0; i < items.length; i++) {
            PaymentItem item = items[i];

            result.add(new LineItem(
                    item.label, "", formatter.format(item.amount.value), item.pending));
        }

        return Collections.unmodifiableList(result);
    }

    /**
     * Converts a list of shipping options and returns their parsed representation.
     *
     * @param options   The raw shipping options to parse. Can be null.
     * @param formatter A formatter for the currency amount value.
     * @return The UI representation of the shipping options.
     */
    private static SectionInformation getShippingOptions(
            @Nullable PaymentShippingOption[] options, CurrencyFormatter formatter) {
        // Shipping options are optional.
        if (options == null || options.length == 0) {
            return new SectionInformation(PaymentRequestUI.TYPE_SHIPPING_OPTIONS);
        }

        List<PaymentOption> result = new ArrayList<>();
        int selectedItemIndex = SectionInformation.NO_SELECTION;
        for (int i = 0; i < options.length; i++) {
            PaymentShippingOption option = options[i];
            result.add(new PaymentOption(option.id, option.label,
                    formatter.format(option.amount.value), null));
            if (option.selected) selectedItemIndex = i;
        }

        return new SectionInformation(PaymentRequestUI.TYPE_SHIPPING_OPTIONS, selectedItemIndex,
                Collections.unmodifiableList(result));
    }

    /**
     * Called to retrieve the data to show in the initial PaymentRequest UI.
     */
    @Override
    public void getDefaultPaymentInformation(Callback<PaymentInformation> callback) {
        mPaymentInformationCallback = callback;

        if (mPaymentMethodsSection == null) return;

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                providePaymentInformation();
            }
        });
    }

    private void providePaymentInformation() {
        mPaymentInformationCallback.onResult(
                new PaymentInformation(mUiShoppingCart, mShippingAddressesSection,
                        mUiShippingOptions, mContactSection, mPaymentMethodsSection));
        mPaymentInformationCallback = null;
    }

    @Override
    public void getShoppingCart(final Callback<ShoppingCart> callback) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                callback.onResult(mUiShoppingCart);
            }
        });
    }

    @Override
    public void getSectionInformation(@PaymentRequestUI.DataType final int optionType,
            final Callback<SectionInformation> callback) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (optionType == PaymentRequestUI.TYPE_SHIPPING_ADDRESSES) {
                    callback.onResult(mShippingAddressesSection);
                } else if (optionType == PaymentRequestUI.TYPE_SHIPPING_OPTIONS) {
                    callback.onResult(mUiShippingOptions);
                } else if (optionType == PaymentRequestUI.TYPE_CONTACT_DETAILS) {
                    callback.onResult(mContactSection);
                } else if (optionType == PaymentRequestUI.TYPE_PAYMENT_METHODS) {
                    assert mPaymentMethodsSection != null;
                    callback.onResult(mPaymentMethodsSection);
                }
            }
        });
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionOptionSelected(@PaymentRequestUI.DataType int optionType,
            PaymentOption option, Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.TYPE_SHIPPING_ADDRESSES) {
            assert option instanceof AutofillAddress;
            // Log the change of shipping address.
            mJourneyLogger.incrementSelectionChanges(
                    PaymentRequestJourneyLogger.SECTION_SHIPPING_ADDRESS);
            AutofillAddress address = (AutofillAddress) option;
            if (address.isComplete()) {
                mShippingAddressesSection.setSelectedItem(option);
                // This updates the line items and the shipping options asynchronously.
                mClient.onShippingAddressChange(address.toPaymentAddress());
            } else {
                editAddress(address);
            }
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SELECTION_RESULT_ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.TYPE_SHIPPING_OPTIONS) {
            // This may update the line items.
            mUiShippingOptions.setSelectedItem(option);
            mClient.onShippingOptionChange(option.getIdentifier());
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SELECTION_RESULT_ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.TYPE_CONTACT_DETAILS) {
            assert option instanceof AutofillContact;
            // Log the change of contact info.
            mJourneyLogger.incrementSelectionChanges(
                    PaymentRequestJourneyLogger.SECTION_CONTACT_INFO);
            AutofillContact contact = (AutofillContact) option;

            if (contact.isComplete()) {
                mContactSection.setSelectedItem(option);
            } else {
                editContact(contact);
                return PaymentRequestUI.SELECTION_RESULT_EDITOR_LAUNCH;
            }
        } else if (optionType == PaymentRequestUI.TYPE_PAYMENT_METHODS) {
            assert option instanceof PaymentInstrument;
            if (option instanceof AutofillPaymentInstrument) {
                // Log the change of credit card.
                mJourneyLogger.incrementSelectionChanges(
                        PaymentRequestJourneyLogger.SECTION_CREDIT_CARDS);
                AutofillPaymentInstrument card = (AutofillPaymentInstrument) option;

                if (!card.isComplete()) {
                    editCard(card);
                    return PaymentRequestUI.SELECTION_RESULT_EDITOR_LAUNCH;
                }
            }

            updateOrderSummary((PaymentInstrument) option);
            mPaymentMethodsSection.setSelectedItem(option);
        }

        return PaymentRequestUI.SELECTION_RESULT_NONE;
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionEditOption(@PaymentRequestUI.DataType int optionType, PaymentOption option,
            Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.TYPE_SHIPPING_ADDRESSES) {
            assert option instanceof AutofillAddress;
            editAddress((AutofillAddress) option);
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SELECTION_RESULT_ASYNCHRONOUS_VALIDATION;
        }

        if (optionType == PaymentRequestUI.TYPE_CONTACT_DETAILS) {
            assert option instanceof AutofillContact;
            editContact((AutofillContact) option);
            return PaymentRequestUI.SELECTION_RESULT_EDITOR_LAUNCH;
        }

        if (optionType == PaymentRequestUI.TYPE_PAYMENT_METHODS) {
            assert option instanceof AutofillPaymentInstrument;
            editCard((AutofillPaymentInstrument) option);
            return PaymentRequestUI.SELECTION_RESULT_EDITOR_LAUNCH;
        }

        assert false;
        return PaymentRequestUI.SELECTION_RESULT_NONE;
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionAddOption(
            @PaymentRequestUI.DataType int optionType, Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.TYPE_SHIPPING_ADDRESSES) {
            editAddress(null);
            mPaymentInformationCallback = callback;
            // Log the add of shipping address.
            mJourneyLogger.incrementSelectionAdds(
                    PaymentRequestJourneyLogger.SECTION_SHIPPING_ADDRESS);
            return PaymentRequestUI.SELECTION_RESULT_ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.TYPE_CONTACT_DETAILS) {
            editContact(null);
            // Log the add of contact info.
            mJourneyLogger.incrementSelectionAdds(PaymentRequestJourneyLogger.SECTION_CONTACT_INFO);
            return PaymentRequestUI.SELECTION_RESULT_EDITOR_LAUNCH;
        } else if (optionType == PaymentRequestUI.TYPE_PAYMENT_METHODS) {
            editCard(null);
            // Log the add of credit card.
            mJourneyLogger.incrementSelectionAdds(PaymentRequestJourneyLogger.SECTION_CREDIT_CARDS);
            return PaymentRequestUI.SELECTION_RESULT_EDITOR_LAUNCH;
        }

        return PaymentRequestUI.SELECTION_RESULT_NONE;
    }

    private void editAddress(final AutofillAddress toEdit) {
        if (toEdit != null) {
            // Log the edit of a shipping address.
            mJourneyLogger.incrementSelectionEdits(
                    PaymentRequestJourneyLogger.SECTION_SHIPPING_ADDRESS);
        }
        mAddressEditor.edit(toEdit, new Callback<AutofillAddress>() {
            @Override
            public void onResult(AutofillAddress editedAddress) {
                if (mUI == null) return;

                if (editedAddress != null) {
                    // Sets or updates the shipping address label.
                    editedAddress.setShippingAddressLabelWithCountry();

                    mCardEditor.updateBillingAddressIfComplete(editedAddress);

                    // A partial or complete address came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedAddress.isComplete()) {
                        // If the address is not complete, unselect it (editor can return incomplete
                        // information when cancelled).
                        mShippingAddressesSection.setSelectedItemIndex(
                                SectionInformation.NO_SELECTION);
                        providePaymentInformation();
                    } else {
                        if (toEdit == null) {
                            // Address is complete and user was in the "Add flow": add an item to
                            // the list.
                            mShippingAddressesSection.addAndSelectItem(editedAddress);
                        }

                        if (mContactSection != null) {
                            // Update |mContactSection| with the new/edited address, which will
                            // update an existing item or add a new one to the end of the list.
                            mContactSection.addOrUpdateWithAutofillAddress(editedAddress);
                            mUI.updateSection(
                                    PaymentRequestUI.TYPE_CONTACT_DETAILS, mContactSection);
                        }

                        // This updates the line items and the shipping options asynchronously by
                        // sending the new address to the merchant website.
                        mClient.onShippingAddressChange(editedAddress.toPaymentAddress());
                    }
                } else {
                    providePaymentInformation();
                }
            }
        });
    }

    private void editContact(final AutofillContact toEdit) {
        if (toEdit != null) {
            // Log the edit of a contact info.
            mJourneyLogger.incrementSelectionEdits(
                    PaymentRequestJourneyLogger.SECTION_CONTACT_INFO);
        }
        mContactEditor.edit(toEdit, new Callback<AutofillContact>() {
            @Override
            public void onResult(AutofillContact editedContact) {
                if (mUI == null) return;

                if (editedContact != null) {
                    // A partial or complete contact came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedContact.isComplete()) {
                        // If the contact is not complete according to the requirements of the flow,
                        // unselect it (editor can return incomplete information when cancelled).
                        mContactSection.setSelectedItemIndex(SectionInformation.NO_SELECTION);
                    } else if (toEdit == null) {
                        // Contact is complete and we were in the "Add flow": add an item to the
                        // list.
                        mContactSection.addAndSelectItem(editedContact);
                    }
                    // If contact is complete and (toEdit != null), no action needed: the contact
                    // was already selected in the UI.
                }
                // If |editedContact| is null, the user has cancelled out of the "Add flow". No
                // action to take (if a contact was selected in the UI, it will stay selected).

                mUI.updateSection(PaymentRequestUI.TYPE_CONTACT_DETAILS, mContactSection);
            }
        });
    }

    private void editCard(final AutofillPaymentInstrument toEdit) {
        if (toEdit != null) {
            // Log the edit of a credit card.
            mJourneyLogger.incrementSelectionEdits(
                    PaymentRequestJourneyLogger.SECTION_CREDIT_CARDS);
        }
        mCardEditor.edit(toEdit, new Callback<AutofillPaymentInstrument>() {
            @Override
            public void onResult(AutofillPaymentInstrument editedCard) {
                if (mUI == null) return;

                if (editedCard != null) {
                    // A partial or complete card came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedCard.isComplete()) {
                        // If the card is not complete, unselect it (editor can return incomplete
                        // information when cancelled).
                        mPaymentMethodsSection.setSelectedItemIndex(
                                SectionInformation.NO_SELECTION);
                    } else if (toEdit == null) {
                        // Card is complete and we were in the "Add flow": add an item to the list.
                        mPaymentMethodsSection.addAndSelectItem(editedCard);
                    }
                    // If card is complete and (toEdit != null), no action needed: the card was
                    // already selected in the UI.
                }
                // If |editedCard| is null, the user has cancelled out of the "Add flow". No action
                // to take (if another card was selected prior to the add flow, it will stay
                // selected).

                updateInstrumentModifiedTotals();
                mUI.updateSection(PaymentRequestUI.TYPE_PAYMENT_METHODS, mPaymentMethodsSection);
            }
        });
    }

    @Override
    public void onInstrumentDetailsLoadingWithoutUI() {
        if (mClient == null || mUI == null || mPaymentResponseHelper == null) return;

        mUI.showProcessingMessage();
        mPaymentResponseHelper.onInstrumentsDetailsLoading();
    }

    @Override
    public boolean onPayClicked(PaymentOption selectedShippingAddress,
            PaymentOption selectedShippingOption, PaymentOption selectedPaymentMethod) {
        assert selectedPaymentMethod instanceof PaymentInstrument;
        PaymentInstrument instrument = (PaymentInstrument) selectedPaymentMethod;
        mPaymentAppRunning = true;

        PaymentOption selectedContact = mContactSection != null ? mContactSection.getSelectedItem()
                : null;
        mPaymentResponseHelper = new PaymentResponseHelper(
                selectedShippingAddress, selectedShippingOption, selectedContact, this);

        // Create a map that is the subset of mMethodData that contains the
        // payment methods supported by the selected payment instrument. If this
        // intersection contains more than one payment method, the payment app is
        // at liberty to choose (or have the user choose) one of the methods.
        Map<String, PaymentMethodData> methodData = new HashMap<>();
        for (String instrumentMethodName : instrument.getInstrumentMethodNames()) {
            if (mMethodData.containsKey(instrumentMethodName)) {
                methodData.put(instrumentMethodName, mMethodData.get(instrumentMethodName));
            }
        }

        instrument.invokePaymentApp(mMerchantName, mOrigin, mRawTotal, mRawLineItems,
                Collections.unmodifiableMap(methodData), this);
        recordSuccessFunnelHistograms("PayClicked");
        return !(instrument instanceof AutofillPaymentInstrument);
    }

    @Override
    public void onDismiss() {
        disconnectFromClientWithDebugMessage("Dialog dismissed");
        recordAbortReasonHistogram(PaymentRequestMetrics.ABORT_REASON_ABORTED_BY_USER);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage) {
        disconnectFromClientWithDebugMessage(debugMessage, PaymentErrorReason.USER_CANCEL);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage, int reason) {
        Log.d(TAG, debugMessage);
        if (mClient != null) mClient.onError(reason);
        closeClient();
        closeUI(true);
    }

    /**
     * Called by the merchant website to abort the payment.
     */
    @Override
    public void abort() {
        if (mClient == null) return;
        mClient.onAbort(!mPaymentAppRunning);
        if (mPaymentAppRunning) {
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceUnableToAbort();
        } else {
            closeClient();
            closeUI(true);
            recordAbortReasonHistogram(PaymentRequestMetrics.ABORT_REASON_ABORTED_BY_MERCHANT);
        }
    }

    /**
     * Called when the merchant website has processed the payment.
     */
    @Override
    public void complete(int result) {
        if (mClient == null) return;
        recordSuccessFunnelHistograms("Completed");
        if (!isPaymentCompleteOnce()) setPaymentCompleteOnce();
        closeUI(PaymentComplete.FAIL != result);
    }

    private static boolean isPaymentCompleteOnce() {
        return ContextUtils.getAppSharedPreferences().getBoolean(PAYMENT_COMPLETE_ONCE, false);
    }

    private static void setPaymentCompleteOnce() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(PAYMENT_COMPLETE_ONCE, true)
                .apply();
    }

    @Override
    public void onCardAndAddressSettingsClicked() {
        Context context = ChromeActivity.fromWebContents(mWebContents);
        if (context == null) {
            disconnectFromClientWithDebugMessage("Unable to find Chrome activity");
            recordAbortReasonHistogram(PaymentRequestMetrics.ABORT_REASON_OTHER);
            return;
        }

        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                context, AutofillPreferences.class.getName());
        context.startActivity(intent);
        disconnectFromClientWithDebugMessage("Card and address settings clicked");
        recordAbortReasonHistogram(PaymentRequestMetrics.ABORT_REASON_ABORTED_BY_USER);
    }

    /**
     * Called by the merchant website to check if the user has complete payment instruments.
     */
    @Override
    public void canMakePayment() {
        if (mClient == null) return;

        CanMakePaymentQuery query = sCanMakePaymentQueries.get(mOrigin);
        if (query == null) {
            query = new CanMakePaymentQuery(mMethodData.keySet());
            sCanMakePaymentQueries.put(mOrigin, query);
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    sCanMakePaymentQueries.remove(mOrigin);
                }
            }, CAN_MAKE_PAYMENT_QUERY_PERIOD_MS);
        }

        if (!query.matchesPaymentMethods(mMethodData.keySet())) {
            mClient.onCanMakePayment(CanMakePaymentQueryResult.QUERY_QUOTA_EXCEEDED);
            if (sObserverForTest != null) {
                sObserverForTest.onPaymentRequestServiceCanMakePaymentQueryResponded();
            }
            return;
        }

        if (query.getPreviousResponse() != null) {
            respondCanMakePaymentQuery(query.getPreviousResponse().booleanValue());
            return;
        }

        query.addObserver(this);
        if (isFinishedQueryingPaymentApps()) {
            query.setResponse(mCanMakePayment);
            mJourneyLogger.setCanMakePaymentValue(mCanMakePayment);
        }
    }

    private void respondCanMakePaymentQuery(boolean response) {
        mClient.onCanMakePayment(response ? CanMakePaymentQueryResult.CAN_MAKE_PAYMENT
                : CanMakePaymentQueryResult.CANNOT_MAKE_PAYMENT);
        mJourneyLogger.setCanMakePaymentValue(mCanMakePayment);
        if (sObserverForTest != null) {
            sObserverForTest.onPaymentRequestServiceCanMakePaymentQueryResponded();
        }
    }

    /**
     * Called when the renderer closes the Mojo connection.
     */
    @Override
    public void close() {
        if (mClient == null) return;
        closeClient();
        closeUI(true);
        recordAbortReasonHistogram(PaymentRequestMetrics.ABORT_REASON_MOJO_RENDERER_CLOSING);
    }

    /**
     * Called when the Mojo connection encounters an error.
     */
    @Override
    public void onConnectionError(MojoException e) {
        if (mClient == null) return;
        closeClient();
        closeUI(true);
        recordAbortReasonHistogram(PaymentRequestMetrics.ABORT_REASON_MOJO_CONNECTION_ERROR);
    }

    /**
     * Called after retrieving the list of payment instruments in an app.
     */
    @Override
    public void onInstrumentsReady(PaymentApp app, List<PaymentInstrument> instruments) {
        if (mClient == null) return;
        mPendingApps.remove(app);

        // Place the instruments into either "autofill" or "non-autofill" list to be displayed when
        // all apps have responded.
        if (instruments != null) {
            for (int i = 0; i < instruments.size(); i++) {
                PaymentInstrument instrument = instruments.get(i);
                Set<String> instrumentMethodNames = new HashSet<>(
                        instrument.getInstrumentMethodNames());
                instrumentMethodNames.retainAll(mMethodData.keySet());
                if (!instrumentMethodNames.isEmpty()) {
                    addPendingInstrument(instrument);
                } else {
                    instrument.dismissInstrument();
                }
            }
        }

        // Some payment apps still have not responded. Continue waiting for them.
        if (!mPendingApps.isEmpty()) return;

        if (disconnectIfNoPaymentMethodsSupported()) return;

        // Load the validation rules for each unique region code in the credit card billing
        // addresses and check for validity.
        Set<String> uniqueCountryCodes = new HashSet<>();
        for (int i = 0; i < mPendingAutofillInstruments.size(); ++i) {
            assert mPendingAutofillInstruments.get(i) instanceof AutofillPaymentInstrument;
            AutofillPaymentInstrument creditCard =
                    (AutofillPaymentInstrument) mPendingAutofillInstruments.get(i);

            String countryCode = AutofillAddress.getCountryCode(creditCard.getBillingAddress());
            if (!uniqueCountryCodes.contains(countryCode)) {
                uniqueCountryCodes.add(countryCode);
                PersonalDataManager.getInstance().loadRulesForRegion(countryCode);
            }

            // If there's a card on file with a valid number and a name, then
            // PaymentRequest.canMakePayment() returns true.
            mCanMakePayment |= creditCard.isValidCard();
        }

        // List order:
        // > Non-autofill instruments.
        // > Complete autofill instruments.
        // > Incomplete autofill instruments.
        Collections.sort(mPendingAutofillInstruments, COMPLETENESS_COMPARATOR);
        mPendingInstruments.addAll(mPendingAutofillInstruments);

        // Log the number of suggested credit cards.
        mJourneyLogger.setNumberOfSuggestionsShown(PaymentRequestJourneyLogger.SECTION_CREDIT_CARDS,
                mPendingAutofillInstruments.size());

        mPendingAutofillInstruments.clear();

        // Possibly pre-select the first instrument on the list.
        int selection = SectionInformation.NO_SELECTION;
        if (!mPendingInstruments.isEmpty()) {
            PaymentInstrument first = mPendingInstruments.get(0);
            if (first instanceof AutofillPaymentInstrument) {
                AutofillPaymentInstrument creditCard = (AutofillPaymentInstrument) first;
                if (creditCard.isComplete()) selection = 0;
            } else {
                // If a payment app is available, then PaymentRequest.canMakePayment() returns true.
                mCanMakePayment = true;
                selection = 0;
            }
        }

        CanMakePaymentQuery query = sCanMakePaymentQueries.get(mOrigin);
        if (query != null) query.setResponse(mCanMakePayment);

        // The list of payment instruments is ready to display.
        mPaymentMethodsSection = new SectionInformation(PaymentRequestUI.TYPE_PAYMENT_METHODS,
                selection, mPendingInstruments);

        mPendingInstruments.clear();

        updateInstrumentModifiedTotals();

        // UI has requested the full list of payment instruments. Provide it now.
        if (mPaymentInformationCallback != null) providePaymentInformation();
    }

    /**
     * If no payment methods are supported, disconnect from the client and return true.
     *
     * @return True if no payment methods are supported
     */
    private boolean disconnectIfNoPaymentMethodsSupported() {
        if (!isFinishedQueryingPaymentApps()) return false;

        boolean foundPaymentMethods = mPaymentMethodsSection != null
                && !mPaymentMethodsSection.isEmpty();
        boolean userCanAddCreditCard = mMerchantSupportsAutofillPaymentInstruments
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.NO_CREDIT_CARD_ABORT);

        if (!mArePaymentMethodsSupported
                || (getIsShowing() && !foundPaymentMethods && !userCanAddCreditCard)) {
            // All payment apps have responded, but none of them have instruments. It's possible to
            // add credit cards, but the merchant does not support them either. The payment request
            // must be rejected.
            disconnectFromClientWithDebugMessage("Requested payment methods have no instruments",
                    PaymentErrorReason.NOT_SUPPORTED);
            recordAbortReasonHistogram(mArePaymentMethodsSupported
                    ? PaymentRequestMetrics.ABORT_REASON_NO_MATCHING_PAYMENT_METHOD
                    : PaymentRequestMetrics.ABORT_REASON_NO_SUPPORTED_PAYMENT_METHOD);
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
            return true;
        }

        return false;
    }

    /** @return True after payment apps have been queried. */
    private boolean isFinishedQueryingPaymentApps() {
        return mPendingApps != null && mPendingApps.isEmpty() && mPendingInstruments.isEmpty();
    }

    /**
     * Saves the given instrument in either "autofill" or "non-autofill" list. The separation
     * enables placing autofill instruments on the bottom of the list.
     *
     * @param instrument The instrument to add to either "autofill" or "non-autofill" list.
     */
    private void addPendingInstrument(PaymentInstrument instrument) {
        if (instrument instanceof AutofillPaymentInstrument) {
            mPendingAutofillInstruments.add(instrument);
        } else {
            mPendingInstruments.add(instrument);
        }
    }

    /**
     * Called after retrieving instrument details.
     */
    @Override
    public void onInstrumentDetailsReady(String methodName, String stringifiedDetails) {
        if (mClient == null || mPaymentResponseHelper == null) return;

        // Record the payment method used to complete the transaction. If the payment method was an
        // Autofill credit card with an identifier, record its use.
        PaymentOption selectedPaymentMethod = mPaymentMethodsSection.getSelectedItem();
        if (selectedPaymentMethod instanceof AutofillPaymentInstrument) {
            if (!selectedPaymentMethod.getIdentifier().isEmpty()) {
                PersonalDataManager.getInstance().recordAndLogCreditCardUse(
                        selectedPaymentMethod.getIdentifier());
            }
            PaymentRequestMetrics.recordSelectedPaymentMethodHistogram(
                    PaymentRequestMetrics.SELECTED_METHOD_CREDIT_CARD);
        } else if (methodName.equals(ANDROID_PAY_METHOD_NAME)) {
            PaymentRequestMetrics.recordSelectedPaymentMethodHistogram(
                    PaymentRequestMetrics.SELECTED_METHOD_ANDROID_PAY);
        } else {
            PaymentRequestMetrics.recordSelectedPaymentMethodHistogram(
                    PaymentRequestMetrics.SELECTED_METHOD_OTHER_PAYMENT_APP);
        }

        recordSuccessFunnelHistograms("ReceivedInstrumentDetails");

        mPaymentResponseHelper.onInstrumentDetailsReceived(methodName, stringifiedDetails);
    }

    @Override
    public void onPaymentResponseReady(PaymentResponse response) {
        mClient.onPaymentResponse(response);
        mPaymentResponseHelper = null;
        PersonalDataManager.getInstance().cancelPendingAddressNormalizations();
    }

    /**
     * Called if unable to retrieve instrument details.
     */
    @Override
    public void onInstrumentDetailsError() {
        if (mClient == null) return;
        mUI.onPayButtonProcessingCancelled();
        mPaymentAppRunning = false;
    }

    @Override
    public void onFocusChanged(@PaymentRequestUI.DataType int dataType, boolean willFocus) {
        assert dataType == PaymentRequestUI.TYPE_SHIPPING_ADDRESSES;

        if (mShippingAddressesSection.getSelectedItem() == null) return;

        assert mShippingAddressesSection.getSelectedItem() instanceof AutofillAddress;
        AutofillAddress selectedAddress = (AutofillAddress) mShippingAddressesSection
                .getSelectedItem();

        // The label should only include the country if the view is focused.
        if (willFocus) {
            selectedAddress.setShippingAddressLabelWithCountry();
        } else {
            selectedAddress.setShippingAddressLabelWithoutCountry();
        }

        mUI.updateSection(PaymentRequestUI.TYPE_SHIPPING_ADDRESSES, mShippingAddressesSection);
    }

    /**
     * Closes the UI. If the client is still connected, then it's notified of UI hiding.
     *
     * @param immediateClose If true, then UI immediately closes. If false, the UI shows the error
     *                       message "There was an error processing your order." This message
     *                       implies that the merchant attempted to process the order, failed, and
     *                       called complete("fail") to notify the user. Therefore, this parameter
     *                       may be "false" only when called from
     *                       {@link PaymentRequestImpl#complete(int)}. All other callers should
     *                       always pass "true."
     */
    private void closeUI(boolean immediateClose) {
        if (mUI != null) {
            mUI.close(immediateClose, new Runnable() {
                @Override
                public void run() {
                    if (mClient != null) mClient.onComplete();
                    closeClient();
                }
            });
            mUI = null;
        }

        if (mPaymentMethodsSection != null) {
            for (int i = 0; i < mPaymentMethodsSection.getSize(); i++) {
                PaymentOption option = mPaymentMethodsSection.getItem(i);
                assert option instanceof PaymentInstrument;
                ((PaymentInstrument) option).dismissInstrument();
            }
            mPaymentMethodsSection = null;
        }

        if (mObservedTabModelSelector != null) {
            mObservedTabModelSelector.removeObserver(mSelectorObserver);
            mObservedTabModelSelector = null;
        }

        if (mObservedTabModel != null) {
            mObservedTabModel.removeObserver(mTabModelObserver);
            mObservedTabModel = null;
        }
    }

    private void closeClient() {
        if (mClient != null) mClient.close();
        mClient = null;
        setIsShowing(false);
    }

    private static boolean getIsShowing() {
        return sIsShowing;
    }

    private static void setIsShowing(boolean isShowing) {
        sIsShowing = isShowing;
    }

    @VisibleForTesting
    public static void setObserverForTest(PaymentRequestServiceObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }

    /**
     * Records specific histograms related to the different steps of a successful checkout.
     */
    private void recordSuccessFunnelHistograms(String funnelPart) {
        RecordHistogram.recordBooleanHistogram("PaymentRequest.CheckoutFunnel." + funnelPart, true);

        if (funnelPart.equals("Completed")) {
            mJourneyLogger.recordJourneyStatsHistograms("Completed");
        }
    }

    /**
     * Adds an entry to the aborted Payment Request histogram in the bucket corresponding to the
     * reason for aborting. Only records the initial reason for aborting, as some closing code calls
     * other closing code that can log too.
     */
    private void recordAbortReasonHistogram(int abortReason) {
        assert abortReason < PaymentRequestMetrics.ABORT_REASON_MAX;
        if (mHasRecordedAbortReason) return;

        mHasRecordedAbortReason = true;
        RecordHistogram.recordEnumeratedHistogram(
                "PaymentRequest.CheckoutFunnel.Aborted", abortReason,
                PaymentRequestMetrics.ABORT_REASON_MAX);

        if (abortReason == PaymentRequestMetrics.ABORT_REASON_ABORTED_BY_USER) {
            mJourneyLogger.recordJourneyStatsHistograms("UserAborted");
        } else {
            mJourneyLogger.recordJourneyStatsHistograms("OtherAborted");
        }
    }
}
