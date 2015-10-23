// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/wallet/real_pan_wallet_client.h"
#include "components/autofill/core/common/form_data.h"

// This define protects some debugging code (see DumpAutofillData). This
// is here to make it easier to delete this code when the test is complete,
// and to prevent adding the code on mobile where there is no desktop (the
// debug dump file is written to the desktop) or command-line flags to enable.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#define ENABLE_FORM_DEBUG_DUMP
#endif

class ChromeUIWebViewWebTest;
class ChromeWKWebViewWebTest;

namespace gfx {
class Rect;
class RectF;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {

class AutofillDataModel;
class AutofillDownloadManager;
class AutofillExternalDelegate;
class AutofillField;
class AutofillClient;
class AutofillManagerTestDelegate;
class AutofillProfile;
class AutofillType;
class CreditCard;
class FormStructureBrowserTest;
template <class WebTestT> class FormStructureBrowserTestIos;

struct FormData;
struct FormFieldData;

// Manages saving and restoring the user's personal information entered into web
// forms. One per frame; owned by the AutofillDriver.
class AutofillManager : public AutofillDownloadManager::Observer,
                        public CardUnmaskDelegate,
                        public wallet::RealPanWalletClient::Delegate {
 public:
  enum AutofillDownloadManagerState {
    ENABLE_AUTOFILL_DOWNLOAD_MANAGER,
    DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
  };

  // Registers our Enable/Disable Autofill pref.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  static void MigrateUserPrefs(PrefService* prefs);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  AutofillManager(AutofillDriver* driver,
                  AutofillClient* client,
                  const std::string& app_locale,
                  AutofillDownloadManagerState enable_download_manager);
  ~AutofillManager() override;

  // Sets an external delegate.
  void SetExternalDelegate(AutofillExternalDelegate* delegate);

  void ShowAutofillSettings();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Whether the |field| should show an entry to prompt the user to give Chrome
  // access to the user's address book.
  bool ShouldShowAccessAddressBookSuggestion(const FormData& form,
                                             const FormFieldData& field);

  // If Chrome has not prompted for access to the user's address book, the
  // method prompts the user for permission and blocks the process. Otherwise,
  // this method has no effect. The return value reflects whether the user was
  // prompted with a modal dialog.
  bool AccessAddressBook();

  // The access Address Book prompt was shown for a form.
  void ShowedAccessAddressBookPrompt();

  // The number of times that the access address book prompt was shown.
  int AccessAddressBookPromptCount();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  // Whether the |field| should show an entry to scan a credit card.
  virtual bool ShouldShowScanCreditCard(const FormData& form,
                                        const FormFieldData& field);

  // Called from our external delegate so they cannot be private.
  virtual void FillOrPreviewForm(AutofillDriver::RendererFormDataAction action,
                                 int query_id,
                                 const FormData& form,
                                 const FormFieldData& field,
                                 int unique_id);
  virtual void FillCreditCardForm(int query_id,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const CreditCard& credit_card);
  void DidShowSuggestions(bool is_new_popup,
                          const FormData& form,
                          const FormFieldData& field);
  void OnDidFillAutofillFormData(const base::TimeTicks& timestamp);
  void OnDidPreviewAutofillFormData();

  // Returns true if the value/identifier is deletable. Fills out
  // |title| and |body| with relevant user-facing text.
  bool GetDeletionConfirmationText(const base::string16& value,
                                   int identifier,
                                   base::string16* title,
                                   base::string16* body);

  // Remove the credit card or Autofill profile that matches |unique_id|
  // from the database. Returns true if deletion is allowed.
  bool RemoveAutofillProfileOrCreditCard(int unique_id);

  // Remove the specified Autocomplete entry.
  void RemoveAutocompleteEntry(const base::string16& name,
                               const base::string16& value);

  // Returns true when the Wallet card unmask prompt is being displayed.
  bool IsShowingUnmaskPrompt();

  // Returns the present form structures seen by Autofill manager.
  const std::vector<FormStructure*>& GetFormStructures();

  // Happens when the autocomplete dialog runs its callback when being closed.
  void RequestAutocompleteDialogClosed();

  AutofillClient* client() const { return client_; }

  const std::string& app_locale() const { return app_locale_; }

  // Only for testing.
  void SetTestDelegate(AutofillManagerTestDelegate* delegate);

  void OnFormsSeen(const std::vector<FormData>& forms,
                   const base::TimeTicks& timestamp);

