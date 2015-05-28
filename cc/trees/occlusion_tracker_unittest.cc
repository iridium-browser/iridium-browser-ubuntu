// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/occlusion_tracker.h"

#include "cc/animation/layer_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_occlusion_tracker.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestContentLayer : public Layer {
 public:
  TestContentLayer() : Layer(), override_opaque_contents_rect_(false) {
    SetIsDrawable(true);
  }

  SimpleEnclosedRegion VisibleContentOpaqueRegion() const override {
    if (override_opaque_contents_rect_) {
      return SimpleEnclosedRegion(
          gfx::IntersectRects(opaque_contents_rect_, visible_content_rect()));
    }
    return Layer::VisibleContentOpaqueRegion();
  }
  void SetOpaqueContentsRect(const gfx::Rect& opaque_contents_rect) {
    override_opaque_contents_rect_ = true;
    opaque_contents_rect_ = opaque_contents_rect;
  }

 private:
  ~TestContentLayer() override {}

  bool override_opaque_contents_rect_;
  gfx::Rect opaque_contents_rect_;
};

class TestContentLayerImpl : public LayerImpl {
 public:
  TestContentLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id), override_opaque_contents_rect_(false) {
    SetDrawsContent(true);
  }

  SimpleEnclosedRegion VisibleContentOpaqueRegion() const override {
    if (override_opaque_contents_rect_) {
      return SimpleEnclosedRegion(
          gfx::IntersectRects(opaque_contents_rect_, visible_content_rect()));
    }
    return LayerImpl::VisibleContentOpaqueRegion();
  }
  void SetOpaqueContentsRect(const gfx::Rect& opaque_contents_rect) {
    override_opaque_contents_rect_ = true;
    opaque_contents_rect_ = opaque_contents_rect;
  }

 private:
  bool override_opaque_contents_rect_;
  gfx::Rect opaque_contents_rect_;
};

template <typename LayerType>
class TestOcclusionTrackerWithClip : public TestOcclusionTracker<LayerType> {
 public:
  explicit TestOcclusionTrackerWithClip(const gfx::Rect& viewport_rect)
      : TestOcclusionTracker<LayerType>(viewport_rect) {}

  bool OccludedLayer(const LayerType* layer,
                     const gfx::Rect& content_rect) const {
    DCHECK(layer->visible_content_rect().Contains(content_rect));
    return this->GetCurrentOcclusionForLayer(layer->draw_transform())
        .IsOccluded(content_rect);
  }

  // Gives an unoccluded sub-rect of |content_rect| in the content space of the
  // layer. Simple wrapper around GetUnoccludedContentRect.
  gfx::Rect UnoccludedLayerContentRect(const LayerType* layer,
                                       const gfx::Rect& content_rect) const {
    DCHECK(layer->visible_content_rect().Contains(content_rect));
    return this->GetCurrentOcclusionForLayer(layer->draw_transform())
        .GetUnoccludedContentRect(content_rect);
  }

  gfx::Rect UnoccludedSurfaceContentRect(const LayerType* layer,
                                         const gfx::Rect& content_rect) const {
    typename LayerType::RenderSurfaceType* surface = layer->render_surface();
    return this->GetCurrentOcclusionForContributingSurface(
                     surface->draw_transform())
        .GetUnoccludedContentRect(content_rect);
  }
};

struct OcclusionTrackerTestMainThreadTypes {
  typedef Layer LayerType;
  typedef FakeLayerTreeHost HostType;
  typedef RenderSurface RenderSurfaceType;
  typedef TestContentLayer ContentLayerType;
  typedef scoped_refptr<Layer> LayerPtrType;
  typedef scoped_refptr<ContentLayerType> ContentLayerPtrType;
  typedef LayerIterator<Layer> TestLayerIterator;
  typedef OcclusionTracker<Layer> OcclusionTrackerType;

  static LayerPtrType CreateLayer(HostType*  host) { return Layer::Create(); }
  static ContentLayerPtrType CreateContentLayer(HostType* host) {
    return make_scoped_refptr(new ContentLayerType());
  }

  template <typename T>
  static LayerPtrType PassLayerPtr(T* layer) {
    LayerPtrType ref(*layer);
    *layer = NULL;
    return ref;
  }
  static void SetForceRenderSurface(LayerType* layer, bool force) {
    layer->SetForceRenderSurface(force);
  }

  static void DestroyLayer(LayerPtrType* layer) { *layer = NULL; }

  static void RecursiveUpdateNumChildren(LayerType* layerType) {}
};

struct OcclusionTrackerTestImplThreadTypes {
  typedef LayerImpl LayerType;
  typedef LayerTreeImpl HostType;
  typedef RenderSurfaceImpl RenderSurfaceType;
  typedef TestContentLayerImpl ContentLayerType;
  typedef scoped_ptr<LayerImpl> LayerPtrType;
  typedef scoped_ptr<ContentLayerType> ContentLayerPtrType;
  typedef LayerIterator<LayerImpl> TestLayerIterator;
  typedef OcclusionTracker<LayerImpl> OcclusionTrackerType;

  static LayerPtrType CreateLayer(HostType* host) {
    return LayerImpl::Create(host, next_layer_impl_id++);
  }
  static ContentLayerPtrType CreateContentLayer(HostType* host) {
    return make_scoped_ptr(new ContentLayerType(host, next_layer_impl_id++));
  }
  static int next_layer_impl_id;

  template <typename T>
  static LayerPtrType PassLayerPtr(T* layer) {
    return layer->Pass();
  }

  static void SetForceRenderSurface(LayerType* layer, bool force) {
    layer->SetHasRenderSurface(force);
  }
  static void DestroyLayer(LayerPtrType* layer) { layer->reset(); }

  static void RecursiveUpdateNumChildren(LayerType* layer) {
    FakeLayerTreeHostImpl::RecursiveUpdateNumChildren(layer);
  }
};

int OcclusionTrackerTestImplThreadTypes::next_layer_impl_id = 1;

