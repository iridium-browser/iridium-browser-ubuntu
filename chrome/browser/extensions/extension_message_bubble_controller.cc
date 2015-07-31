// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_bubble_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {
// How many extensions to show in the bubble (max).
const int kMaxExtensionsToShow = 7;
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController::Delegate

ExtensionMessageBubbleController::Delegate::Delegate(Profile* profile)
    : profile_(profile) {
}

ExtensionMessageBubbleController::Delegate::~Delegate() {
}

void ExtensionMessageBubbleController::Delegate::RestrictToSingleExtension(
    const std::string& extension_id) {
  NOTIMPLEMENTED();  // Derived classes that need this should implement.
}

base::string16 ExtensionMessageBubbleController::Delegate::GetLearnMoreLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool ExtensionMessageBubbleController::Delegate::HasBubbleInfoBeenAcknowledged(
    const std::string& extension_id) {
  std::string pref_name = get_acknowledged_flag_pref_name();
  if (pref_name.empty())
    return false;
  bool pref_state = false;
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  prefs->ReadPrefAsBoolean(extension_id, pref_name, &pref_state);
  return pref_state;
}

void ExtensionMessageBubbleController::Delegate::SetBubbleInfoBeenAcknowledged(
    const std::string& extension_id,
    bool value) {
  std::string pref_name = get_acknowledged_flag_pref_name();
  if (pref_name.empty())
    return;
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  prefs->UpdateExtensionPref(extension_id,
                             pref_name,
                             value ? new base::FundamentalValue(value) : NULL);
}

Profile* ExtensionMessageBubbleController::Delegate::profile() const {
  return profile_;
}

std::string
ExtensionMessageBubbleController::Delegate::get_acknowledged_flag_pref_name()
    const {
  return acknowledged_pref_name_;
}

void
ExtensionMessageBubbleController::Delegate::set_acknowledged_flag_pref_name(
    std::string pref_name) {
  acknowledged_pref_name_ = pref_name;
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController

ExtensionMessageBubbleController::ExtensionMessageBubbleController(
    Delegate* delegate, Profile* profile)
    : profile_(profile),
      user_action_(ACTION_BOUNDARY),
      delegate_(delegate),
      initialized_(false),
      did_highlight_(false) {
}

ExtensionMessageBubbleController::~ExtensionMessageBubbleController() {
}

std::vector<base::string16>
ExtensionMessageBubbleController::GetExtensionList() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  if (list->empty())
    return std::vector<base::string16>();

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  std::vector<base::string16> return_value;
  for (ExtensionIdList::const_iterator it = list->begin();
       it != list->end(); ++it) {
    const Extension* extension =
        registry->GetExtensionById(*it, ExtensionRegistry::EVERYTHING);
    if (extension) {
      return_value.push_back(base::UTF8ToUTF16(extension->name()));
    } else {
      return_value.push_back(
          base::ASCIIToUTF16(std::string("(unknown name) ") + *it));
      // TODO(finnur): Add this as a string to the grd, for next milestone.
    }
  }
  return return_value;
}

base::string16 ExtensionMessageBubbleController::GetExtensionListForDisplay() {
  if (!delegate_->ShouldShowExtensionList())
    return base::string16();

  std::vector<base::string16> extension_list = GetExtensionList();
  if (extension_list.size() > kMaxExtensionsToShow) {
    int old_size = extension_list.size();
    extension_list.erase(extension_list.begin() + kMaxExtensionsToShow,
                         extension_list.end());
    extension_list.push_back(delegate_->GetOverflowText(base::IntToString16(
        old_size - kMaxExtensionsToShow)));
  }
  const base::char16 bullet_point = 0x2022;
  base::string16 prefix = bullet_point + base::ASCIIToUTF16(" ");
  for (base::string16& str : extension_list)
    str.insert(0, prefix);
  return JoinString(extension_list, base::ASCIIToUTF16("\n"));
}

const ExtensionIdList& ExtensionMessageBubbleController::GetExtensionIdList() {
  return *GetOrCreateExtensionList();
}

bool ExtensionMessageBubbleController::CloseOnDeactivate() { return false; }

void ExtensionMessageBubbleController::HighlightExtensionsIfNecessary() {
  if (delegate_->ShouldHighlightExtensions() && !did_highlight_) {
    did_highlight_ = true;
    const ExtensionIdList& extension_ids = GetExtensionIdList();
    DCHECK(!extension_ids.empty());
    ExtensionToolbarModel::Get(profile_)->HighlightExtensions(extension_ids);
  }
}

void ExtensionMessageBubbleController::Show(ExtensionMessageBubble* bubble) {
  bubble->Show();
}

void ExtensionMessageBubbleController::OnBubbleAction() {
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_EXECUTE;

  delegate_->LogAction(ACTION_EXECUTE);
  delegate_->PerformAction(*GetOrCreateExtensionList());

  OnClose();
}

void ExtensionMessageBubbleController::OnBubbleDismiss() {
  // OnBubbleDismiss() can be called twice when we receive multiple
  // "OnWidgetDestroying" notifications (this can at least happen when we close
  // a window with a notification open). Handle this gracefully.
  if (user_action_ != ACTION_BOUNDARY) {
    DCHECK(user_action_ == ACTION_DISMISS);
    return;
  }

  user_action_ = ACTION_DISMISS;

  delegate_->LogAction(ACTION_DISMISS);

  OnClose();
}

void ExtensionMessageBubbleController::OnLinkClicked() {
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_LEARN_MORE;

  delegate_->LogAction(ACTION_LEARN_MORE);
  Browser* browser =
      chrome::FindBrowserWithProfile(profile_, chrome::GetActiveDesktop());
  if (browser) {
    browser->OpenURL(
        content::OpenURLParams(delegate_->GetLearnMoreUrl(),
                               content::Referrer(),
                               NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               false));
  }
  OnClose();
}

void ExtensionMessageBubbleController::AcknowledgeExtensions() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  for (ExtensionIdList::const_iterator it = list->begin();
       it != list->end(); ++it)
    delegate_->AcknowledgeExtension(*it, user_action_);
}

ExtensionIdList* ExtensionMessageBubbleController::GetOrCreateExtensionList() {
  if (!initialized_) {
    scoped_ptr<const ExtensionSet> extension_set(
        ExtensionRegistry::Get(profile_)->GenerateInstalledExtensionsSet());
    for (ExtensionSet::const_iterator it = extension_set->begin();
         it != extension_set->end(); ++it) {
      std::string id = (*it)->id();
      if (!delegate_->ShouldIncludeExtension(id))
        continue;
      extension_list_.push_back(id);
    }

    delegate_->LogExtensionCount(extension_list_.size());
    initialized_ = true;
  }

  return &extension_list_;
}

void ExtensionMessageBubbleController::OnClose() {
  AcknowledgeExtensions();
  if (did_highlight_)
    ExtensionToolbarModel::Get(profile_)->StopHighlighting();
}

}  // namespace extensions
