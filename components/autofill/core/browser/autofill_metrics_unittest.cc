// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_metrics.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/wallet/real_pan_wallet_client.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/rappor/test_rappor_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/webdata/common/web_data_results.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using base::TimeTicks;

namespace autofill {
namespace {

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager()
      : PersonalDataManager("en-US"),
        autofill_enabled_(true) {
    CreateTestAutofillProfiles(&web_profiles_);
  }

  using PersonalDataManager::set_account_tracker;
  using PersonalDataManager::set_database;
  using PersonalDataManager::SetPrefService;

  // Overridden to avoid a trip to the database. This should be a no-op except
  // for the side-effect of logging the profile count.
  void LoadProfiles() override {
    {
      std::vector<AutofillProfile*> profiles;
      web_profiles_.release(&profiles);
      WDResult<std::vector<AutofillProfile*> > result(AUTOFILL_PROFILES_RESULT,
                                                      profiles);
      pending_profiles_query_ = 123;
      OnWebDataServiceRequestDone(pending_profiles_query_, &result);
    }
    {
      std::vector<AutofillProfile*> profiles;
      server_profiles_.release(&profiles);
      WDResult<std::vector<AutofillProfile*> > result(AUTOFILL_PROFILES_RESULT,
                                                      profiles);
      pending_server_profiles_query_ = 124;
      OnWebDataServiceRequestDone(pending_server_profiles_query_, &result);
    }
  }

  // Overridden to avoid a trip to the database.
  void LoadCreditCards() override {
    {
      std::vector<CreditCard*> credit_cards;
      local_credit_cards_.release(&credit_cards);
      WDResult<std::vector<CreditCard*> > result(
          AUTOFILL_CREDITCARDS_RESULT, credit_cards);
      pending_creditcards_query_ = 125;
      OnWebDataServiceRequestDone(pending_creditcards_query_, &result);
    }
    {
      std::vector<CreditCard*> credit_cards;
      server_credit_cards_.release(&credit_cards);
      WDResult<std::vector<CreditCard*> > result(
          AUTOFILL_CREDITCARDS_RESULT, credit_cards);
      pending_server_creditcards_query_ = 126;
      OnWebDataServiceRequestDone(pending_server_creditcards_query_, &result);
    }
  }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  // Removes all existing profiles and creates 0 or 1 local profiles and 0 or 1
  // server profile according to the paramters.
  void RecreateProfiles(bool include_local_profile,
                        bool include_server_profile) {
    web_profiles_.clear();
    server_profiles_.clear();
    if (include_local_profile) {
      AutofillProfile* profile = new AutofillProfile;
      test::SetProfileInfo(profile, "Elvis", "Aaron",
                           "Presley", "theking@gmail.com", "RCA",
                           "3734 Elvis Presley Blvd.", "Apt. 10",
                           "Memphis", "Tennessee", "38116", "US",
                           "12345678901");
      profile->set_guid("00000000-0000-0000-0000-000000000001");
      web_profiles_.push_back(profile);
    }
    if (include_server_profile) {
      AutofillProfile* profile = new AutofillProfile(
          AutofillProfile::SERVER_PROFILE, "server_id");
      test::SetProfileInfo(profile, "Charles", "Hardin",
                           "Holley", "buddy@gmail.com", "Decca",
                           "123 Apple St.", "unit 6", "Lubbock",
                           "Texas", "79401", "US", "2345678901");
      profile->set_guid("00000000-0000-0000-0000-000000000002");
      server_profiles_.push_back(profile);
    }
    Refresh();
  }

  // Removes all existing credit cards and creates 0 or 1 local profiles and
  // 0 or 1 server profile according to the paramters.
  void RecreateCreditCards(bool include_local_credit_card,
                           bool include_masked_server_credit_card,
                           bool include_full_server_credit_card) {
    local_credit_cards_.clear();
    server_credit_cards_.clear();
    if (include_local_credit_card) {
      CreditCard* credit_card = new CreditCard;
      credit_card->set_guid("10000000-0000-0000-0000-000000000001");
      local_credit_cards_.push_back(credit_card);
    }
    if (include_masked_server_credit_card) {
      CreditCard* credit_card = new CreditCard(
          CreditCard::MASKED_SERVER_CARD, "server_id");
      credit_card->set_guid("10000000-0000-0000-0000-000000000002");
      credit_card->SetTypeForMaskedCard(kDiscoverCard);
      server_credit_cards_.push_back(credit_card);
    }
    if (include_full_server_credit_card) {
      CreditCard* credit_card = new CreditCard(
          CreditCard::FULL_SERVER_CARD, "server_id");
      credit_card->set_guid("10000000-0000-0000-0000-000000000003");
      server_credit_cards_.push_back(credit_card);
    }
    Refresh();
  }

  bool IsAutofillEnabled() const override { return autofill_enabled_; }