template <typename Types> class OcclusionTrackerTest : public testing::Test {
 protected:
  explicit OcclusionTrackerTest(bool opaque_layers)
      : opaque_layers_(opaque_layers),
        client_(FakeLayerTreeHostClient::DIRECT_3D),
        host_(FakeLayerTreeHost::Create(&client_)) {}

  virtual void RunMyTest() = 0;

  void TearDown() override { DestroyLayers(); }

  typename Types::HostType* GetHost();

  typename Types::ContentLayerType* CreateRoot(const gfx::Transform& transform,
                                               const gfx::PointF& position,
                                               const gfx::Size& bounds) {
    typename Types::ContentLayerPtrType layer(
        Types::CreateContentLayer(GetHost()));
    typename Types::ContentLayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);

    DCHECK(!root_.get());
    root_ = Types::PassLayerPtr(&layer);

    Types::SetForceRenderSurface(layer_ptr, true);
    SetRootLayerOnMainThread(layer_ptr);

    return layer_ptr;
  }

  typename Types::LayerType* CreateLayer(typename Types::LayerType* parent,
                                         const gfx::Transform& transform,
                                         const gfx::PointF& position,
                                         const gfx::Size& bounds) {
    typename Types::LayerPtrType layer(Types::CreateLayer(GetHost()));
    typename Types::LayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);
    parent->AddChild(Types::PassLayerPtr(&layer));
    return layer_ptr;
  }

  typename Types::LayerType* CreateSurface(typename Types::LayerType* parent,
                                           const gfx::Transform& transform,
                                           const gfx::PointF& position,
                                           const gfx::Size& bounds) {
    typename Types::LayerType* layer =
        CreateLayer(parent, transform, position, bounds);
    Types::SetForceRenderSurface(layer, true);
    return layer;
  }

  typename Types::ContentLayerType* CreateDrawingLayer(
      typename Types::LayerType* parent,
      const gfx::Transform& transform,
      const gfx::PointF& position,
      const gfx::Size& bounds,
      bool opaque) {
    typename Types::ContentLayerPtrType layer(
        Types::CreateContentLayer(GetHost()));
    typename Types::ContentLayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);

    if (opaque_layers_) {
      layer_ptr->SetContentsOpaque(opaque);
    } else {
      layer_ptr->SetContentsOpaque(false);
      if (opaque)
        layer_ptr->SetOpaqueContentsRect(gfx::Rect(bounds));
      else
        layer_ptr->SetOpaqueContentsRect(gfx::Rect());
    }

    parent->AddChild(Types::PassLayerPtr(&layer));
    return layer_ptr;
  }

  typename Types::LayerType* CreateReplicaLayer(
      typename Types::LayerType* owning_layer,
      const gfx::Transform& transform,
      const gfx::PointF& position,
      const gfx::Size& bounds) {
    typename Types::ContentLayerPtrType layer(
        Types::CreateContentLayer(GetHost()));
    typename Types::ContentLayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);
    SetReplica(owning_layer, Types::PassLayerPtr(&layer));
    return layer_ptr;
  }

  typename Types::LayerType* CreateMaskLayer(
      typename Types::LayerType* owning_layer,
      const gfx::Size& bounds) {
    typename Types::ContentLayerPtrType layer(
        Types::CreateContentLayer(GetHost()));
    typename Types::ContentLayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, identity_matrix, gfx::PointF(), bounds);
    SetMask(owning_layer, Types::PassLayerPtr(&layer));
    return layer_ptr;
  }

  typename Types::ContentLayerType* CreateDrawingSurface(
      typename Types::LayerType* parent,
      const gfx::Transform& transform,
      const gfx::PointF& position,
      const gfx::Size& bounds,
      bool opaque) {
    typename Types::ContentLayerType* layer =
        CreateDrawingLayer(parent, transform, position, bounds, opaque);
    Types::SetForceRenderSurface(layer, true);
    return layer;
  }

  void DestroyLayers() {
    Types::DestroyLayer(&root_);
    render_surface_layer_list_ = nullptr;
    render_surface_layer_list_impl_.clear();
    replica_layers_.clear();
    mask_layers_.clear();
    ResetLayerIterator();
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {}

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &OcclusionTrackerTest<Types>::CopyOutputCallback,
            base::Unretained(this))));
  }

  void AddCopyRequest(LayerImpl* layer) {
    ScopedPtrVector<CopyOutputRequest> requests;
    requests.push_back(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &OcclusionTrackerTest<Types>::CopyOutputCallback,
            base::Unretained(this))));
    layer->SetHasRenderSurface(true);
    layer->PassCopyRequests(&requests);
  }

  void CalcDrawEtc(TestContentLayerImpl* root) {
    DCHECK(root == root_.get());

    Types::RecursiveUpdateNumChildren(root);
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), &render_surface_layer_list_impl_);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    layer_iterator_ = layer_iterator_begin_ =
        Types::TestLayerIterator::Begin(&render_surface_layer_list_impl_);
  }

  void CalcDrawEtc(TestContentLayer* root) {
    DCHECK(root == root_.get());
    DCHECK(!root->render_surface());

    render_surface_layer_list_.reset(new RenderSurfaceLayerList);
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root, root->bounds(), render_surface_layer_list_.get());
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    layer_iterator_ = layer_iterator_begin_ =
        Types::TestLayerIterator::Begin(render_surface_layer_list_.get());
  }

  void EnterLayer(typename Types::LayerType* layer,
                  typename Types::OcclusionTrackerType* occlusion) {
    ASSERT_EQ(*layer_iterator_, layer);
    ASSERT_TRUE(layer_iterator_.represents_itself());
    occlusion->EnterLayer(layer_iterator_);
  }

  void LeaveLayer(typename Types::LayerType* layer,
                  typename Types::OcclusionTrackerType* occlusion) {
    ASSERT_EQ(*layer_iterator_, layer);
    ASSERT_TRUE(layer_iterator_.represents_itself());
    occlusion->LeaveLayer(layer_iterator_);
    ++layer_iterator_;
  }

  void VisitLayer(typename Types::LayerType* layer,
                  typename Types::OcclusionTrackerType* occlusion) {
    EnterLayer(layer, occlusion);
    LeaveLayer(layer, occlusion);
  }

  void EnterContributingSurface(
      typename Types::LayerType* layer,
      typename Types::OcclusionTrackerType* occlusion) {
    ASSERT_EQ(*layer_iterator_, layer);
    ASSERT_TRUE(layer_iterator_.represents_target_render_surface());
    occlusion->EnterLayer(layer_iterator_);
    occlusion->LeaveLayer(layer_iterator_);
    ++layer_iterator_;
    ASSERT_TRUE(layer_iterator_.represents_contributing_render_surface());
    occlusion->EnterLayer(layer_iterator_);
  }

  void LeaveContributingSurface(
      typename Types::LayerType* layer,
      typename Types::OcclusionTrackerType* occlusion) {
    ASSERT_EQ(*layer_iterator_, layer);
    ASSERT_TRUE(layer_iterator_.represents_contributing_render_surface());
    occlusion->LeaveLayer(layer_iterator_);
    ++layer_iterator_;
  }

  void VisitContributingSurface(
      typename Types::LayerType* layer,
      typename Types::OcclusionTrackerType* occlusion) {
    EnterContributingSurface(layer, occlusion);
    LeaveContributingSurface(layer, occlusion);
  }

  void ResetLayerIterator() { layer_iterator_ = layer_iterator_begin_; }

  const gfx::Transform identity_matrix;

 private:
  void SetRootLayerOnMainThread(Layer* root) {
    host_->SetRootLayer(scoped_refptr<Layer>(root));
  }

  void SetRootLayerOnMainThread(LayerImpl* root) {}

  void SetBaseProperties(typename Types::LayerType* layer,
                         const gfx::Transform& transform,
                         const gfx::PointF& position,
                         const gfx::Size& bounds) {
    layer->SetTransform(transform);
    layer->SetPosition(position);
    layer->SetBounds(bounds);
  }

  void SetProperties(Layer* layer,
                     const gfx::Transform& transform,
                     const gfx::PointF& position,
                     const gfx::Size& bounds) {
    SetBaseProperties(layer, transform, position, bounds);
  }

  void SetProperties(LayerImpl* layer,
                     const gfx::Transform& transform,
                     const gfx::PointF& position,
                     const gfx::Size& bounds) {
    SetBaseProperties(layer, transform, position, bounds);

    layer->SetContentBounds(layer->bounds());
  }

  void SetReplica(Layer* owning_layer, scoped_refptr<Layer> layer) {
    owning_layer->SetReplicaLayer(layer.get());
    replica_layers_.push_back(layer);
  }

  void SetReplica(LayerImpl* owning_layer, scoped_ptr<LayerImpl> layer) {
    owning_layer->SetReplicaLayer(layer.Pass());
  }

  void SetMask(Layer* owning_layer, scoped_refptr<Layer> layer) {
    owning_layer->SetMaskLayer(layer.get());
    mask_layers_.push_back(layer);
  }

  void SetMask(LayerImpl* owning_layer, scoped_ptr<LayerImpl> layer) {
    owning_layer->SetMaskLayer(layer.Pass());
  }

  bool opaque_layers_;
  FakeLayerTreeHostClient client_;
  scoped_ptr<FakeLayerTreeHost> host_;
  // These hold ownership of the layers for the duration of the test.
  typename Types::LayerPtrType root_;
  scoped_ptr<RenderSurfaceLayerList> render_surface_layer_list_;
  LayerImplList render_surface_layer_list_impl_;
  typename Types::TestLayerIterator layer_iterator_begin_;
  typename Types::TestLayerIterator layer_iterator_;
  typename Types::LayerType* last_layer_visited_;
  LayerList replica_layers_;
  LayerList mask_layers_;
};

template <>
FakeLayerTreeHost*
OcclusionTrackerTest<OcclusionTrackerTestMainThreadTypes>::GetHost() {
  return host_.get();
}

template <>
LayerTreeImpl*
OcclusionTrackerTest<OcclusionTrackerTestImplThreadTypes>::GetHost() {
  return host_->host_impl()->active_tree();
}

