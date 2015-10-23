// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "content/public/browser/notification_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/test/fake_server/fake_server_entity.h"
#include "sync/test/fake_server/unique_client_entity.h"

using autofill_helper::GetPersonalDataManager;
using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {

// Setting the Preferences files contents to this string (before Profile is
// created) will bypass the Sync experiment logic for enabling this feature.
const char kWalletSyncEnabledPreferencesContents[] =
    "{\"autofill\": { \"wallet_import_sync_experiment_enabled\": true } }";

const char kWalletSyncExperimentTag[] = "wallet_sync";

const char kDefaultCardID[] = "wallet entity ID";
const int kDefaultCardExpMonth = 8;
const int kDefaultCardExpYear = 2087;
const char kDefaultCardLastFour[] = "1234";
const char kDefaultCardName[] = "Patrick Valenzuela";
const sync_pb::WalletMaskedCreditCard_WalletCardType kDefaultCardType =
    sync_pb::WalletMaskedCreditCard::AMEX;

void AddDefaultCard(fake_server::FakeServer* server) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      specifics.mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* credit_card =
      wallet_specifics->mutable_masked_card();
  credit_card->set_id(kDefaultCardID);
  credit_card->set_exp_month(kDefaultCardExpMonth);
  credit_card->set_exp_year(kDefaultCardExpYear);
  credit_card->set_last_four(kDefaultCardLastFour);
  credit_card->set_name_on_card(kDefaultCardName);
  credit_card->set_status(sync_pb::WalletMaskedCreditCard::VALID);
  credit_card->set_type(kDefaultCardType);

  server->InjectEntity(fake_server::UniqueClientEntity::CreateForInjection(
      kDefaultCardID,
      specifics));
}

}  // namespace

class SingleClientWalletSyncTest : public SyncTest {
 public:
  SingleClientWalletSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientWalletSyncTest() override {}

  void TriggerSyncCycle() {
    // Note: we use the experiments type here as we want to be able to trigger a
    // sync cycle even when wallet is not enabled yet.
    const syncer::ModelTypeSet kExperimentsType(syncer::EXPERIMENTS);
    TriggerSyncForModelTypes(0, kExperimentsType);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSyncTest);
};

// Checker that will wait until an asynchronous Wallet datatype enable event
// happens, or times out.
class WalletEnabledChecker : public SingleClientStatusChangeChecker {
 public:
  WalletEnabledChecker()
      : SingleClientStatusChangeChecker(
            sync_datatype_helper::test()->GetSyncService(0)) {}
  ~WalletEnabledChecker() override {}

  // SingleClientStatusChangeChecker overrides.
  bool IsExitConditionSatisfied() override {
    return service()->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
  }
  std::string GetDebugMessage() const override {
    return "Waiting for wallet enable event.";
  }
};

// Checker that will wait until an asynchronous Wallet datatype disable event
// happens, or times out
class WalletDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  WalletDisabledChecker()
      : SingleClientStatusChangeChecker(
            sync_datatype_helper::test()->GetSyncService(0)) {}
  ~WalletDisabledChecker() override {}

  // SingleClientStatusChangeChecker overrides.
  bool IsExitConditionSatisfied() override {
    return !service()->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
  }
  std::string GetDebugMessage() const override {
    return "Waiting for wallet disable event.";
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, DisabledByDefault) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  // The type should not be enabled without the experiment enabled.
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, EnabledViaPreference) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  // The type should not be enabled without the experiment enabled.
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  // TODO(pvalenzuela): Assert that the local root node for AUTOFILL_WALLET_DATA
  // exists.
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

// Tests that an experiment received at sync startup time (during sign-in)
// enables the wallet datatype.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       EnabledViaExperimentStartup) {
  sync_pb::EntitySpecifics experiment_entity;
  sync_pb::ExperimentsSpecifics* experiment_specifics =
      experiment_entity.mutable_experiments();
  experiment_specifics->mutable_wallet_sync()->set_enabled(true);
  GetFakeServer()->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          kWalletSyncExperimentTag,
          experiment_entity));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

// Tests receiving an enable experiment at runtime, followed by a disabled
// experiment, and verifies the datatype is enabled/disabled as necessary.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       EnabledDisabledViaExperiment) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().
      Has(syncer::AUTOFILL_WALLET_DATA));

  sync_pb::EntitySpecifics experiment_entity;
  sync_pb::ExperimentsSpecifics* experiment_specifics =
      experiment_entity.mutable_experiments();

  // First enable the experiment.
  experiment_specifics->mutable_wallet_sync()->set_enabled(true);
  GetFakeServer()->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          kWalletSyncExperimentTag, experiment_entity));
  TriggerSyncCycle();

  WalletEnabledChecker enabled_checker;
  enabled_checker.Wait();
  ASSERT_FALSE(enabled_checker.TimedOut());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));

  // Then disable the experiment.
  experiment_specifics->mutable_wallet_sync()->set_enabled(false);
  GetFakeServer()->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          kWalletSyncExperimentTag, experiment_entity));
  TriggerSyncCycle();

  WalletDisabledChecker disable_checker;
  disable_checker.Wait();
  ASSERT_FALSE(disable_checker.TimedOut());
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().
      Has(syncer::AUTOFILL_WALLET_DATA));
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, Download) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);
  AddDefaultCard(GetFakeServer());
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_TRUE(pdm != nullptr);
  std::vector<autofill::CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  autofill::CreditCard* card = cards[0];
  ASSERT_EQ(autofill::CreditCard::MASKED_SERVER_CARD, card->record_type());
  ASSERT_EQ(kDefaultCardID, card->server_id());
  ASSERT_EQ(base::UTF8ToUTF16(kDefaultCardLastFour), card->LastFourDigits());
  ASSERT_EQ(autofill::kAmericanExpressCard, card->type());
  ASSERT_EQ(kDefaultCardExpMonth, card->expiration_month());
  ASSERT_EQ(kDefaultCardExpYear, card->expiration_year());
  ASSERT_EQ(base::UTF8ToUTF16(kDefaultCardName),
            card->GetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NAME));
}

// Wallet data should get cleared from the database when sync is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnDisableSync) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);
  AddDefaultCard(GetFakeServer());
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_TRUE(pdm != nullptr);
  std::vector<autofill::CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  // Turn off sync, the card should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSyncForAllDatatypes());
  cards = pdm->GetCreditCards();
  ASSERT_EQ(0uL, cards.size());
}

// Wallet data should get cleared from the database when the wallet sync type
// flag is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnDisableWalletSync) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);
  AddDefaultCard(GetFakeServer());
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_TRUE(pdm != nullptr);
  std::vector<autofill::CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  // Turn off autofill sync, the card should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::AUTOFILL));
  cards = pdm->GetCreditCards();
  ASSERT_EQ(0uL, cards.size());
}

// Wallet data should get cleared from the database when the wallet autofill
// integration flag is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       ClearOnDisableWalletAutofill) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);
  AddDefaultCard(GetFakeServer());
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_TRUE(pdm != nullptr);
  std::vector<autofill::CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  // Turn off the wallet autofill pref, the card should be gone as a side
  // effect of the wallet data type controller noticing.
  GetProfile(0)->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillWalletImportEnabled, false);
  cards = pdm->GetCreditCards();
  ASSERT_EQ(0uL, cards.size());
}
