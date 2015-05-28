// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_

#include <stddef.h>
#include <string>

#include "base/basictypes.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"

namespace base {
class TimeDelta;
}

namespace autofill {

class AutofillMetrics {
 public:
  enum DeveloperEngagementMetric {
    // Parsed a form that is potentially autofillable.
    FILLABLE_FORM_PARSED = 0,
    // Parsed a form that is potentially autofillable and contains at least one
    // web developer-specified field type hint, a la
    // http://is.gd/whatwg_autocomplete
    FILLABLE_FORM_CONTAINS_TYPE_HINTS,
    NUM_DEVELOPER_ENGAGEMENT_METRICS,
  };

  // The action the user took to dismiss a dialog.
  enum DialogDismissalAction {
    DIALOG_ACCEPTED = 0,  // The user accepted, i.e. submitted, the dialog.
    DIALOG_CANCELED,      // The user canceled out of the dialog.
  };

  // The state of the Autofill dialog when it was dismissed.
  enum DialogDismissalState {
    // The user submitted with no data available to save.
    DEPRECATED_DIALOG_ACCEPTED_EXISTING_DATA,
    // The saved details to Online Wallet on submit.
    DIALOG_ACCEPTED_SAVE_TO_WALLET,
    // The saved details to the local Autofill database on submit.
    DIALOG_ACCEPTED_SAVE_TO_AUTOFILL,
    // The user submitted without saving any edited sections.
    DIALOG_ACCEPTED_NO_SAVE,
    // The user canceled with no edit UI showing.
    DIALOG_CANCELED_NO_EDITS,
    // The user canceled with edit UI showing, but no invalid fields.
    DIALOG_CANCELED_NO_INVALID_FIELDS,
    // The user canceled with at least one invalid field.
    DIALOG_CANCELED_WITH_INVALID_FIELDS,
    // The user canceled while the sign-in form was showing.
    DIALOG_CANCELED_DURING_SIGNIN,
    // The user submitted using data already stored in Wallet.
    DIALOG_ACCEPTED_EXISTING_WALLET_DATA,
    // The user submitted using data already stored in Autofill.
    DIALOG_ACCEPTED_EXISTING_AUTOFILL_DATA,
    NUM_DIALOG_DISMISSAL_STATES
  };

  // The initial state of user that's interacting with a freshly shown Autofill
  // dialog.
  enum DialogInitialUserStateMetric {
    // Could not determine the user's state due to failure to communicate with
    // the Wallet server.
    DIALOG_USER_STATE_UNKNOWN = 0,
    // Not signed in, no verified Autofill profiles.
    DIALOG_USER_NOT_SIGNED_IN_NO_AUTOFILL,
    // Not signed in, has verified Autofill profiles.
    DIALOG_USER_NOT_SIGNED_IN_HAS_AUTOFILL,
    // Signed in, no Wallet items, no verified Autofill profiles.
    DIALOG_USER_SIGNED_IN_NO_WALLET_NO_AUTOFILL,
    // Signed in, no Wallet items, has verified Autofill profiles.
    DIALOG_USER_SIGNED_IN_NO_WALLET_HAS_AUTOFILL,
    // Signed in, has Wallet items, no verified Autofill profiles.
    DIALOG_USER_SIGNED_IN_HAS_WALLET_NO_AUTOFILL,
    // Signed in, has Wallet items, has verified Autofill profiles.
    DIALOG_USER_SIGNED_IN_HAS_WALLET_HAS_AUTOFILL,
    NUM_DIALOG_INITIAL_USER_STATE_METRICS
  };

  // Events related to the Autofill popup shown in a requestAutocomplete
  // dialog.
  enum DialogPopupEvent {
    // An Autofill popup was shown.
    DIALOG_POPUP_SHOWN = 0,
    // The user chose to fill the form with a suggestion from the popup.
    DIALOG_POPUP_FORM_FILLED,
    NUM_DIALOG_POPUP_EVENTS
  };

  // For measuring the frequency of security warnings or errors that can come
  // up as part of the requestAutocomplete flow.
  enum DialogSecurityMetric {
    // Baseline metric: The dialog was shown.
    SECURITY_METRIC_DIALOG_SHOWN = 0,
    // Credit card requested over non-secure protocol.
    SECURITY_METRIC_CREDIT_CARD_OVER_HTTP,
    // Autocomplete data requested from a frame hosted on an origin not matching
    // the main frame's origin.
    SECURITY_METRIC_CROSS_ORIGIN_FRAME,
    NUM_DIALOG_SECURITY_METRICS
  };