#define RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)                          \
  class ClassName##MainThreadOpaqueLayers                                      \
      : public ClassName<OcclusionTrackerTestMainThreadTypes> {                \
   public: /* NOLINT(whitespace/indent) */                                     \
    ClassName##MainThreadOpaqueLayers()                                        \
        : ClassName<OcclusionTrackerTestMainThreadTypes>(true) {}              \
  };                                                                           \
  TEST_F(ClassName##MainThreadOpaqueLayers, RunTest) { RunMyTest(); }
#define RUN_TEST_MAIN_THREAD_OPAQUE_PAINTS(ClassName)                          \
  class ClassName##MainThreadOpaquePaints                                      \
      : public ClassName<OcclusionTrackerTestMainThreadTypes> {                \
   public: /* NOLINT(whitespace/indent) */                                     \
    ClassName##MainThreadOpaquePaints()                                        \
        : ClassName<OcclusionTrackerTestMainThreadTypes>(false) {}             \
  };                                                                           \
  TEST_F(ClassName##MainThreadOpaquePaints, RunTest) { RunMyTest(); }

#define RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)                          \
  class ClassName##ImplThreadOpaqueLayers                                      \
      : public ClassName<OcclusionTrackerTestImplThreadTypes> {                \
   public: /* NOLINT(whitespace/indent) */                                     \
    ClassName##ImplThreadOpaqueLayers()                                        \
        : ClassName<OcclusionTrackerTestImplThreadTypes>(true) {}              \
  };                                                                           \
  TEST_F(ClassName##ImplThreadOpaqueLayers, RunTest) { RunMyTest(); }
#define RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName)                          \
  class ClassName##ImplThreadOpaquePaints                                      \
      : public ClassName<OcclusionTrackerTestImplThreadTypes> {                \
   public: /* NOLINT(whitespace/indent) */                                     \
    ClassName##ImplThreadOpaquePaints()                                        \
        : ClassName<OcclusionTrackerTestImplThreadTypes>(false) {}             \
  };                                                                           \
  TEST_F(ClassName##ImplThreadOpaquePaints, RunTest) { RunMyTest(); }

#define ALL_OCCLUSIONTRACKER_TEST(ClassName)                                   \
  RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)                                \
      RUN_TEST_MAIN_THREAD_OPAQUE_PAINTS(ClassName)                            \
      RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)                            \
      RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName)

#define MAIN_THREAD_TEST(ClassName)                                            \
  RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)

#define IMPL_THREAD_TEST(ClassName)                                            \
  RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)

#define MAIN_AND_IMPL_THREAD_TEST(ClassName)                                   \
  RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)                                \
      RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)

template <class Types>
class OcclusionTrackerTestIdentityTransforms
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestIdentityTransforms(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}

  void RunMyTest() override {
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestIdentityTransforms);

template <class Types>
class OcclusionTrackerTestRotatedChild : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestRotatedChild(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform layer_transform;
    layer_transform.Translate(250.0, 250.0);
    layer_transform.Rotate(90.0);
    layer_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(0, 0), gfx::Size(200, 200));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestRotatedChild);

template <class Types>
class OcclusionTrackerTestTranslatedChild : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestTranslatedChild(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform layer_transform;
    layer_transform.Translate(20.0, 20.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 50, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestTranslatedChild);

template <class Types>
class OcclusionTrackerTestChildInRotatedChild
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestChildInRotatedChild(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(90.0);
    child_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* child = this->CreateSurface(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500));
    child->SetMasksToBounds(true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(child,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 10.f),
                                 gfx::Size(500, 500),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 40, 70, 60).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    /* Justification for the above occlusion from |layer|:
                  100
         +---------------------+
         |                     |
         |    30               |           rotate(90)
         | 30 + ---------------------------------+
     100 |    |  10            |                 |            ==>
         |    |10+---------------------------------+
         |    |  |             |                 | |
         |    |  |             |                 | |
         |    |  |             |                 | |
         +----|--|-------------+                 | |
              |  |                               | |
              |  |                               | |
              |  |                               | |500
              |  |                               | |
              |  |                               | |
              |  |                               | |
              |  |                               | |
              +--|-------------------------------+ |
                 |                                 |
                 +---------------------------------+
                                500

        +---------------------+
        |                     |30  Visible region of |layer|: /////
        |                     |
        |     +---------------------------------+
     100|     |               |10               |
        |  +---------------------------------+  |
        |  |  |///////////////|     420      |  |
        |  |  |///////////////|60            |  |
        |  |  |///////////////|              |  |
        +--|--|---------------+              |  |
         20|10|     70                       |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |10|
           |  +------------------------------|--+
           |                 490             |
           +---------------------------------+
                          500

     */
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestChildInRotatedChild);

template <class Types>
class OcclusionTrackerTestScaledRenderSurface
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestScaledRenderSurface(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}

  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));

    gfx::Transform layer1_matrix;
    layer1_matrix.Scale(2.0, 2.0);
    typename Types::ContentLayerType* layer1 = this->CreateDrawingLayer(
        parent, layer1_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    Types::SetForceRenderSurface(layer1, true);

    gfx::Transform layer2_matrix;
    layer2_matrix.Translate(25.0, 25.0);
    typename Types::ContentLayerType* layer2 = this->CreateDrawingLayer(
        layer1, layer2_matrix, gfx::PointF(), gfx::Size(50, 50), true);
    typename Types::ContentLayerType* occluder =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(100.f, 100.f),
                                 gfx::Size(500, 500),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluder, &occlusion);
    this->EnterLayer(layer2, &occlusion);

    EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledRenderSurface);