 private:
  void CreateTestAutofillProfiles(ScopedVector<AutofillProfile>* profiles) {
    AutofillProfile* profile = new AutofillProfile;
    test::SetProfileInfo(profile, "Elvis", "Aaron",
                         "Presley", "theking@gmail.com", "RCA",
                         "3734 Elvis Presley Blvd.", "Apt. 10",
                         "Memphis", "Tennessee", "38116", "US",
                         "12345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(profile);
    profile = new AutofillProfile;
    test::SetProfileInfo(profile, "Charles", "Hardin",
                         "Holley", "buddy@gmail.com", "Decca",
                         "123 Apple St.", "unit 6", "Lubbock",
                         "Texas", "79401", "US", "2345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(profile);
  }

  bool autofill_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class TestFormStructure : public FormStructure {
 public:
  explicit TestFormStructure(const FormData& form) : FormStructure(form) {}
  ~TestFormStructure() override {}

  void SetFieldTypes(const std::vector<ServerFieldType>& heuristic_types,
                     const std::vector<ServerFieldType>& server_types) {
    ASSERT_EQ(field_count(), heuristic_types.size());
    ASSERT_EQ(field_count(), server_types.size());

    for (size_t i = 0; i < field_count(); ++i) {
      AutofillField* form_field = field(i);
      ASSERT_TRUE(form_field);
      form_field->set_heuristic_type(heuristic_types[i]);
      form_field->set_server_type(server_types[i]);
    }

    UpdateAutofillCount();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFormStructure);
};

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(AutofillDriver* driver,
                      AutofillClient* autofill_client,
                      TestPersonalDataManager* personal_manager)
      : AutofillManager(driver, autofill_client, personal_manager),
        autofill_enabled_(true) {}
  ~TestAutofillManager() override {}

  bool IsAutofillEnabled() const override { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  void AddSeenForm(const FormData& form,
                   const std::vector<ServerFieldType>& heuristic_types,
                   const std::vector<ServerFieldType>& server_types) {
    FormData empty_form = form;
    for (size_t i = 0; i < empty_form.fields.size(); ++i) {
      empty_form.fields[i].value = base::string16();
    }

    // |form_structure| will be owned by |form_structures()|.
    TestFormStructure* form_structure = new TestFormStructure(empty_form);
    form_structure->SetFieldTypes(heuristic_types, server_types);
    form_structures()->push_back(form_structure);
  }

  // Calls AutofillManager::OnWillSubmitForm and waits for it to complete.
  void WillSubmitForm(const FormData& form, const TimeTicks& timestamp) {
    run_loop_.reset(new base::RunLoop());
    if (!OnWillSubmitForm(form, timestamp))
      return;

    // Wait for the asynchronous OnWillSubmitForm() call to complete.
    run_loop_->Run();
  }

  // Calls both AutofillManager::OnWillSubmitForm and
  // AutofillManager::OnFormSubmitted.
  void SubmitForm(const FormData& form, const TimeTicks& timestamp) {
    WillSubmitForm(form, timestamp);
    OnFormSubmitted(form);
  }

  void UploadFormDataAsyncCallback(
      const FormStructure* submitted_form,
      const base::TimeTicks& load_time,
      const base::TimeTicks& interaction_time,
      const base::TimeTicks& submission_time) override {
    run_loop_->Quit();

    AutofillManager::UploadFormDataAsyncCallback(submitted_form,
                                                 load_time,
                                                 interaction_time,
                                                 submission_time);
  }

 private:
  bool autofill_enabled_;
  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

}  // namespace

// This is defined in the autofill_metrics.cc implementation file.
int GetFieldTypeGroupMetric(ServerFieldType field_type,
                            AutofillMetrics::FieldTypeQualityMetric metric);

class AutofillMetricsTest : public testing::Test {
 public:
  ~AutofillMetricsTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  void EnableWalletSync();

  base::MessageLoop message_loop_;
  TestAutofillClient autofill_client_;
  scoped_ptr<AccountTrackerService> account_tracker_;
  scoped_ptr<TestSigninClient> signin_client_;
  scoped_ptr<TestAutofillDriver> autofill_driver_;
  scoped_ptr<TestAutofillManager> autofill_manager_;
  scoped_ptr<TestPersonalDataManager> personal_data_;
  scoped_ptr<AutofillExternalDelegate> external_delegate_;
};

AutofillMetricsTest::~AutofillMetricsTest() {
  // Order of destruction is important as AutofillManager relies on
  // PersonalDataManager to be around when it gets destroyed.
  autofill_manager_.reset();
}

void AutofillMetricsTest::SetUp() {
  autofill_client_.SetPrefs(test::PrefServiceForTesting());

  // Ensure Mac OS X does not pop up a modal dialog for the Address Book.
  test::DisableSystemServices(autofill_client_.GetPrefs());

  // Setup account tracker.
  signin_client_.reset(new TestSigninClient(autofill_client_.GetPrefs()));
  account_tracker_.reset(new AccountTrackerService());
  account_tracker_->Initialize(
      autofill_client_.GetIdentityProvider()->GetTokenService(),
      signin_client_.get());

  personal_data_.reset(new TestPersonalDataManager());
  personal_data_->set_database(autofill_client_.GetDatabase());
  personal_data_->SetPrefService(autofill_client_.GetPrefs());
  personal_data_->set_account_tracker(account_tracker_.get());
  autofill_driver_.reset(new TestAutofillDriver());
  autofill_manager_.reset(new TestAutofillManager(
      autofill_driver_.get(), &autofill_client_, personal_data_.get()));

  external_delegate_.reset(new AutofillExternalDelegate(
      autofill_manager_.get(),
      autofill_driver_.get()));
  autofill_manager_->SetExternalDelegate(external_delegate_.get());
}

void AutofillMetricsTest::TearDown() {
  // Order of destruction is important as AutofillManager relies on
  // PersonalDataManager to be around when it gets destroyed.
  autofill_manager_.reset();
  autofill_driver_.reset();
  personal_data_.reset();
  account_tracker_->Shutdown();
  account_tracker_.reset();
  signin_client_.reset();
}

void AutofillMetricsTest::EnableWalletSync() {
  autofill_client_.GetPrefs()->SetBoolean(
      prefs::kAutofillWalletSyncExperimentEnabled, true);
  std::string account_id =
      account_tracker_->SeedAccountInfo("12345", "syncuser@example.com");
  autofill_client_.GetPrefs()->SetString(
      ::prefs::kGoogleServicesAccountId, account_id);
}

// Test that we log quality metrics appropriately.
TEST_F(AutofillMetricsTest, QualityMetrics) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField(
      "Autofilled", "autofilled", "Elvis Aaron Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField(
      "Autofill Failed", "autofillfailed", "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Empty", "empty", "", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField("Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Select", "select", "USA", "select-one", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // Heuristic predictions.
  // Unknown:
  histogram_tester.ExpectBucketCount("Autofill.Quality.HeuristicType",
                                     AutofillMetrics::TYPE_UNKNOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.HeuristicType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                              AutofillMetrics::TYPE_UNKNOWN),
      1);
  // Match:
  histogram_tester.ExpectBucketCount("Autofill.Quality.HeuristicType",
                                     AutofillMetrics::TYPE_MATCH, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.HeuristicType.ByFieldType",
      GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TYPE_MATCH), 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.HeuristicType.ByFieldType",
      GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                              AutofillMetrics::TYPE_MATCH),
      1);
  // Mismatch:
  histogram_tester.ExpectBucketCount("Autofill.Quality.HeuristicType",
                                     AutofillMetrics::TYPE_MISMATCH, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.HeuristicType.ByFieldType",
      GetFieldTypeGroupMetric(EMAIL_ADDRESS, AutofillMetrics::TYPE_MISMATCH),
      1);

  // Server predictions:
  // Unknown:
  histogram_tester.ExpectBucketCount("Autofill.Quality.ServerType",
                                     AutofillMetrics::TYPE_UNKNOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.ServerType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                              AutofillMetrics::TYPE_UNKNOWN),
      1);
  // Match:
  histogram_tester.ExpectBucketCount("Autofill.Quality.ServerType",
                                     AutofillMetrics::TYPE_MATCH, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.ServerType.ByFieldType",
      GetFieldTypeGroupMetric(EMAIL_ADDRESS, AutofillMetrics::TYPE_MATCH), 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.ServerType.ByFieldType",
      GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                              AutofillMetrics::TYPE_MATCH),
      1);
  // Mismatch:
  histogram_tester.ExpectBucketCount("Autofill.Quality.ServerType",
                                     AutofillMetrics::TYPE_MISMATCH, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.ServerType.ByFieldType",
      GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TYPE_MISMATCH), 1);

  // Overall predictions:
  // Unknown:
  histogram_tester.ExpectBucketCount("Autofill.Quality.PredictedType",
                                     AutofillMetrics::TYPE_UNKNOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.PredictedType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                              AutofillMetrics::TYPE_UNKNOWN),
      1);
  // Match:
  histogram_tester.ExpectBucketCount("Autofill.Quality.PredictedType",
                                     AutofillMetrics::TYPE_MATCH, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.PredictedType.ByFieldType",
      GetFieldTypeGroupMetric(EMAIL_ADDRESS, AutofillMetrics::TYPE_MATCH), 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.PredictedType.ByFieldType",
      GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                              AutofillMetrics::TYPE_MATCH),
      1);
  // Mismatch:
  histogram_tester.ExpectBucketCount("Autofill.Quality.PredictedType",
                                     AutofillMetrics::TYPE_MISMATCH, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.PredictedType.ByFieldType",
      GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TYPE_MISMATCH), 1);
}

