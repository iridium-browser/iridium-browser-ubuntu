// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_H_

#include "base/macros.h"
#include "components/subresource_filter/core/common/activation_state.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

// The content-layer-specific driver for the subresource filter component. There
// is one instance per RenderFrameHost.
class ContentSubresourceFilterDriver {
 public:
  explicit ContentSubresourceFilterDriver(
      content::RenderFrameHost* render_frame_host);
  virtual ~ContentSubresourceFilterDriver();

  // Instructs the agent on the renderer to set up the subresource filter for
  // the currently ongoing provisional document load in the frame.
  virtual void ActivateForProvisionalLoad(ActivationState activation_state,
                                          const GURL& url,
                                          bool measure_performance);

 private:
  // The RenderFrameHost that this driver belongs to.
  content::RenderFrameHost* render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriver);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_H_