  // For measuring how users are interacting with the Autofill dialog UI.
  enum DialogUiEvent {
    // Baseline metric: The dialog was shown.
    DIALOG_UI_SHOWN = 0,

    // Dialog dismissal actions:
    DIALOG_UI_ACCEPTED,
    DIALOG_UI_CANCELED,

    // Selections within the account switcher:
    // Switched from a Wallet account to local Autofill data.
    DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_TO_AUTOFILL,
    // Switched from local Autofill data to a Wallet account.
    DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_TO_WALLET,
    // Switched from one Wallet account to another one.
    DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_WALLET_ACCOUNT,

    // The sign-in UI was shown.
    DIALOG_UI_SIGNIN_SHOWN,

    // Selecting a different item from a suggestion menu dropdown:
    DEPRECATED_DIALOG_UI_EMAIL_SELECTED_SUGGESTION_CHANGED,
    DIALOG_UI_BILLING_SELECTED_SUGGESTION_CHANGED,
    DIALOG_UI_CC_BILLING_SELECTED_SUGGESTION_CHANGED,
    DIALOG_UI_SHIPPING_SELECTED_SUGGESTION_CHANGED,
    DIALOG_UI_CC_SELECTED_SUGGESTION_CHANGED,

    // Showing the editing UI for a section of the dialog:
    DEPRECATED_DIALOG_UI_EMAIL_EDIT_UI_SHOWN,
    DEPRECATED_DIALOG_UI_BILLING_EDIT_UI_SHOWN,
    DEPRECATED_DIALOG_UI_CC_BILLING_EDIT_UI_SHOWN,
    DEPRECATED_DIALOG_UI_SHIPPING_EDIT_UI_SHOWN,
    DEPRECATED_DIALOG_UI_CC_EDIT_UI_SHOWN,

    // Adding a new item in a section of the dialog:
    DEPRECATED_DIALOG_UI_EMAIL_ITEM_ADDED,
    DIALOG_UI_BILLING_ITEM_ADDED,
    DIALOG_UI_CC_BILLING_ITEM_ADDED,
    DIALOG_UI_SHIPPING_ITEM_ADDED,
    DIALOG_UI_CC_ITEM_ADDED,

    // Also an account switcher menu item. The user selected the
    // "add account" option.
    DIALOG_UI_ACCOUNT_CHOOSER_TRIED_TO_ADD_ACCOUNT,

    NUM_DIALOG_UI_EVENTS
  };

  enum InfoBarMetric {
    INFOBAR_SHOWN = 0,  // We showed an infobar, e.g. prompting to save credit
                        // card info.
    INFOBAR_ACCEPTED,   // The user explicitly accepted the infobar.
    INFOBAR_DENIED,     // The user explicitly denied the infobar.
    INFOBAR_IGNORED,    // The user completely ignored the infobar (logged on
                        // tab close).
    NUM_INFO_BAR_METRICS,
  };

  // Metrics measuring how well we predict field types.  Exactly three such
  // metrics are logged for each fillable field in a submitted form: for
  // the heuristic prediction, for the crowd-sourced prediction, and for the
  // overall prediction.
  enum FieldTypeQualityMetric {
    TYPE_UNKNOWN = 0,  // Offered no prediction.
    TYPE_MATCH,        // Predicted correctly.
    TYPE_MISMATCH,     // Predicted incorrectly.
    NUM_FIELD_TYPE_QUALITY_METRICS,
  };

  // Each of these is logged at most once per query to the server, which in turn
  // occurs at most once per page load.
  enum ServerQueryMetric {
    QUERY_SENT = 0,           // Sent a query to the server.
    QUERY_RESPONSE_RECEIVED,  // Received a response.
    QUERY_RESPONSE_PARSED,    // Successfully parsed the server response.

    // The response was parseable, but provided no improvements relative to our
    // heuristics.
    QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS,

    // Our heuristics detected at least one auto-fillable field, and the server
    // response overrode the type of at least one field.
    QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS,