template <class Types>
class OcclusionTrackerTestVisitTargetTwoTimes
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestVisitTargetTwoTimes(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    typename Types::LayerType* surface = this->CreateSurface(
        root, this->identity_matrix, gfx::PointF(30.f, 30.f), gfx::Size());
    typename Types::ContentLayerType* surface_child =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 10.f),
                                 gfx::Size(50, 50),
                                 true);
    // |top_layer| makes |root|'s surface get considered by OcclusionTracker
    // first, instead of |surface|'s. This exercises different code in
    // LeaveToRenderTarget, as the target surface has already been seen when
    // leaving |surface| later.
    typename Types::ContentLayerType* top_layer =
        this->CreateDrawingLayer(root,
                                 this->identity_matrix,
                                 gfx::PointF(40.f, 90.f),
                                 gfx::Size(50, 20),
                                 true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(top_layer, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(40, 90, 50, 20).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface_child, &occlusion);

    EXPECT_EQ(gfx::Rect(10, 60, 50, 20).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect(10, 60, 50, 20).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Occlusion from |top_layer| already in the root target should get merged
    // with the occlusion from the |surface| we are leaving now.
    this->LeaveContributingSurface(surface, &occlusion);
    this->EnterLayer(root, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(40, 40, 50, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestVisitTargetTwoTimes);

template <class Types>
class OcclusionTrackerTestSurfaceRotatedOffAxis
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceRotatedOffAxis(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(95.0);
    child_transform.Translate(-250.0, -250.0);

    gfx::Transform layer_transform;
    layer_transform.Translate(10.0, 10.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(1000, 1000));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::LayerType* child = this->CreateSurface(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500));
    typename Types::ContentLayerType* layer = this->CreateDrawingLayer(
        child, layer_transform, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    gfx::Rect clipped_layer_in_child = MathUtil::MapEnclosingClippedRect(
        layer_transform, layer->visible_content_rect());

    this->VisitLayer(layer, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(clipped_layer_in_child.ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceRotatedOffAxis);

template <class Types>
class OcclusionTrackerTestSurfaceWithTwoOpaqueChildren
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceWithTwoOpaqueChildren(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(90.0);
    child_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(1000, 1000));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    parent->SetMasksToBounds(true);
    typename Types::ContentLayerType* child =
        this->CreateDrawingSurface(parent,
                                 child_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 false);
    child->SetMasksToBounds(true);
    typename Types::ContentLayerType* layer1 =
        this->CreateDrawingLayer(child,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 10.f),
                                 gfx::Size(500, 500),
                                 true);
    typename Types::ContentLayerType* layer2 =
        this->CreateDrawingLayer(child,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 450.f),
                                 gfx::Size(500, 60),
                                 true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->VisitLayer(child, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 40, 70, 60).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    /* Justification for the above occlusion from |layer1| and |layer2|:

           +---------------------+
           |                     |30  Visible region of |layer1|: /////
           |                     |    Visible region of |layer2|: \\\\\
           |     +---------------------------------+
           |     |               |10               |
           |  +---------------+-----------------+  |
           |  |  |\\\\\\\\\\\\|//|     420      |  |
           |  |  |\\\\\\\\\\\\|//|60            |  |
           |  |  |\\\\\\\\\\\\|//|              |  |
           +--|--|------------|--+              |  |
            20|10|     70     |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |10|
              |  +------------|-----------------|--+
              |               | 490             |
              +---------------+-----------------+
                     60               440
         */
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceWithTwoOpaqueChildren);

template <class Types>
class OcclusionTrackerTestOverlappingSurfaceSiblings
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOverlappingSurfaceSiblings(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* child1 = this->CreateSurface(
        parent, this->identity_matrix, gfx::PointF(10.f, 0.f), gfx::Size());
    typename Types::LayerType* child2 = this->CreateSurface(
        parent, this->identity_matrix, gfx::PointF(30.f, 0.f), gfx::Size());
    typename Types::ContentLayerType* layer1 = this->CreateDrawingLayer(
        child1, this->identity_matrix, gfx::PointF(), gfx::Size(40, 50), true);
    typename Types::ContentLayerType* layer2 =
        this->CreateDrawingLayer(child2,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 0.f),
                                 gfx::Size(40, 50),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->EnterContributingSurface(child2, &occlusion);

    // layer2's occlusion.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 0, 40, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->EnterContributingSurface(child1, &occlusion);

    // layer2's occlusion in the target space of layer1.
    EXPECT_EQ(gfx::Rect(30, 0, 40, 50).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    // layer1's occlusion.
    EXPECT_EQ(gfx::Rect(0, 0, 40, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child1, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // The occlusion from from layer1 and layer2 is merged.
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(10, 0, 70, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOverlappingSurfaceSiblings);

template <class Types>
class OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform child1_transform;
    child1_transform.Translate(250.0, 250.0);
    child1_transform.Rotate(-90.0);
    child1_transform.Translate(-250.0, -250.0);

    gfx::Transform child2_transform;
    child2_transform.Translate(250.0, 250.0);
    child2_transform.Rotate(90.0);
    child2_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* child1 = this->CreateSurface(
        parent, child1_transform, gfx::PointF(30.f, 20.f), gfx::Size(10, 10));
    typename Types::LayerType* child2 =
        this->CreateDrawingSurface(parent,
                                   child2_transform,
                                   gfx::PointF(20.f, 40.f),
                                   gfx::Size(10, 10),
                                   false);
    typename Types::ContentLayerType* layer1 =
        this->CreateDrawingLayer(child1,
                                 this->identity_matrix,
                                 gfx::PointF(-10.f, -20.f),
                                 gfx::Size(510, 510),
                                 true);
    typename Types::ContentLayerType* layer2 =
        this->CreateDrawingLayer(child2,
                                 this->identity_matrix,
                                 gfx::PointF(-10.f, -10.f),
                                 gfx::Size(510, 510),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->EnterLayer(child2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(-10, 420, 70, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(child2, &occlusion);
    this->EnterContributingSurface(child2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(-10, 420, 70, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->EnterContributingSurface(child1, &occlusion);

    EXPECT_EQ(gfx::Rect(420, -10, 70, 80).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(420, -20, 80, 90).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child1, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 20, 90, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    /* Justification for the above occlusion:
                  100
        +---------------------+
        |20                   |       layer1
       10+----------------------------------+
    100 || 30                 |     layer2  |
        |20+----------------------------------+
        || |                  |             | |
        || |                  |             | |
        || |                  |             | |
        +|-|------------------+             | |
         | |                                | | 510
         | |                            510 | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                520             | |
         +----------------------------------+ |
           |                                  |
           +----------------------------------+
                           510
     */
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms);

template <class Types>
class OcclusionTrackerTestFilters : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestFilters(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform layer_transform;
    layer_transform.Translate(250.0, 250.0);
    layer_transform.Rotate(90.0);
    layer_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::ContentLayerType* blur_layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    typename Types::ContentLayerType* opaque_layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    typename Types::ContentLayerType* opacity_layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);

    Types::SetForceRenderSurface(blur_layer, true);
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(10.f));
    blur_layer->SetFilters(filters);

    Types::SetForceRenderSurface(opaque_layer, true);
    filters.Clear();
    filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
    opaque_layer->SetFilters(filters);

    Types::SetForceRenderSurface(opacity_layer, true);
    filters.Clear();
    filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
    opacity_layer->SetFilters(filters);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // Opacity layer won't contribute to occlusion.
    this->VisitLayer(opacity_layer, &occlusion);
    this->EnterContributingSurface(opacity_layer, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // And has nothing to contribute to its parent surface.
    this->LeaveContributingSurface(opacity_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // Opaque layer will contribute to occlusion.
    this->VisitLayer(opaque_layer, &occlusion);
    this->EnterContributingSurface(opaque_layer, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(0, 430, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // And it gets translated to the parent surface.
    this->LeaveContributingSurface(opaque_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The blur layer needs to throw away any occlusion from outside its
    // subtree.
    this->EnterLayer(blur_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // And it won't contribute to occlusion.
    this->LeaveLayer(blur_layer, &occlusion);
    this->EnterContributingSurface(blur_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // But the opaque layer's occlusion is preserved on the parent.
    this->LeaveContributingSurface(blur_layer, &occlusion);
    this->EnterLayer(parent, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestFilters);

template <class Types>
class OcclusionTrackerTestReplicaDoesOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestReplicaDoesOcclude(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface = this->CreateDrawingSurface(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(50, 50), true);
    this->CreateReplicaLayer(
        surface, this->identity_matrix, gfx::PointF(0.f, 50.f), gfx::Size());
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitContributingSurface(surface, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // The surface and replica should both be occluding the parent.
    EXPECT_EQ(gfx::Rect(50, 100).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaDoesOcclude);

template <class Types>
class OcclusionTrackerTestReplicaWithClipping
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestReplicaWithClipping(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 170));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(0.f, 100.f),
                                   gfx::Size(50, 50),
                                   true);
    this->CreateReplicaLayer(
        surface, this->identity_matrix, gfx::PointF(0.f, 50.f), gfx::Size());
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);

    // The surface layer's occlusion in its own space.
    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

    this->VisitContributingSurface(surface, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // The surface and replica should both be occluding the parent, the
    // replica's occlusion is clipped by the parent.
    EXPECT_EQ(gfx::Rect(0, 100, 50, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaWithClipping);

template <class Types>
class OcclusionTrackerTestReplicaWithMask : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestReplicaWithMask(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(0.f, 100.f),
                                   gfx::Size(50, 50),
                                   true);
    typename Types::LayerType* replica = this->CreateReplicaLayer(
        surface, this->identity_matrix, gfx::PointF(50.f, 50.f), gfx::Size());
    this->CreateMaskLayer(replica, gfx::Size(10, 10));
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitContributingSurface(surface, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // The replica should not be occluding the parent, since it has a mask
    // applied to it.
    EXPECT_EQ(gfx::Rect(0, 100, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaWithMask);

template <class Types>
class OcclusionTrackerTestOpaqueContentsRegionEmpty
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOpaqueContentsRegionEmpty(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(200, 200),
                                   false);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    this->EnterLayer(layer, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    this->LeaveLayer(layer, &occlusion);
    this->VisitContributingSurface(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
  }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestOpaqueContentsRegionEmpty);

template <class Types>
class OcclusionTrackerTestOpaqueContentsRegionNonEmpty
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOpaqueContentsRegionNonEmpty(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(100.f, 100.f),
                                 gfx::Size(200, 200),
                                 false);
    this->CalcDrawEtc(parent);
    {
      TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
          gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(0, 0, 100, 100));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(),
                occlusion.occlusion_from_inside_target().ToString());
    }
    {
      TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
          gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(20, 20, 180, 180));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(120, 120, 180, 180).ToString(),
                occlusion.occlusion_from_inside_target().ToString());
    }
    {
      TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
          gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(150, 150, 100, 100));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(250, 250, 50, 50).ToString(),
                occlusion.occlusion_from_inside_target().ToString());
    }
  }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestOpaqueContentsRegionNonEmpty);

template <class Types>
class OcclusionTrackerTestUnsorted3dLayers
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestUnsorted3dLayers(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    // Currently, The main thread layer iterator does not iterate over 3d items
    // in sorted order, because layer sorting is not performed on the main
    // thread.  Because of this, the occlusion tracker cannot assume that a 3d
    // layer occludes other layers that have not yet been iterated over. For
    // now, the expected behavior is that a 3d layer simply does not add any
    // occlusion to the occlusion tracker.

    gfx::Transform translation_to_front;
    translation_to_front.Translate3d(0.0, 0.0, -10.0);
    gfx::Transform translation_to_back;
    translation_to_front.Translate3d(0.0, 0.0, -100.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* child1 = this->CreateDrawingLayer(
        parent, translation_to_back, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::ContentLayerType* child2 =
        this->CreateDrawingLayer(parent,
                                 translation_to_front,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 true);
    parent->SetShouldFlattenTransform(false);
    parent->Set3dSortingContextId(1);
    child1->Set3dSortingContextId(1);
    child2->Set3dSortingContextId(1);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    this->VisitLayer(child2, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    this->VisitLayer(child1, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
  }
};

// This test will have different layer ordering on the impl thread; the test
// will only work on the main thread.
MAIN_THREAD_TEST(OcclusionTrackerTestUnsorted3dLayers);

template <class Types>
class OcclusionTrackerTestLayerBehindCameraDoesNotOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestLayerBehindCameraDoesNotOcclude(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform transform;
    transform.Translate(50.0, 50.0);
    transform.ApplyPerspectiveDepth(100.0);
    transform.Translate3d(0.0, 0.0, 110.0);
    transform.Translate(-50.0, -50.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    typename Types::ContentLayerType* layer = this->CreateDrawingLayer(
        parent, transform, gfx::PointF(), gfx::Size(100, 100), true);
    parent->SetShouldFlattenTransform(false);
    parent->Set3dSortingContextId(1);
    layer->SetShouldFlattenTransform(false);
    layer->Set3dSortingContextId(1);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // The |layer| is entirely behind the camera and should not occlude.
    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

template <class Types>
class OcclusionTrackerTestAnimationOpacity1OnMainThread
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestAnimationOpacity1OnMainThread(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    // parent
    // +--layer
    // +--surface
    // |  +--surface_child
    // |  +--surface_child2
    // +--parent2
    // +--topmost

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 true);
    typename Types::ContentLayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(300, 300),
                                   true);
    typename Types::ContentLayerType* surface_child =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(200, 300),
                                 true);
    typename Types::ContentLayerType* surface_child2 =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(100, 300),
                                 true);
    typename Types::ContentLayerType* parent2 =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 false);
    typename Types::ContentLayerType* topmost =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(250.f, 0.f),
                                 gfx::Size(50, 300),
                                 true);

    AddOpacityTransitionToController(
        layer->layer_animation_controller(), 10.0, 0.f, 1.f, false);
    AddOpacityTransitionToController(
        surface->layer_animation_controller(), 10.0, 0.f, 1.f, false);
    this->CalcDrawEtc(parent);

    EXPECT_TRUE(layer->draw_opacity_is_animating());
    EXPECT_FALSE(surface->draw_opacity_is_animating());
    EXPECT_TRUE(surface->render_surface()->draw_opacity_is_animating());

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(topmost, &occlusion);
    this->EnterLayer(parent2, &occlusion);

    // This occlusion will affect all surfaces.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(parent2, &occlusion);
    this->VisitLayer(surface_child2, &occlusion);
    this->EnterLayer(surface_child, &occlusion);
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(surface_child, &occlusion);
    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 200, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(surface, &occlusion);
    this->EnterContributingSurface(surface, &occlusion);
    // Occlusion within the surface is lost when leaving the animating surface.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(surface, &occlusion);
    // Occlusion from outside the animating surface still exists.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // Occlusion is not added for the animating |layer|.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
  }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationOpacity1OnMainThread);

template <class Types>
class OcclusionTrackerTestAnimationOpacity0OnMainThread
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestAnimationOpacity0OnMainThread(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 true);
    typename Types::ContentLayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(300, 300),
                                   true);
    typename Types::ContentLayerType* surface_child =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(200, 300),
                                 true);
    typename Types::ContentLayerType* surface_child2 =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(100, 300),
                                 true);
    typename Types::ContentLayerType* parent2 =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 false);
    typename Types::ContentLayerType* topmost =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(250.f, 0.f),
                                 gfx::Size(50, 300),
                                 true);

    AddOpacityTransitionToController(
        layer->layer_animation_controller(), 10.0, 1.f, 0.f, false);
    AddOpacityTransitionToController(
        surface->layer_animation_controller(), 10.0, 1.f, 0.f, false);
    this->CalcDrawEtc(parent);

    EXPECT_TRUE(layer->draw_opacity_is_animating());
    EXPECT_FALSE(surface->draw_opacity_is_animating());
    EXPECT_TRUE(surface->render_surface()->draw_opacity_is_animating());

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(topmost, &occlusion);
    this->EnterLayer(parent2, &occlusion);
    // This occlusion will affect all surfaces.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->LeaveLayer(parent2, &occlusion);
    this->VisitLayer(surface_child2, &occlusion);
    this->EnterLayer(surface_child, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->LeaveLayer(surface_child, &occlusion);
    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 200, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->LeaveLayer(surface, &occlusion);
    this->EnterContributingSurface(surface, &occlusion);
    // Occlusion within the surface is lost when leaving the animating surface.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->LeaveContributingSurface(surface, &occlusion);
    // Occlusion from outside the animating surface still exists.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // Occlusion is not added for the animating |layer|.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
  }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationOpacity0OnMainThread);

template <class Types>
class OcclusionTrackerTestAnimationTranslateOnMainThread
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestAnimationTranslateOnMainThread(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 true);
    typename Types::ContentLayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(300, 300),
                                   true);
    typename Types::ContentLayerType* surface_child =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(200, 300),
                                 true);
    typename Types::ContentLayerType* surface_child2 =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(100, 300),
                                 true);
    typename Types::ContentLayerType* surface2 = this->CreateDrawingSurface(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(50, 300), true);

    AddAnimatedTransformToController(
        layer->layer_animation_controller(), 10.0, 30, 0);
    AddAnimatedTransformToController(
        surface->layer_animation_controller(), 10.0, 30, 0);
    AddAnimatedTransformToController(
        surface_child->layer_animation_controller(), 10.0, 30, 0);
    this->CalcDrawEtc(parent);

    EXPECT_TRUE(layer->draw_transform_is_animating());
    EXPECT_TRUE(layer->screen_space_transform_is_animating());
    EXPECT_TRUE(
        surface->render_surface()->target_surface_transforms_are_animating());
    EXPECT_TRUE(
        surface->render_surface()->screen_space_transforms_are_animating());
    // The surface owning layer doesn't animate against its own surface.
    EXPECT_FALSE(surface->draw_transform_is_animating());
    EXPECT_TRUE(surface->screen_space_transform_is_animating());
    EXPECT_TRUE(surface_child->draw_transform_is_animating());
    EXPECT_TRUE(surface_child->screen_space_transform_is_animating());

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface2, &occlusion);
    this->EnterContributingSurface(surface2, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(surface2, &occlusion);
    this->EnterLayer(surface_child2, &occlusion);
    // surface_child2 is moving in screen space but not relative to its target,
    // so occlusion should happen in its target space only.  It also means that
    // things occluding from outside the target (e.g. surface2) cannot occlude
    // this layer.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(surface_child2, &occlusion);
    this->EnterLayer(surface_child, &occlusion);
    // surface_child2 added to the occlusion since it is not moving relative
    // to its target.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(surface_child, &occlusion);
    // surface_child is moving relative to its target, so it does not add
    // occlusion.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(surface, &occlusion);
    // The surface's owning layer is moving in screen space but not relative to
    // its target, so it adds to the occlusion.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 300, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(surface, &occlusion);
    this->LeaveContributingSurface(surface, &occlusion);
    // The |surface| is moving in the screen and in its target, so all occlusion
    // within the surface is lost when leaving it. Only the |surface2| occlusion
    // is left.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(layer, &occlusion);
    // The |layer| is animating in the screen and in its target, so no occlusion
    // is added.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationTranslateOnMainThread);

template <class Types>
class OcclusionTrackerTestSurfaceOcclusionTranslatesToParent
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceOcclusionTranslatesToParent(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform surface_transform;
    surface_transform.Translate(300.0, 300.0);
    surface_transform.Scale(2.0, 2.0);
    surface_transform.Translate(-150.0, -150.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(500, 500));
    typename Types::ContentLayerType* surface = this->CreateDrawingSurface(
        parent, surface_transform, gfx::PointF(), gfx::Size(300, 300), false);
    typename Types::ContentLayerType* surface2 =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(50.f, 50.f),
                                   gfx::Size(300, 300),
                                   false);
    surface->SetOpaqueContentsRect(gfx::Rect(0, 0, 200, 200));
    surface2->SetOpaqueContentsRect(gfx::Rect(0, 0, 200, 200));
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface2, &occlusion);
    this->VisitContributingSurface(surface2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 50, 200, 200).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Clear any stored occlusion.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());

    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 400, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

