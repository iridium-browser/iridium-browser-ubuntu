// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host_factory.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/test/test_render_frame_host.h"

namespace content {

TestRenderFrameHostFactory::TestRenderFrameHostFactory() {
  RenderFrameHostFactory::RegisterFactory(this);
}

TestRenderFrameHostFactory::~TestRenderFrameHostFactory() {
  RenderFrameHostFactory::UnregisterFactory();
}

scoped_ptr<RenderFrameHostImpl>
TestRenderFrameHostFactory::CreateRenderFrameHost(
    SiteInstance* site_instance,
    RenderViewHostImpl* render_view_host,
    RenderFrameHostDelegate* delegate,
    RenderWidgetHostDelegate* rwh_delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int routing_id,
    int flags) {
  return make_scoped_ptr(new TestRenderFrameHost(
      site_instance, render_view_host, delegate, rwh_delegate, frame_tree,
      frame_tree_node, routing_id, flags));
}

}  // namespace content
