// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"

#include "base/auto_reset.h"
#include "base/profiler/scoped_tracker.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace {

using WeakToolbarActions = std::vector<ToolbarActionViewController*>;

// Matches ToolbarView::kStandardSpacing;
const int kLeftPadding = 3;
const int kRightPadding = kLeftPadding;
const int kItemSpacing = kLeftPadding;
const int kOverflowLeftPadding = kItemSpacing;
const int kOverflowRightPadding = kItemSpacing;

enum DimensionType { WIDTH, HEIGHT };

// Returns the width or height of the toolbar action icon size.
int GetIconDimension(DimensionType type) {
  static bool initialized = false;
  static int icon_height = 0;
  static int icon_width = 0;
  if (!initialized) {
    initialized = true;
    gfx::ImageSkia* skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_BROWSER_ACTION);
    icon_height = skia->height();
    icon_width = skia->width();
  }
  return type == WIDTH ? icon_width : icon_height;
}

// Takes a reference vector |reference| of length n, where n is less than or
// equal to the length of |to_sort|, and rearranges |to_sort| so that
// |to_sort|'s first n elements match the n elements of |reference| (the order
// of any remaining elements in |to_sort| is unspecified).
// |equal| is used to compare the elements of |to_sort| and |reference|.
// This allows us to sort a vector to match another vector of two different
// types without needing to construct a more cumbersome comparator class.
// |FunctionType| should equate to (something similar to)
// bool Equal(const Type1&, const Type2&), but we can't enforce this
// because of MSVC compilation limitations.
template<typename Type1, typename Type2, typename FunctionType>
void SortContainer(std::vector<Type1>* to_sort,
                   const std::vector<Type2>& reference,
                   FunctionType equal) {
  DCHECK_GE(to_sort->size(), reference.size()) <<
      "|to_sort| must contain all elements in |reference|.";
  if (reference.empty())
    return;
  // Run through the each element and compare it to the reference. If something
  // is out of place, find the correct spot for it.
  for (size_t i = 0; i < reference.size() - 1; ++i) {
    if (!equal(to_sort->at(i), reference[i])) {
      // Find the correct index (it's guaranteed to be after our current
      // index, since everything up to this point is correct), and swap.
      size_t j = i + 1;
      while (!equal(to_sort->at(j), reference[i])) {
        ++j;
        DCHECK_LE(j, to_sort->size()) <<
            "Item in |reference| not found in |to_sort|.";
      }
      std::swap(to_sort->at(i), to_sort->at(j));
    }
  }
}

}  // namespace

// static
bool ToolbarActionsBar::disable_animations_for_testing_ = false;

// static
bool ToolbarActionsBar::send_overflowed_action_changes_ = true;

ToolbarActionsBar::PlatformSettings::PlatformSettings(bool in_overflow_mode)
    : left_padding(in_overflow_mode ? kOverflowLeftPadding : kLeftPadding),
      right_padding(in_overflow_mode ? kOverflowRightPadding : kRightPadding),
      item_spacing(kItemSpacing),
      icons_per_overflow_menu_row(1),
      chevron_enabled(!extensions::FeatureSwitch::extension_action_redesign()->
                          IsEnabled()) {
}

ToolbarActionsBar::ToolbarActionsBar(ToolbarActionsBarDelegate* delegate,
                                     Browser* browser,
                                     ToolbarActionsBar* main_bar)
    : delegate_(delegate),
      browser_(browser),
      model_(extensions::ExtensionToolbarModel::Get(browser_->profile())),
      main_bar_(main_bar),
      platform_settings_(main_bar != nullptr),
      popup_owner_(nullptr),
      model_observer_(this),
      suppress_layout_(false),
      suppress_animation_(true),
      overflowed_action_wants_to_run_(false),
      checked_extension_bubble_(false),
      popped_out_action_(nullptr),
      weak_ptr_factory_(this) {
  if (model_)  // |model_| can be null in unittests.
    model_observer_.Add(model_);
}

ToolbarActionsBar::~ToolbarActionsBar() {
  // We don't just call DeleteActions() here because it makes assumptions about
  // the order of deletion between the views and the ToolbarActionsBar.
  DCHECK(toolbar_actions_.empty()) <<
      "Must call DeleteActions() before destruction.";
}

// static
int ToolbarActionsBar::IconWidth(bool include_padding) {
  return GetIconDimension(WIDTH) + (include_padding ? kItemSpacing : 0);
}

