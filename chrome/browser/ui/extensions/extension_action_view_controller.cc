// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_view_controller.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::ActionInfo;
using extensions::CommandService;

ExtensionActionViewController::ExtensionActionViewController(
    const extensions::Extension* extension,
    Browser* browser,
    ExtensionAction* extension_action,
    ToolbarActionsBar* toolbar_actions_bar)
    : extension_(extension),
      browser_(browser),
      extension_action_(extension_action),
      toolbar_actions_bar_(toolbar_actions_bar),
      popup_host_(nullptr),
      view_delegate_(nullptr),
      platform_delegate_(ExtensionActionPlatformDelegate::Create(this)),
      icon_factory_(browser->profile(), extension, extension_action, this),
      icon_observer_(nullptr),
      extension_registry_(
          extensions::ExtensionRegistry::Get(browser_->profile())),
      popup_host_observer_(this),
      weak_factory_(this) {
  DCHECK(extension_action);
  DCHECK(extension_action->action_type() == ActionInfo::TYPE_PAGE ||
         extension_action->action_type() == ActionInfo::TYPE_BROWSER);
  DCHECK(extension);
}

ExtensionActionViewController::~ExtensionActionViewController() {
  DCHECK(!is_showing_popup());
}

const std::string& ExtensionActionViewController::GetId() const {
  return extension_->id();
}

void ExtensionActionViewController::SetDelegate(
    ToolbarActionViewDelegate* delegate) {
  DCHECK((delegate == nullptr) ^ (view_delegate_ == nullptr));
  if (delegate) {
    view_delegate_ = delegate;
    platform_delegate_->OnDelegateSet();
  } else {
    if (is_showing_popup())
      HidePopup();
    platform_delegate_.reset();
    view_delegate_ = nullptr;
  }
}

gfx::Image ExtensionActionViewController::GetIcon(
    content::WebContents* web_contents) {
  if (!ExtensionIsValid())
    return gfx::Image();

  return icon_factory_.GetIcon(SessionTabHelper::IdForTab(web_contents));
}

gfx::ImageSkia ExtensionActionViewController::GetIconWithBadge() {
  if (!ExtensionIsValid())
    return gfx::ImageSkia();

  content::WebContents* web_contents = view_delegate_->GetCurrentWebContents();
  gfx::Size spacing(0, 3);
  gfx::ImageSkia icon = *GetIcon(web_contents).ToImageSkia();
  if (!IsEnabled(web_contents))
    icon = gfx::ImageSkiaOperations::CreateTransparentImage(icon, .25);
  return extension_action_->GetIconWithBadge(
      icon, SessionTabHelper::IdForTab(web_contents), spacing);
}

base::string16 ExtensionActionViewController::GetActionName() const {
  if (!ExtensionIsValid())
    return base::string16();

  return base::UTF8ToUTF16(extension_->name());
}

base::string16 ExtensionActionViewController::GetAccessibleName(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return base::string16();

  std::string title =
      extension_action()->GetTitle(SessionTabHelper::IdForTab(web_contents));
  return base::UTF8ToUTF16(title.empty() ? extension()->name() : title);
}

base::string16 ExtensionActionViewController::GetTooltip(
    content::WebContents* web_contents) const {
  return GetAccessibleName(web_contents);
}

bool ExtensionActionViewController::IsEnabled(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return false;

  return extension_action_->GetIsVisible(
      SessionTabHelper::IdForTab(web_contents)) ||
      extensions::ExtensionActionAPI::Get(browser_->profile())->
          ExtensionWantsToRun(extension(), web_contents);
}

bool ExtensionActionViewController::WantsToRun(
    content::WebContents* web_contents) const {
  return extensions::ExtensionActionAPI::Get(browser_->profile())->
      ExtensionWantsToRun(extension(), web_contents);
}

bool ExtensionActionViewController::HasPopup(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return false;

  int tab_id = SessionTabHelper::IdForTab(web_contents);
  return (tab_id < 0) ? false : extension_action_->HasPopup(tab_id);
}

