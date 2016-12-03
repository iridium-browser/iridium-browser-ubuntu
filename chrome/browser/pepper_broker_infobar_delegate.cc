// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pepper_broker_infobar_delegate.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/webplugininfo.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"


// static
void PepperBrokerInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    const base::Callback<void(bool)>& callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // TODO(wad): Add ephemeral device ID support for broker in guest mode.
  if (profile->IsGuestSession()) {
    callback.Run(false);
    return;
  }

  TabSpecificContentSettings* tab_content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSetting setting =
      content_settings->GetContentSetting(url, url,
                                          CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
                                          std::string());

  if (setting == CONTENT_SETTING_ASK) {
    content::RecordAction(
        base::UserMetricsAction("PPAPI.BrokerInfobarDisplayed"));
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
        std::unique_ptr<ConfirmInfoBarDelegate>(
            new PepperBrokerInfoBarDelegate(url, plugin_path, content_settings,
                                            tab_content_settings, callback))));
    return;
  }

  bool allowed = (setting == CONTENT_SETTING_ALLOW);
  content::RecordAction(allowed ?
      base::UserMetricsAction("PPAPI.BrokerSettingAllow") :
      base::UserMetricsAction("PPAPI.BrokerSettingDeny"));
  tab_content_settings->SetPepperBrokerAllowed(allowed);
  callback.Run(allowed);
}

PepperBrokerInfoBarDelegate::PepperBrokerInfoBarDelegate(
    const GURL& url,
    const base::FilePath& plugin_path,
    HostContentSettingsMap* content_settings,
    TabSpecificContentSettings* tab_content_settings,
    const base::Callback<void(bool)>& callback)
    : ConfirmInfoBarDelegate(),
      url_(url),
      plugin_path_(plugin_path),
      content_settings_(content_settings),
      tab_content_settings_(tab_content_settings),
      callback_(callback) {
}

PepperBrokerInfoBarDelegate::~PepperBrokerInfoBarDelegate() {
  if (!callback_.is_null())
    callback_.Run(false);
}

infobars::InfoBarDelegate::InfoBarIdentifier
PepperBrokerInfoBarDelegate::GetIdentifier() const {
  return PEPPER_BROKER_INFOBAR_DELEGATE;
}

int PepperBrokerInfoBarDelegate::GetIconId() const {
  return IDR_INFOBAR_PLUGIN_INSTALL;
}

base::string16 PepperBrokerInfoBarDelegate::GetMessageText() const {
  content::PluginService* plugin_service =
      content::PluginService::GetInstance();
  content::WebPluginInfo plugin;
  bool success = plugin_service->GetPluginInfoByPath(plugin_path_, &plugin);
  DCHECK(success);
  std::unique_ptr<PluginMetadata> plugin_metadata(
      PluginFinder::GetInstance()->GetPluginMetadata(plugin));
  return l10n_util::GetStringFUTF16(
      IDS_PEPPER_BROKER_MESSAGE, plugin_metadata->name(),
      url_formatter::FormatUrlForSecurityDisplay(url_));
}

base::string16 PepperBrokerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PEPPER_BROKER_ALLOW_BUTTON : IDS_PEPPER_BROKER_DENY_BUTTON);
}

bool PepperBrokerInfoBarDelegate::Accept() {
  DispatchCallback(true);
  return true;
}

bool PepperBrokerInfoBarDelegate::Cancel() {
  DispatchCallback(false);
  return true;
}

base::string16 PepperBrokerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL PepperBrokerInfoBarDelegate::GetLinkURL() const {
  return GURL("https://support.google.com/chrome/?p=ib_pepper_broker");
}

void PepperBrokerInfoBarDelegate::DispatchCallback(bool result) {
  content::RecordAction(result ?
      base::UserMetricsAction("PPAPI.BrokerInfobarClickedAllow") :
      base::UserMetricsAction("PPAPI.BrokerInfobarClickedDeny"));
  callback_.Run(result);
  callback_ = base::Callback<void(bool)>();
  content_settings_->SetContentSettingDefaultScope(
      url_, GURL(), CONTENT_SETTINGS_TYPE_PPAPI_BROKER, std::string(),
      result ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  tab_content_settings_->SetPepperBrokerAllowed(result);
}