MAIN_AND_IMPL_THREAD_TEST(
    OcclusionTrackerTestSurfaceOcclusionTranslatesToParent);

template <class Types>
class OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    parent->SetMasksToBounds(true);
    typename Types::ContentLayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(500, 300),
                                   false);
    surface->SetOpaqueContentsRect(gfx::Rect(0, 0, 400, 200));
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 300, 200).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

MAIN_AND_IMPL_THREAD_TEST(
    OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping);

template <class Types>
class OcclusionTrackerTestSurfaceWithReplicaUnoccluded
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceWithReplicaUnoccluded(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(100, 100),
                                   true);
    this->CreateReplicaLayer(surface,
                             this->identity_matrix,
                             gfx::PointF(0.f, 100.f),
                             gfx::Size(100, 100));
    typename Types::LayerType* topmost =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(100, 110),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // |topmost| occludes the surface, but not the entire surface's replica.
    this->VisitLayer(topmost, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 110).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface, &occlusion);

    // Render target with replica ignores occlusion from outside.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(surface, &occlusion);

    // Only occlusion from outside the surface occludes the surface/replica.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_on_contributing_surface_from_outside_target()
                  .ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 110).ToString(),
              occlusion.occlusion_on_contributing_surface_from_inside_target()
                  .ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceWithReplicaUnoccluded);

