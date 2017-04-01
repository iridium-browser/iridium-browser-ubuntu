// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"

#include <vector>

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_browser.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_tab.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image.h"
#include "ui/wm/core/window_animations.h"

namespace {

bool IsSettingsBrowser(Browser* browser) {
  // Normally this test is sufficient. TODO(stevenjb): Replace this with a
  // better mechanism (Settings WebUI or Browser type).
  if (chrome::IsTrustedPopupWindowWithScheme(browser, content::kChromeUIScheme))
    return true;
  // If a settings window navigates away from a kChromeUIScheme (e.g. after a
  // crash), the above may not be true, so also test against the known list
  // of settings browsers (which will not be valid during chrome::Navigate
  // which is why we still need the above test).
  if (chrome::SettingsWindowManager::GetInstance()->IsSettingsBrowser(browser))
    return true;
  return false;
}

}  // namespace

BrowserShortcutLauncherItemController::BrowserShortcutLauncherItemController(
    ChromeLauncherController* launcher_controller,
    ash::ShelfModel* shelf_model)
    : LauncherItemController(extension_misc::kChromeAppId,
                             "",
                             launcher_controller),
      shelf_model_(shelf_model) {}

BrowserShortcutLauncherItemController::
    ~BrowserShortcutLauncherItemController() {
}

void BrowserShortcutLauncherItemController::UpdateBrowserItemState() {
  // Determine the new browser's active state and change if necessary.
  int browser_index =
      shelf_model_->GetItemIndexForType(ash::TYPE_BROWSER_SHORTCUT);
  DCHECK_GE(browser_index, 0);
  ash::ShelfItem browser_item = shelf_model_->items()[browser_index];
  ash::ShelfItemStatus browser_status = ash::STATUS_CLOSED;

  aura::Window* window = ash::wm::GetActiveWindow();
  if (window) {
    // Check if the active browser / tab is a browser which is not an app,
    // a windowed app, a popup or any other item which is not a browser of
    // interest.
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (IsBrowserRepresentedInBrowserList(browser)) {
      browser_status = ash::STATUS_ACTIVE;
      // If an app that has item is running in active WebContents, browser item
      // status cannot be active.
      content::WebContents* contents =
          browser->tab_strip_model()->GetActiveWebContents();
      if (contents &&
          (launcher_controller()->GetShelfIDForWebContents(contents) !=
              browser_item.id))
        browser_status = ash::STATUS_RUNNING;
    }
  }

  if (browser_status == ash::STATUS_CLOSED) {
    for (auto* browser : *BrowserList::GetInstance()) {
      if (IsBrowserRepresentedInBrowserList(browser)) {
        browser_status = ash::STATUS_RUNNING;
        break;
      }
    }
  }

  if (browser_status != browser_item.status) {
    browser_item.status = browser_status;
    shelf_model_->Set(browser_index, browser_item);
  }
}

void BrowserShortcutLauncherItemController::SetShelfIDForBrowserWindowContents(
    Browser* browser,
    content::WebContents* web_contents) {
  // We need to set the window ShelfID for V1 applications since they are
  // content which might change and as such change the application type.
  if (!browser || !IsBrowserFromActiveUser(browser) ||
      IsSettingsBrowser(browser))
    return;

  ash::WmWindow::Get(browser->window()->GetNativeWindow())
      ->SetIntProperty(
          ash::WmWindowProperty::SHELF_ID,
          launcher_controller()->GetShelfIDForWebContents(web_contents));
}

void BrowserShortcutLauncherItemController::Launch(ash::LaunchSource source,
                                                   int event_flags) {
}

ash::ShelfItemDelegate::PerformedAction
BrowserShortcutLauncherItemController::Activate(ash::LaunchSource source) {
  Browser* last_browser =
      chrome::FindTabbedBrowser(launcher_controller()->profile(), true);

  if (!last_browser) {
    chrome::NewEmptyWindow(launcher_controller()->profile());
    return kNewWindowCreated;
  }

  return launcher_controller()->ActivateWindowOrMinimizeIfActive(
      last_browser->window(), GetApplicationList(0).size() == 2);
}

void BrowserShortcutLauncherItemController::Close() {
  for (auto* browser : GetListOfActiveBrowsers())
    browser->window()->Close();
}

ChromeLauncherAppMenuItems
BrowserShortcutLauncherItemController::GetApplicationList(int event_flags) {
  ChromeLauncherAppMenuItems items;
  bool found_tabbed_browser = false;
  // Add the application name to the menu.
  base::string16 app_title = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  items.push_back(
      base::MakeUnique<ChromeLauncherAppMenuItem>(app_title, nullptr, false));
  for (auto* browser : GetListOfActiveBrowsers()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (tab_strip->active_index() == -1)
      continue;
    if (browser->is_type_tabbed())
      found_tabbed_browser = true;
    if (!(event_flags & ui::EF_SHIFT_DOWN)) {
      content::WebContents* web_contents =
          tab_strip->GetWebContentsAt(tab_strip->active_index());
      gfx::Image app_icon = GetBrowserListIcon(web_contents);
      base::string16 title = GetBrowserListTitle(web_contents);
      items.push_back(base::MakeUnique<ChromeLauncherAppMenuItemBrowser>(
          title, &app_icon, browser, items.size() == 1));
    } else {
      for (int index = 0; index  < tab_strip->count(); ++index) {
        content::WebContents* web_contents =
            tab_strip->GetWebContentsAt(index);
        gfx::Image app_icon =
            launcher_controller()->GetAppListIcon(web_contents);
        base::string16 title =
            launcher_controller()->GetAppListTitle(web_contents);
        // Check if we need to insert a separator in front.
        bool leading_separator = !index;
        items.push_back(base::MakeUnique<ChromeLauncherAppMenuItemTab>(
            title, &app_icon, web_contents, leading_separator));
      }
    }
  }
  // If only windowed applications are open, we return an empty list to
  // enforce the creation of a new browser.
  if (!found_tabbed_browser)
    items.clear();
  return items;
}