void ExtensionActionViewController::HidePopup() {
  if (is_showing_popup()) {
    popup_host_->Close();
    // We need to do these actions synchronously (instead of closing and then
    // performing the rest of the cleanup in OnExtensionHostDestroyed()) because
    // the extension host can close asynchronously, and we need to keep the view
    // delegate up-to-date.
    OnPopupClosed();
  }
}

gfx::NativeView ExtensionActionViewController::GetPopupNativeView() {
  return popup_host_ ? popup_host_->view()->GetNativeView() : nullptr;
}

ui::MenuModel* ExtensionActionViewController::GetContextMenu() {
  if (!ExtensionIsValid() || !extension()->ShowConfigureContextMenus())
    return nullptr;

  ExtensionContextMenuModel::ButtonVisibility visibility =
      ExtensionContextMenuModel::VISIBLE;
  if (toolbar_actions_bar_) {
    if (toolbar_actions_bar_->popped_out_action() == this)
      visibility = ExtensionContextMenuModel::TRANSITIVELY_VISIBLE;
    else if (!toolbar_actions_bar_->IsActionVisible(this))
      visibility = ExtensionContextMenuModel::OVERFLOWED;
    // Else, VISIBLE is correct.
  }
  // Reconstruct the menu every time because the menu's contents are dynamic.
  context_menu_model_ = make_scoped_refptr(new ExtensionContextMenuModel(
      extension(), browser_, visibility, this));
  return context_menu_model_.get();
}

void ExtensionActionViewController::OnContextMenuClosed() {
  if (toolbar_actions_bar_ &&
      toolbar_actions_bar_->popped_out_action() == this &&
      !is_showing_popup()) {
    toolbar_actions_bar_->UndoPopOut();
  }
}

bool ExtensionActionViewController::CanDrag() const {
  return true;
}

bool ExtensionActionViewController::ExecuteAction(bool by_user) {
  return ExecuteAction(SHOW_POPUP, by_user);
}

void ExtensionActionViewController::UpdateState() {
  if (!ExtensionIsValid())
    return;

  view_delegate_->UpdateState();
}

bool ExtensionActionViewController::ExecuteAction(PopupShowAction show_action,
                                                  bool grant_tab_permissions) {
  if (!ExtensionIsValid())
    return false;

  if (extensions::ExtensionActionAPI::Get(browser_->profile())
          ->ExecuteExtensionAction(
              extension_.get(), browser_, grant_tab_permissions) ==
      ExtensionAction::ACTION_SHOW_POPUP) {
    GURL popup_url = extension_action_->GetPopupUrl(
        SessionTabHelper::IdForTab(view_delegate_->GetCurrentWebContents()));
    return GetPreferredPopupViewController()
        ->TriggerPopupWithUrl(show_action, popup_url, grant_tab_permissions);
  }
  return false;
}

void ExtensionActionViewController::PaintExtra(
    gfx::Canvas* canvas,
    const gfx::Rect& bounds,
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return;

  int tab_id = SessionTabHelper::IdForTab(web_contents);
  if (tab_id >= 0)
    extension_action_->PaintBadge(canvas, bounds, tab_id);
}

void ExtensionActionViewController::RegisterCommand() {
  if (!ExtensionIsValid())
    return;

  platform_delegate_->RegisterCommand();
}

void ExtensionActionViewController::InspectPopup() {
  ExecuteAction(SHOW_POPUP_AND_INSPECT, true);
}

void ExtensionActionViewController::OnIconUpdated() {
  if (icon_observer_)
    icon_observer_->OnIconUpdated();
  if (view_delegate_)
    view_delegate_->UpdateState();
}

void ExtensionActionViewController::OnExtensionHostDestroyed(
    const extensions::ExtensionHost* host) {
  OnPopupClosed();
}