template <class Types>
class OcclusionTrackerTestSurfaceChildOfSurface
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceChildOfSurface(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    // This test verifies that the surface cliprect does not end up empty and
    // clip away the entire unoccluded rect.

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(100, 100),
                                   false);
    typename Types::LayerType* surface_child =
        this->CreateDrawingSurface(surface,
                                   this->identity_matrix,
                                   gfx::PointF(0.f, 10.f),
                                   gfx::Size(100, 50),
                                   true);
    typename Types::LayerType* topmost = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(100, 50), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(-100, -100, 1000, 1000));

    // |topmost| occludes everything partially so we know occlusion is happening
    // at all.
    this->VisitLayer(topmost, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface_child, &occlusion);

    // surface_child increases the occlusion in the screen by a narrow sliver.
    EXPECT_EQ(gfx::Rect(0, -10, 100, 50).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    // In its own surface, surface_child is at 0,0 as is its occlusion.
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The root layer always has a clip rect. So the parent of |surface| has a
    // clip rect. However, the owning layer for |surface| does not mask to
    // bounds, so it doesn't have a clip rect of its own. Thus the parent of
    // |surface_child| exercises different code paths as its parent does not
    // have a clip rect.

    this->EnterContributingSurface(surface_child, &occlusion);
    // The |surface_child| can't occlude its own surface, but occlusion from
    // |topmost| can.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_on_contributing_surface_from_outside_target()
                  .ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_on_contributing_surface_from_inside_target()
                  .ToString());
    this->LeaveContributingSurface(surface_child, &occlusion);

    // When the surface_child's occlusion is transformed up to its parent, make
    // sure it is not clipped away inappropriately.
    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 10, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    this->LeaveLayer(surface, &occlusion);

    this->EnterContributingSurface(surface, &occlusion);
    // The occlusion from inside |surface| can't affect the surface, but
    // |topmost| can.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_on_contributing_surface_from_outside_target()
                  .ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_on_contributing_surface_from_inside_target()
                  .ToString());

    this->LeaveContributingSurface(surface, &occlusion);
    this->EnterLayer(parent, &occlusion);
    // The occlusion in |surface| and without are merged into the parent.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 60).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceChildOfSurface);

template <class Types>
class OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(10.f));

    // Save the distance of influence for the blur effect.
    int outset_top, outset_right, outset_bottom, outset_left;
    filters.GetOutsets(
        &outset_top, &outset_right, &outset_bottom, &outset_left);

    enum Direction {
      LEFT,
      RIGHT,
      TOP,
      BOTTOM,
      LAST_DIRECTION = BOTTOM,
    };

    for (int i = 0; i <= LAST_DIRECTION; ++i) {
      SCOPED_TRACE(i);

      // Make a 50x50 filtered surface that is adjacent to occluding layers
      // which are above it in the z-order in various configurations. The
      // surface is scaled to test that the pixel moving is done in the target
      // space, where the background filter is applied.
      typename Types::ContentLayerType* parent = this->CreateRoot(
          this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
      typename Types::LayerType* filtered_surface =
          this->CreateDrawingLayer(parent,
                                   scale_by_half,
                                   gfx::PointF(50.f, 50.f),
                                   gfx::Size(100, 100),
                                   false);
      Types::SetForceRenderSurface(filtered_surface, true);
      filtered_surface->SetBackgroundFilters(filters);
      gfx::Rect occlusion_rect;
      switch (i) {
        case LEFT:
          occlusion_rect = gfx::Rect(0, 0, 50, 200);
          break;
        case RIGHT:
          occlusion_rect = gfx::Rect(100, 0, 50, 200);
          break;
        case TOP:
          occlusion_rect = gfx::Rect(0, 0, 200, 50);
          break;
        case BOTTOM:
          occlusion_rect = gfx::Rect(0, 100, 200, 50);
          break;
      }

      typename Types::LayerType* occluding_layer =
          this->CreateDrawingLayer(parent,
                                   this->identity_matrix,
                                   occlusion_rect.origin(),
                                   occlusion_rect.size(),
                                   true);
      this->CalcDrawEtc(parent);

      TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
          gfx::Rect(0, 0, 200, 200));

      // This layer occludes pixels directly beside the filtered_surface.
      // Because filtered surface blends pixels in a radius, it will need to see
      // some of the pixels (up to radius far) underneath the occluding layers.
      this->VisitLayer(occluding_layer, &occlusion);

      EXPECT_EQ(occlusion_rect.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

      this->VisitLayer(filtered_surface, &occlusion);

      // The occlusion is used fully inside the surface.
      gfx::Rect occlusion_inside_surface =
          occlusion_rect - gfx::Vector2d(50, 50);
      EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
      EXPECT_EQ(occlusion_inside_surface.ToString(),
                occlusion.occlusion_from_outside_target().ToString());

      // The surface has a background blur, so it needs pixels that are
      // currently considered occluded in order to be drawn. So the pixels it
      // needs should be removed some the occluded area so that when we get to
      // the parent they are drawn.
      this->VisitContributingSurface(filtered_surface, &occlusion);
      this->EnterLayer(parent, &occlusion);

      gfx::Rect expected_occlusion = occlusion_rect;
      switch (i) {
        case LEFT:
          expected_occlusion.Inset(0, 0, outset_right, 0);
          break;
        case RIGHT:
          expected_occlusion.Inset(outset_right, 0, 0, 0);
          break;
        case TOP:
          expected_occlusion.Inset(0, 0, 0, outset_right);
          break;
        case BOTTOM:
          expected_occlusion.Inset(0, outset_right, 0, 0);
          break;
      }

      EXPECT_EQ(expected_occlusion.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

      this->DestroyLayers();
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter);

template <class Types>
class OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Makes two surfaces that completely cover |parent|. The occlusion both
    // above and below the filters will be reduced by each of them.
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(75, 75));
    typename Types::LayerType* parent = this->CreateSurface(
        root, scale_by_half, gfx::PointF(), gfx::Size(150, 150));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* filtered_surface1 = this->CreateDrawingLayer(
        parent, scale_by_half, gfx::PointF(), gfx::Size(300, 300), false);
    typename Types::LayerType* filtered_surface2 = this->CreateDrawingLayer(
        parent, scale_by_half, gfx::PointF(), gfx::Size(300, 300), false);
    typename Types::LayerType* occluding_layer_above =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(100.f, 100.f),
                                 gfx::Size(50, 50),
                                 true);

    // Filters make the layers own surfaces.
    Types::SetForceRenderSurface(filtered_surface1, true);
    Types::SetForceRenderSurface(filtered_surface2, true);
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(1.f));
    filtered_surface1->SetBackgroundFilters(filters);
    filtered_surface2->SetBackgroundFilters(filters);

    // Save the distance of influence for the blur effect.
    int outset_top, outset_right, outset_bottom, outset_left;
    filters.GetOutsets(
        &outset_top, &outset_right, &outset_bottom, &outset_left);

    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluding_layer_above, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(100 / 2, 100 / 2, 50 / 2, 50 / 2).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(filtered_surface2, &occlusion);
    this->VisitContributingSurface(filtered_surface2, &occlusion);
    this->VisitLayer(filtered_surface1, &occlusion);
    this->VisitContributingSurface(filtered_surface1, &occlusion);

    // Test expectations in the target.
    gfx::Rect expected_occlusion =
        gfx::Rect(100 / 2 + outset_right * 2,
                  100 / 2 + outset_bottom * 2,
                  50 / 2 - (outset_left + outset_right) * 2,
                  50 / 2 - (outset_top + outset_bottom) * 2);
    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Test expectations in the screen are the same as in the target, as the
    // render surface is 1:1 with the screen.
    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_outside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice);