ash::ShelfItemDelegate::PerformedAction
BrowserShortcutLauncherItemController::ItemSelected(const ui::Event& event) {
  if (event.flags() & ui::EF_CONTROL_DOWN) {
    chrome::NewEmptyWindow(launcher_controller()->profile());
    return kNewWindowCreated;
  }

  // In case of a keyboard event, we were called by a hotkey. In that case we
  // activate the next item in line if an item of our list is already active.
  if (event.type() == ui::ET_KEY_RELEASED) {
    return ActivateOrAdvanceToNextBrowser();
  }

  return Activate(ash::LAUNCH_FROM_UNKNOWN);
}

ash::ShelfMenuModel*
BrowserShortcutLauncherItemController::CreateApplicationMenu(int event_flags) {
  return new LauncherApplicationMenuItemModel(GetApplicationList(event_flags));
}

bool BrowserShortcutLauncherItemController::IsListOfActiveBrowserEmpty() {
  return GetListOfActiveBrowsers().empty();
}

gfx::Image BrowserShortcutLauncherItemController::GetBrowserListIcon(
    content::WebContents* web_contents) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(IsIncognito(web_contents) ?
      IDR_ASH_SHELF_LIST_INCOGNITO_BROWSER :
      IDR_ASH_SHELF_LIST_BROWSER);
}

base::string16 BrowserShortcutLauncherItemController::GetBrowserListTitle(
    content::WebContents* web_contents) const {
  base::string16 title = web_contents->GetTitle();
  if (!title.empty())
    return title;
  return l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
}

bool BrowserShortcutLauncherItemController::IsIncognito(
    content::WebContents* web_contents) const {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return profile->IsOffTheRecord() && !profile->IsGuestSession();
}

ash::ShelfItemDelegate::PerformedAction
BrowserShortcutLauncherItemController::ActivateOrAdvanceToNextBrowser() {
  // Create a list of all suitable running browsers.
  std::vector<Browser*> items;
  // We use the list in the order of how the browsers got created - not the LRU
  // order.
  const BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end(); ++it) {
    if (IsBrowserRepresentedInBrowserList(*it))
      items.push_back(*it);
  }
  // If there are no suitable browsers we create a new one.
  if (items.empty()) {
    chrome::NewEmptyWindow(launcher_controller()->profile());
    return kNewWindowCreated;
  }
  Browser* browser = chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
  if (items.size() == 1) {
    // If there is only one suitable browser, we can either activate it, or
    // bounce it (if it is already active).
    if (browser == items[0]) {
      AnimateWindow(browser->window()->GetNativeWindow(),
                    wm::WINDOW_ANIMATION_TYPE_BOUNCE);
      return kNoAction;
    }
    browser = items[0];
  } else {
    // If there is more than one suitable browser, we advance to the next if
    // |browser| is already active - or - check the last used browser if it can
    // be used.
    std::vector<Browser*>::iterator i =
        std::find(items.begin(), items.end(), browser);
    if (i != items.end()) {
      browser = (++i == items.end()) ? items[0] : *i;
    } else {
      browser =
          chrome::FindTabbedBrowser(launcher_controller()->profile(), true);
      if (!browser || !IsBrowserRepresentedInBrowserList(browser))
        browser = items[0];
    }
  }
  DCHECK(browser);
  browser->window()->Show();
  browser->window()->Activate();
  return kExistingWindowActivated;
}

bool BrowserShortcutLauncherItemController::IsBrowserRepresentedInBrowserList(
    Browser* browser) {
  // Only Ash desktop browser windows for the active user are represented.
  if (!browser || !IsBrowserFromActiveUser(browser))
    return false;

  // v1 App popup windows with a valid app id have their own icon.
  if (browser->is_app() && browser->is_type_popup() &&
      ash::WmShell::Get()->shelf_delegate()->GetShelfIDForAppID(
          web_app::GetExtensionIdFromApplicationName(browser->app_name())) > 0)
    return false;

  // Settings browsers have their own icon.
  if (IsSettingsBrowser(browser))
    return false;

  // Tabbed browser and other popup windows are all represented.
  return true;
}

BrowserList::BrowserVector
BrowserShortcutLauncherItemController::GetListOfActiveBrowsers() {
  BrowserList::BrowserVector active_browsers;
  for (auto* browser : *BrowserList::GetInstance()) {
    // Make sure that the browser is from the current user, has a proper window,
    // and the window was already shown.
    if (!IsBrowserFromActiveUser(browser))
      continue;
    if (!browser->window()->GetNativeWindow()->IsVisible() &&
        !browser->window()->IsMinimized()) {
      continue;
    }
    if (!IsBrowserRepresentedInBrowserList(browser) &&
        !browser->is_type_tabbed())
      continue;
    active_browsers.push_back(browser);
  }
  return active_browsers;
}
