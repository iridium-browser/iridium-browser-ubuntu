// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_H_
#define CC_SURFACES_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class LatencyInfo;
}

namespace cc {

class BeginFrameSource;
class CompositorFrame;
class CopyOutputRequest;
class SurfaceFactory;

class CC_SURFACES_EXPORT Surface {
 public:
  using DrawCallback = SurfaceFactory::DrawCallback;

  Surface(const SurfaceId& id, base::WeakPtr<SurfaceFactory> factory);
  ~Surface();

  const SurfaceId& surface_id() const { return surface_id_; }
  const SurfaceId& previous_frame_surface_id() const {
    return previous_frame_surface_id_;
  }

  void SetPreviousFrameSurface(Surface* surface);

  void QueueFrame(CompositorFrame frame, const DrawCallback& draw_callback);
  void EvictFrame();
  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> copy_request);
  // Adds each CopyOutputRequest in the current frame to copy_requests. The
  // caller takes ownership of them. |copy_requests| is keyed by RenderPass ids.
  void TakeCopyOutputRequests(
      std::multimap<int, std::unique_ptr<CopyOutputRequest>>* copy_requests);

  // Returns the most recent frame that is eligible to be rendered.
  // You must check whether HasFrame() returns true before calling this method.
  const CompositorFrame& GetEligibleFrame() const;

  // Returns a number that increments by 1 every time a new frame is enqueued.
  int frame_index() const { return frame_index_; }

  void TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info);
  void RunDrawCallbacks();

  base::WeakPtr<SurfaceFactory> factory() { return factory_; }

  // Add a SurfaceSequence that must be satisfied before the Surface is
  // destroyed.
  void AddDestructionDependency(SurfaceSequence sequence);

  // Satisfy all destruction dependencies that are contained in sequences, and
  // remove them from sequences.
  void SatisfyDestructionDependencies(
      std::unordered_set<SurfaceSequence, SurfaceSequenceHash>* sequences,
      std::unordered_set<FrameSinkId, FrameSinkIdHash>* valid_id_namespaces);
  size_t GetDestructionDependencyCount() const {
    return destruction_dependencies_.size();
  }

  const std::vector<SurfaceId>& referenced_surfaces() const {
    return referenced_surfaces_;
  }

  bool HasFrame() const { return current_frame_.has_value(); }

  bool destroyed() const { return destroyed_; }
  void set_destroyed(bool destroyed) { destroyed_ = destroyed; }

 private:
  void UnrefFrameResources(const CompositorFrame& frame_data);
  void ClearCopyRequests();

  SurfaceId surface_id_;
  SurfaceId previous_frame_surface_id_;
  base::WeakPtr<SurfaceFactory> factory_;
  // TODO(jamesr): Support multiple frames in flight.
  base::Optional<CompositorFrame> current_frame_;
  int frame_index_;
  bool destroyed_;
  std::vector<SurfaceSequence> destruction_dependencies_;

  // This surface may have multiple BeginFrameSources if it is
  // on multiple Displays.
  std::set<BeginFrameSource*> begin_frame_sources_;

  std::vector<SurfaceId> referenced_surfaces_;

  DrawCallback draw_callback_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_H_