template <class Types>
class OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a surface and its replica, each 50x50, with a smaller 30x30 layer
    // centered below each.  The surface is scaled to test that the pixel moving
    // is done in the target space, where the background filter is applied, but
    // the surface appears at 50, 50 and the replica at 200, 50.
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 150));
    typename Types::LayerType* behind_surface_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(60.f, 60.f),
                                 gfx::Size(30, 30),
                                 true);
    typename Types::LayerType* behind_replica_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(210.f, 60.f),
                                 gfx::Size(30, 30),
                                 true);
    typename Types::LayerType* filtered_surface =
        this->CreateDrawingLayer(parent,
                                 scale_by_half,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 false);
    this->CreateReplicaLayer(filtered_surface,
                             this->identity_matrix,
                             gfx::PointF(300.f, 0.f),
                             gfx::Size());

    // Filters make the layer own a surface.
    Types::SetForceRenderSurface(filtered_surface, true);
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->SetBackgroundFilters(filters);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // The surface has a background blur, so it blurs non-opaque pixels below
    // it.
    this->VisitLayer(filtered_surface, &occlusion);
    this->VisitContributingSurface(filtered_surface, &occlusion);

    this->VisitLayer(behind_replica_layer, &occlusion);

    // The layers behind the surface are not blurred, and their occlusion does
    // not change, until we leave the surface.  So it should not be modified by
    // the filter here.
    gfx::Rect occlusion_behind_replica = gfx::Rect(210, 60, 30, 30);
    EXPECT_EQ(occlusion_behind_replica.ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

    // Clear the occlusion so the |behind_surface_layer| can add its occlusion
    // without existing occlusion interfering.
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());

    this->VisitLayer(behind_surface_layer, &occlusion);

    // The layers behind the surface are not blurred, and their occlusion does
    // not change, until we leave the surface.  So it should not be modified by
    // the filter here.
    gfx::Rect occlusion_behind_surface = gfx::Rect(60, 60, 30, 30);
    EXPECT_EQ(occlusion_behind_surface.ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter);

template <class Types>
class OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a 50x50 filtered surface that is completely occluded by an opaque
    // layer which is above it in the z-order.  The surface is
    // scaled to test that the pixel moving is done in the target space, where
    // the background filter is applied, but the surface appears at 50, 50.
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 150));
    typename Types::LayerType* filtered_surface =
        this->CreateDrawingLayer(parent,
                                 scale_by_half,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 false);
    typename Types::LayerType* occluding_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(50, 50),
                                 true);

    // Filters make the layer own a surface.
    Types::SetForceRenderSurface(filtered_surface, true);
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->SetBackgroundFilters(filters);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluding_layer, &occlusion);

    this->VisitLayer(filtered_surface, &occlusion);
    {
      // The layers above the filtered surface occlude from outside.
      gfx::Rect occlusion_above_surface = gfx::Rect(0, 0, 50, 50);

      EXPECT_EQ(gfx::Rect().ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_EQ(occlusion_above_surface.ToString(),
                occlusion.occlusion_from_outside_target().ToString());
    }

    // The surface has a background blur, so it blurs non-opaque pixels below
    // it.
    this->VisitContributingSurface(filtered_surface, &occlusion);
    {
      // The filter is completely occluded, so it should not blur anything and
      // reduce any occlusion.
      gfx::Rect occlusion_above_surface = gfx::Rect(50, 50, 50, 50);

      EXPECT_EQ(occlusion_above_surface.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_EQ(gfx::Rect().ToString(),
                occlusion.occlusion_from_outside_target().ToString());
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded);

template <class Types>
class OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit
  OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a surface and its replica, each 50x50, that are partially occluded
    // by opaque layers which are above them in the z-order.  The surface is
    // scaled to test that the pixel moving is done in the target space, where
    // the background filter is applied, but the surface appears at 50, 50 and
    // the replica at 200, 50.
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 150));
    typename Types::LayerType* filtered_surface =
        this->CreateDrawingLayer(parent,
                                 scale_by_half,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 false);
    this->CreateReplicaLayer(filtered_surface,
                             this->identity_matrix,
                             gfx::PointF(300.f, 0.f),
                             gfx::Size());
    typename Types::LayerType* above_surface_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(70.f, 50.f),
                                 gfx::Size(30, 50),
                                 true);
    typename Types::LayerType* above_replica_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(200.f, 50.f),
                                 gfx::Size(30, 50),
                                 true);
    typename Types::LayerType* beside_surface_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(90.f, 40.f),
                                 gfx::Size(10, 10),
                                 true);
    typename Types::LayerType* beside_replica_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(200.f, 40.f),
                                 gfx::Size(10, 10),
                                 true);

    // Filters make the layer own a surface.
    Types::SetForceRenderSurface(filtered_surface, true);
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->SetBackgroundFilters(filters);

    // Save the distance of influence for the blur effect.
    int outset_top, outset_right, outset_bottom, outset_left;
    filters.GetOutsets(
        &outset_top, &outset_right, &outset_bottom, &outset_left);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(beside_replica_layer, &occlusion);
    this->VisitLayer(beside_surface_layer, &occlusion);
    this->VisitLayer(above_replica_layer, &occlusion);
    this->VisitLayer(above_surface_layer, &occlusion);

    // The surface has a background blur, so it blurs non-opaque pixels below
    // it.
    this->VisitLayer(filtered_surface, &occlusion);
    this->VisitContributingSurface(filtered_surface, &occlusion);

    // The filter in the surface and replica are partially unoccluded. Only the
    // unoccluded parts should reduce occlusion.  This means it will push back
    // the occlusion that touches the unoccluded part (occlusion_above___), but
    // it will not touch occlusion_beside____ since that is not beside the
    // unoccluded part of the surface, even though it is beside the occluded
    // part of the surface.
    gfx::Rect occlusion_above_surface =
        gfx::Rect(70 + outset_right, 50, 30 - outset_right, 50);
    gfx::Rect occlusion_above_replica =
        gfx::Rect(200, 50, 30 - outset_left, 50);
    gfx::Rect occlusion_beside_surface = gfx::Rect(90, 40, 10, 10);
    gfx::Rect occlusion_beside_replica = gfx::Rect(200, 40, 10, 10);

    SimpleEnclosedRegion expected_occlusion;
    expected_occlusion.Union(occlusion_beside_replica);
    expected_occlusion.Union(occlusion_beside_surface);
    expected_occlusion.Union(occlusion_above_replica);
    expected_occlusion.Union(occlusion_above_surface);

    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

    const SimpleEnclosedRegion& actual_occlusion =
        occlusion.occlusion_from_inside_target();
    for (size_t i = 0; i < expected_occlusion.GetRegionComplexity(); ++i) {
      ASSERT_LT(i, actual_occlusion.GetRegionComplexity());
      EXPECT_EQ(expected_occlusion.GetRect(i), actual_occlusion.GetRect(i));
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded);