    // Our heuristics did not detect any auto-fillable fields, but the server
    // response did detect at least one.
    QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS,
    NUM_SERVER_QUERY_METRICS,
  };

  // Logs usage of "Scan card" control item.
  enum ScanCreditCardPromptMetric {
    // "Scan card" was presented to the user.
    SCAN_CARD_ITEM_SHOWN,
    // "Scan card" was selected by the user.
    SCAN_CARD_ITEM_SELECTED,
    // The user selected something in the dropdown besides "scan card".
    SCAN_CARD_OTHER_ITEM_SELECTED,
    NUM_SCAN_CREDIT_CARD_PROMPT_METRICS,
  };

  // Each of these metrics is logged only for potentially autofillable forms,
  // i.e. forms with at least three fields, etc.
  // These are used to derive certain "user happiness" metrics.  For example, we
  // can compute the ratio (USER_DID_EDIT_AUTOFILLED_FIELD / USER_DID_AUTOFILL)
  // to see how often users have to correct autofilled data.
  enum UserHappinessMetric {
    // Loaded a page containing forms.
    FORMS_LOADED,
    // Submitted a fillable form -- i.e. one with at least three field values
    // that match the user's stored Autofill data -- and all matching fields
    // were autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_ALL,
    // Submitted a fillable form and some (but not all) matching fields were
    // autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME,
    // Submitted a fillable form and no fields were autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_NONE,
    // Submitted a non-fillable form.
    SUBMITTED_NON_FILLABLE_FORM,

    // User manually filled one of the form fields.
    USER_DID_TYPE,
    // We showed a popup containing Autofill suggestions.
    SUGGESTIONS_SHOWN,
    // Same as above, but only logged once per page load.
    SUGGESTIONS_SHOWN_ONCE,
    // User autofilled at least part of the form.
    USER_DID_AUTOFILL,
    // Same as above, but only logged once per page load.
    USER_DID_AUTOFILL_ONCE,
    // User edited a previously autofilled field.
    USER_DID_EDIT_AUTOFILLED_FIELD,
    // Same as above, but only logged once per page load.
    USER_DID_EDIT_AUTOFILLED_FIELD_ONCE,
    NUM_USER_HAPPINESS_METRICS,
  };

  // Form Events for autofill.
  // These events are triggered separetly for address and credit card forms.
  enum FormEvent {
    // User interacted with a field of this kind of form. Logged only once per
    // page load.
    FORM_EVENT_INTERACTED_ONCE = 0,
    // A dropdown with suggestions was shown.
    FORM_EVENT_SUGGESTIONS_SHOWN,
    // Same as above, but recoreded only once per page load.
    FORM_EVENT_SUGGESTIONS_SHOWN_ONCE,
    // A local suggestion was used to fill the form.
    FORM_EVENT_LOCAL_SUGGESTION_FILLED,
    // A server suggestion was used to fill the form.
    // When dealing with credit cards, this means a full server card was used
    // to fill.
    FORM_EVENT_SERVER_SUGGESTION_FILLED,
    // A masked server card suggestion was used to fill the form.
    FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED,
    // A suggestion was used to fill the form. The origin type (local or server
    // or masked server card) of the first selected within a page load will
    // determine which of the following two will be fired.
    FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
    FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE,
    FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
    // A form was submitted. Depending on the user filling a local, server,
    // masked server card or no suggestion one of the following will be
    // triggered. Only one of the following four will be triggered per page
    // load.
    FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
    FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE,
    FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE,
    FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
    // A masked server card suggestion was selected to fill the form.
    FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
    // Same as above but only triggered once per page load.
    FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
    // An autofillable form is about to be submitted. If the submission is not
    // interrupted by JavaScript, the "form submitted" events above will also be
    // logged. Depending on the user filling a local, server, masked server card
    // or no suggestion one of the following will be triggered, at most once per
    // page load.
    FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE,
    FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE,
    FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE,
    FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,

    NUM_FORM_EVENTS,
  };