  // IMPORTANT: On iOS, this method is called when the form is submitted,
  // immediately before OnFormSubmitted() is called. Do not assume that
  // OnWillSubmitForm() will run before the form submits.
  // TODO(mathp): Revisit this and use a single method to track form submission.
  //
  // Processes the about-to-be-submitted |form|, uploading the possible field
  // types for the submitted fields to the crowdsourcing server. Returns false
  // if this form is not relevant for Autofill.
  bool OnWillSubmitForm(const FormData& form, const base::TimeTicks& timestamp);

  // Processes the submitted |form|, saving any new Autofill data to the user's
  // personal profile. Returns false if this form is not relevant for Autofill.
  bool OnFormSubmitted(const FormData& form);

  void OnTextFieldDidChange(const FormData& form,
                            const FormFieldData& field,
                            const base::TimeTicks& timestamp);

  // The |bounding_box| is a window relative value.
  void OnQueryFormFieldAutofill(int query_id,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box);
  void OnDidEndTextFieldEditing();
  void OnHidePopup();
  void OnSetDataList(const std::vector<base::string16>& values,
                     const std::vector<base::string16>& labels);

  // Try to label password fields and upload |form|. This differs from
  // OnFormSubmitted() in a few ways.
  //   - This function will only label the first <input type="password"> field
  //     as |password_type|. Other fields will stay unlabeled, as they
  //     should have been labeled during the upload for OnFormSubmitted().
  //   - If the |username_field| attribute is nonempty, we will additionally
  //     label the field with that name as the username field.
  //   - This function does not assume that |form| is being uploaded during
  //     the same browsing session as it was originally submitted (as we may
  //     not have the necessary information to classify the form at that time)
  //     so it bypasses the cache and doesn't log the same quality UMA metrics.
  // |login_form_signature| may be empty. It is non-empty when the user fills
  // and submits a login form using a generated password. In this case,
  // |login_form_signature| should be set to the submitted form's signature.
  // Note that in this case, |form.FormSignature()| gives the signature for the
  // registration form on which the password was generated, rather than the
  // submitted form's signature.
  virtual bool UploadPasswordForm(const FormData& form,
                                  const base::string16& username_field,
                                  const ServerFieldType& pasword_type,
                                  const std::string& login_form_signature);

  // Resets cache.
  virtual void Reset();

  // Returns the value of the AutofillEnabled pref.
  virtual bool IsAutofillEnabled() const;

 protected:
  // Test code should prefer to use this constructor.
  AutofillManager(AutofillDriver* driver,
                  AutofillClient* client,
                  PersonalDataManager* personal_data);

  // Uploads the form data to the Autofill server.
  virtual void UploadFormData(const FormStructure& submitted_form);

  // Logs quality metrics for the |submitted_form| and uploads the form data
  // to the crowdsourcing server, if appropriate.
  virtual void UploadFormDataAsyncCallback(
      const FormStructure* submitted_form,
      const base::TimeTicks& load_time,
      const base::TimeTicks& interaction_time,
      const base::TimeTicks& submission_time);

  // Maps suggestion backend ID to and from an integer identifying it. Two of
  // these intermediate integers are packed by MakeFrontendID to make the IDs
  // that this class generates for the UI and for IPC.
  virtual int BackendIDToInt(const std::string& backend_id) const;
  virtual std::string IntToBackendID(int int_id) const;

  // Methods for packing and unpacking credit card and profile IDs for sending
  // and receiving to and from the renderer process.
  int MakeFrontendID(const std::string& cc_backend_id,
                     const std::string& profile_backend_id) const;
  void SplitFrontendID(int frontend_id,
                       std::string* cc_backend_id,
                       std::string* profile_backend_id) const;

  ScopedVector<FormStructure>* form_structures() { return &form_structures_; }

  // Exposed for testing.
  AutofillExternalDelegate* external_delegate() {
    return external_delegate_;
  }

 private:
  // AutofillDownloadManager::Observer:
  void OnLoadedServerPredictions(const std::string& response_xml) override;

  // CardUnmaskDelegate:
  void OnUnmaskResponse(const UnmaskResponse& response) override;
  void OnUnmaskPromptClosed() override;

  // wallet::RealPanWalletClient::Delegate:
  IdentityProvider* GetIdentityProvider() override;
  void OnDidGetRealPan(AutofillClient::GetRealPanResult result,
                       const std::string& real_pan) override;

  // Returns false if Autofill is disabled or if no Autofill data is available.
  bool RefreshDataModels();