template <class Types>
class OcclusionTrackerTestBlendModeDoesNotOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestBlendModeDoesNotOcclude(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    typename Types::LayerType* blend_mode_layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(0.f, 0.f),
        gfx::Size(100, 100), true);
    typename Types::LayerType* top_layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(10.f, 12.f),
        gfx::Size(20, 22), true);

    // Blend mode makes the layer own a surface.
    Types::SetForceRenderSurface(blend_mode_layer, true);
    blend_mode_layer->SetBlendMode(SkXfermode::kMultiply_Mode);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(top_layer, &occlusion);
    // |top_layer| occludes.
    EXPECT_EQ(gfx::Rect(10, 12, 20, 22).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

    this->VisitLayer(blend_mode_layer, &occlusion);
    // |top_layer| occludes but not |blend_mode_layer|.
    EXPECT_EQ(gfx::Rect(10, 12, 20, 22).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    this->VisitContributingSurface(blend_mode_layer, &occlusion);
    // |top_layer| occludes but not |blend_mode_layer|.
    EXPECT_EQ(gfx::Rect(10, 12, 20, 22).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestBlendModeDoesNotOcclude);

template <class Types>
class OcclusionTrackerTestMinimumTrackingSize
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestMinimumTrackingSize(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Size tracking_size(100, 100);
    gfx::Size below_tracking_size(99, 99);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    typename Types::LayerType* large = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(), tracking_size, true);
    typename Types::LayerType* small =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 below_tracking_size,
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    occlusion.set_minimum_tracking_size(tracking_size);

    // The small layer is not tracked because it is too small.
    this->VisitLayer(small, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The large layer is tracked as it is large enough.
    this->VisitLayer(large, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(tracking_size).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestMinimumTrackingSize);

template <class Types>
class OcclusionTrackerTestScaledLayerIsClipped
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestScaledLayerIsClipped(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_transform;
    scale_transform.Scale(512.0, 512.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    typename Types::LayerType* clip = this->CreateLayer(parent,
                                                        this->identity_matrix,
                                                        gfx::PointF(10.f, 10.f),
                                                        gfx::Size(50, 50));
    clip->SetMasksToBounds(true);
    typename Types::LayerType* scale = this->CreateLayer(
        clip, scale_transform, gfx::PointF(), gfx::Size(1, 1));
    typename Types::LayerType* scaled = this->CreateDrawingLayer(
        scale, this->identity_matrix, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(scaled, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledLayerIsClipped)

template <class Types>
class OcclusionTrackerTestScaledLayerInSurfaceIsClipped
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestScaledLayerInSurfaceIsClipped(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_transform;
    scale_transform.Scale(512.0, 512.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    typename Types::LayerType* clip = this->CreateLayer(parent,
                                                        this->identity_matrix,
                                                        gfx::PointF(10.f, 10.f),
                                                        gfx::Size(50, 50));
    clip->SetMasksToBounds(true);
    typename Types::LayerType* surface = this->CreateDrawingSurface(
        clip, this->identity_matrix, gfx::PointF(), gfx::Size(400, 30), false);
    typename Types::LayerType* scale = this->CreateLayer(
        surface, scale_transform, gfx::PointF(), gfx::Size(1, 1));
    typename Types::LayerType* scaled = this->CreateDrawingLayer(
        scale, this->identity_matrix, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(scaled, &occlusion);
    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledLayerInSurfaceIsClipped)

template <class Types>
class OcclusionTrackerTestCopyRequestDoesOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestCopyRequestDoesOcclude(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(), gfx::Size(400, 400));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::Point(), gfx::Size(400, 400), true);
    typename Types::LayerType* copy = this->CreateLayer(parent,
                                                        this->identity_matrix,
                                                        gfx::Point(100, 0),
                                                        gfx::Size(200, 400));
    this->AddCopyRequest(copy);
    typename Types::LayerType* copy_child = this->CreateDrawingLayer(
        copy,
        this->identity_matrix,
        gfx::PointF(),
        gfx::Size(200, 400),
        true);
    typename Types::LayerType* top_layer =
        this->CreateDrawingLayer(root, this->identity_matrix,
                                 gfx::PointF(50, 0), gfx::Size(50, 400), true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(top_layer, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 0, 50, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(copy_child, &occlusion);
    // Layers outside the copy request do not occlude.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(200, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // CopyRequests cause the layer to own a surface.
    this->VisitContributingSurface(copy, &occlusion);

    // The occlusion from the copy should be kept.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 0, 250, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestCopyRequestDoesOcclude)

template <class Types>
class OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(), gfx::Size(400, 400));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::Point(), gfx::Size(400, 400), true);
    typename Types::LayerType* hide = this->CreateLayer(
        parent, this->identity_matrix, gfx::Point(), gfx::Size());
    typename Types::LayerType* copy = this->CreateLayer(
        hide, this->identity_matrix, gfx::Point(100, 0), gfx::Size(200, 400));
    this->AddCopyRequest(copy);
    typename Types::LayerType* copy_child = this->CreateDrawingLayer(
        copy, this->identity_matrix, gfx::PointF(), gfx::Size(200, 400), true);

    // The |copy| layer is hidden but since it is being copied, it will be
    // drawn.
    hide->SetHideLayerAndSubtree(true);

    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(copy_child, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(200, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // CopyRequests cause the layer to own a surface.
    this->VisitContributingSurface(copy, &occlusion);

    // The occlusion from the copy should be dropped since it is hidden.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude)

template <class Types>
class OcclusionTrackerTestOccludedLayer : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOccludedLayer(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform translate;
    translate.Translate(10.0, 20.0);
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(), gfx::Size(200, 200));
    typename Types::LayerType* surface = this->CreateSurface(
        root, this->identity_matrix, gfx::Point(), gfx::Size(200, 200));
    typename Types::LayerType* layer = this->CreateDrawingLayer(
        surface, translate, gfx::Point(), gfx::Size(200, 200), false);
    typename Types::ContentLayerType* outside_layer = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::Point(), gfx::Size(200, 200), false);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 200, 200));
    this->VisitLayer(outside_layer, &occlusion);
    this->EnterLayer(layer, &occlusion);

    // No occlusion, is not occluded.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(100, 100)));

    // Partial occlusion from outside, is not occluded.
    occlusion.set_occlusion_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from outside, is occluded.
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from inside, is not occluded.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from inside, is occluded.
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from both, is not occluded.
    occlusion.set_occlusion_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 50));
    occlusion.set_occlusion_from_inside_target(
        SimpleEnclosedRegion(50, 100, 100, 50));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from both, is occluded.
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(80, 70, 50, 50)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOccludedLayer)

template <class Types>
class OcclusionTrackerTestUnoccludedLayerQuery
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestUnoccludedLayerQuery(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform translate;
    translate.Translate(10.0, 20.0);
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(), gfx::Size(200, 200));
    typename Types::LayerType* surface = this->CreateSurface(
        root, this->identity_matrix, gfx::Point(), gfx::Size(200, 200));
    typename Types::LayerType* layer = this->CreateDrawingLayer(
        surface, translate, gfx::Point(), gfx::Size(200, 200), false);
    typename Types::ContentLayerType* outside_layer = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::Point(), gfx::Size(200, 200), false);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 200, 200));
    this->VisitLayer(outside_layer, &occlusion);
    this->EnterLayer(layer, &occlusion);

    // No occlusion, is not occluded.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());
    EXPECT_EQ(gfx::Rect(100, 100),
              occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(100, 100)));

    // Partial occlusion from outside.
    occlusion.set_occlusion_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());
    EXPECT_EQ(
        gfx::Rect(0, 0, 100, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(140, 30, 50, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(0, 0, 80, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from outside, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from inside, is not occluded.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    EXPECT_EQ(
        gfx::Rect(0, 0, 100, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(140, 30, 50, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(0, 0, 80, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from inside, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from both, is not occluded.
    occlusion.set_occlusion_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 50));
    occlusion.set_occlusion_from_inside_target(
        SimpleEnclosedRegion(50, 100, 100, 50));
    EXPECT_EQ(
        gfx::Rect(0, 0, 100, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 100, 100)));
    // This could be (140, 30, 50, 100). But because we do a lossy subtract,
    // it's larger.
    EXPECT_EQ(gfx::Rect(90, 30, 100, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(0, 0, 80, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from both, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(80, 70, 50, 50)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestUnoccludedLayerQuery)

template <class Types>
class OcclusionTrackerTestUnoccludedSurfaceQuery
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestUnoccludedSurfaceQuery(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform translate;
    translate.Translate(10.0, 20.0);
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(), gfx::Size(200, 200));
    typename Types::LayerType* surface =
        this->CreateSurface(root, translate, gfx::Point(), gfx::Size(200, 200));
    typename Types::LayerType* layer =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::Point(),
                                 gfx::Size(200, 200),
                                 false);
    typename Types::ContentLayerType* outside_layer = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::Point(), gfx::Size(200, 200), false);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 200, 200));
    this->VisitLayer(outside_layer, &occlusion);
    this->VisitLayer(layer, &occlusion);
    this->EnterContributingSurface(surface, &occlusion);

    // No occlusion, is not occluded.
    occlusion.set_occlusion_on_contributing_surface_from_outside_target(
        SimpleEnclosedRegion());
    occlusion.set_occlusion_on_contributing_surface_from_inside_target(
        SimpleEnclosedRegion());
    EXPECT_EQ(
        gfx::Rect(100, 100),
        occlusion.UnoccludedSurfaceContentRect(surface, gfx::Rect(100, 100)));

    // Partial occlusion from outside.
    occlusion.set_occlusion_on_contributing_surface_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    occlusion.set_occlusion_on_contributing_surface_from_inside_target(
        SimpleEnclosedRegion());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(140, 30, 50, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 0, 80, 100),
              occlusion.UnoccludedSurfaceContentRect(surface,
                                                     gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from outside, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from inside, is not occluded.
    occlusion.set_occlusion_on_contributing_surface_from_outside_target(
        SimpleEnclosedRegion());
    occlusion.set_occlusion_on_contributing_surface_from_inside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(140, 30, 50, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 0, 80, 100),
              occlusion.UnoccludedSurfaceContentRect(surface,
                                                     gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from inside, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from both, is not occluded.
    occlusion.set_occlusion_on_contributing_surface_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 50));
    occlusion.set_occlusion_on_contributing_surface_from_inside_target(
        SimpleEnclosedRegion(50, 100, 100, 50));
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 0, 100, 100)));
    // This could be (140, 30, 50, 100). But because we do a lossy subtract,
    // it's larger.
    EXPECT_EQ(gfx::Rect(90, 30, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 0, 80, 100),
              occlusion.UnoccludedSurfaceContentRect(surface,
                                                     gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from both, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(80, 70, 50, 50)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestUnoccludedSurfaceQuery)

}  // namespace
}  // namespace cc