  // Events related to the Unmask Credit Card Prompt.
  enum UnmaskPromptEvent {
    // The prompt was shown.
    UNMASK_PROMPT_SHOWN = 0,
    // The prompt was closed without attempting to unmask the card.
    UNMASK_PROMPT_CLOSED_NO_ATTEMPTS,
    // The prompt was closed without unmasking the card, but with at least
    // one attempt. The last failure was retriable.
    UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE,
    // The prompt was closed without unmasking the card, but with at least
    // one attempt. The last failure was non retriable.
    UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE,
    // Successfully unmasked the card in the first attempt.
    UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT,
    // Successfully unmasked the card after retriable failures.
    UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS,
    // Saved the card locally (masked card was upgraded to a full card).
    UNMASK_PROMPT_SAVED_CARD_LOCALLY,
    // User chose to opt in (checked the checkbox when it was empty).
    // Only logged if there was an attempt to unmask.
    UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN,
    // User did not opt in when he had the chance (left the checkbox unchecked).
    // Only logged if there was an attempt to unmask.
    UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN,
    // User chose to opt out (unchecked the checkbox when it was check).
    // Only logged if there was an attempt to unmask.
    UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT,
    // User did not opt out when he had a chance (left the checkbox checked).
    // Only logged if there was an attempt to unmask.
    UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT,
    // The prompt was closed while chrome was unmasking the card (user pressed
    // verify and we were waiting for the server response).
    UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING,
    NUM_UNMASK_PROMPT_EVENTS,
  };

  // Possible results of the GetRealPan call.
  enum GetRealPanResult {
    // Request succeeded.
    GET_REAL_PAN_RESULT_SUCCESS = 0,
    // Request failed; try again.
    GET_REAL_PAN_RESULT_TRY_AGAIN_FAILURE,
    // Request failed; don't try again.
    GET_REAL_PAN_RESULT_PERMANENT_FAILURE,
    // Unable to connect to Wallet servers.
    GET_REAL_PAN_RESULT_NETWORK_ERROR,
    NUM_GET_REAL_PAN_RESULTS,
  };

  // For measuring the network request time of various Wallet API calls. See
  // WalletClient::RequestType.
  enum WalletApiCallMetric {
    UNKNOWN_API_CALL,  // Catch all. Should never be used.
    ACCEPT_LEGAL_DOCUMENTS,
    AUTHENTICATE_INSTRUMENT,
    GET_FULL_WALLET,
    GET_WALLET_ITEMS,
    SAVE_TO_WALLET,
    NUM_WALLET_API_CALLS
  };

  // For measuring the frequency of errors while communicating with the Wallet
  // server.
  enum WalletErrorMetric {
    // Baseline metric: Issued a request to the Wallet server.
    WALLET_ERROR_BASELINE_ISSUED_REQUEST = 0,
    // A fatal error occured while communicating with the Wallet server. This
    // value has been deprecated.
    WALLET_FATAL_ERROR_DEPRECATED,
    // Received a malformed response from the Wallet server.
    WALLET_MALFORMED_RESPONSE,
    // A network error occured while communicating with the Wallet server.
    WALLET_NETWORK_ERROR,
    // The request was malformed.
    WALLET_BAD_REQUEST,
    // Risk deny, unsupported country, or account closed.
    WALLET_BUYER_ACCOUNT_ERROR,
    // Unknown server side error.
    WALLET_INTERNAL_ERROR,
    // API call had missing or invalid parameters.
    WALLET_INVALID_PARAMS,
    // Online Wallet is down.
    WALLET_SERVICE_UNAVAILABLE,
    // User needs make a cheaper transaction or not use Online Wallet.
    WALLET_SPENDING_LIMIT_EXCEEDED,
    // The server API version of the request is no longer supported.
    WALLET_UNSUPPORTED_API_VERSION,
    // Catch all error type.
    WALLET_UNKNOWN_ERROR,
    // The merchant has been blacklisted for Online Wallet due to some manner
    // of compliance violation.
    WALLET_UNSUPPORTED_MERCHANT,
    // Buyer Legal Address has a country which is unsupported by Wallet.
    WALLET_BUYER_LEGAL_ADDRESS_NOT_SUPPORTED,
    // Wallet's Know Your Customer(KYC) action is pending/failed for this user.
    WALLET_UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS,
    // Chrome version is unsupported or provided API key not allowed.
    WALLET_UNSUPPORTED_USER_AGENT_OR_API_KEY,
    NUM_WALLET_ERROR_METRICS
  };

