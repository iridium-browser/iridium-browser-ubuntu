// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/test_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

namespace keys = tabs_constants;
namespace utils = extension_function_test_utils;

namespace {
using ExtensionTabsTest = InProcessBrowserTest;

class ExtensionWindowCreateTest : public InProcessBrowserTest {
 public:
  // Runs chrome.windows.create(), expecting an error.
  std::string RunCreateWindowExpectError(const std::string& args) {
    scoped_refptr<WindowsCreateFunction> function(new WindowsCreateFunction);
    function->set_extension(test_util::CreateEmptyExtension().get());
    return api_test_utils::RunFunctionAndReturnError(function.get(), args,
                                                     browser()->profile());
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Invalid window ID error.
  scoped_refptr<WindowsGetFunction> function = new WindowsGetFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf("[%u]", window_id + 1),
          browser()),
      keys::kWindowNotFoundError));

  // Basic window details.
  gfx::Rect bounds;
  if (browser()->window()->IsMinimized())
    bounds = browser()->window()->GetRestoredBounds();
  else
    bounds = browser()->window()->GetBounds();

  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[%u]", window_id),
          browser())));
  EXPECT_EQ(window_id, api_test_utils::GetInteger(result.get(), "id"));
  EXPECT_FALSE(api_test_utils::GetBoolean(result.get(), "incognito"));
  EXPECT_EQ("normal", api_test_utils::GetString(result.get(), "type"));
  EXPECT_EQ(bounds.x(), api_test_utils::GetInteger(result.get(), "left"));
  EXPECT_EQ(bounds.y(), api_test_utils::GetInteger(result.get(), "top"));
  EXPECT_EQ(bounds.width(), api_test_utils::GetInteger(result.get(), "width"));
  EXPECT_EQ(bounds.height(),
            api_test_utils::GetInteger(result.get(), "height"));

  // With "populate" enabled.
  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[%u, {\"populate\": true}]", window_id),
          browser())));

  EXPECT_EQ(window_id, api_test_utils::GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  base::ListValue* tabs = NULL;
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));

  // TODO(aa): Can't assume window is focused. On mac, calling Activate() from a
  // browser test doesn't seem to do anything, so can't test the opposite
  // either.
  EXPECT_EQ(browser()->window()->IsActive(),
            api_test_utils::GetBoolean(result.get(), "focused"));

  // TODO(aa): Minimized and maximized dimensions. Is there a way to set
  // minimize/maximize programmatically?

  // Popup.
  Browser* popup_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(),
                            browser()->host_desktop_type()));
  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf(
              "[%u]", ExtensionTabUtil::GetWindowId(popup_browser)),
          browser())));
  EXPECT_EQ("popup", api_test_utils::GetString(result.get(), "type"));

  // Incognito.
  Browser* incognito_browser = CreateIncognitoBrowser();
  int incognito_window_id = ExtensionTabUtil::GetWindowId(incognito_browser);

  // Without "include_incognito".
  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser()),
      keys::kWindowNotFoundError));

  // With "include_incognito".
  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser(),
          utils::INCLUDE_INCOGNITO)));
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetCurrentWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());
  Browser* new_browser = CreateBrowser(browser()->profile());
  int new_id = ExtensionTabUtil::GetWindowId(new_browser);

  // Get the current window using new_browser.
  scoped_refptr<WindowsGetCurrentFunction> function =
      new WindowsGetCurrentFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[]",
                                              new_browser)));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(new_id, api_test_utils::GetInteger(result.get(), "id"));
  base::ListValue* tabs = NULL;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  // Get the current window using the old window and make the tabs populated.
  function = new WindowsGetCurrentFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"populate\": true}]",
                                              browser())));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(window_id, api_test_utils::GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetAllWindows) {
  const size_t NUM_WINDOWS = 5;
  std::set<int> window_ids;
  std::set<int> result_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));

  for (size_t i = 0; i < NUM_WINDOWS - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Undocked DevTools window should not be accessible.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  scoped_refptr<WindowsGetAllFunction> function = new WindowsGetAllFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[]",
                                              browser())));

  base::ListValue* windows = result.get();
  EXPECT_EQ(NUM_WINDOWS, windows->GetSize());
  for (size_t i = 0; i < NUM_WINDOWS; ++i) {
    base::DictionaryValue* result_window = NULL;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(api_test_utils::GetInteger(result_window, "id"));

    // "populate" was not passed in so tabs are not populated.
    base::ListValue* tabs = NULL;
    EXPECT_FALSE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current browser instance ids.
  EXPECT_EQ(window_ids, result_ids);

  result_ids.clear();
  function = new WindowsGetAllFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"populate\": true}]",
                                              browser())));

  windows = result.get();
  EXPECT_EQ(NUM_WINDOWS, windows->GetSize());
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    base::DictionaryValue* result_window = NULL;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(api_test_utils::GetInteger(result_window, "id"));

    // "populate" was enabled so tabs should be populated.
    base::ListValue* tabs = NULL;
    EXPECT_TRUE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current browser instance ids.
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, UpdateNoPermissions) {
  // The test empty extension has no permissions, therefore it should not get
  // tab data in the function result.
  scoped_refptr<TabsUpdateFunction> update_tab_function(
      new TabsUpdateFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  update_tab_function->set_extension(empty_extension.get());
  // Without a callback the function will not generate a result.
  update_tab_function->set_has_callback(true);

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          update_tab_function.get(),
          "[null, {\"url\": \"about:blank\", \"pinned\": true}]",
          browser())));
  // The url is stripped since the extension does not have tab permissions.
  EXPECT_FALSE(result->HasKey("url"));
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "pinned"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForced) {
  static const char kArgsWithoutExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\"}]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Run without an explicit "incognito" param.
  scoped_refptr<WindowsCreateFunction> function(new WindowsCreateFunction());
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          kArgsWithoutExplicitIncognitoParam,
          browser(),
          utils::INCLUDE_INCOGNITO)));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()),
            api_test_utils::GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          kArgsWithoutExplicitIncognitoParam,
          incognito_browser,
          utils::INCLUDE_INCOGNITO)));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            api_test_utils::GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForcedAndNoArgs) {
  static const char kEmptyArgs[] = "[]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Run without an explicit "incognito" param.
  scoped_refptr<WindowsCreateFunction> function = new WindowsCreateFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              kEmptyArgs,
                                              browser(),
                                              utils::INCLUDE_INCOGNITO)));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()),
            api_test_utils::GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              kEmptyArgs,
                                              incognito_browser,
                                              utils::INCLUDE_INCOGNITO)));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            api_test_utils::GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DontCreateNormalWindowWhenIncognitoForced) {
  static const char kArgsWithExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\", \"incognito\": false }]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);

  // Run with an explicit "incognito" param.
  scoped_refptr<WindowsCreateFunction> function = new WindowsCreateFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgsWithExplicitIncognitoParam,
                                       browser()),
      keys::kIncognitoModeIsForced));

  // Now try opening a normal window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run with an explicit "incognito" param.
  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgsWithExplicitIncognitoParam,
                                       incognito_browser),
      keys::kIncognitoModeIsForced));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DontCreateIncognitoWindowWhenIncognitoDisabled) {
  static const char kArgs[] =
      "[{\"url\": \"about:blank\", \"incognito\": true }]";

  Browser* incognito_browser = CreateIncognitoBrowser();
  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  // Run in normal window.
  scoped_refptr<WindowsCreateFunction> function = new WindowsCreateFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgs,
                                       browser()),
      keys::kIncognitoModeIsDisabled));

  // Run in incognito window.
  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgs,
                                       incognito_browser),
      keys::kIncognitoModeIsDisabled));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryCurrentWindowTabs) {
  const size_t kExtraWindows = 3;
  for (size_t i = 0; i < kExtraWindows; ++i)
    CreateBrowser(browser()->profile());

  GURL url(url::kAboutBlankURL);
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_LINK);
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Get tabs in the 'current' window called from non-focused browser.
  scoped_refptr<TabsQueryFunction> function = new TabsQueryFunction();
  function->set_extension(test_util::CreateEmptyExtension().get());
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"currentWindow\":true}]",
                                              browser())));

  base::ListValue* result_tabs = result.get();
  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    base::DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_EQ(window_id,
              api_test_utils::GetInteger(result_tab, keys::kWindowIdKey));
  }

  // Get tabs NOT in the 'current' window called from non-focused browser.
  function = new TabsQueryFunction();
  function->set_extension(test_util::CreateEmptyExtension().get());
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"currentWindow\":false}]",
                                              browser())));

  result_tabs = result.get();
  // We should have one tab for each extra window.
  EXPECT_EQ(kExtraWindows, result_tabs->GetSize());
  for (size_t i = 0; i < kExtraWindows; ++i) {
    base::DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_NE(window_id,
              api_test_utils::GetInteger(result_tab, keys::kWindowIdKey));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryAllTabsWithDevTools) {
  const size_t kNumWindows = 3;
  std::set<int> window_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));
  for (size_t i = 0; i < kNumWindows - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Undocked DevTools window should not be accessible.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  // Get tabs in the 'current' window called from non-focused browser.
  scoped_refptr<TabsQueryFunction> function = new TabsQueryFunction();
  function->set_extension(test_util::CreateEmptyExtension().get());
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{}]",
                                              browser())));

  std::set<int> result_ids;
  base::ListValue* result_tabs = result.get();
  // We should have one tab per browser except for DevTools.
  EXPECT_EQ(kNumWindows, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    base::DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    result_ids.insert(
        api_test_utils::GetInteger(result_tab, keys::kWindowIdKey));
  }
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DontCreateTabInClosingPopupWindow) {
  // Test creates new popup window, closes it right away and then tries to open
  // a new tab in it. Tab should not be opened in the popup window, but in a
  // tabbed browser window.
  Browser* popup_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(),
                            browser()->host_desktop_type()));
  int window_id = ExtensionTabUtil::GetWindowId(popup_browser);
  chrome::CloseWindow(popup_browser);

  scoped_refptr<TabsCreateFunction> create_tab_function(
      new TabsCreateFunction());
  create_tab_function->set_extension(test_util::CreateEmptyExtension().get());
  // Without a callback the function will not generate a result.
  create_tab_function->set_has_callback(true);

  static const char kNewBlankTabArgs[] =
      "[{\"url\": \"about:blank\", \"windowId\": %u}]";

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          create_tab_function.get(),
          base::StringPrintf(kNewBlankTabArgs, window_id),
          browser())));

  EXPECT_NE(window_id, api_test_utils::GetInteger(result.get(), "windowId"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, InvalidUpdateWindowState) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  static const char kArgsMinimizedWithFocus[] =
      "[%u, {\"state\": \"minimized\", \"focused\": true}]";
  scoped_refptr<WindowsUpdateFunction> function = new WindowsUpdateFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithFocus, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithoutFocus[] =
      "[%u, {\"state\": \"maximized\", \"focused\": false}]";
  function = new WindowsUpdateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithoutFocus, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMinimizedWithBounds[] =
      "[%u, {\"state\": \"minimized\", \"width\": 500}]";
  function = new WindowsUpdateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithBounds, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithBounds[] =
      "[%u, {\"state\": \"maximized\", \"width\": 500}]";
  function = new WindowsUpdateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithBounds, window_id),
          browser()),
      keys::kInvalidWindowStateError));
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, AcceptState) {
  scoped_refptr<WindowsCreateFunction> function(new WindowsCreateFunction());
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());

  scoped_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"state\": \"minimized\"}]", browser(),
          utils::INCLUDE_INCOGNITO)));
  int window_id = api_test_utils::GetInteger(result.get(), "id");
  std::string error;
  Browser* new_window = ExtensionTabUtil::GetBrowserFromWindowID(
      function.get(), window_id, &error);
  EXPECT_TRUE(error.empty());
