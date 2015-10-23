// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"

#include <vector>

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

using content::RenderViewHost;
using content::WebContents;

namespace {

// A mock context menu used in this test. This class overrides virtual methods
// derived from the RenderViewContextMenuProxy class to monitor calls from the
// SpellingMenuObserver class.
class MockRenderViewContextMenu : public RenderViewContextMenuProxy {
 public:
  // A menu item used in this test. This test uses a vector of this struct to
  // hold menu items added by this test.
  struct MockMenuItem {
    MockMenuItem()
        : command_id(0),
          enabled(false),
          checked(false),
          hidden(true) {
    }
    int command_id;
    bool enabled;
    bool checked;
    bool hidden;
    base::string16 title;
  };

  explicit MockRenderViewContextMenu(bool incognito);
  virtual ~MockRenderViewContextMenu();

  // RenderViewContextMenuProxy implementation.
  void AddMenuItem(int command_id, const base::string16& title) override;
  void AddCheckItem(int command_id, const base::string16& title) override;
  void AddSeparator() override;
  void AddSubMenu(int command_id,
                  const base::string16& label,
                  ui::MenuModel* model) override;
  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const base::string16& title) override;
  RenderViewHost* GetRenderViewHost() const override;
  WebContents* GetWebContents() const override;
  content::BrowserContext* GetBrowserContext() const override;

  // Attaches a RenderViewContextMenuObserver to be tested.
  void SetObserver(RenderViewContextMenuObserver* observer);

  // Returns the number of items added by the test.
  size_t GetMenuSize() const;

  // Returns the i-th item.
  bool GetMenuItem(size_t i, MockMenuItem* item) const;

  // Returns the writable profile used in this test.
  PrefService* GetPrefs();

 private:
  // An observer used for initializing the status of menu items added in this
  // test. A test should delete this RenderViewContextMenuObserver object.
  RenderViewContextMenuObserver* observer_;

  // A dummy profile used in this test. Call GetPrefs() when a test needs to
  // change this profile and use PrefService methods.
  scoped_ptr<TestingProfile> original_profile_;

  // Either |original_profile_| or its incognito profile.
  Profile* profile_;

  // A list of menu items added by the SpellingMenuObserver class.
  std::vector<MockMenuItem> items_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderViewContextMenu);
};

MockRenderViewContextMenu::MockRenderViewContextMenu(bool incognito)
    : observer_(NULL) {
  original_profile_ = TestingProfile::Builder().Build();
  profile_ = incognito ? original_profile_->GetOffTheRecordProfile()
                       : original_profile_.get();
}

MockRenderViewContextMenu::~MockRenderViewContextMenu() {
}

void MockRenderViewContextMenu::AddMenuItem(int command_id,
                                            const base::string16& title) {
  MockMenuItem item;
  item.command_id = command_id;
  item.enabled = observer_->IsCommandIdEnabled(command_id);
  item.checked = false;
  item.hidden = false;
  item.title = title;
  items_.push_back(item);
}

void MockRenderViewContextMenu::AddCheckItem(int command_id,
                                             const base::string16& title) {
  MockMenuItem item;
  item.command_id = command_id;
  item.enabled = observer_->IsCommandIdEnabled(command_id);
  item.checked = observer_->IsCommandIdChecked(command_id);
  item.hidden = false;
  item.title = title;
  items_.push_back(item);
}

void MockRenderViewContextMenu::AddSeparator() {
  MockMenuItem item;
  item.command_id = -1;
  item.enabled = false;
  item.checked = false;
  item.hidden = false;
  items_.push_back(item);
}

void MockRenderViewContextMenu::AddSubMenu(int command_id,
                                           const base::string16& label,
                                           ui::MenuModel* model) {
  MockMenuItem item;
  item.command_id = -1;
  item.enabled = false;
  item.checked = false;
  item.hidden = false;
  items_.push_back(item);
}

void MockRenderViewContextMenu::UpdateMenuItem(int command_id,
                                               bool enabled,
                                               bool hidden,
                                               const base::string16& title) {
  for (std::vector<MockMenuItem>::iterator it = items_.begin();
       it != items_.end(); ++it) {
    if (it->command_id == command_id) {
      it->enabled = enabled;
      it->hidden = hidden;
      it->title = title;
      return;
    }
  }

  // The SpellingMenuObserver class tries to change a menu item not added by the
  // class. This is an unexpected behavior and we should stop now.
  FAIL();
}