// Test that we do not log RAPPOR metrics when the number of mismatches is not
// high enough.
TEST_F(AutofillMetricsTest, Rappor_LowMismatchRate_NoMetricsReported) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(EMAIL_ADDRESS);
  server_types.push_back(NAME_LAST);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // The number of mismatches did not trigger the RAPPOR metric logging.
  EXPECT_EQ(0, autofill_client_.test_rappor_service()->GetReportsCount());
}

// Test that we don't log RAPPOR metrics in the case heuristics and/or server
// have no data.
TEST_F(AutofillMetricsTest, Rappor_NoDataServerAndHeuristic_NoMetricsReported) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // No RAPPOR metrics are logged in the case of multiple UNKNOWN_TYPE and
  // NO_SERVER_DATA for heuristics and server predictions, respectively.
  EXPECT_EQ(0, autofill_client_.test_rappor_service()->GetReportsCount());
}

// Test that we log high number of mismatches for the server prediction.
TEST_F(AutofillMetricsTest, Rappor_HighServerMismatchRate_MetricsReported) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(NAME_LAST);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // The number of mismatches did trigger the RAPPOR metric logging for server
  // predictions.
  EXPECT_EQ(1, autofill_client_.test_rappor_service()->GetReportsCount());
  std::string sample;
  rappor::RapporType type;
  EXPECT_FALSE(
      autofill_client_.test_rappor_service()->GetRecordedSampleForMetric(
          "Autofill.HighNumberOfHeuristicMismatches", &sample, &type));
  EXPECT_TRUE(
      autofill_client_.test_rappor_service()->GetRecordedSampleForMetric(
          "Autofill.HighNumberOfServerMismatches", &sample, &type));
  EXPECT_EQ("example.com", sample);
  EXPECT_EQ(rappor::ETLD_PLUS_ONE_RAPPOR_TYPE, type);
}

