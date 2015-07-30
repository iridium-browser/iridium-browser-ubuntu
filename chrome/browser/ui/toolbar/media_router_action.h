// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"

// The class for the Media Router component action that will be shown in
// the toolbar.
class MediaRouterAction : public ToolbarActionViewController {
 public:
  MediaRouterAction();
  ~MediaRouterAction() override;

  // ToolbarActionViewController implementation.
  const std::string& GetId() const override;
  void SetDelegate(ToolbarActionViewDelegate* delegate) override;
  gfx::Image GetIcon(content::WebContents* web_contents) override;
  gfx::ImageSkia GetIconWithBadge() override;
  base::string16 GetActionName() const override;
  base::string16 GetAccessibleName(content::WebContents* web_contents)
      const override;
  base::string16 GetTooltip(content::WebContents* web_contents)
      const override;
  bool IsEnabled(content::WebContents* web_contents) const override;
  bool WantsToRun(content::WebContents* web_contents) const override;
  bool HasPopup(content::WebContents* web_contents) const override;
  void HidePopup() override;
  gfx::NativeView GetPopupNativeView() override;
  ui::MenuModel* GetContextMenu() override;
  bool CanDrag() const override;
  bool ExecuteAction(bool by_user) override;
  void UpdateState() override;

 private:
  const std::string id_;
  const base::string16 name_;

  // Cached icons.
  gfx::Image media_router_idle_icon_;

  ToolbarActionViewDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterAction);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_
