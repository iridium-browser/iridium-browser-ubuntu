// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_HOST_REMOTE_FOR_TESTING_H_
#define CC_TEST_LAYER_TREE_HOST_REMOTE_FOR_TESTING_H_

#include "base/macros.h"
#include "cc/blimp/compositor_state_deserializer.h"
#include "cc/blimp/layer_tree_host_remote.h"

namespace cc {

class AnimationHost;
class CompositorProtoState;
class FakeImageSerializationProcessor;
class LayerTreeHostInProcess;
class MutatorHost;
class TaskGraphRunner;

// This is a version of LayerTreeHostRemote meant to be used for tests that want
// to inspect the CompositorFrame produced when state updates from the remote
// host are used by a compositor on the client.
class LayerTreeHostRemoteForTesting : public LayerTreeHostRemote,
                                      public CompositorStateDeserializerClient {
 public:
  static std::unique_ptr<LayerTreeHostRemoteForTesting> Create(
      LayerTreeHostClient* client,
      MutatorHost* mutator_host,
      LayerTreeSettings const* settings,
      TaskGraphRunner* task_graph_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);

  static std::unique_ptr<RemoteCompositorBridge> CreateRemoteCompositorBridge(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  ~LayerTreeHostRemoteForTesting() override;

  // LayerTreeHost interface.
  void SetVisible(bool visible) override;
  void SetCompositorFrameSink(
      std::unique_ptr<CompositorFrameSink> compositor_frame_sink) override;
  std::unique_ptr<CompositorFrameSink> ReleaseCompositorFrameSink() override;
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override;
  void SetNextCommitForcesRedraw() override;
  void NotifyInputThrottledUntilCommit() override;
  const base::WeakPtr<InputHandler>& GetInputHandler() const override;

  LayerTreeHostInProcess* layer_tree_host_in_process() const {
    return layer_tree_host_in_process_.get();
  }

 protected:
  explicit LayerTreeHostRemoteForTesting(InitParams* params);

  void Initialize(TaskGraphRunner* task_graph_runner,
                  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
                  std::unique_ptr<FakeImageSerializationProcessor>
                      image_serialization_processor);

  virtual std::unique_ptr<LayerTreeHostInProcess> CreateLayerTreeHostInProcess(
      LayerTreeHostClient* client,
      TaskGraphRunner* task_graph_runner,
      const LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      MutatorHost* mutator_host);

 private:
  class LayerTreeHostInProcessClient;
  class RemoteCompositorBridgeImpl;

  // CompositorStateDeserializerClient implementation.
  void DidUpdateLocalState() override;

  // LayerTreeHostRemote interface.
  void DispatchDrawAndSubmitCallbacks() override;

  void BeginRemoteMainFrame();

  void RemoteHostNeedsMainFrame();
  void ProcessRemoteCompositorUpdate(
      std::unique_ptr<CompositorProtoState> compositor_proto_state);

  std::unique_ptr<LayerTreeHostInProcess> layer_tree_host_in_process_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<CompositorStateDeserializer> compositor_state_deserializer_;

  bool client_state_dirty_ = false;

  std::unique_ptr<LayerTreeHostInProcessClient>
      layer_tree_host_in_process_client_;

  std::unique_ptr<FakeImageSerializationProcessor>
      image_serialization_processor_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHostRemoteForTesting);
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_HOST_REMOTE_FOR_TESTING_H_