  // For measuring the frequency of "required actions" returned by the Wallet
  // server.  This is similar to the autofill::wallet::RequiredAction enum;
  // but unlike that enum, the values in this one must remain constant over
  // time, so that the metrics can be consistently interpreted on the
  // server-side.
  enum WalletRequiredActionMetric {
    // Baseline metric: Issued a request to the Wallet server.
    WALLET_REQUIRED_ACTION_BASELINE_ISSUED_REQUEST = 0,
    // Values from the autofill::wallet::RequiredAction enum:
    UNKNOWN_REQUIRED_ACTION,  // Catch all type.
    GAIA_AUTH,
    PASSIVE_GAIA_AUTH,
    SETUP_WALLET,
    ACCEPT_TOS,
    UPDATE_EXPIRATION_DATE,
    UPGRADE_MIN_ADDRESS,
    CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS,
    VERIFY_CVV,
    INVALID_FORM_FIELD,
    REQUIRE_PHONE_NUMBER,
    NUM_WALLET_REQUIRED_ACTIONS
  };

  // For measuring the increased load on the Autofill server if the restriction
  // on querying for password forms with fewer than 3 fields were omitted.
  enum PasswordFormQueryVolumeMetric {
    NEW_PASSWORD_QUERY,
    CURRENT_QUERY,
    NUM_PASSWORD_FORM_QUERY_VOLUME_METRIC,
  };

  static void LogCreditCardInfoBarMetric(InfoBarMetric metric);
  static void LogScanCreditCardPromptMetric(ScanCreditCardPromptMetric metric);

  // Should be called when credit card scan is finished. |duration| should be
  // the time elapsed between launching the credit card scanner and getting back
  // the result. |completed| should be true if a credit card was scanned, false
  // if the scan was cancelled.
  static void LogScanCreditCardCompleted(const base::TimeDelta& duration,
                                         bool completed);

  static void LogDeveloperEngagementMetric(DeveloperEngagementMetric metric);

  static void LogHeuristicTypePrediction(FieldTypeQualityMetric metric,
                                         ServerFieldType field_type);
  static void LogOverallTypePrediction(FieldTypeQualityMetric metric,
                                       ServerFieldType field_type);
  static void LogServerTypePrediction(FieldTypeQualityMetric metric,
                                      ServerFieldType field_type);

  static void LogServerQueryMetric(ServerQueryMetric metric);

  static void LogUserHappinessMetric(UserHappinessMetric metric);

  // Logs |state| to the dismissal states histogram.
  static void LogDialogDismissalState(DialogDismissalState state);

  // This should be called as soon as the user's signed-in status and Wallet
  // item count is known.  Records that a user starting out in |user_state| is
  // interacting with a dialog.
  static void LogDialogInitialUserState(DialogInitialUserStateMetric user_type);

  // Logs the time elapsed between the dialog being shown and when it is ready
  // for user interaction.
  static void LogDialogLatencyToShow(const base::TimeDelta& duration);

  // Logs |event| to the popup events histogram.
  static void LogDialogPopupEvent(DialogPopupEvent event);

  // Logs |metric| to the security metrics histogram.
  static void LogDialogSecurityMetric(DialogSecurityMetric metric);

  // This should be called when the Autofill dialog is closed.  |duration|
  // should be the time elapsed between the dialog being shown and it being
  // closed.  |dismissal_action| should indicate whether the user dismissed
  // the dialog by submitting the form data or by canceling.
  static void LogDialogUiDuration(const base::TimeDelta& duration,
                                  DialogDismissalAction dismissal_action);

  // Logs |event| to the UI events histogram.
  static void LogDialogUiEvent(DialogUiEvent event);

  // Logs |event| to the unmask prompt events histogram.
  static void LogUnmaskPromptEvent(UnmaskPromptEvent event);

  // Logs the time elapsed between the unmask prompt being shown and it
  // being closed.
  static void LogUnmaskPromptEventDuration(const base::TimeDelta& duration,
                                           UnmaskPromptEvent close_event);

  // Logs the time elapsed between the user clicking Verify and
  // hitting cancel when abandoning a pending unmasking operation
  // (aka GetRealPan).
  static void LogTimeBeforeAbandonUnmasking(const base::TimeDelta& duration);

  // Logs |result| to the get real pan result histogram.
  static void LogRealPanResult(AutofillClient::GetRealPanResult result);