#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
  // DesktopWindowTreeHostX11::IsMinimized() relies on an asynchronous update
  // from the window server.
  EXPECT_TRUE(new_window->window()->IsMinimized());
#endif

// TODO(limasdf): Flaky on mac. See http://crbug.com/482433.
#if !defined(OS_MACOSX)
  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"state\": \"fullscreen\"}]", browser(),
      utils::INCLUDE_INCOGNITO)));
  window_id = api_test_utils::GetInteger(result.get(), "id");
  new_window = ExtensionTabUtil::GetBrowserFromWindowID(function.get(),
                                                        window_id, &error);
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(new_window->window()->IsFullscreen());
#endif  // !defined(OS_MACOSX)
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, ValidateCreateWindowState) {
  EXPECT_TRUE(
      MatchPattern(RunCreateWindowExpectError(
                       "[{\"state\": \"fullscreen\", \"type\": \"panel\"}]"),
                   keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      MatchPattern(RunCreateWindowExpectError(
                       "[{\"state\": \"maximized\", \"type\": \"panel\"}]"),
                   keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      MatchPattern(RunCreateWindowExpectError(
                       "[{\"state\": \"minimized\", \"type\": \"panel\"}]"),
                   keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      MatchPattern(RunCreateWindowExpectError(
                       "[{\"state\": \"minimized\", \"focused\": true}]"),
                   keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      MatchPattern(RunCreateWindowExpectError(
                       "[{\"state\": \"maximized\", \"focused\": false}]"),
                   keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      MatchPattern(RunCreateWindowExpectError(
                       "[{\"state\": \"fullscreen\", \"focused\": false}]"),
                   keys::kInvalidWindowStateError));
  EXPECT_TRUE(MatchPattern(RunCreateWindowExpectError(
                               "[{\"state\": \"minimized\", \"width\": 500}]"),
                           keys::kInvalidWindowStateError));
  EXPECT_TRUE(MatchPattern(RunCreateWindowExpectError(
                               "[{\"state\": \"maximized\", \"width\": 500}]"),
                           keys::kInvalidWindowStateError));
  EXPECT_TRUE(MatchPattern(RunCreateWindowExpectError(
                               "[{\"state\": \"fullscreen\", \"width\": 500}]"),
                           keys::kInvalidWindowStateError));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DuplicateTab) {
  content::OpenURLParams params(GURL(url::kAboutBlankURL),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int tab_index = -1;
  TabStripModel* tab_strip;
  ExtensionTabUtil::GetTabStripModel(web_contents, &tab_strip, &tab_index);

  scoped_refptr<TabsDuplicateFunction> duplicate_tab_function(
      new TabsDuplicateFunction());
  scoped_ptr<base::DictionaryValue> test_extension_value(
      api_test_utils::ParseDictionary(
          "{\"name\": \"Test\", \"version\": \"1.0\", \"permissions\": "
          "[\"tabs\"]}"));
  scoped_refptr<Extension> empty_tab_extension(
      api_test_utils::CreateExtension(test_extension_value.get()));
  duplicate_tab_function->set_extension(empty_tab_extension.get());
  duplicate_tab_function->set_has_callback(true);

  scoped_ptr<base::DictionaryValue> duplicate_result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          duplicate_tab_function.get(), base::StringPrintf("[%u]", tab_id),
          browser())));

  int duplicate_tab_id =
      api_test_utils::GetInteger(duplicate_result.get(), "id");
  int duplicate_tab_window_id =
      api_test_utils::GetInteger(duplicate_result.get(), "windowId");
  int duplicate_tab_index =
      api_test_utils::GetInteger(duplicate_result.get(), "index");
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, duplicate_result->GetType());
  // Duplicate tab id should be different from the original tab id.
  EXPECT_NE(tab_id, duplicate_tab_id);
  EXPECT_EQ(window_id, duplicate_tab_window_id);
  EXPECT_EQ(tab_index + 1, duplicate_tab_index);
  // The test empty tab extension has tabs permissions, therefore
  // |duplicate_result| should contain url, title, and faviconUrl
  // in the function result.
  EXPECT_TRUE(utils::HasPrivacySensitiveFields(duplicate_result.get()));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DuplicateTabNoPermission) {
  content::OpenURLParams params(GURL(url::kAboutBlankURL),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int tab_index = -1;
  TabStripModel* tab_strip;
  ExtensionTabUtil::GetTabStripModel(web_contents, &tab_strip, &tab_index);

  scoped_refptr<TabsDuplicateFunction> duplicate_tab_function(
      new TabsDuplicateFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  duplicate_tab_function->set_extension(empty_extension.get());
  duplicate_tab_function->set_has_callback(true);

  scoped_ptr<base::DictionaryValue> duplicate_result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          duplicate_tab_function.get(), base::StringPrintf("[%u]", tab_id),
          browser())));

  int duplicate_tab_id =
      api_test_utils::GetInteger(duplicate_result.get(), "id");
  int duplicate_tab_window_id =
      api_test_utils::GetInteger(duplicate_result.get(), "windowId");
  int duplicate_tab_index =
      api_test_utils::GetInteger(duplicate_result.get(), "index");
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, duplicate_result->GetType());
  // Duplicate tab id should be different from the original tab id.
  EXPECT_NE(tab_id, duplicate_tab_id);
  EXPECT_EQ(window_id, duplicate_tab_window_id);
  EXPECT_EQ(tab_index + 1, duplicate_tab_index);
  // The test empty extension has no permissions, therefore |duplicate_result|
  // should not contain url, title, and faviconUrl in the function result.
  EXPECT_FALSE(utils::HasPrivacySensitiveFields(duplicate_result.get()));
}