// Test that we log high number of mismatches for the heuristic predictions.
TEST_F(AutofillMetricsTest, Rappor_HighHeuristicMismatchRate_MetricsReported) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FIRST);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(NAME_LAST);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(EMAIL_ADDRESS);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // The number of mismatches did trigger the RAPPOR metric logging for
  // heuristic predictions.
  EXPECT_EQ(1, autofill_client_.test_rappor_service()->GetReportsCount());
  std::string sample;
  rappor::RapporType type;
  EXPECT_FALSE(
      autofill_client_.test_rappor_service()->GetRecordedSampleForMetric(
          "Autofill.HighNumberOfServerMismatches", &sample, &type));
  EXPECT_TRUE(
      autofill_client_.test_rappor_service()->GetRecordedSampleForMetric(
          "Autofill.HighNumberOfHeuristicMismatches", &sample, &type));
  EXPECT_EQ("example.com", sample);
  EXPECT_EQ(rappor::ETLD_PLUS_ONE_RAPPOR_TYPE, type);
}

// Verify that when a field is annotated with the autocomplete attribute, its
// predicted type is remembered when quality metrics are logged.
TEST_F(AutofillMetricsTest, PredictedMetricsWithAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field1;
  test::CreateTestFormField("Select", "select", "USA", "select-one", &field1);
  field1.autocomplete_attribute = "country";
  form.fields.push_back(field1);

  // Two other fields to have the minimum of 3 to be parsed by autofill. Note
  // that they have default values not found in the user profiles. They will be
  // changed between the time the form is seen/parsed, and the time it is
  // submitted.
  FormFieldData field2;
  test::CreateTestFormField("Unknown", "Unknown", "", "text", &field2);
  form.fields.push_back(field2);
  FormFieldData field3;
  test::CreateTestFormField("Phone", "phone", "", "tel", &field3);
  form.fields.push_back(field3);

  std::vector<FormData> forms(1, form);

  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    // We change the value of the text fields to change the default/seen values
    // (hence the values are not cleared in UpdateFromCache). The new values
    // match what is in the test profile.
    form.fields[1].value = base::ASCIIToUTF16("79401");
    form.fields[2].value = base::ASCIIToUTF16("2345678901");
    autofill_manager_->SubmitForm(form, TimeTicks::Now());

    // First verify that country was not predicted by client or server.
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.ServerType.ByFieldType",
        GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                                AutofillMetrics::TYPE_UNKNOWN),
        1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.HeuristicType.ByFieldType",
        GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                                AutofillMetrics::TYPE_UNKNOWN),
        1);
    // We expect a match for country because it had |autocomplete_attribute|.
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.PredictedType.ByFieldType",
        GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                                AutofillMetrics::TYPE_MATCH),
        1);

    // We did not predict zip code or phone number, because they did not have
    // |autocomplete_attribute|, nor client or server predictions.
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.ServerType.ByFieldType",
        GetFieldTypeGroupMetric(ADDRESS_HOME_ZIP,
                                AutofillMetrics::TYPE_UNKNOWN),
        1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.HeuristicType.ByFieldType",
        GetFieldTypeGroupMetric(ADDRESS_HOME_ZIP,
                                AutofillMetrics::TYPE_UNKNOWN),
        1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.PredictedType.ByFieldType",
        GetFieldTypeGroupMetric(ADDRESS_HOME_ZIP,
                                AutofillMetrics::TYPE_UNKNOWN),
        1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.ServerType.ByFieldType",
        GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                                AutofillMetrics::TYPE_UNKNOWN),
        1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.HeuristicType.ByFieldType",
        GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                                AutofillMetrics::TYPE_UNKNOWN),
        1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Quality.PredictedType.ByFieldType",
        GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                                AutofillMetrics::TYPE_UNKNOWN),
        1);

    // Sanity check.
    histogram_tester.ExpectTotalCount("Autofill.Quality.PredictedType", 3);
  }
}

