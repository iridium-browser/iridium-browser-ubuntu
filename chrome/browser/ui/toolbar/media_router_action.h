// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_

#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"

class Browser;
class MediaRouterActionPlatformDelegate;

namespace media_router {
class MediaRouterDialogController;
}  // namespace media_router

// The class for the Media Router component action that will be shown in
// the toolbar.
class MediaRouterAction : public ToolbarActionViewController,
                          public media_router::IssuesObserver,
                          public media_router::MediaRoutesObserver {
 public:
  explicit MediaRouterAction(Browser* browser);
  ~MediaRouterAction() override;

  // ToolbarActionViewController implementation.
  std::string GetId() const override;
  void SetDelegate(ToolbarActionViewDelegate* delegate) override;
  gfx::Image GetIcon(content::WebContents* web_contents,
                     const gfx::Size& size) override;
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
  bool DisabledClickOpensMenu() const override;

  // media_router::IssuesObserver:
  void OnIssueUpdated(const media_router::Issue* issue) override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const std::vector<media_router::MediaRoute>& routes)
      override;

 private:
  // Returns a reference to the MediaRouterDialogController associated with
  // |delegate_|'s current WebContents. Guaranteed to be non-null.
  // |delegate_| and its current WebContents must not be null.
  media_router::MediaRouterDialogController* GetMediaRouterDialogController();

  // Overridden by tests.
  virtual media_router::MediaRouter* GetMediaRouter(Browser* browser);

  // Checks if the current icon of MediaRouterAction has changed. If so,
  // updates |current_icon_|.
  void MaybeUpdateIcon();

  const gfx::Image* GetCurrentIcon() const;

  // Cached icons.
  // Indicates that the current Chrome profile is using at least one device.
  const gfx::Image media_router_active_icon_;
  // Indicates a failure, e.g. session launch failure.
  const gfx::Image media_router_error_icon_;
  // Indicates that the current Chrome profile is not using any devices.
  // Devices may or may not be available.
  const gfx::Image media_router_idle_icon_;
  // Indicates there is a warning message.
  const gfx::Image media_router_warning_icon_;

  // The current icon to show. This is updated based on the current issues and
  // routes since |this| is an IssueObserver and MediaRoutesObserver.
  const gfx::Image* current_icon_;

  // The current issue shown in the Media Router WebUI. Can be null. It is set
  // in OnIssueUpdated(), which is called by the IssueManager.
  scoped_ptr<media_router::Issue> issue_;

  // Whether a local active route exists.
  bool has_local_route_;

  ToolbarActionViewDelegate* delegate_;

  // The delegate to handle platform-specific implementations.
  scoped_ptr<MediaRouterActionPlatformDelegate> platform_delegate_;

  MediaRouterContextualMenu contextual_menu_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterAction);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_