  // Returns true if the unique_id refers to a credit card and false if
  // it refers to a profile.
  bool IsCreditCard(int unique_id);

  // Gets the profile referred by |unique_id|. Returns true if the profile
  // exists.
  bool GetProfile(int unique_id, const AutofillProfile** profile);

  // Gets the credit card referred by |unique_id|. Returns true if the credit
  // card exists.
  bool GetCreditCard(int unique_id, const CreditCard** credit_card);

  // Determines whether a fill on |form| initiated from |field| will wind up
  // filling a credit card number. This is useful to determine if we will need
  // to unmask a card.
  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& field);

  // Fills or previews the credit card form.
  // Assumes the form and field are valid.
  void FillOrPreviewCreditCardForm(
      AutofillDriver::RendererFormDataAction action,
      int query_id,
      const FormData& form,
      const FormFieldData& field,
      const CreditCard& credit_card);

  // Fills or previews the profile form.
  // Assumes the form and field are valid.
  void FillOrPreviewProfileForm(AutofillDriver::RendererFormDataAction action,
                                int query_id,
                                const FormData& form,
                                const FormFieldData& field,
                                const AutofillProfile& profile);

  // Fills or previews |data_model| in the |form|.
  void FillOrPreviewDataModelForm(AutofillDriver::RendererFormDataAction action,
                                  int query_id,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const AutofillDataModel& data_model,
                                  bool is_credit_card);

  // Creates a FormStructure using the FormData received from the renderer. Will
  // return an empty scoped_ptr if the data should not be processed for upload
  // or personal data.
  scoped_ptr<FormStructure> ValidateSubmittedForm(const FormData& form);

  // Fills |form_structure| cached element corresponding to |form|.
  // Returns false if the cached element was not found.
  bool FindCachedForm(const FormData& form,
                      FormStructure** form_structure) const WARN_UNUSED_RESULT;

  // Fills |form_structure| and |autofill_field| with the cached elements
  // corresponding to |form| and |field|.  This might have the side-effect of
  // updating the cache.  Returns false if the |form| is not autofillable, or if
  // it is not already present in the cache and the cache is full.
  bool GetCachedFormAndField(const FormData& form,
                             const FormFieldData& field,
                             FormStructure** form_structure,
                             AutofillField** autofill_field) WARN_UNUSED_RESULT;

  // Returns the field corresponding to |form| and |field| that can be
  // autofilled. Returns NULL if the field cannot be autofilled.
  AutofillField* GetAutofillField(const FormData& form,
                                  const FormFieldData& field)
      WARN_UNUSED_RESULT;

  // Re-parses |live_form| and adds the result to |form_structures_|.
  // |cached_form| should be a pointer to the existing version of the form, or
  // NULL if no cached version exists.  The updated form is then written into
  // |updated_form|.  Returns false if the cache could not be updated.
  bool UpdateCachedForm(const FormData& live_form,
                        const FormStructure* cached_form,
                        FormStructure** updated_form) WARN_UNUSED_RESULT;

  // Returns a list of values from the stored profiles that match |type| and the
  // value of |field| and returns the labels of the matching profiles. |labels|
  // is filled with the Profile label.
  std::vector<Suggestion> GetProfileSuggestions(
      const FormStructure& form,
      const FormFieldData& field,
      const AutofillField& autofill_field) const;

  // Returns a list of values from the stored credit cards that match |type| and
  // the value of |field| and returns the labels of the matching credit cards.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const FormFieldData& field,
      const AutofillType& type) const;

  // Parses the forms using heuristic matching and querying the Autofill server.
  void ParseForms(const std::vector<FormData>& forms);

  // Imports the form data, submitted by the user, into |personal_data_|.
  void ImportFormData(const FormStructure& submitted_form);

  // If |initial_interaction_timestamp_| is unset or is set to a later time than
  // |interaction_timestamp|, updates the cached timestamp.  The latter check is
  // needed because IPC messages can arrive out of order.
  void UpdateInitialInteractionTimestamp(
      const base::TimeTicks& interaction_timestamp);

  // Shared code to determine if |form| should be uploaded.
  bool ShouldUploadForm(const FormStructure& form);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Emits a UMA metric indicating whether the accepted Autofill suggestion is
  // from the Mac Address Book.
  void EmitIsFromAddressBookMetric(int unique_id);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

#ifdef ENABLE_FORM_DEBUG_DUMP
  // Dumps the cached forms to a file on disk.
  void DumpAutofillData(bool imported_cc) const;