// Test that we behave sanely when the cached form differs from the submitted
// one.
TEST_F(AutofillMetricsTest, SaneMetricsWithCacheMismatch) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  std::vector<ServerFieldType> heuristic_types, server_types;

  FormFieldData field;
  test::CreateTestFormField(
      "Both match", "match", "Elvis Aaron Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);
  test::CreateTestFormField(
      "Both mismatch", "mismatch", "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(PHONE_HOME_NUMBER);
  test::CreateTestFormField(
      "Only heuristics match", "mixed", "Memphis", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_CITY);
  server_types.push_back(PHONE_HOME_NUMBER);
  test::CreateTestFormField("Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(UNKNOWN_TYPE);

  // Simulate having seen this form with the desired heuristic and server types.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);


  // Add a field and re-arrange the remaining form fields before submitting.
  std::vector<FormFieldData> cached_fields = form.fields;
  form.fields.clear();
  test::CreateTestFormField(
      "New field", "new field", "Tennessee", "text", &field);
  form.fields.push_back(field);
  form.fields.push_back(cached_fields[2]);
  form.fields.push_back(cached_fields[1]);
  form.fields.push_back(cached_fields[3]);
  form.fields.push_back(cached_fields[0]);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // Heuristic predictions.
  // Unknown:
  histogram_tester.ExpectBucketCount("Autofill.Quality.HeuristicType",
                                     AutofillMetrics::TYPE_UNKNOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.HeuristicType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_STATE,
                              AutofillMetrics::TYPE_UNKNOWN),
      1);
  // Match:
  histogram_tester.ExpectBucketCount("Autofill.Quality.HeuristicType",
                                     AutofillMetrics::TYPE_MATCH, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.HeuristicType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_CITY, AutofillMetrics::TYPE_MATCH),
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.HeuristicType.ByFieldType",
      GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TYPE_MATCH), 1);
  // Mismatch:
  histogram_tester.ExpectBucketCount("Autofill.Quality.HeuristicType",
                                     AutofillMetrics::TYPE_MISMATCH, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.HeuristicType.ByFieldType",
      GetFieldTypeGroupMetric(EMAIL_ADDRESS, AutofillMetrics::TYPE_MISMATCH),
      1);

  // Server predictions:
  // Unknown:
  histogram_tester.ExpectBucketCount("Autofill.Quality.ServerType",
                                     AutofillMetrics::TYPE_UNKNOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.ServerType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_STATE,
                              AutofillMetrics::TYPE_UNKNOWN),
      1);
  // Match:
  histogram_tester.ExpectBucketCount("Autofill.Quality.ServerType",
                                     AutofillMetrics::TYPE_MATCH, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.ServerType.ByFieldType",
      GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TYPE_MATCH), 1);
  // Mismatch:
  histogram_tester.ExpectBucketCount("Autofill.Quality.ServerType",
                                     AutofillMetrics::TYPE_MISMATCH, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.ServerType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_CITY,
                              AutofillMetrics::TYPE_MISMATCH),
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.ServerType.ByFieldType",
      GetFieldTypeGroupMetric(EMAIL_ADDRESS, AutofillMetrics::TYPE_MISMATCH),
      1);

  // Overall predictions:
  // Unknown:
  histogram_tester.ExpectBucketCount("Autofill.Quality.PredictedType",
                                     AutofillMetrics::TYPE_UNKNOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.PredictedType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_STATE,
                              AutofillMetrics::TYPE_UNKNOWN),
      1);
  // Match:
  histogram_tester.ExpectBucketCount("Autofill.Quality.PredictedType",
                                     AutofillMetrics::TYPE_MATCH, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.PredictedType.ByFieldType",
      GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TYPE_MATCH), 1);
  // Mismatch:
  histogram_tester.ExpectBucketCount("Autofill.Quality.PredictedType",
                                     AutofillMetrics::TYPE_MISMATCH, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.PredictedType.ByFieldType",
      GetFieldTypeGroupMetric(ADDRESS_HOME_CITY,
                              AutofillMetrics::TYPE_MISMATCH),
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Quality.PredictedType.ByFieldType",
      GetFieldTypeGroupMetric(EMAIL_ADDRESS, AutofillMetrics::TYPE_MISMATCH),
      1);
}

// Verify that when submitting an autofillable form, the stored profile metric
// is logged.
TEST_F(AutofillMetricsTest, StoredProfileCountAutofillableFormSubmission) {
  // Construct a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  // Three fields is enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->OnFormsSeen(forms, TimeTicks());
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // An autofillable form was submitted, and the number of stored profiles is
  // logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 2, 1);
}

// Verify that when submitting a non-autofillable form, the stored profile
// metric is not logged.
TEST_F(AutofillMetricsTest, StoredProfileCountNonAutofillableFormSubmission) {
  // Construct a non-fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  // Two fields is not enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->OnFormsSeen(forms, TimeTicks());
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // A non-autofillable form was submitted, and number of stored profiles is NOT
  // logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 0);
}

