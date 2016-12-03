// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_loader_handler.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "chrome/browser/ui/webui/extensions/install_extension_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/common/extension_urls.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/ui/webui/extensions/chromeos/kiosk_apps_handler.h"
#endif

namespace extensions {

namespace {

content::WebUIDataSource* CreateMdExtensionsSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIExtensionsHost);

  source->SetJsonPath("strings.js");

  source->AddLocalizedString("title",
                             IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);
  source->AddLocalizedString("toolbarTitle", IDS_MD_EXTENSIONS_TOOLBAR_TITLE);
  source->AddLocalizedString("search", IDS_MD_EXTENSIONS_SEARCH);
  source->AddLocalizedString("sidebarApps", IDS_MD_EXTENSIONS_SIDEBAR_APPS);
  source->AddLocalizedString("sidebarExtensions",
                             IDS_MD_EXTENSIONS_SIDEBAR_EXTENSIONS);
  source->AddLocalizedString("sidebarLoadUnpacked",
                             IDS_MD_EXTENSIONS_SIDEBAR_LOAD_UNPACKED);
  source->AddLocalizedString("sidebarPack", IDS_MD_EXTENSIONS_SIDEBAR_PACK);
  source->AddLocalizedString("sidebarUpdateNow",
                             IDS_MD_EXTENSIONS_SIDEBAR_UPDATE_NOW);
  source->AddLocalizedString("developerMode",
                             IDS_MD_EXTENSIONS_SIDEBAR_DEVELOPER_MODE);
  source->AddLocalizedString("dropToInstall",
                             IDS_EXTENSIONS_INSTALL_DROP_TARGET);
  source->AddLocalizedString("getMoreExtensions",
                             IDS_MD_EXTENSIONS_SIDEBAR_GET_MORE_EXTENSIONS);
  source->AddLocalizedString("keyboardShortcuts",
                             IDS_MD_EXTENSIONS_SIDEBAR_KEYBOARD_SHORTCUTS);
  source->AddLocalizedString("itemId", IDS_MD_EXTENSIONS_ITEM_ID);
  source->AddLocalizedString("itemInspectViews",
                             IDS_MD_EXTENSIONS_ITEM_INSPECT_VIEWS);
  source->AddLocalizedString("itemAllowIncognito",
                             IDS_MD_EXTENSIONS_ITEM_ALLOW_INCOGNITO);
  source->AddLocalizedString("itemDescriptionLabel",
                             IDS_MD_EXTENSIONS_ITEM_DESCRIPTION);
  source->AddLocalizedString("itemDependencies",
                             IDS_MD_EXTENSIONS_ITEM_DEPENDENCIES);
  source->AddLocalizedString("itemDependentEntry",
                             IDS_MD_EXTENSIONS_DEPENDENT_ENTRY);
  source->AddLocalizedString("itemDetails", IDS_MD_EXTENSIONS_ITEM_DETAILS);
  source->AddLocalizedString("itemPermissions",
                             IDS_MD_EXTENSIONS_ITEM_PERMISSIONS);
  source->AddLocalizedString("itemPermissionsEmpty",
                             IDS_MD_EXTENSIONS_ITEM_PERMISSIONS_EMPTY);
  source->AddLocalizedString("itemRemove", IDS_MD_EXTENSIONS_ITEM_REMOVE);
  source->AddLocalizedString("itemSource",
                             IDS_MD_EXTENSIONS_ITEM_SOURCE);
  source->AddLocalizedString("itemSourcePolicy",
                             IDS_MD_EXTENSIONS_ITEM_SOURCE_POLICY);
  source->AddLocalizedString("itemSourceSideloaded",
                             IDS_MD_EXTENSIONS_ITEM_SOURCE_SIDELOADED);
  source->AddLocalizedString("itemSourceUnpacked",
                             IDS_MD_EXTENSIONS_ITEM_SOURCE_UNPACKED);
  source->AddLocalizedString("itemVersion",
                             IDS_MD_EXTENSIONS_ITEM_SOURCE);
  source->AddLocalizedString("itemAllowOnFileUrls",
                             IDS_EXTENSIONS_ALLOW_FILE_ACCESS);
  source->AddLocalizedString("itemAllowOnAllSites",
                             IDS_EXTENSIONS_ALLOW_ON_ALL_URLS);
  source->AddLocalizedString("itemCollectErrors",
                             IDS_EXTENSIONS_ENABLE_ERROR_COLLECTION);
  source->AddLocalizedString("itemCorruptInstall",
                             IDS_EXTENSIONS_CORRUPTED_EXTENSION);
  source->AddLocalizedString("itemRepair", IDS_EXTENSIONS_REPAIR_CORRUPTED);
  source->AddString(
      "itemSuspiciousInstall",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE,
          l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)));
  source->AddLocalizedString("packDialogTitle",
                             IDS_MD_EXTENSIONS_PACK_DIALOG_TITLE);
  source->AddLocalizedString("packDialogBrowse",
                             IDS_MD_EXTENSIONS_PACK_DIALOG_BROWSE_BUTTON);
  source->AddLocalizedString(
      "packDialogExtensionRoot",
      IDS_MD_EXTENSIONS_PACK_DIALOG_EXTENSION_ROOT_LABEL);
  source->AddLocalizedString("packDialogKeyFile",
                             IDS_MD_EXTENSIONS_PACK_DIALOG_KEY_FILE_LABEL);
  source->AddLocalizedString("packDialogContent",
                             IDS_EXTENSION_PACK_DIALOG_HEADING);
  source->AddLocalizedString("packDialogCancel",
                             IDS_MD_EXTENSIONS_PACK_DIALOG_CANCEL_BUTTON);
  source->AddLocalizedString("packDialogConfirm",
                             IDS_MD_EXTENSIONS_PACK_DIALOG_CONFIRM_BUTTON);
  source->AddLocalizedString("shortcutNotSet",
                             IDS_MD_EXTENSIONS_SHORTCUT_NOT_SET);
  source->AddLocalizedString("shortcutScopeGlobal",
                             IDS_MD_EXTENSIONS_SHORTCUT_SCOPE_GLOBAL);
  source->AddLocalizedString("shortcutScopeLabel",
                             IDS_MD_EXTENSIONS_SHORTCUT_SCOPE_LABEL);
  source->AddLocalizedString("shortcutScopeInChrome",
                             IDS_MD_EXTENSIONS_SHORTCUT_SCOPE_IN_CHROME);
  source->AddLocalizedString("shortcutTypeAShortcut",
                             IDS_MD_EXTENSIONS_TYPE_A_SHORTCUT);
  source->AddLocalizedString("viewBackgroundPage",
                             IDS_EXTENSIONS_BACKGROUND_PAGE);
  source->AddLocalizedString("viewIncognito",
                             IDS_EXTENSIONS_VIEW_INCOGNITO);
  source->AddLocalizedString("viewInactive",
                             IDS_EXTENSIONS_VIEW_INACTIVE);
  source->AddLocalizedString("viewIframe",
                             IDS_EXTENSIONS_VIEW_IFRAME);
  source->AddString(
      "getMoreExtensionsUrl",
      base::ASCIIToUTF16(
          google_util::AppendGoogleLocaleParam(
              GURL(extension_urls::GetWebstoreExtensionsCategoryURL()),
              g_browser_process->GetApplicationLocale()).spec()));

  source->AddResourcePath("animation_helper.html",
                          IDR_MD_EXTENSIONS_ANIMATION_HELPER_HTML);
  source->AddResourcePath("animation_helper.js",
                          IDR_MD_EXTENSIONS_ANIMATION_HELPER_JS);
  source->AddResourcePath("extensions.js", IDR_MD_EXTENSIONS_EXTENSIONS_JS);
  source->AddResourcePath("drag_and_drop_handler.html",
                          IDR_EXTENSIONS_DRAG_AND_DROP_HANDLER_HTML);
  source->AddResourcePath("drag_and_drop_handler.js",
                          IDR_EXTENSIONS_DRAG_AND_DROP_HANDLER_JS);
  source->AddResourcePath("detail_view.html",
                          IDR_MD_EXTENSIONS_DETAIL_VIEW_HTML);
  source->AddResourcePath("detail_view.js", IDR_MD_EXTENSIONS_DETAIL_VIEW_JS);
  source->AddResourcePath("drop_overlay.html",
                          IDR_MD_EXTENSIONS_DROP_OVERLAY_HTML);
  source->AddResourcePath("drop_overlay.js", IDR_MD_EXTENSIONS_DROP_OVERLAY_JS);
  source->AddResourcePath("keyboard_shortcuts.html",
                          IDR_MD_EXTENSIONS_KEYBOARD_SHORTCUTS_HTML);
  source->AddResourcePath("keyboard_shortcuts.js",
                          IDR_MD_EXTENSIONS_KEYBOARD_SHORTCUTS_JS);
  source->AddResourcePath("manager.css", IDR_MD_EXTENSIONS_MANAGER_CSS);
  source->AddResourcePath("manager.html", IDR_MD_EXTENSIONS_MANAGER_HTML);
  source->AddResourcePath("manager.js", IDR_MD_EXTENSIONS_MANAGER_JS);
  source->AddResourcePath("icons.html", IDR_MD_EXTENSIONS_ICONS_HTML);
  source->AddResourcePath("item.css", IDR_MD_EXTENSIONS_ITEM_CSS);
  source->AddResourcePath("item.html", IDR_MD_EXTENSIONS_ITEM_HTML);
  source->AddResourcePath("item.js", IDR_MD_EXTENSIONS_ITEM_JS);
  source->AddResourcePath("item_list.css", IDR_MD_EXTENSIONS_ITEM_LIST_CSS);
  source->AddResourcePath("item_list.html", IDR_MD_EXTENSIONS_ITEM_LIST_HTML);
  source->AddResourcePath("item_list.js", IDR_MD_EXTENSIONS_ITEM_LIST_JS);
  source->AddResourcePath("pack_dialog.html",
                          IDR_MD_EXTENSIONS_PACK_DIALOG_HTML);
  source->AddResourcePath("pack_dialog.js", IDR_MD_EXTENSIONS_PACK_DIALOG_JS);
  source->AddResourcePath("service.html", IDR_MD_EXTENSIONS_SERVICE_HTML);
  source->AddResourcePath("service.js", IDR_MD_EXTENSIONS_SERVICE_JS);
  source->AddResourcePath("shortcut_input.html",
                          IDR_MD_EXTENSIONS_SHORTCUT_INPUT_HTML);
  source->AddResourcePath("shortcut_input.js",
                          IDR_MD_EXTENSIONS_SHORTCUT_INPUT_JS);
  source->AddResourcePath("shortcut_util.html",
                          IDR_EXTENSIONS_SHORTCUT_UTIL_HTML);
  source->AddResourcePath("shortcut_util.js", IDR_EXTENSIONS_SHORTCUT_UTIL_JS);
  source->AddResourcePath("sidebar.css", IDR_MD_EXTENSIONS_SIDEBAR_CSS);
  source->AddResourcePath("sidebar.html", IDR_MD_EXTENSIONS_SIDEBAR_HTML);
  source->AddResourcePath("sidebar.js", IDR_MD_EXTENSIONS_SIDEBAR_JS);
  source->AddResourcePath("strings.html", IDR_MD_EXTENSIONS_STRINGS_HTML);
  source->SetDefaultResource(IDR_MD_EXTENSIONS_EXTENSIONS_HTML);

  return source;
}

