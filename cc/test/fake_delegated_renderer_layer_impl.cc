// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_delegated_renderer_layer_impl.h"

#include "base/bind.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/returned_resource.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

FakeDelegatedRendererLayerImpl::FakeDelegatedRendererLayerImpl(
    LayerTreeImpl* tree_impl,
    int id)
    : DelegatedRendererLayerImpl(tree_impl, id) {
}

FakeDelegatedRendererLayerImpl::~FakeDelegatedRendererLayerImpl() {}

scoped_ptr<LayerImpl> FakeDelegatedRendererLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return FakeDelegatedRendererLayerImpl::Create(tree_impl, id());
}

static ResourceProvider::ResourceId AddResourceToFrame(
    ResourceProvider* resource_provider,
    DelegatedFrameData* frame,
    ResourceProvider::ResourceId resource_id) {
  TransferableResource resource;
  resource.id = resource_id;
  resource.mailbox_holder.texture_target =
      resource_provider->TargetForTesting(resource_id);
  frame->resource_list.push_back(resource);
  return resource_id;
}

ResourceProvider::ResourceIdSet FakeDelegatedRendererLayerImpl::Resources()
    const {
  ResourceProvider::ResourceIdSet set;
  ResourceProvider::ResourceIdArray array;
  array = ResourcesForTesting();
  for (size_t i = 0; i < array.size(); ++i)
    set.insert(array[i]);
  return set;
}

void NoopReturnCallback(const ReturnedResourceArray& returned,
                        BlockingTaskRunner* main_thread_task_runner) {
}

void FakeDelegatedRendererLayerImpl::SetFrameDataForRenderPasses(
    float device_scale_factor,
    const RenderPassList& pass_list) {
  scoped_ptr<DelegatedFrameData> delegated_frame(new DelegatedFrameData);
  delegated_frame->device_scale_factor = device_scale_factor;
  RenderPass::CopyAll(pass_list, &delegated_frame->render_pass_list);

  ResourceProvider* resource_provider = layer_tree_impl()->resource_provider();

  DrawQuad::ResourceIteratorCallback add_resource_to_frame_callback =
      base::Bind(&AddResourceToFrame, resource_provider, delegated_frame.get());
  for (const auto& pass : delegated_frame->render_pass_list) {
    for (const auto& quad : pass->quad_list)
      quad->IterateResources(add_resource_to_frame_callback);
  }

  CreateChildIdIfNeeded(base::Bind(&NoopReturnCallback));
  SetFrameData(delegated_frame.get(), gfx::RectF());
}

}  // namespace cc