// Tester class for the tabs.zoom* api functions.
class ExtensionTabsZoomTest : public ExtensionTabsTest {
 public:
  void SetUpOnMainThread() override;

  // Runs chrome.tabs.setZoom().
  bool RunSetZoom(int tab_id, double zoom_factor);

  // Runs chrome.tabs.getZoom().
  testing::AssertionResult RunGetZoom(int tab_id, double* zoom_factor);

  // Runs chrome.tabs.setZoomSettings().
  bool RunSetZoomSettings(int tab_id, const char* mode, const char* scope);

  // Runs chrome.tabs.getZoomSettings().
  testing::AssertionResult RunGetZoomSettings(int tab_id,
                                              std::string* mode,
                                              std::string* scope);

  // Runs chrome.tabs.getZoomSettings() and returns default zoom.
  testing::AssertionResult RunGetDefaultZoom(int tab_id,
                                             double* default_zoom_factor);

  // Runs chrome.tabs.setZoom(), expecting an error.
  std::string RunSetZoomExpectError(int tab_id,
                                    double zoom_factor);

  // Runs chrome.tabs.setZoomSettings(), expecting an error.
  std::string RunSetZoomSettingsExpectError(int tab_id,
                                            const char* mode,
                                            const char* scope);

  content::WebContents* OpenUrlAndWaitForLoad(const GURL& url);