  // Logs |result| to duration of the GetRealPan RPC.
  static void LogRealPanDuration(const base::TimeDelta& duration,
                                 AutofillClient::GetRealPanResult result);

  // Logs |result| to the get real pan result histogram.
  static void LogUnmaskingDuration(const base::TimeDelta& duration,
                                   AutofillClient::GetRealPanResult result);

  // Logs |metric| to the Wallet errors histogram.
  static void LogWalletErrorMetric(WalletErrorMetric metric);

  // Logs the network request time of Wallet API calls.
  static void LogWalletApiCallDuration(WalletApiCallMetric metric,
                                       const base::TimeDelta& duration);

  // Logs that the Wallet API call corresponding to |metric| was malformed.
  static void LogWalletMalformedResponseMetric(WalletApiCallMetric metric);

  // Logs |required_action| to the required actions histogram.
  static void LogWalletRequiredActionMetric(
      WalletRequiredActionMetric required_action);

  // Logs HTTP response codes recieved by wallet client.
  static void LogWalletResponseCode(int response_code);

  // This should be called when a form that has been Autofilled is submitted.
  // |duration| should be the time elapsed between form load and submission.
  static void LogFormFillDurationFromLoadWithAutofill(
      const base::TimeDelta& duration);

  // This should be called when a fillable form that has not been Autofilled is
  // submitted.  |duration| should be the time elapsed between form load and
  // submission.
  static void LogFormFillDurationFromLoadWithoutAutofill(
      const base::TimeDelta& duration);

  // This should be called when a form that has been Autofilled is submitted.
  // |duration| should be the time elapsed between the initial form interaction
  // and submission.
  static void LogFormFillDurationFromInteractionWithAutofill(
      const base::TimeDelta& duration);

  // This should be called when a fillable form that has not been Autofilled is
  // submitted.  |duration| should be the time elapsed between the initial form
  // interaction and submission.
  static void LogFormFillDurationFromInteractionWithoutAutofill(
      const base::TimeDelta& duration);

  // This should be called each time a page containing forms is loaded.
  static void LogIsAutofillEnabledAtPageLoad(bool enabled);

  // This should be called each time a new profile is launched.
  static void LogIsAutofillEnabledAtStartup(bool enabled);

  // This should be called each time a new profile is launched.
  static void LogStoredProfileCount(size_t num_profiles);

  // Log the number of profiles available when an autofillable form is
  // submitted.
  static void LogNumberOfProfilesAtAutofillableFormSubmission(
      size_t num_profiles);

  // Log the number of Autofill suggestions presented to the user when filling a
  // form.
  static void LogAddressSuggestionsCount(size_t num_suggestions);

  // Log password form query: current and if one-to-two fields password forms
  // were allowed.
  static void LogPasswordFormQueryVolume(PasswordFormQueryVolumeMetric metric);

  // Utility to autofill form events in the relevant histograms depending on
  // the presence of server and/or local data.
  class FormEventLogger {
   public:
    FormEventLogger(bool is_for_credit_card);

    inline void set_is_server_data_available(bool is_server_data_available) {
      is_server_data_available_ = is_server_data_available;
    }

    inline void set_is_local_data_available(bool is_local_data_available) {
      is_local_data_available_ = is_local_data_available;
    }

    void OnDidInteractWithAutofillableForm();

    void OnDidShowSuggestions();

    void OnDidSelectMaskedServerCardSuggestion();

    // In case of masked cards, caller must make sure this gets called before
    // the card is upgraded to a full card.
    void OnDidFillSuggestion(const CreditCard& credit_card);

    void OnDidFillSuggestion(const AutofillProfile& profile);

    void OnWillSubmitForm();

    void OnFormSubmitted();

   private:
    void Log(FormEvent event) const;

    bool is_for_credit_card_;
    bool is_server_data_available_;
    bool is_local_data_available_;
    bool has_logged_interacted_;
    bool has_logged_suggestions_shown_;
    bool has_logged_masked_server_card_suggestion_selected_;
    bool has_logged_suggestion_filled_;
    bool has_logged_will_submit_;
    bool has_logged_submitted_;
    bool logged_suggestion_filled_was_server_data_;
    bool logged_suggestion_filled_was_masked_server_card_;
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AutofillMetrics);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_
