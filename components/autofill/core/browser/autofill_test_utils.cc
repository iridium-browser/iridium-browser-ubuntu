// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_test_utils.h"

#include "base/guid.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/os_crypt.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/common/signin_pref_names.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace test {

namespace {

const char kSettingsOrigin[] = "Chrome settings";

}  // namespace

scoped_ptr<PrefService> PrefServiceForTesting() {
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());
  AutofillManager::RegisterProfilePrefs(registry.get());

  // PDM depends on this pref, which is normally registered in
  // SigninManagerFactory.
  registry->RegisterStringPref(::prefs::kGoogleServicesAccountId,
                               std::string());

  // PDM depends on these prefs, which are normally registered in
  // AccountTrackerServiceFactory.
  registry->RegisterListPref(AccountTrackerService::kAccountInfoPref);
  registry->RegisterIntegerPref(::prefs::kAccountIdMigrationState,
                                AccountTrackerService::MIGRATION_NOT_STARTED);
  registry->RegisterInt64Pref(
      AccountTrackerService::kAccountTrackerServiceLastUpdate, 0);

  base::PrefServiceFactory factory;
  factory.set_user_prefs(make_scoped_refptr(new TestingPrefStore()));
  return factory.Create(registry.get());
}

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         FormFieldData* field) {
  field->label = ASCIIToUTF16(label);
  field->name = ASCIIToUTF16(name);
  field->value = ASCIIToUTF16(value);
  field->form_control_type = type;
  field->is_focusable = true;
}

void CreateTestAddressFormData(FormData* form) {
  std::vector<ServerFieldTypeSet> types;
  CreateTestAddressFormData(form, &types);
}

void CreateTestAddressFormData(FormData* form,
                               std::vector<ServerFieldTypeSet>* types) {
  form->name = ASCIIToUTF16("MyForm");
  form->origin = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");
  form->user_submitted = true;
  types->clear();

  FormFieldData field;
  ServerFieldTypeSet type_set;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_FIRST);
  types->push_back(type_set);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_MIDDLE);
  types->push_back(type_set);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_LAST);
  types->push_back(type_set);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_LINE1);
  types->push_back(type_set);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_LINE2);
  types->push_back(type_set);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_CITY);
  types->push_back(type_set);
  test::CreateTestFormField("State", "state", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_STATE);
  types->push_back(type_set);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_ZIP);
  types->push_back(type_set);
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_COUNTRY);
  types->push_back(type_set);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(PHONE_HOME_WHOLE_NUMBER);
  types->push_back(type_set);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(EMAIL_ADDRESS);
  types->push_back(type_set);
}

inline void check_and_set(
    FormGroup* profile, ServerFieldType type, const char* value) {
  if (value)
    profile->SetRawInfo(type, base::UTF8ToUTF16(value));
}

AutofillProfile GetFullProfile() {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  SetProfileInfo(&profile,
                 "John",
                 "H.",
                 "Doe",
                 "johndoe@hades.com",
                 "Underworld",
                 "666 Erebus St.",
                 "Apt 8",
                 "Elysium", "CA",
                 "91111",
                 "US",
                 "16502111111");
  return profile;
}

AutofillProfile GetFullProfile2() {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  SetProfileInfo(&profile,
                 "Jane",
                 "A.",
                 "Smith",
                 "jsmith@example.com",
                 "ACME",
                 "123 Main Street",
                 "Unit 1",
                 "Greensdale", "MI",
                 "48838",
                 "US",
                 "13105557889");
  return profile;
}

AutofillProfile GetVerifiedProfile() {
  AutofillProfile profile(GetFullProfile());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

AutofillProfile GetVerifiedProfile2() {
  AutofillProfile profile(GetFullProfile2());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

CreditCard GetCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), "http://www.example.com");
  SetCreditCardInfo(
      &credit_card, "Test User", "4111111111111111" /* Visa */, "11", "2017");
  return credit_card;
}

CreditCard GetCreditCard2() {
  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com");
  SetCreditCardInfo(
      &credit_card, "Someone Else", "378282246310005" /* AmEx */, "07", "2019");
  return credit_card;
}

CreditCard GetVerifiedCreditCard() {
  CreditCard credit_card(GetCreditCard());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetVerifiedCreditCard2() {
  CreditCard credit_card(GetCreditCard2());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetMaskedServerCard() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, "12", "2012");
  credit_card.SetTypeForMaskedCard(kMasterCard);
  return credit_card;
}

CreditCard GetMaskedServerCardAmex() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "b456");
  test::SetCreditCardInfo(&credit_card, "Justin Thyme",
                          "8431" /* Amex */, "9", "2020");
  credit_card.SetTypeForMaskedCard(kAmericanExpressCard);
  return credit_card;
}

void SetProfileInfo(AutofillProfile* profile,
    const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone) {
  check_and_set(profile, NAME_FIRST, first_name);
  check_and_set(profile, NAME_MIDDLE, middle_name);
  check_and_set(profile, NAME_LAST, last_name);
  check_and_set(profile, EMAIL_ADDRESS, email);
  check_and_set(profile, COMPANY_NAME, company);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2);
  check_and_set(profile, ADDRESS_HOME_CITY, city);
  check_and_set(profile, ADDRESS_HOME_STATE, state);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone);
}

void SetProfileInfoWithGuid(AutofillProfile* profile,
    const char* guid, const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone) {
  if (guid)
    profile->set_guid(guid);
  SetProfileInfo(profile, first_name, middle_name, last_name, email,
                 company, address1, address2, city, state, zipcode, country,
                 phone);
}

void SetCreditCardInfo(CreditCard* credit_card,
    const char* name_on_card, const char* card_number,
    const char* expiration_month, const char* expiration_year) {
  check_and_set(credit_card, CREDIT_CARD_NAME, name_on_card);
  check_and_set(credit_card, CREDIT_CARD_NUMBER, card_number);
  check_and_set(credit_card, CREDIT_CARD_EXP_MONTH, expiration_month);
  check_and_set(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR, expiration_year);
}

void DisableSystemServices(PrefService* prefs) {
  // Use a mock Keychain rather than the OS one to store credit card data.
#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychain(true);
#endif  // defined(OS_MACOSX)

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Don't use the Address Book on Mac, as it reaches out to system services.
  if (prefs)
    prefs->SetBoolean(prefs::kAutofillUseMacAddressBook, false);
#else
  // Disable auxiliary profiles for unit testing by default.
  if (prefs)
    prefs->SetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled, false);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
}

void SetServerCreditCards(AutofillTable* table,
                          const std::vector<CreditCard>& cards) {
  std::vector<CreditCard> as_masked_cards = cards;
  for (CreditCard& card : as_masked_cards) {
    card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    std::string type = card.type();
    card.SetNumber(card.LastFourDigits());
    card.SetTypeForMaskedCard(type.c_str());
  }
  table->SetServerCreditCards(as_masked_cards);

  for (const CreditCard& card : cards) {
    if (card.record_type() != CreditCard::FULL_SERVER_CARD)
      continue;

    table->UnmaskServerCreditCard(card, card.number());
  }
}

}  // namespace test
}  // namespace autofill