 private:
  scoped_refptr<Extension> extension_;
};

void ExtensionTabsZoomTest::SetUpOnMainThread() {
  ExtensionTabsTest::SetUpOnMainThread();
  extension_ = test_util::CreateEmptyExtension();
}

bool ExtensionTabsZoomTest::RunSetZoom(int tab_id, double zoom_factor) {
  scoped_refptr<TabsSetZoomFunction> set_zoom_function(
      new TabsSetZoomFunction());
  set_zoom_function->set_extension(extension_.get());
  set_zoom_function->set_has_callback(true);

  return utils::RunFunction(
      set_zoom_function.get(),
      base::StringPrintf("[%u, %lf]", tab_id, zoom_factor), browser(),
      extension_function_test_utils::NONE);
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetZoom(
    int tab_id,
    double* zoom_factor) {
  scoped_refptr<TabsGetZoomFunction> get_zoom_function(
      new TabsGetZoomFunction());
  get_zoom_function->set_extension(extension_.get());
  get_zoom_function->set_has_callback(true);

  scoped_ptr<base::Value> get_zoom_result(
      utils::RunFunctionAndReturnSingleResult(
          get_zoom_function.get(),
          base::StringPrintf("[%u]", tab_id),
          browser()));

  if (!get_zoom_result)
    return testing::AssertionFailure() << "no result";
  if (!get_zoom_result->GetAsDouble(zoom_factor))
    return testing::AssertionFailure() << "result was not a double";

  return testing::AssertionSuccess();
}

bool ExtensionTabsZoomTest::RunSetZoomSettings(int tab_id,
                                               const char* mode,
                                               const char* scope) {
  scoped_refptr<TabsSetZoomSettingsFunction> set_zoom_settings_function(
      new TabsSetZoomSettingsFunction());
  set_zoom_settings_function->set_extension(extension_.get());

  std::string args;
  if (scope) {
    args = base::StringPrintf("[%u, {\"mode\": \"%s\", \"scope\": \"%s\"}]",
                              tab_id, mode, scope);
  } else {
    args = base::StringPrintf("[%u, {\"mode\": \"%s\"}]", tab_id, mode);
  }

  return utils::RunFunction(set_zoom_settings_function.get(),
                            args,
                            browser(),
                            extension_function_test_utils::NONE);
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetZoomSettings(
    int tab_id,
    std::string* mode,
    std::string* scope) {
  DCHECK(mode);
  DCHECK(scope);
  scoped_refptr<TabsGetZoomSettingsFunction> get_zoom_settings_function(
      new TabsGetZoomSettingsFunction());
  get_zoom_settings_function->set_extension(extension_.get());
  get_zoom_settings_function->set_has_callback(true);

  scoped_ptr<base::DictionaryValue> get_zoom_settings_result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          get_zoom_settings_function.get(),
          base::StringPrintf("[%u]", tab_id),
          browser())));

  if (!get_zoom_settings_result)
    return testing::AssertionFailure() << "no result";

  *mode = api_test_utils::GetString(get_zoom_settings_result.get(), "mode");
  *scope = api_test_utils::GetString(get_zoom_settings_result.get(), "scope");

  return testing::AssertionSuccess();
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetDefaultZoom(
    int tab_id,
    double* default_zoom_factor) {
  DCHECK(default_zoom_factor);
  scoped_refptr<TabsGetZoomSettingsFunction> get_zoom_settings_function(
      new TabsGetZoomSettingsFunction());
  get_zoom_settings_function->set_extension(extension_.get());
  get_zoom_settings_function->set_has_callback(true);

  scoped_ptr<base::DictionaryValue> get_zoom_settings_result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          get_zoom_settings_function.get(),
          base::StringPrintf("[%u]", tab_id),
          browser())));

  if (!get_zoom_settings_result)
    return testing::AssertionFailure() << "no result";

  if (!get_zoom_settings_result->GetDouble("defaultZoomFactor",
                                           default_zoom_factor)) {
    return testing::AssertionFailure()
           << "default zoom factor not found in result";
  }

  return testing::AssertionSuccess();
}