// static
int ToolbarActionsBar::IconHeight() {
  return GetIconDimension(HEIGHT);
}

// static
void ToolbarActionsBar::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kToolbarIconSurfacingBubbleAcknowledged,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterInt64Pref(prefs::kToolbarIconSurfacingBubbleLastShowTime,
                              0);
}

gfx::Size ToolbarActionsBar::GetPreferredSize() const {
  int icon_count = GetIconCount();
  if (in_overflow_mode()) {
    // In overflow, we always have a preferred size of a full row (even if we
    // don't use it), and always of at least one row. The parent may decide to
    // show us even when empty, e.g. as a drag target for dragging in icons from
    // the main container.
    int row_count = ((std::max(0, icon_count - 1)) /
        platform_settings_.icons_per_overflow_menu_row) + 1;
    return gfx::Size(
        IconCountToWidth(platform_settings_.icons_per_overflow_menu_row),
        row_count * IconHeight());
  }

  // If there are no actions to show (and this isn't an overflow container),
  // then don't show the container at all.
  if (toolbar_actions_.empty())
    return gfx::Size();

  return gfx::Size(IconCountToWidth(icon_count), IconHeight());
}

int ToolbarActionsBar::GetMinimumWidth() const {
  if (!platform_settings_.chevron_enabled || toolbar_actions_.empty())
    return kLeftPadding;
  return kLeftPadding + delegate_->GetChevronWidth() + kRightPadding;
}

int ToolbarActionsBar::GetMaximumWidth() const {
  return IconCountToWidth(-1);
}

int ToolbarActionsBar::IconCountToWidth(int icons) const {
  if (icons < 0)
    icons = toolbar_actions_.size();
  bool display_chevron =
      platform_settings_.chevron_enabled &&
      static_cast<size_t>(icons) < toolbar_actions_.size();
  if (icons == 0 && !display_chevron)
    return platform_settings_.left_padding;
  int icons_size = (icons == 0) ? 0 :
      (icons * IconWidth(true)) - platform_settings_.item_spacing;
  int chevron_size = display_chevron ? delegate_->GetChevronWidth() : 0;
  int padding = platform_settings_.left_padding +
                platform_settings_.right_padding;
  return icons_size + chevron_size + padding;
}

size_t ToolbarActionsBar::WidthToIconCount(int pixels) const {
  // Check for widths large enough to show the entire icon set.
  if (pixels >= IconCountToWidth(-1))
    return toolbar_actions_.size();

  // We reserve space for the padding on either side of the toolbar...
  int available_space = pixels -
      (platform_settings_.left_padding + platform_settings_.right_padding);
  // ... and, if the chevron is enabled, the chevron.
  if (platform_settings_.chevron_enabled)
    available_space -= delegate_->GetChevronWidth();

  // Now we add an extra between-item padding value so the space can be divided
  // evenly by (size of icon with padding).
  return static_cast<size_t>(std::max(
      0, available_space + platform_settings_.item_spacing) / IconWidth(true));
}

size_t ToolbarActionsBar::GetIconCount() const {
  if (!model_)
    return 0u;

  int pop_out_modifier = 0;
  // If there is a popped out action, it could affect the number of visible
  // icons - but only if it wouldn't otherwise be visible.
  if (popped_out_action_) {
    size_t popped_out_index =
        std::find(toolbar_actions_.begin(),
                  toolbar_actions_.end(),
                  popped_out_action_) - toolbar_actions_.begin();
    pop_out_modifier = popped_out_index >= model_->visible_icon_count() ? 1 : 0;
  }

  // We purposefully do not account for any "popped out" actions in overflow
  // mode. This is because the popup cannot be showing while the overflow menu
  // is open, so there's no concern there. Also, if the user has a popped out
  // action, and immediately opens the overflow menu, we *want* the action there
  // (since it will close the popup, but do so asynchronously, and we don't
  // want to "slide" the action back in.
  size_t visible_icons = in_overflow_mode() ?
      toolbar_actions_.size() - model_->visible_icon_count() :
      model_->visible_icon_count() + pop_out_modifier;

#if DCHECK_IS_ON()
  // Good time for some sanity checks: We should never try to display more
  // icons than we have, and we should always have a view per item in the model.
  // (The only exception is if this is in initialization.)
  if (!toolbar_actions_.empty() && !suppress_layout_ &&
      model_->extensions_initialized()) {
    size_t num_extension_actions = 0u;
    for (ToolbarActionViewController* action : toolbar_actions_) {
      // No component action should ever have a valid extension id, so we can
      // use this to check the extension amount.
      if (crx_file::id_util::IdIsValid(action->GetId()))
        ++num_extension_actions;
    }

    int num_component_actions =
        ComponentToolbarActionsFactory::GetInstance()->
            GetNumComponentActions();
    size_t num_total_actions = num_extension_actions + num_component_actions;

    DCHECK_LE(visible_icons, num_total_actions);
    DCHECK_EQ(model_->toolbar_items().size(), num_extension_actions);
  }
#endif

  return visible_icons;
}

