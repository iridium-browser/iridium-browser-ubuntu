// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using content::BrowserContext;

namespace {

// A corrupted BDICT data used in DeleteCorruptedBDICT. Please do not use this
// BDICT data for other tests.
const uint8 kCorruptedBDICT[] = {
  0x42, 0x44, 0x69, 0x63, 0x02, 0x00, 0x01, 0x00,
  0x20, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00,
  0x65, 0x72, 0xe0, 0xac, 0x27, 0xc7, 0xda, 0x66,
  0x6d, 0x1e, 0xa6, 0x35, 0xd1, 0xf6, 0xb7, 0x35,
  0x32, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00,
  0x39, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00,
  0x0a, 0x0a, 0x41, 0x46, 0x20, 0x30, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xe6, 0x49, 0x00, 0x68, 0x02,
  0x73, 0x06, 0x74, 0x0b, 0x77, 0x11, 0x79, 0x15,
};

}  // namespace

class SpellcheckServiceBrowserTest : public InProcessBrowserTest {
 public:
  BrowserContext* GetContext() {
    return static_cast<BrowserContext*>(browser()->profile());
  }
};

// Tests that we can delete a corrupted BDICT file used by hunspell. We do not
// run this test on Mac because Mac does not use hunspell by default.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT) {
  // Write the corrupted BDICT data to create a corrupted BDICT file.
  base::FilePath dict_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir));
  base::FilePath bdict_path =
      chrome::spellcheck_common::GetVersionedFileName("en-US", dict_dir);

  size_t actual = base::WriteFile(bdict_path,
      reinterpret_cast<const char*>(kCorruptedBDICT),
      arraysize(kCorruptedBDICT));
  EXPECT_EQ(arraysize(kCorruptedBDICT), actual);

  // Attach an event to the SpellcheckService object so we can receive its
  // status updates.
  base::WaitableEvent event(true, false);
  SpellcheckService::AttachStatusEvent(&event);

  BrowserContext* context = GetContext();

  // Ensure that the SpellcheckService object does not already exist. Otherwise
  // the next line will not force creation of the SpellcheckService and the
  // test will fail.
  SpellcheckService* service = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context,
          false));
  ASSERT_EQ(NULL, service);

  // Getting the spellcheck_service will initialize the SpellcheckService
  // object with the corrupted BDICT file created above since the hunspell
  // dictionary is loaded in the SpellcheckService constructor right now.
  // The SpellCheckHost object will send a BDICT_CORRUPTED event.
  SpellcheckServiceFactory::GetForContext(context);

  // Check the received event. Also we check if Chrome has successfully deleted
  // the corrupted dictionary. We delete the corrupted dictionary to avoid
  // leaking it when this test fails.
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  EXPECT_EQ(SpellcheckService::BDICT_CORRUPTED,
            SpellcheckService::GetStatusEvent());
  if (base::PathExists(bdict_path)) {
    ADD_FAILURE();
    EXPECT_TRUE(base::DeleteFile(bdict_path, true));
  }
}

// Checks that preferences migrate correctly.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesMigrated) {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetContext());
  prefs->Set(prefs::kSpellCheckDictionaries, base::ListValue());
  prefs->SetString(prefs::kSpellCheckDictionary, "en-US");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have been migrated.
  std::string new_pref;
  EXPECT_TRUE(
      prefs->GetList(prefs::kSpellCheckDictionaries)->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
  EXPECT_TRUE(prefs->GetString(prefs::kSpellCheckDictionary).empty());
}

// Checks that preferences are not migrated when they shouldn't be.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesNotMigrated) {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetContext());
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  prefs->Set(prefs::kSpellCheckDictionaries, dictionaries);
  prefs->SetString(prefs::kSpellCheckDictionary, "fr");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have not been migrated.
  std::string new_pref;
  EXPECT_TRUE(
      prefs->GetList(prefs::kSpellCheckDictionaries)->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
  EXPECT_TRUE(prefs->GetString(prefs::kSpellCheckDictionary).empty());
}

// Checks that if a user starts multilingual mode with spellchecking disabled
// that all languages get deselected and spellchecking gets enabled.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       SpellcheckingDisabledPreferenceMigration) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableMultilingualSpellChecker);

  PrefService* prefs = user_prefs::UserPrefs::Get(GetContext());
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  prefs->Set(prefs::kSpellCheckDictionaries, dictionaries);
  prefs->SetBoolean(prefs::kEnableContinuousSpellcheck, false);

  // Migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_TRUE(prefs->GetBoolean(prefs::kEnableContinuousSpellcheck));
  EXPECT_EQ(0U, prefs->GetList(prefs::kSpellCheckDictionaries)->GetSize());
}

// Make sure that there is only one language in the preference when not using
// multilingual spellchecking.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       MultilingualToSingleLanguagePreferenceMigration) {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetContext());
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  dictionaries.AppendString("fr");
  prefs->Set(prefs::kSpellCheckDictionaries, dictionaries);

  // Migrate the preference.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_EQ(1U, prefs->GetList(prefs::kSpellCheckDictionaries)->GetSize());
  std::string new_pref;
  ASSERT_TRUE(
      prefs->GetList(prefs::kSpellCheckDictionaries)->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
}

// If using multilingual spellchecking with spellchecking enabled, make sure the
// preference stays the same and spellchecking stays enabled.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       MultilingualPreferenceNotMigrated) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableMultilingualSpellChecker);

  PrefService* prefs = user_prefs::UserPrefs::Get(GetContext());
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  dictionaries.AppendString("fr");
  prefs->Set(prefs::kSpellCheckDictionaries, dictionaries);
  prefs->SetBoolean(prefs::kEnableContinuousSpellcheck, true);

  // Should not migrate any preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_TRUE(prefs->GetBoolean(prefs::kEnableContinuousSpellcheck));
  EXPECT_EQ(2U, prefs->GetList(prefs::kSpellCheckDictionaries)->GetSize());
  std::string pref;
  ASSERT_TRUE(
      prefs->GetList(prefs::kSpellCheckDictionaries)->GetString(0, &pref));
  EXPECT_EQ("en-US", pref);
  ASSERT_TRUE(
      prefs->GetList(prefs::kSpellCheckDictionaries)->GetString(1, &pref));
  EXPECT_EQ("fr", pref);
}

// If not using multilingual spellchecking and only one language is selected,
// the preference should not change.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       SingleLanguagePreferenceNotMigrated) {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetContext());
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  prefs->Set(prefs::kSpellCheckDictionaries, dictionaries);

  // Should not migrate any preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_EQ(1U, prefs->GetList(prefs::kSpellCheckDictionaries)->GetSize());
  std::string pref;
  ASSERT_TRUE(
      prefs->GetList(prefs::kSpellCheckDictionaries)->GetString(0, &pref));
  EXPECT_EQ("en-US", pref);
}
