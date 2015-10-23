// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/create_presentation_session_request.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

// An abstract base class for Media Router dialog controllers. Tied to a
// WebContents known as the |initiator|, and is lazily created when a Media
// Router dialog needs to be shown. The MediaRouterDialogController allows
// showing and closing a Media Router dialog modal to the initiator WebContents.
// This class is not thread safe and must be called on the UI thread.
class MediaRouterDialogController {
 public:
  virtual ~MediaRouterDialogController();

  // Gets a reference to the MediaRouterDialogController associated with
  // |web_contents|, creating one if it does not exist. The returned pointer is
  // guaranteed to be non-null.
  static MediaRouterDialogController* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Shows the media router dialog modal to |initiator_| and the parameters
  // specified in |request|.
  // Creates the dialog if it did not exist prior to this call, returns true.
  // If the dialog already exists, brings it to the front but doesn't change the
  // dialog with |request|, returns false and |request| is deleted.
  bool ShowMediaRouterDialogForPresentation(
      scoped_ptr<CreatePresentationSessionRequest> request);

  // Shows the media router dialog modal to |initiator_|.
  // Creates the dialog if it did not exist prior to this call, returns true.
  // If the dialog already exists, brings it to the front, returns false.
  virtual bool ShowMediaRouterDialog();

  // Hides the media router dialog.
  // It is a no-op to call this function if there is currently no dialog.
  void HideMediaRouterDialog();

 protected:
  // Use MediaRouterDialogController::GetOrCreateForWebContents() to create an
  // instance.
  explicit MediaRouterDialogController(content::WebContents* initiator);

  // Activates the WebContents that initiated the dialog, e.g. focuses the tab.
  void ActivateInitiatorWebContents();

  // Passes the ownership of the CreatePresentationSessionRequest to the caller.
  scoped_ptr<CreatePresentationSessionRequest> TakePresentationRequest();

  // Returns the CreatePresentationSessionRequest to the caller but keeps the
  // ownership with the MediaRouterDialogController.
  const CreatePresentationSessionRequest* presentation_request() const {
    return presentation_request_.get();
  }

  // Returns the WebContents that initiated showing the dialog.
  content::WebContents* initiator() const { return initiator_; }

  // Resets the state of the controller. Must be called from the overrides.
  virtual void Reset();
  // Creates a new media router dialog modal to |initiator_|.
  virtual void CreateMediaRouterDialog() = 0;
  // Closes the media router dialog if it exists.
  virtual void CloseMediaRouterDialog() = 0;
  // Indicates if the media router dialog already exists.
  virtual bool IsShowingMediaRouterDialog() const = 0;

  base::ThreadChecker thread_checker_;

 private:
  class InitiatorWebContentsObserver;

  // An observer for the |initiator_| that closes the dialog when |initiator_|
  // is destroyed or navigated.
  scoped_ptr<InitiatorWebContentsObserver> initiator_observer_;
  content::WebContents* const initiator_;

  // Data for dialogs created at the request of the Presentation API.
  // Passed from the caller via ShowMediaRouterDialogForPresentation to the
  // dialog when it is initialized.
  scoped_ptr<CreatePresentationSessionRequest> presentation_request_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogController);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