#endif

  // Provides driver-level context to the shared code of the component. Must
  // outlive this object.
  AutofillDriver* driver_;

  AutofillClient* const client_;

  // Handles real PAN requests.
  wallet::RealPanWalletClient real_pan_client_;

  std::string app_locale_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_;

  std::list<std::string> autofilled_form_signatures_;

  // Handles queries and uploads to Autofill servers. Will be NULL if
  // the download manager functionality is disabled.
  scoped_ptr<AutofillDownloadManager> download_manager_;

  // Handles single-field autocomplete form data.
  scoped_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;

  // Utilities for logging form events.
  scoped_ptr<AutofillMetrics::FormEventLogger> address_form_event_logger_;
  scoped_ptr<AutofillMetrics::FormEventLogger> credit_card_form_event_logger_;

  // Have we logged whether Autofill is enabled for this page load?
  bool has_logged_autofill_enabled_;
  // Have we logged an address suggestions count metric for this page?
  bool has_logged_address_suggestions_count_;
  // Have we shown Autofill suggestions at least once?
  bool did_show_suggestions_;
  // Has the user manually edited at least one form field among the autofillable
  // ones?
  bool user_did_type_;
  // Has the user autofilled a form on this page?
  bool user_did_autofill_;
  // Has the user edited a field that was previously autofilled?
  bool user_did_edit_autofilled_field_;
  // When the form finished loading.
  std::map<FormData, base::TimeTicks> forms_loaded_timestamps_;
  // When the user first interacted with a potentially fillable form on this
  // page.
  base::TimeTicks initial_interaction_timestamp_;

  // Our copy of the form data.
  ScopedVector<FormStructure> form_structures_;

  // A copy of the credit card that's currently being unmasked, and data about
  // the form.
  CreditCard unmasking_card_;
  // A copy of the latest card unmasking response.
  UnmaskResponse unmask_response_;
  int unmasking_query_id_;
  FormData unmasking_form_;
  FormFieldData unmasking_field_;
  // Time when we requested the last real pan
  base::Time real_pan_request_timestamp_;

  // Masked copies of recently unmasked cards, to help avoid double-asking to
  // save the card (in the prompt and in the infobar after submit).
  std::vector<CreditCard> recently_unmasked_cards_;

#ifdef ENABLE_FORM_DEBUG_DUMP
  // The last few autofilled forms (key/value pairs) submitted, for debugging.
  // TODO(brettw) this should be removed. See DumpAutofillData.
  std::vector<std::map<std::string, base::string16>>
      recently_autofilled_forms_;
#endif

  // Suggestion backend ID to ID mapping. We keep two maps to convert back and
  // forth. These should be used only by BackendIDToInt and IntToBackendID.
  // Note that the integers are not frontend IDs.
  mutable std::map<std::string, int> backend_to_int_map_;
  mutable std::map<int, std::string> int_to_backend_map_;

  // Delegate to perform external processing (display, selection) on
  // our behalf.  Weak.
  AutofillExternalDelegate* external_delegate_;

  // Delegate used in test to get notifications on certain events.
  AutofillManagerTestDelegate* test_delegate_;

  base::WeakPtrFactory<AutofillManager> weak_ptr_factory_;

  friend class AutofillManagerTest;
  friend class FormStructureBrowserTest;
  friend class FormStructureBrowserTestIos<ChromeUIWebViewWebTest>;
  friend class FormStructureBrowserTestIos<ChromeWKWebViewWebTest>;
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DeterminePossibleFieldTypesForUpload);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DeterminePossibleFieldTypesForUploadStressTest);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DisabledAutofillDispatchesError);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressFilledFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressSubmittedFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressWillSubmitFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressSuggestionsCount);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillIsEnabledAtPageLoad);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardSelectedFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardFilledFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardGetRealPanDuration);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardWillSubmitFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardSubmittedFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, DeveloperEngagement);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, FormFillDuration);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           NoQualityMetricsForNonAutofillableForms);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetrics);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetricsForFailure);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetricsWithExperimentId);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, SaneMetricsWithCacheMismatch);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, TestExternalDelegate);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           TestTabContentsWithExternalDelegate);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           UserHappinessFormLoadAndSubmission);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, UserHappinessFormInteraction);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           FormSubmittedAutocompleteEnabled);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           AutocompleteOffRespectedForAutocomplete);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DontSaveCvcInAutocompleteHistory);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, DontOfferToSaveWalletCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillInUpdatedExpirationDate);
  DISALLOW_COPY_AND_ASSIGN(AutofillManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
