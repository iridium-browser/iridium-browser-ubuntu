// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PermissionBubbleRequest;

// Provides access to permissions bubbles. Allows clients to add a request
// callback interface to the existing permission bubble configuration.
// Depending on the situation and policy, that may add new UI to an existing
// permission bubble, create and show a new permission bubble, or provide no
// visible UI action at all. (In that case, the request will be immediately
// informed that the permission request failed.)
//
// A PermissionBubbleManager is associated with a particular WebContents.
// Requests attached to a particular WebContents' PBM must outlive it.
//
// The PermissionBubbleManager should be addressed on the UI thread.
class PermissionBubbleManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PermissionBubbleManager>,
      public PermissionBubbleView::Delegate {
 public:
  // Return the enabled state of permissions bubbles.
  // Controlled by a flag and FieldTrial.
  static bool Enabled();

  ~PermissionBubbleManager() override;

  // Adds a new request to the permission bubble. Ownership of the request
  // remains with the caller. The caller must arrange for the request to
  // outlive the PermissionBubbleManager. If a bubble is visible when this
  // call is made, the request will be queued up and shown after the current
  // bubble closes. A request with message text identical to an outstanding
  // request will receive a RequestFinished call immediately and not be added.
  virtual void AddRequest(PermissionBubbleRequest* request);

  // Cancels an outstanding request. This may have different effects depending
  // on what is going on with the bubble. If the request is pending, it will be
  // removed and never shown. If the request is showing, it will continue to be
  // shown, but the user's action won't be reported back to the request object.
  // In some circumstances, we can remove the request from the bubble, and may
  // do so. The request will have RequestFinished executed on it if it is found,
  // at which time the caller is free to delete the request.
  virtual void CancelRequest(PermissionBubbleRequest* request);

  // Sets the active view for the permission bubble. If this is NULL, it
  // means any existing permission bubble can no longer be shown. Does not
  // take ownership of the view.
  void SetView(PermissionBubbleView* view) override;

  // Controls whether incoming permission requests require user gestures.
  // If |required| is false, requests will be displayed as soon as they come in.
  // If |required| is true, requests will be silently queued until a request
  // comes in with a user gesture.
  void RequireUserGesture(bool required);

 private:
  friend class DownloadRequestLimiterTest;
  friend class GeolocationBrowserTest;
  friend class GeolocationPermissionContextTests;
  friend class GeolocationPermissionContextParamTests;
  friend class PermissionBubbleManagerTest;
  friend class PermissionContextBaseTests;
  friend class content::WebContentsUserData<PermissionBubbleManager>;

  explicit PermissionBubbleManager(content::WebContents* web_contents);

  // WebContentsObserver:
  void DocumentOnLoadCompletedInMainFrame() override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override;

  // If a page on which permissions requests are pending is navigated,
  // they will be finalized as if canceled by the user.
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) override;
  void WebContentsDestroyed() override;

  // PermissionBubbleView::Delegate:
  void ToggleAccept(int request_index, bool new_value) override;
  void Accept() override;
  void Deny() override;
  void Closing() override;

  // Posts a task which will allow the bubble to become visible if it is needed.
  void ScheduleShowBubble();

  // Shows the bubble if it is not already visible and there are pending
  // requests.
  void TriggerShowBubble();

  // Finalize the pending permissions request.
  void FinalizeBubble();

  // Cancel any pending requests. This is called if the WebContents
  // on which permissions calls are pending is destroyed or navigated away
  // from the requesting page.
  void CancelPendingQueues();

  // Returns whether or not |request| has already been added to |queue|.
  // |same_object| must be non-null.  It will be set to true if |request|
  // is the same object as an existing request in |queue|, false otherwise.
  bool ExistingRequest(PermissionBubbleRequest* request,
                       const std::vector<PermissionBubbleRequest*>& queue,
                       bool* same_object);

  // Returns true if |queue| contains a request which was generated by a user
  // gesture.  Returns false otherwise.
  bool HasUserGestureRequest(
      const std::vector<PermissionBubbleRequest*>& queue);

  // Whether to delay displaying the bubble until a request with a user gesture.
  // False by default, unless RequireUserGesture(bool) changes the value.
  bool require_user_gesture_;

  // Whether or not we are showing the bubble in this tab.
  bool bubble_showing_;

  // Set to the UI surface to be used to display the permissions requests.
  PermissionBubbleView* view_;

  std::vector<PermissionBubbleRequest*> requests_;
  std::vector<PermissionBubbleRequest*> queued_requests_;
  std::vector<PermissionBubbleRequest*> queued_frame_requests_;

  // URL of the main frame in the WebContents to which this manager is attached.
  // TODO(gbillock): if there are iframes in the page, we need to deal with it.
  GURL request_url_;
  bool request_url_has_loaded_;

  std::vector<bool> accept_states_;

  base::WeakPtrFactory<PermissionBubbleManager> weak_factory_;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_