std::vector<ToolbarActionViewController*>
ToolbarActionsBar::GetActions() const {
  std::vector<ToolbarActionViewController*> actions = toolbar_actions_.get();

  // If there is an action that should be popped out, and it's not visible by
  // default, make it the final action in the list.
  if (popped_out_action_) {
    size_t index =
        std::find(actions.begin(), actions.end(), popped_out_action_) -
        actions.begin();
    DCHECK_NE(actions.size(), index);
    size_t visible = GetIconCount();
    if (index >= visible) {
      size_t rindex = actions.size() - index - 1;
      std::rotate(actions.rbegin() + rindex,
                  actions.rbegin() + rindex + 1,
                  actions.rend() - visible + 1);
    }
  }

  return actions;
}

void ToolbarActionsBar::CreateActions() {
  DCHECK(toolbar_actions_.empty());
  // We wait for the extension system to be initialized before we add any
  // actions, as they rely on the extension system to function.
  if (!model_ || !model_->extensions_initialized())
    return;

  {
    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/463337
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile1(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("ToolbarActionsBar::CreateActions1"));
    // We don't redraw the view while creating actions.
    base::AutoReset<bool> layout_resetter(&suppress_layout_, true);

    // Extension actions come first.
    extensions::ExtensionActionManager* action_manager =
        extensions::ExtensionActionManager::Get(browser_->profile());
    const extensions::ExtensionList& toolbar_items = model_->toolbar_items();
    for (const scoped_refptr<const extensions::Extension>& extension :
             toolbar_items) {
      toolbar_actions_.push_back(new ExtensionActionViewController(
          extension.get(),
          browser_,
          action_manager->GetExtensionAction(*extension),
          this));
    }

    // Component actions come second, and are suppressed if the extension
    // actions are being highlighted.
    if (!model_->is_highlighting()) {
      // TODO(robliao): Remove ScopedTracker below once https://crbug.com/463337
      // is fixed.
      tracked_objects::ScopedTracker tracking_profile2(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "ToolbarActionsBar::CreateActions2"));

      ScopedVector<ToolbarActionViewController> component_actions =
          ComponentToolbarActionsFactory::GetInstance()->
              GetComponentToolbarActions();
      DCHECK(component_actions.empty() ||
          extensions::FeatureSwitch::extension_action_redesign()->IsEnabled());
      toolbar_actions_.insert(toolbar_actions_.end(),
                              component_actions.begin(),
                              component_actions.end());
      component_actions.weak_clear();
    }

    if (!toolbar_actions_.empty()) {
      // TODO(robliao): Remove ScopedTracker below once https://crbug.com/463337
      // is fixed.
      tracked_objects::ScopedTracker tracking_profile3(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "ToolbarActionsBar::CreateActions3"));
      ReorderActions();
    }

    tracked_objects::ScopedTracker tracking_profile4(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("ToolbarActionsBar::CreateActions4"));

    for (size_t i = 0; i < toolbar_actions_.size(); ++i)
      delegate_->AddViewForAction(toolbar_actions_[i], i);
  }

  // Once the actions are created, we should animate the changes.
  suppress_animation_ = false;

  // CreateActions() can be called multiple times, so we need to make sure we
  // haven't already shown the bubble.
  if (!checked_extension_bubble_) {
    checked_extension_bubble_ = true;
    // CreateActions() can be called as part of the browser window set up, which
    // we need to let finish before showing the actions.
    scoped_ptr<extensions::ExtensionMessageBubbleController> controller =
        ExtensionMessageBubbleFactory(browser_->profile()).GetController();
    if (controller) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ToolbarActionsBar::MaybeShowExtensionBubble,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(controller.Pass())));
    }
  }
}

void ToolbarActionsBar::DeleteActions() {
  HideActivePopup();
  delegate_->RemoveAllViews();
  toolbar_actions_.clear();
}

