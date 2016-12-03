// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
#define CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/extensions/api/automation_internal/automation_action_adapter.h"
#include "chrome/browser/ui/aura/accessibility/ax_tree_source_aura.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace views {
class AXAuraObjWrapper;
class View;
}  // namespace views

using AuraAXTreeSerializer =
    ui::AXTreeSerializer<views::AXAuraObjWrapper*,
                         ui::AXNodeData,
                         ui::AXTreeData>;

// Manages a tree of automation nodes.
class AutomationManagerAura : public extensions::AutomationActionAdapter {
 public:
  // Get the single instance of this class.
  static AutomationManagerAura* GetInstance();

  // Enable automation support for views.
  void Enable(content::BrowserContext* context);

  // Disable automation support for views.
  void Disable();

  // Handle an event fired upon a |View|.
  void HandleEvent(content::BrowserContext* context,
                   views::View* view,
                   ui::AXEvent event_type);

  void HandleAlert(content::BrowserContext* context, const std::string& text);

  // AutomationActionAdapter implementation.
  void DoDefault(int32_t id) override;
  void Focus(int32_t id) override;
  void MakeVisible(int32_t id) override;
  void SetSelection(int32_t anchor_id,
                    int32_t anchor_offset,
                    int32_t focus_id,
                    int32_t focus_offset) override;
  void ShowContextMenu(int32_t id) override;

 private:
  friend struct base::DefaultSingletonTraits<AutomationManagerAura>;

  AutomationManagerAura();
  virtual ~AutomationManagerAura();

  // Reset all state in this manager.
  void ResetSerializer();

  void SendEvent(content::BrowserContext* context,
                 views::AXAuraObjWrapper* aura_obj,
                 ui::AXEvent event_type);

  // Whether automation support for views is enabled.
  bool enabled_;

  // Holds the active views-based accessibility tree. A tree currently consists
  // of all views descendant to a |Widget| (see |AXTreeSourceViews|).
  // A tree becomes active when an event is fired on a descendant view.
  std::unique_ptr<AXTreeSourceAura> current_tree_;

  // Serializes incremental updates on the currently active tree
  // |current_tree_|.
  std::unique_ptr<AuraAXTreeSerializer> current_tree_serializer_;

  bool processing_events_;

  std::vector<std::pair<views::AXAuraObjWrapper*, ui::AXEvent>> pending_events_;

  DISALLOW_COPY_AND_ASSIGN(AutomationManagerAura);
};

#endif  // CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