// Verify that we correctly log metrics regarding developer engagement.
TEST_F(AutofillMetricsTest, DeveloperEngagement) {
  // Start with a non-fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Ensure no metrics are logged when loading a non-fillable form.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    autofill_manager_->Reset();
    histogram_tester.ExpectTotalCount("Autofill.DeveloperEngagement", 0);
  }

  // Add another field to the form, so that it becomes fillable.
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  forms.back().fields.push_back(field);

  // Expect only the "form parsed" metric to be logged; no metrics about
  // author-specified field type hints.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    autofill_manager_->Reset();
    histogram_tester.ExpectUniqueSample("Autofill.DeveloperEngagement",
                                        AutofillMetrics::FILLABLE_FORM_PARSED,
                                        1);
  }

  // Add some fields with an author-specified field type to the form.
  // We need to add at least three fields, because a form must have at least
  // three fillable fields to be considered to be autofillable; and if at least
  // one field specifies an explicit type hint, we don't apply any of our usual
  // local heuristics to detect field types in the rest of the form.
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "email";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "address-line1";
  forms.back().fields.push_back(field);

  // Expect both the "form parsed" metric and the author-specified field type
  // hints metric to be logged.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    autofill_manager_->Reset();
    histogram_tester.ExpectBucketCount("Autofill.DeveloperEngagement",
                                       AutofillMetrics::FILLABLE_FORM_PARSED,
                                       1);
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_CONTAINS_TYPE_HINTS, 1);
  }
}

// Test that the profile count is logged correctly.
TEST_F(AutofillMetricsTest, StoredProfileCount) {
  // The metric should be logged when the profiles are first loaded.
  {
    base::HistogramTester histogram_tester;
    personal_data_->LoadProfiles();
    histogram_tester.ExpectUniqueSample("Autofill.StoredProfileCount", 2, 1);
  }

  // The metric should only be logged once.
  {
    base::HistogramTester histogram_tester;
    personal_data_->LoadProfiles();
    histogram_tester.ExpectTotalCount("Autofill.StoredProfileCount", 0);
  }
}

// Test that we correctly log when Autofill is enabled.
TEST_F(AutofillMetricsTest, AutofillIsEnabledAtStartup) {
  base::HistogramTester histogram_tester;
  personal_data_->set_autofill_enabled(true);
  personal_data_->Init(
      autofill_client_.GetDatabase(), autofill_client_.GetPrefs(),
      account_tracker_.get(), false);
  histogram_tester.ExpectUniqueSample("Autofill.IsEnabled.Startup", true, 1);
}

// Test that we correctly log when Autofill is disabled.
TEST_F(AutofillMetricsTest, AutofillIsDisabledAtStartup) {
  base::HistogramTester histogram_tester;
  personal_data_->set_autofill_enabled(false);
  personal_data_->Init(
      autofill_client_.GetDatabase(), autofill_client_.GetPrefs(),
      account_tracker_.get(), false);
  histogram_tester.ExpectUniqueSample("Autofill.IsEnabled.Startup", false, 1);
}

// Test that we log the number of Autofill suggestions when filling a form.
TEST_F(AutofillMetricsTest, AddressSuggestionsCount) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(NAME_FULL);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  field_types.push_back(EMAIL_ADDRESS);
  test::CreateTestFormField("Phone", "phone", "", "tel", &field);
  form.fields.push_back(field);
  field_types.push_back(PHONE_HOME_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the phone field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 2,
                                        1);
  }

  {
    // Simulate activating the autofill popup for the email field after typing.
    // No new metric should be logged, since we're still on the same page.
    test::CreateTestFormField("Email", "email", "b", "email", &field);
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectTotalCount("Autofill.AddressSuggestionsCount", 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the email field after typing.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 1,
                                        1);
  }

  // Reset the autofill manager state again.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the email field after typing.
    form.fields[0].is_autofilled = true;
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectTotalCount("Autofill.AddressSuggestionsCount", 0);
  }
}

// Test that we log interacted form event for credit cards related.
TEST_F(AutofillMetricsTest, CreditCardInteractedFormEvents) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the credit card field twice.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->OnQueryFormFieldAutofill(1, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log suggestion shown form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardShownFormEvents) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(false /* is_new_popup */, form,
                                          field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0);
  }
}

// Test that we log selected form event for credit cards.
TEST_F(AutofillMetricsTest, CreditCardSelectedFormEvents) {
  EnableWalletSync();
  // Creating all kinds of cards.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting a masked card server suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000002", 0); // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting multiple times a masked card server.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000002", 0); // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
        1);
  }
}

// Test that we log filled form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardFilledFormEvents) {
  autofill_client_.GetPrefs()->SetBoolean(
      prefs::kAutofillWalletSyncExperimentEnabled, true);
  // Creating all kinds of cards.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a local card suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000001", 0); // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000002", 0); // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                       "6011000990139424");
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
        1);
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a full card server suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000003", 0); // full server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling multiple times.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000001", 0); // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1);
  }
}

// Test that we log submitted form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration) {
  EnableWalletSync();
  // Creating masked card
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid("10000000-0000-0000-0000-000000000002",
                             0);  // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                       "6011000990139424");
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.Success", 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  // Creating masked card
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid("10000000-0000-0000-0000-000000000002",
                             0);  // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::PERMANENT_FAILURE,
                                       std::string());
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.Failure", 1);
  }
}

// Test that we log submitted form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardSubmittedFormEvents) {
  EnableWalletSync();
  // Creating all kinds of cards.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000001", 0); // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000003", 0); // full server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "10000000-0000-0000-0000-000000000002", 0); // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                       "6011000990139424");
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
        1);
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission without previous interaction.
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }
}