void ToolbarActionsBar::Update() {
  if (toolbar_actions_.empty())
    return;  // Nothing to do.

  {
    // Don't layout until the end.
    base::AutoReset<bool> layout_resetter(&suppress_layout_, true);
    for (ToolbarActionViewController* action : toolbar_actions_)
      action->UpdateState();
  }

  ReorderActions();  // Also triggers a draw.
}

void ToolbarActionsBar::SetOverflowRowWidth(int width) {
  DCHECK(in_overflow_mode());
  platform_settings_.icons_per_overflow_menu_row =
      std::max((width - kItemSpacing) / IconWidth(true), 1);
}

void ToolbarActionsBar::OnResizeComplete(int width) {
  DCHECK(!in_overflow_mode());  // The user can't resize the overflow container.
  size_t resized_count = WidthToIconCount(width);
  // Save off the desired number of visible icons. We do this now instead of
  // at the end of the animation so that even if the browser is shut down
  // while animating, the right value will be restored on next run.
  model_->SetVisibleIconCount(resized_count);
}

void ToolbarActionsBar::OnDragDrop(int dragged_index,
                                   int dropped_index,
                                   DragType drag_type) {
  // All drag-and-drop commands should go to the main bar.
  if (in_overflow_mode()) {
    main_bar_->OnDragDrop(dragged_index, dropped_index, drag_type);
    return;
  }

  int delta = 0;
  if (drag_type == DRAG_TO_OVERFLOW)
    delta = -1;
  else if (drag_type == DRAG_TO_MAIN)
    delta = 1;
  model_->MoveExtensionIcon(toolbar_actions_[dragged_index]->GetId(),
                            dropped_index);
  if (delta)
    model_->SetVisibleIconCount(model_->visible_icon_count() + delta);
}

void ToolbarActionsBar::OnAnimationEnded() {
  // Check if we were waiting for animation to complete to either show a
  // message bubble, or to show a popup.
  if (pending_extension_bubble_controller_) {
    MaybeShowExtensionBubble(pending_extension_bubble_controller_.Pass());
  } else if (!popped_out_closure_.is_null()) {
    popped_out_closure_.Run();
    popped_out_closure_.Reset();
  }
}

bool ToolbarActionsBar::IsActionVisible(
    const ToolbarActionViewController* action) const {
  size_t index = std::find(toolbar_actions_.begin(),
                           toolbar_actions_.end(),
                           action) - toolbar_actions_.begin();
  return index < GetIconCount() || action == popped_out_action_;
}

void ToolbarActionsBar::PopOutAction(ToolbarActionViewController* controller,
                                     const base::Closure& closure) {
  DCHECK(!popped_out_action_) << "Only one action can be popped out at a time!";
  bool needs_redraw = !IsActionVisible(controller);
  popped_out_action_ = controller;
  if (needs_redraw) {
    // We suppress animation for this draw, because we need the action to get
    // into position immediately, since it's about to show its popup.
    base::AutoReset<bool> layout_resetter(&suppress_animation_, false);
    delegate_->Redraw(true);
  }

  ResizeDelegate(gfx::Tween::LINEAR, false);
  if (!delegate_->IsAnimating()) {
    // Don't call the closure re-entrantly.
    base::MessageLoop::current()->PostTask(FROM_HERE, closure);
  } else {
    popped_out_closure_ = closure;
  }
}

void ToolbarActionsBar::UndoPopOut() {
  DCHECK(popped_out_action_);
  ToolbarActionViewController* controller = popped_out_action_;
  popped_out_action_ = nullptr;
  popped_out_closure_.Reset();
  if (!IsActionVisible(controller))
    delegate_->Redraw(true);
  ResizeDelegate(gfx::Tween::LINEAR, false);
}

void ToolbarActionsBar::SetPopupOwner(
    ToolbarActionViewController* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((!popup_owner_ && popup_owner) ||
         (popup_owner_ && !popup_owner));
  popup_owner_ = popup_owner;
}

void ToolbarActionsBar::HideActivePopup() {
  if (popup_owner_)
    popup_owner_->HidePopup();
  DCHECK(!popup_owner_);
}

ToolbarActionViewController* ToolbarActionsBar::GetMainControllerForAction(
    ToolbarActionViewController* action) {
  return in_overflow_mode() ?
      main_bar_->GetActionForId(action->GetId()) : action;
}