content::WebUIDataSource* CreateExtensionsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIExtensionsFrameHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("extensions.js", IDR_EXTENSIONS_JS);
  source->AddResourcePath("extension_command_list.js",
                          IDR_EXTENSION_COMMAND_LIST_JS);
  source->AddResourcePath("extension_list.js", IDR_EXTENSION_LIST_JS);
  source->SetDefaultResource(IDR_EXTENSIONS_HTML);
  source->DisableDenyXFrameOptions();
  return source;
}

}  // namespace

ExtensionsUI::ExtensionsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = nullptr;

  if (base::FeatureList::IsEnabled(features::kMaterialDesignExtensions)) {
    source = CreateMdExtensionsSource();
    InstallExtensionHandler* install_extension_handler =
        new InstallExtensionHandler();
    install_extension_handler->GetLocalizedValues(source);
    web_ui->AddMessageHandler(install_extension_handler);
  } else {
    source = CreateExtensionsHTMLSource();

    ExtensionSettingsHandler* handler = new ExtensionSettingsHandler();
    web_ui->AddMessageHandler(handler);
    handler->GetLocalizedValues(source);

    ExtensionLoaderHandler* extension_loader_handler =
        new ExtensionLoaderHandler(profile);
    extension_loader_handler->GetLocalizedValues(source);
    web_ui->AddMessageHandler(extension_loader_handler);

    InstallExtensionHandler* install_extension_handler =
        new InstallExtensionHandler();
    install_extension_handler->GetLocalizedValues(source);
    web_ui->AddMessageHandler(install_extension_handler);

#if defined(OS_CHROMEOS)
    chromeos::KioskAppsHandler* kiosk_app_handler =
        new chromeos::KioskAppsHandler(
            chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
                profile));
    kiosk_app_handler->GetLocalizedValues(source);
    web_ui->AddMessageHandler(kiosk_app_handler);
#endif

    web_ui->AddMessageHandler(new MetricsHandler());

    // Need to allow <object> elements so that the <extensionoptions> browser
    // plugin can be loaded within chrome://extensions.
    source->OverrideContentSecurityPolicyObjectSrc("object-src 'self';");
  }

  content::WebUIDataSource::Add(profile, source);
}

ExtensionsUI::~ExtensionsUI() {}

// static
base::RefCountedMemory* ExtensionsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.LoadDataResourceBytesForScale(IDR_EXTENSIONS_FAVICON, scale_factor);
}

}  // namespace extensions