// Test that we log "will submit" (but not submitted) form events for credit
// cards. Mirrors CreditCardSubmittedFormEvents test but does not expect any
// "submitted" metrics.
TEST_F(AutofillMetricsTest, CreditCardWillSubmitFormEvents) {
  EnableWalletSync();
  // Creating all kinds of cards.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    SuggestionBackendID guid("10000000-0000-0000-0000-000000000001",
                             0);  // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    SuggestionBackendID guid("10000000-0000-0000-0000-000000000003",
                             0);  // full server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid("10000000-0000-0000-0000-000000000002",
                             0);  // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, SuggestionBackendID()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                       "6011000990139424");
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
        1);
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics
            ::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission without previous interaction.
    base::HistogramTester histogram_tester;
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }
}

// Test that we log interacted form events for address.
TEST_F(AutofillMetricsTest, AddressInteractedFormEvents) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the street field twice.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->OnQueryFormFieldAutofill(1, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log suggestion shown form events for address.
TEST_F(AutofillMetricsTest, AddressShownFormEvents) {
  EnableWalletSync();
  // Creating all kinds of profiles.
  personal_data_->RecreateProfiles(true /* include_local_profile */,
                                   true /* include_server_profile */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(false /* is_new_popup */, form,
                                          field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0);
  }
}

// Test that we log filled form events for address.
TEST_F(AutofillMetricsTest, AddressFilledFormEvents) {
  EnableWalletSync();
  // Creating all kinds of profiles.
  personal_data_->RecreateProfiles(true /* include_local_profile */,
                                   true /* include_server_profile */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting/filling a local profile suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "00000000-0000-0000-0000-000000000001", 0); // local profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting/filling a server profile suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "00000000-0000-0000-0000-000000000002", 0); // server profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting/filling a local profile suggestion.
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "00000000-0000-0000-0000-000000000001", 0); // local profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1);
  }
}

// Test that we log submitted form events for address.
TEST_F(AutofillMetricsTest, AddressSubmittedFormEvents) {
  EnableWalletSync();
  // Creating all kinds of profiles.
  personal_data_->RecreateProfiles(true /* include_local_profile */,
                                   true /* include_server_profile */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    SuggestionBackendID guid(
        "00000000-0000-0000-0000-000000000001", 0); // local profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    SuggestionBackendID guid(
        "00000000-0000-0000-0000-000000000002", 0); // server profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission without previous interaction.
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }
}

// Test that we log "will submit" (but not submitted) form events for address.
// Mirrors AddressSubmittedFormEvents test but does not expect any "submitted"
// metrics.
TEST_F(AutofillMetricsTest, AddressWillSubmitFormEvents) {
  EnableWalletSync();
  // Creating all kinds of profiles.
  personal_data_->RecreateProfiles(true /* include_local_profile */,
                                   true /* include_server_profile */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    SuggestionBackendID guid("00000000-0000-0000-0000-000000000001",
                             0);  // local profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    SuggestionBackendID guid("00000000-0000-0000-0000-000000000002",
                             0);  // server profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission without previous interaction.
    base::HistogramTester histogram_tester;
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }
}

// Test that we log interacted form event for credit cards only once.
TEST_F(AutofillMetricsTest, CreditCardFormEventsAreSegmented) {
  EnableWalletSync();

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithNoData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithBothServerAndLocalData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log interacted form event for address only once.
TEST_F(AutofillMetricsTest, AddressFormEventsAreSegmented) {
  EnableWalletSync();

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateProfiles(false /* include_local_profile */,
                                   false /* include_server_profile */);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithNoData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateProfiles(true /* include_local_profile */,
                                   false /* include_server_profile */);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithOnlyLocalData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateProfiles(false /* include_local_profile */,
                                   true /* include_server_profile */);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithOnlyServerData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateProfiles(true /* include_local_profile */,
                                   true /* include_server_profile */);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::Rect());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithBothServerAndLocalData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }
}


// Test that we log that Autofill is enabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillIsEnabledAtPageLoad) {
  base::HistogramTester histogram_tester;
  autofill_manager_->set_autofill_enabled(true);
  autofill_manager_->OnFormsSeen(std::vector<FormData>(), TimeTicks());
  histogram_tester.ExpectUniqueSample("Autofill.IsEnabled.PageLoad", true, 1);
}

// Test that we log that Autofill is disabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillIsDisabledAtPageLoad) {
  base::HistogramTester histogram_tester;
  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->OnFormsSeen(std::vector<FormData>(), TimeTicks());
  histogram_tester.ExpectUniqueSample("Autofill.IsEnabled.PageLoad", false, 1);
}