RenderViewHost* MockRenderViewContextMenu::GetRenderViewHost() const {
  return NULL;
}

WebContents* MockRenderViewContextMenu::GetWebContents() const {
  return NULL;
}

content::BrowserContext* MockRenderViewContextMenu::GetBrowserContext() const {
  return profile_;
}

size_t MockRenderViewContextMenu::GetMenuSize() const {
  return items_.size();
}

bool MockRenderViewContextMenu::GetMenuItem(size_t i,
                                            MockMenuItem* item) const {
  if (i >= items_.size())
    return false;
  item->command_id = items_[i].command_id;
  item->enabled = items_[i].enabled;
  item->checked = items_[i].checked;
  item->hidden = items_[i].hidden;
  item->title = items_[i].title;
  return true;
}

void MockRenderViewContextMenu::SetObserver(
    RenderViewContextMenuObserver* observer) {
  observer_ = observer;
}

PrefService* MockRenderViewContextMenu::GetPrefs() {
  return profile_->GetPrefs();
}

// A test class used in this file. This test should be a browser test because it
// accesses resources.
class SpellingMenuObserverTest : public InProcessBrowserTest {
 public:
  SpellingMenuObserverTest();

  void SetUpOnMainThread() override { Reset(false); }

  void TearDownOnMainThread() override {
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    observer_.reset();
    menu_.reset(new MockRenderViewContextMenu(incognito));
    observer_.reset(new SpellingMenuObserver(menu_.get()));
    menu_->SetObserver(observer_.get());
  }

  void InitMenu(const char* word, const char* suggestion) {
    content::ContextMenuParams params;
    params.is_editable = true;
    params.misspelled_word = base::ASCIIToUTF16(word);
    params.dictionary_suggestions.clear();
    if (suggestion)
      params.dictionary_suggestions.push_back(base::ASCIIToUTF16(suggestion));
    observer_->InitMenu(params);
  }

  void ForceSuggestMode() {
    menu()->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);
    // Force a non-empty and non-"en" locale so SUGGEST is available.
    base::ListValue dictionary;
    dictionary.AppendString("fr");
    menu()->GetPrefs()->Set(prefs::kSpellCheckDictionaries, dictionary);

    ASSERT_TRUE(SpellingServiceClient::IsAvailable(
        menu()->GetBrowserContext(), SpellingServiceClient::SUGGEST));
    ASSERT_FALSE(SpellingServiceClient::IsAvailable(
        menu()->GetBrowserContext(), SpellingServiceClient::SPELLCHECK));
  }

  ~SpellingMenuObserverTest() override;
  MockRenderViewContextMenu* menu() { return menu_.get(); }
  SpellingMenuObserver* observer() { return observer_.get(); }
 private:
  scoped_ptr<SpellingMenuObserver> observer_;
  scoped_ptr<MockRenderViewContextMenu> menu_;
  DISALLOW_COPY_AND_ASSIGN(SpellingMenuObserverTest);
};

SpellingMenuObserverTest::SpellingMenuObserverTest() {
}

SpellingMenuObserverTest::~SpellingMenuObserverTest() {
}

}  // namespace

// Tests that right-clicking a correct word does not add any items.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, InitMenuWithCorrectWord) {
  InitMenu("", NULL);
  EXPECT_EQ(static_cast<size_t>(0), menu()->GetMenuSize());
}

// Tests that right-clicking a misspelled word adds four items:
// "No spelling suggestions", "Add to dictionary", "Ask Google for suggestions",
// and a separator.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, InitMenuWithMisspelledWord) {
  InitMenu("wiimode", NULL);
  EXPECT_EQ(static_cast<size_t>(4), menu()->GetMenuSize());

  // Read all the context-menu items added by this test and verify they are
  // expected ones. We do not check the item titles to prevent resource changes
  // from breaking this test. (I think it is not expected by those who change
  // resources.)
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
  menu()->GetMenuItem(3, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
}

// Tests that right-clicking a correct word when we enable spelling-service
// integration to verify an item "Ask Google for suggestions" is checked. Even
// though this meanu itself does not add this item, its sub-menu adds the item
// and calls SpellingMenuObserver::IsChecked() to check it.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       EnableSpellingServiceWithCorrectWord) {
  menu()->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);
  InitMenu("", NULL);

  EXPECT_TRUE(
      observer()->IsCommandIdChecked(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE));
}