bool ExtensionActionViewController::ExtensionIsValid() const {
  return extension_registry_->enabled_extensions().Contains(extension_->id());
}

void ExtensionActionViewController::HideActivePopup() {
  if (toolbar_actions_bar_) {
    toolbar_actions_bar_->HideActivePopup();
  } else {
    DCHECK_EQ(ActionInfo::TYPE_PAGE, extension_action_->action_type());
    // In the traditional toolbar, page actions only know how to close their own
    // popups.
    HidePopup();
  }
}

bool ExtensionActionViewController::GetExtensionCommand(
    extensions::Command* command) {
  DCHECK(command);
  if (!ExtensionIsValid())
    return false;

  CommandService* command_service = CommandService::Get(browser_->profile());
  if (extension_action_->action_type() == ActionInfo::TYPE_PAGE) {
    return command_service->GetPageActionCommand(
        extension_->id(), CommandService::ACTIVE, command, NULL);
  }
  return command_service->GetBrowserActionCommand(
      extension_->id(), CommandService::ACTIVE, command, NULL);
}

ExtensionActionViewController*
ExtensionActionViewController::GetPreferredPopupViewController() {
  if (toolbar_actions_bar_ && toolbar_actions_bar_->in_overflow_mode()) {
    return static_cast<ExtensionActionViewController*>(
        toolbar_actions_bar_->GetMainControllerForAction(this));
  }

  return this;
}

bool ExtensionActionViewController::TriggerPopupWithUrl(
    PopupShowAction show_action,
    const GURL& popup_url,
    bool grant_tab_permissions) {
  if (!ExtensionIsValid())
    return false;

  bool already_showing = is_showing_popup();

  // Always hide the current popup, even if it's not owned by this extension.
  // Only one popup should be visible at a time.
  HideActivePopup();

  // If we were showing a popup already, then we treat the action to open the
  // same one as a desire to close it (like clicking a menu button that was
  // already open).
  if (already_showing)
    return false;

  scoped_ptr<extensions::ExtensionViewHost> host(
      extensions::ExtensionViewHostFactory::CreatePopupHost(popup_url,
                                                            browser_));
  if (!host)
    return false;

  popup_host_ = host.get();
  popup_host_observer_.Add(popup_host_);
  if (toolbar_actions_bar_)
    toolbar_actions_bar_->SetPopupOwner(this);

  if (toolbar_actions_bar_ &&
      !toolbar_actions_bar_->IsActionVisible(this) &&
      extensions::FeatureSwitch::extension_action_redesign()->IsEnabled()) {
    platform_delegate_->CloseOverflowMenu();
    toolbar_actions_bar_->PopOutAction(
        this,
        base::Bind(&ExtensionActionViewController::ShowPopup,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(host.Pass()),
                   grant_tab_permissions,
                   show_action));
  } else {
    ShowPopup(host.Pass(), grant_tab_permissions, show_action);
  }

  return true;
}

void ExtensionActionViewController::ShowPopup(
    scoped_ptr<extensions::ExtensionViewHost> popup_host,
    bool grant_tab_permissions,
    PopupShowAction show_action) {
  // It's possible that the popup should be closed before it finishes opening
  // (since it can open asynchronously). Check before proceeding.
  if (!popup_host_)
    return;
  platform_delegate_->ShowPopup(
      popup_host.Pass(), grant_tab_permissions, show_action);
  view_delegate_->OnPopupShown(grant_tab_permissions);
}

void ExtensionActionViewController::OnPopupClosed() {
  popup_host_observer_.Remove(popup_host_);
  popup_host_ = nullptr;
  if (toolbar_actions_bar_) {
    toolbar_actions_bar_->SetPopupOwner(nullptr);
    if (toolbar_actions_bar_->popped_out_action() == this &&
        !view_delegate_->IsMenuRunning())
      toolbar_actions_bar_->UndoPopOut();
  }
  view_delegate_->OnPopupClosed();
}