void ToolbarActionsBar::MaybeShowExtensionBubble(
    scoped_ptr<extensions::ExtensionMessageBubbleController> controller) {
  controller->HighlightExtensionsIfNecessary();  // Safe to call multiple times.
  if (delegate_->IsAnimating()) {
    // If the toolbar is animating, we can't effectively anchor the bubble,
    // so wait until animation stops.
    pending_extension_bubble_controller_ = controller.Pass();
  } else {
    const extensions::ExtensionIdList& affected_extensions =
        controller->GetExtensionIdList();
    ToolbarActionViewController* anchor_action = nullptr;
    for (const std::string& id : affected_extensions) {
      anchor_action = GetActionForId(id);
      if (anchor_action)
        break;
    }
    delegate_->ShowExtensionMessageBubble(controller.Pass(), anchor_action);
  }
}

void ToolbarActionsBar::OnToolbarExtensionAdded(
    const extensions::Extension* extension,
    int index) {
  DCHECK(GetActionForId(extension->id()) == nullptr) <<
      "Asked to add a toolbar action view for an extension that already exists";

  toolbar_actions_.insert(
      toolbar_actions_.begin() + index,
      new ExtensionActionViewController(
          extension,
          browser_,
          extensions::ExtensionActionManager::Get(browser_->profile())->
              GetExtensionAction(*extension),
          this));

  delegate_->AddViewForAction(toolbar_actions_[index], index);

  // If we are still initializing the container, don't bother animating.
  if (!model_->extensions_initialized())
    return;

  // We may need to resize (e.g. to show the new icon, or the chevron). We don't
  // need to check if the extension is upgrading here, because ResizeDelegate()
  // checks to see if the container is already the proper size, and because
  // if the action is newly incognito enabled, even though it's a reload, it's
  // a new extension to this toolbar.
  // We suppress the chevron during animation because, if we're expanding to
  // show a new icon, we don't want to have the chevron visible only for the
  // duration of the animation.
  ResizeDelegate(gfx::Tween::LINEAR, true);
}

void ToolbarActionsBar::OnToolbarExtensionRemoved(
    const extensions::Extension* extension) {
  ToolbarActions::iterator iter = toolbar_actions_.begin();
  while (iter != toolbar_actions_.end() && (*iter)->GetId() != extension->id())
    ++iter;

  if (iter == toolbar_actions_.end())
    return;

  // The action should outlive the UI element (which is owned by the delegate),
  // so we can't delete it just yet. But we should remove it from the list of
  // actions so that any width calculations are correct.
  scoped_ptr<ToolbarActionViewController> removed_action(*iter);
  toolbar_actions_.weak_erase(iter);
  delegate_->RemoveViewForAction(removed_action.get());
  removed_action.reset();

  // If the extension is being upgraded we don't want the bar to shrink
  // because the icon is just going to get re-added to the same location.
  // There is an exception if this is an off-the-record profile, and the
  // extension is no longer incognito-enabled.
  if (!extensions::ExtensionSystem::Get(browser_->profile())->runtime_data()->
            IsBeingUpgraded(extension->id()) ||
      (browser_->profile()->IsOffTheRecord() &&
       !extensions::util::IsIncognitoEnabled(extension->id(),
                                             browser_->profile()))) {
    if (toolbar_actions_.size() > model_->visible_icon_count()) {
      // If we have more icons than we can show, then we must not be changing
      // the container size (since we either removed an icon from the main
      // area and one from the overflow list will have shifted in, or we
      // removed an entry directly from the overflow list).
      delegate_->Redraw(false);
    } else {
      delegate_->SetChevronVisibility(false);
      // Either we went from overflow to no-overflow, or we shrunk the no-
      // overflow container by 1.  Either way the size changed, so animate.
      ResizeDelegate(gfx::Tween::EASE_OUT, false);
    }
  }
}

void ToolbarActionsBar::OnToolbarExtensionMoved(
    const extensions::Extension* extension,
    int index) {
  DCHECK(index >= 0 && index < static_cast<int>(toolbar_actions_.size()));
  // Unfortunately, |index| doesn't really mean a lot to us, because this
  // window's toolbar could be different (if actions are popped out). Just
  // do a full reorder.
  ReorderActions();
}

void ToolbarActionsBar::OnToolbarExtensionUpdated(
    const extensions::Extension* extension) {
  ToolbarActionViewController* action = GetActionForId(extension->id());
  // There might not be a view in cases where we are highlighting or if we
  // haven't fully initialized the actions.
  if (action) {
    action->UpdateState();
    SetOverflowedActionWantsToRun();
  }
}