// Verify that we correctly log user happiness metrics dealing with form loading
// and form submission.
TEST_F(AutofillMetricsTest, UserHappinessFormLoadAndSubmission) {
  // Start with a form with insufficiently many fields.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Expect no notifications when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness", 0);
  }


  // Expect no notifications when the form is submitted.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness", 0);
  }

  // Add more fields to the form.
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Unknown", "unknown", "", "text", &field);
  form.fields.push_back(field);
  forms.front() = form;

  // Expect a notification when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Expect a notification when the form is submitted.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness", AutofillMetrics::SUBMITTED_NON_FILLABLE_FORM,
        1);
  }

  // Fill in two of the fields.
  form.fields[0].value = ASCIIToUTF16("Elvis Aaron Presley");
  form.fields[1].value = ASCIIToUTF16("theking@gmail.com");
  forms.front() = form;

  // Expect a notification when the form is submitted.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness", AutofillMetrics::SUBMITTED_NON_FILLABLE_FORM,
        1);
  }

  // Fill in the third field.
  form.fields[2].value = ASCIIToUTF16("12345678901");
  forms.front() = form;

  // Expect notifications when the form is submitted.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness",
        AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_NONE, 1);
  }


  // Mark one of the fields as autofilled.
  form.fields[1].is_autofilled = true;
  forms.front() = form;

  // Expect notifications when the form is submitted.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness",
        AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME, 1);
  }

  // Mark all of the fillable fields as autofilled.
  form.fields[0].is_autofilled = true;
  form.fields[2].is_autofilled = true;
  forms.front() = form;

  // Expect notifications when the form is submitted.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness",
        AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_ALL, 1);
  }

  // Clear out the third field's value.
  form.fields[2].value = base::string16();
  forms.front() = form;

  // Expect notifications when the form is submitted.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness", AutofillMetrics::SUBMITTED_NON_FILLABLE_FORM,
        1);
  }
}

// Verify that we correctly log user happiness metrics dealing with form
// interaction.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction) {
  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Expect a notification when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Simulate typing.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_TYPE, 1);
  }

  // Simulate suggestions shown twice for a single edit (i.e. multiple
  // keystrokes in a single field).
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true, form, field);
    autofill_manager_->DidShowSuggestions(false, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
  }

  // Simulate suggestions shown for a different field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true, form, form.fields[1]);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  }

  // Simulate invoking autofill.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnDidFillAutofillFormData(TimeTicks());
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1);
  }

  // Simulate editing an autofilled field.
  {
    base::HistogramTester histogram_tester;
    SuggestionBackendID guid(
        "00000000-0000-0000-0000-000000000001", 0);
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL,
        0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(SuggestionBackendID(), guid));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks());
    // Simulate a second keystroke; make sure we don't log the metric twice.
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks());
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
  }

  // Simulate invoking autofill again.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnDidFillAutofillFormData(TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
  }

  // Simulate editing another autofilled field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnTextFieldDidChange(form, form.fields[1], TimeTicks());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
  }
}

// Verify that we correctly log metrics tracking the duration of form fill.
TEST_F(AutofillMetricsTest, FormFillDuration) {
  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Fill additional form.
  FormData second_form = form;
  test::CreateTestFormField("Second Phone", "second_phone", "", "text", &field);
  second_form.fields.push_back(field);

  std::vector<FormData> second_forms(1, second_form);

  // Fill the field values for form submission.
  form.fields[0].value = ASCIIToUTF16("Elvis Aaron Presley");
  form.fields[1].value = ASCIIToUTF16("theking@gmail.com");
  form.fields[2].value = ASCIIToUTF16("12345678901");

  // Fill the field values for form submission.
  second_form.fields[0].value = ASCIIToUTF16("Elvis Aaron Presley");
  second_form.fields[1].value = ASCIIToUTF16("theking@gmail.com");
  second_form.fields[2].value = ASCIIToUTF16("12345678901");
  second_form.fields[3].value = ASCIIToUTF16("51512345678");

  // Expect only form load metrics to be logged if the form is submitted without
  // user interaction.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager_->Reset();
  }

  // Expect metric to be logged if the user manually edited a form field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks::FromInternalValue(3));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 14, 1);

    autofill_manager_->Reset();
  }

  // Expect metric to be logged if the user autofilled the form.
  form.fields[0].is_autofilled = true;
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnDidFillAutofillFormData(
        TimeTicks::FromInternalValue(5));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager_->Reset();
  }

  // Expect metric to be logged if the user both manually filled some fields
  // and autofilled others.  Messages can arrive out of order, so make sure they
  // take precedence appropriately.
  {
    base::HistogramTester histogram_tester;

    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnDidFillAutofillFormData(
        TimeTicks::FromInternalValue(5));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks::FromInternalValue(3));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 14, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager_->Reset();
  }

  // Make sure that loading another form doesn't affect metrics from the first
  // form.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnFormsSeen(second_forms,
                                   TimeTicks::FromInternalValue(3));
    autofill_manager_->OnDidFillAutofillFormData(
        TimeTicks::FromInternalValue(5));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks::FromInternalValue(3));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 14, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager_->Reset();
  }

  // Make sure that submitting a form that was loaded later will report the
  // later loading time.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnFormsSeen(second_forms,
                                   TimeTicks::FromInternalValue(5));
    autofill_manager_->SubmitForm(second_form,
                                  TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager_->Reset();
  }
}

}  // namespace autofill