std::string ExtensionTabsZoomTest::RunSetZoomExpectError(int tab_id,
                                                         double zoom_factor) {
  scoped_refptr<TabsSetZoomFunction> set_zoom_function(
      new TabsSetZoomFunction());
  set_zoom_function->set_extension(extension_.get());
  set_zoom_function->set_has_callback(true);

  return utils::RunFunctionAndReturnError(
      set_zoom_function.get(),
      base::StringPrintf("[%u, %lf]", tab_id, zoom_factor),
      browser());
}

std::string ExtensionTabsZoomTest::RunSetZoomSettingsExpectError(
    int tab_id,
    const char* mode,
    const char* scope) {
  scoped_refptr<TabsSetZoomSettingsFunction> set_zoom_settings_function(
      new TabsSetZoomSettingsFunction());
  set_zoom_settings_function->set_extension(extension_.get());

  return utils::RunFunctionAndReturnError(set_zoom_settings_function.get(),
                                          base::StringPrintf(
                                              "[%u, {\"mode\": \"%s\", "
                                              "\"scope\": \"%s\"}]",
                                              tab_id,
                                              mode,
                                              scope),
                                          browser());
}

content::WebContents* ExtensionTabsZoomTest::OpenUrlAndWaitForLoad(
    const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      url,
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  return  browser()->tab_strip_model()->GetActiveWebContents();
}