bool ToolbarActionsBar::ShowExtensionActionPopup(
    const extensions::Extension* extension,
    bool grant_active_tab) {
  // Don't override another popup, and only show in the active window.
  if (popup_owner() || !browser_->window()->IsActive())
    return false;

  ToolbarActionViewController* action = GetActionForId(extension->id());
  return action && action->ExecuteAction(grant_active_tab);
}

void ToolbarActionsBar::OnToolbarVisibleCountChanged() {
  ResizeDelegate(gfx::Tween::EASE_OUT, false);
  SetOverflowedActionWantsToRun();
}

void ToolbarActionsBar::ResizeDelegate(gfx::Tween::Type tween_type,
                                       bool suppress_chevron) {
  int desired_width = GetPreferredSize().width();
  if (desired_width != delegate_->GetWidth()) {
    delegate_->ResizeAndAnimate(tween_type, desired_width, suppress_chevron);
  } else if (delegate_->IsAnimating()) {
    // It's possible that we're right where we're supposed to be in terms of
    // width, but that we're also currently resizing. If this is the case, end
    // the current animation with the current width.
    delegate_->StopAnimating();
  } else {
    // We may already be at the right size (this can happen frequently with
    // overflow, where we have a fixed width, and in tests, where we skip
    // animations). If this is the case, we still need to Redraw(), because the
    // icons within the toolbar may have changed (e.g. if we removed one
    // action and added a different one in quick succession).
    delegate_->Redraw(false);
  }
}

void ToolbarActionsBar::OnToolbarHighlightModeChanged(bool is_highlighting) {
  // It's a bit of a pain that we delete and recreate everything here, but given
  // everything else going on (the lack of highlight, [n] more extensions
  // appearing, etc), it's not worth the extra complexity to create and insert
  // only the new actions.
  DeleteActions();
  CreateActions();
  // Resize the delegate. We suppress the chevron so that we don't risk showing
  // it only for the duration of the animation.
  ResizeDelegate(gfx::Tween::LINEAR, true);
}

void ToolbarActionsBar::OnToolbarModelInitialized() {
  // We shouldn't have any actions before the model is initialized.
  DCHECK(toolbar_actions_.empty());
  CreateActions();

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/463337 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "ToolbarActionsBar::OnToolbarModelInitialized"));
  ResizeDelegate(gfx::Tween::EASE_OUT, false);
}

Browser* ToolbarActionsBar::GetBrowser() {
  return browser_;
}

void ToolbarActionsBar::ReorderActions() {
  if (toolbar_actions_.empty())
    return;

  // First, reset the order to that of the model.
  auto compare = [](ToolbarActionViewController* const& action,
                    const scoped_refptr<const extensions::Extension>& ext) {
    return action->GetId() == ext->id();
  };
  SortContainer(&toolbar_actions_.get(), model_->toolbar_items(), compare);

  // Our visible browser actions may have changed - re-Layout() and check the
  // size (if we aren't suppressing the layout).
  if (!suppress_layout_) {
    ResizeDelegate(gfx::Tween::EASE_OUT, false);
    delegate_->Redraw(true);
  }

  SetOverflowedActionWantsToRun();
}

void ToolbarActionsBar::SetOverflowedActionWantsToRun() {
  if (in_overflow_mode())
    return;
  bool overflowed_action_wants_to_run = false;
  content::WebContents* web_contents = GetCurrentWebContents();
  for (size_t i = GetIconCount(); i < toolbar_actions_.size(); ++i) {
    if (toolbar_actions_[i]->WantsToRun(web_contents)) {
      overflowed_action_wants_to_run = true;
      break;
    }
  }

  if (overflowed_action_wants_to_run_ != overflowed_action_wants_to_run) {
    overflowed_action_wants_to_run_ = overflowed_action_wants_to_run;
    if (send_overflowed_action_changes_)
      delegate_->OnOverflowedActionWantsToRunChanged(
          overflowed_action_wants_to_run_);
  }
}

ToolbarActionViewController* ToolbarActionsBar::GetActionForId(
    const std::string& id) {
  for (ToolbarActionViewController* action : toolbar_actions_) {
    if (action->GetId() == id)
      return action;
  }
  return nullptr;
}

content::WebContents* ToolbarActionsBar::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
