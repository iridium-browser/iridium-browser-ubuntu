// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_
#define MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mandoline {

class Frame;
class FrameUserData;
class HTMLMessageEvent;

class FrameTreeDelegate {
 public:
  // Returns whether a request to post a message from |source| to |target|
  // is allowed. |source| and |target| are never null.
  virtual bool CanPostMessageEventToFrame(const Frame* source,
                                          const Frame* target,
                                          HTMLMessageEvent* event) = 0;

  virtual void LoadingStateChanged(bool loading) = 0;
  virtual void ProgressChanged(double progress) = 0;

  // |source| is requesting a navigation. If |target_type| is
  // |EXISTING_FRAME| then |target_frame| identifies the frame to perform the
  // navigation in, otherwise |target_frame| is not used. |target_frame| may
  // be null, even for |EXISTING_FRAME|.
  // TODO(sky): this needs to distinguish between navigate in source, vs new
  // background tab, vs new foreground tab.
  virtual void NavigateTopLevel(Frame* source, mojo::URLRequestPtr request) = 0;

  // Returns true if |target| can navigation to |request|. If the navigation is
  // allowed the client should set the arguments appropriately.
  virtual bool CanNavigateFrame(
      Frame* target,
      mojo::URLRequestPtr request,
      FrameTreeClient** frame_tree_client,
      scoped_ptr<FrameUserData>* frame_user_data,
      mojo::ViewManagerClientPtr* view_manager_client) = 0;

  // Invoked when a navigation in |frame| has been initiated.
  virtual void DidStartNavigation(Frame* frame) = 0;

 protected:
  virtual ~FrameTreeDelegate() {}
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_