namespace {

double GetZoomLevel(const content::WebContents* web_contents) {
  return ui_zoom::ZoomController::FromWebContents(web_contents)->GetZoomLevel();
}

content::OpenURLParams GetOpenParams(const char* url) {
  return content::OpenURLParams(GURL(url),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                false);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, SetAndGetZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Test default values before we set anything.
  double zoom_factor = -1;
  EXPECT_TRUE(RunGetZoom(tab_id, &zoom_factor));
  EXPECT_EQ(1.0, zoom_factor);

  // Test chrome.tabs.setZoom().
  const double kZoomLevel = 0.8;
  EXPECT_TRUE(RunSetZoom(tab_id, kZoomLevel));
  EXPECT_EQ(kZoomLevel,
            content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents)));

  // Test chrome.tabs.getZoom().
  zoom_factor = -1;
  EXPECT_TRUE(RunGetZoom(tab_id, &zoom_factor));
  EXPECT_EQ(kZoomLevel, zoom_factor);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, GetDefaultZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  ui_zoom::ZoomController* zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents);
  double default_zoom_factor = -1.0;
  EXPECT_TRUE(RunGetDefaultZoom(tab_id, &default_zoom_factor));
  EXPECT_TRUE(content::ZoomValuesEqual(
      zoom_controller->GetDefaultZoomLevel(),
      content::ZoomFactorToZoomLevel(default_zoom_factor)));

  // Change the default zoom level and verify GetDefaultZoom returns the
  // correct value.
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          web_contents->GetBrowserContext(), web_contents->GetSiteInstance());
  chrome::ChromeZoomLevelPrefs* zoom_prefs =
      static_cast<chrome::ChromeZoomLevelPrefs*>(
          partition->GetZoomLevelDelegate());

  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();
  zoom_prefs->SetDefaultZoomLevelPref(default_zoom_level + 0.5);
  default_zoom_factor = -1.0;
  EXPECT_TRUE(RunGetDefaultZoom(tab_id, &default_zoom_factor));
  EXPECT_TRUE(content::ZoomValuesEqual(
      default_zoom_level + 0.5,
      content::ZoomFactorToZoomLevel(default_zoom_factor)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, SetToDefaultZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  ui_zoom::ZoomController* zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents);
  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();
  double new_default_zoom_level = default_zoom_level + 0.42;

  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          web_contents->GetBrowserContext(), web_contents->GetSiteInstance());
  chrome::ChromeZoomLevelPrefs* zoom_prefs =
      static_cast<chrome::ChromeZoomLevelPrefs*>(
          partition->GetZoomLevelDelegate());

  zoom_prefs->SetDefaultZoomLevelPref(new_default_zoom_level);

  double observed_zoom_factor = -1.0;
  EXPECT_TRUE(RunSetZoom(tab_id, 0.0));
  EXPECT_TRUE(RunGetZoom(tab_id, &observed_zoom_factor));
  EXPECT_TRUE(content::ZoomValuesEqual(
      new_default_zoom_level,
      content::ZoomFactorToZoomLevel(observed_zoom_factor)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, ZoomSettings) {
  // In this test we need two URLs that (1) represent real pages (i.e. they
  // load without causing an error page load), (2) have different domains, and
  // (3) are zoomable by the extension API (this last condition rules out
  // chrome:// urls). We achieve this by noting that about:blank meets these
  // requirements, allowing us to spin up a spawned http server on localhost to
  // get the other domain.
  net::SpawnedTestServer http_server(
      net::SpawnedTestServer::TYPE_HTTP,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(http_server.Start());

  GURL url_A = http_server.GetURL("files/simple.html");
  GURL url_B("about:blank");

  // Tabs A1 and A2 are navigated to the same origin, while B is navigated
  // to a different one.
  content::WebContents* web_contents_A1 = OpenUrlAndWaitForLoad(url_A);
  content::WebContents* web_contents_A2 = OpenUrlAndWaitForLoad(url_A);
  content::WebContents* web_contents_B = OpenUrlAndWaitForLoad(url_B);

  int tab_id_A1 = ExtensionTabUtil::GetTabId(web_contents_A1);
  int tab_id_A2 = ExtensionTabUtil::GetTabId(web_contents_A2);
  int tab_id_B = ExtensionTabUtil::GetTabId(web_contents_B);

  ASSERT_FLOAT_EQ(
      1.f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  ASSERT_FLOAT_EQ(
      1.f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
  ASSERT_FLOAT_EQ(
      1.f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_B)));

  // Test per-origin automatic zoom settings.
  EXPECT_TRUE(RunSetZoom(tab_id_B, 1.f));
  EXPECT_TRUE(RunSetZoom(tab_id_A2, 1.1f));
  EXPECT_FLOAT_EQ(
      1.1f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(
      1.1f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
  EXPECT_FLOAT_EQ(1.f,
                  content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_B)));

  // Test per-tab automatic zoom settings.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "automatic", "per-tab"));
  EXPECT_TRUE(RunSetZoom(tab_id_A1, 1.2f));
  EXPECT_FLOAT_EQ(
      1.2f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(
      1.1f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));

  // Test 'manual' mode.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "manual", NULL));
  EXPECT_TRUE(RunSetZoom(tab_id_A1, 1.3f));
  EXPECT_FLOAT_EQ(
      1.3f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(
      1.1f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));

  // Test 'disabled' mode, which will reset A1's zoom to 1.f.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "disabled", NULL));
  std::string error = RunSetZoomExpectError(tab_id_A1, 1.4f);
  EXPECT_TRUE(MatchPattern(error, keys::kCannotZoomDisabledTabError));
  EXPECT_FLOAT_EQ(
      1.f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  // We should still be able to zoom A2 though.
  EXPECT_TRUE(RunSetZoom(tab_id_A2, 1.4f));
  EXPECT_FLOAT_EQ(
      1.4f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, PerTabResetsOnNavigation) {
  net::SpawnedTestServer http_server(
      net::SpawnedTestServer::TYPE_HTTP,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(http_server.Start());

  GURL url_A = http_server.GetURL("files/simple.html");
  GURL url_B("about:blank");

  content::WebContents* web_contents = OpenUrlAndWaitForLoad(url_A);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  EXPECT_TRUE(RunSetZoomSettings(tab_id, "automatic", "per-tab"));

  std::string mode;
  std::string scope;
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-tab", scope);

  // Navigation of tab should reset mode to per-origin.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url_B,
                                                            1);
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-origin", scope);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, GetZoomSettings) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  std::string mode;
  std::string scope;

  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-origin", scope);

  EXPECT_TRUE(RunSetZoomSettings(tab_id, "automatic", "per-tab"));
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));

  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-tab", scope);

  std::string error =
      RunSetZoomSettingsExpectError(tab_id, "manual", "per-origin");
  EXPECT_TRUE(MatchPattern(error,
                           keys::kPerOriginOnlyInAutomaticError));
  error =
      RunSetZoomSettingsExpectError(tab_id, "disabled", "per-origin");
  EXPECT_TRUE(MatchPattern(error,
                           keys::kPerOriginOnlyInAutomaticError));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, CannotZoomInvalidTab) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  int bogus_id = tab_id + 100;
  std::string error = RunSetZoomExpectError(bogus_id, 3.14159);
  EXPECT_TRUE(MatchPattern(error, keys::kTabNotFoundError));

  error = RunSetZoomSettingsExpectError(bogus_id, "manual", "per-tab");
  EXPECT_TRUE(MatchPattern(error, keys::kTabNotFoundError));

  const char kNewTestTabArgs[] = "chrome://version";
  params = GetOpenParams(kNewTestTabArgs);
  web_contents = browser()->OpenURL(params);
  tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Test chrome.tabs.setZoom().
  error = RunSetZoomExpectError(tab_id, 3.14159);
  EXPECT_TRUE(MatchPattern(error, manifest_errors::kCannotAccessChromeUrl));

  // chrome.tabs.setZoomSettings().
  error = RunSetZoomSettingsExpectError(tab_id, "manual", "per-tab");
  EXPECT_TRUE(MatchPattern(error, manifest_errors::kCannotAccessChromeUrl));
}

}  // namespace extensions