// Tests that right-clicking a misspelled word when we enable spelling-service
// integration to verify an item "Ask Google for suggestions" is checked. (This
// test does not actually send JSON-RPC requests to the service because it makes
// this test flaky.)
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, EnableSpellingService) {
  menu()->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);
  base::ListValue dictionary;
  menu()->GetPrefs()->Set(prefs::kSpellCheckDictionaries, dictionary);

  InitMenu("wiimode", NULL);
  EXPECT_EQ(static_cast<size_t>(4), menu()->GetMenuSize());

  // To avoid duplicates, this test reads only the "Ask Google for suggestions"
  // item and verifies it is enabled and checked.
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.checked);
  EXPECT_FALSE(item.hidden);
}

// Test that there will be a separator after "no suggestions" if
// SpellingServiceClient::SUGGEST is on.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, SeparatorAfterSuggestions) {
  ForceSuggestMode();
  InitMenu("jhhj", NULL);

  // The test should see a top separator, "No spelling suggestions",
  // "No more Google suggestions" (from SpellingService) and a separator
  // as the first four items, then possibly more (not relevant here).
  EXPECT_LT(4U, menu()->GetMenuSize());

  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(3, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
}

// Test that we don't show "No more suggestions from Google" if the spelling
// service is enabled and that there is only one suggestion.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       NoMoreSuggestionsNotDisplayed) {
  menu()->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);

  // Force a non-empty locale so SPELLCHECK is available.
  base::ListValue dictionary;
  dictionary.AppendString("en");
  menu()->GetPrefs()->Set(prefs::kSpellCheckDictionaries, dictionary);

  EXPECT_TRUE(SpellingServiceClient::IsAvailable(
      menu()->GetBrowserContext(), SpellingServiceClient::SPELLCHECK));
  InitMenu("asdfkj", "asdf");

  // The test should see a separator, a suggestion and another separator
  // as the first two items, then possibly more (not relevant here).
  EXPECT_LT(3U, menu()->GetMenuSize());

  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
}

// Test that "Ask Google For Suggestions" is grayed out when using an
// off the record profile.
// TODO(rlp): Include graying out of autocorrect in this test when autocorrect
// is functional.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       NoSpellingServiceWhenOffTheRecord) {
  // Create a menu in an incognito profile.
  Reset(true);

  // This means spellchecking is allowed. Default is that the service is
  // contacted but this test makes sure that if profile is incognito, that
  // is not an option.
  menu()->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);

  // Force a non-empty locale so SUGGEST normally would be available.
  base::ListValue dictionary;
  dictionary.AppendString("en");
  menu()->GetPrefs()->Set(prefs::kSpellCheckDictionaries, dictionary);

  EXPECT_FALSE(SpellingServiceClient::IsAvailable(
      menu()->GetBrowserContext(), SpellingServiceClient::SUGGEST));
  EXPECT_FALSE(SpellingServiceClient::IsAvailable(
      menu()->GetBrowserContext(), SpellingServiceClient::SPELLCHECK));

  InitMenu("sjxdjiiiiii", NULL);

  // The test should see "No spelling suggestions" (from system checker).
  // They should not see "No more Google suggestions" (from SpellingService) or
  // a separator. The next 2 items should be "Add to Dictionary" followed
  // by "Ask Google for suggestions" which should be disabled.
  // TODO(rlp): add autocorrect here when it is functional.
  EXPECT_LT(3U, menu()->GetMenuSize());

  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
}

// Test that the menu is preceeded by a separator if there are any suggestions,
// or if the SpellingServiceClient is available
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, SuggestionsForceTopSeparator) {
  menu()->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, false);

  // First case: Misspelled word, no suggestions, no spellcheck service.
  InitMenu("asdfkj", NULL);
  // See SpellingMenuObserverTest.InitMenuWithMisspelledWord on why 4 items.
  EXPECT_EQ(static_cast<size_t>(4), menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_NE(-1, item.command_id);

  // Case #2. Misspelled word, suggestions, no spellcheck service.
  Reset(false);
  menu()->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, false);
  InitMenu("asdfkj", "asdf");

  // Expect at least separator and 4 default entries.
  EXPECT_LT(static_cast<size_t>(5), menu()->GetMenuSize());
  // This test only cares that the first one is a separator.
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);

  // Case #3. Misspelled word, suggestion service is on.
  Reset(false);
  ForceSuggestMode();
  InitMenu("asdfkj", NULL);

  // Should have at least 2 entries. Separator, suggestion.
  EXPECT_LT(2U, menu()->GetMenuSize());
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(-1, item.command_id);
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, item.command_id);
}
