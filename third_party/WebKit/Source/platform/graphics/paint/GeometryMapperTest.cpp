// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/geometry/GeometryTestHelpers.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class GeometryMapperTest : public ::testing::Test,
                           public ScopedSlimmingPaintV2ForTest {
 public:
  GeometryMapperTest() : ScopedSlimmingPaintV2ForTest(true) {}

  std::unique_ptr<GeometryMapper> geometryMapper;

  PropertyTreeState rootPropertyTreeState() {
    PropertyTreeState state(
        TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
        EffectPaintPropertyNode::root(), ScrollPaintPropertyNode::root());
    return state;
  }

  PrecomputedDataForAncestor& getPrecomputedDataForAncestor(
      const PropertyTreeState& propertyTreeState) {
    return geometryMapper->getPrecomputedDataForAncestor(
        propertyTreeState.transform());
  }

  const TransformPaintPropertyNode* lowestCommonAncestor(
      const TransformPaintPropertyNode* a,
      const TransformPaintPropertyNode* b) {
    return GeometryMapper::lowestCommonAncestor(a, b);
  }

  FloatRect sourceToDestinationVisualRectInternal(
      const FloatRect& rect,
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      bool& success) {
    return geometryMapper->localToAncestorVisualRectInternal(
        rect, sourceState, destinationState, success);
  }

  FloatRect localToAncestorVisualRectInternal(
      const FloatRect& rect,
      const PropertyTreeState& localState,
      const PropertyTreeState& ancestorState,
      bool& success) {
    return geometryMapper->localToAncestorVisualRectInternal(
        rect, localState, ancestorState, success);
  }

  FloatRect localToAncestorRectInternal(
      const FloatRect& rect,
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      bool& success) {
    return geometryMapper->localToAncestorRectInternal(
        rect, localTransformNode, ancestorTransformNode, success);
  }

 private:
  void SetUp() override {
    geometryMapper = WTF::makeUnique<GeometryMapper>();
  }

  void TearDown() override { geometryMapper.reset(); }
};

const static float kTestEpsilon = 1e-6;

#define EXPECT_RECT_EQ(expected, actual)                                       \
  do {                                                                         \
    const FloatRect& actualRect = actual;                                      \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(expected.x(), actualRect.x(), \
                                                 kTestEpsilon))                \
        << "actual: " << actualRect.x() << ", expected: " << expected.x();     \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(expected.y(), actualRect.y(), \
                                                 kTestEpsilon))                \
        << "actual: " << actualRect.y() << ", expected: " << expected.y();     \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(                              \
        expected.width(), actualRect.width(), kTestEpsilon))                   \
        << "actual: " << actualRect.width()                                    \
        << ", expected: " << expected.width();                                 \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(                              \
        expected.height(), actualRect.height(), kTestEpsilon))                 \
        << "actual: " << actualRect.height()                                   \
        << ", expected: " << expected.height();                                \
  } while (false)

#define CHECK_MAPPINGS(inputRect, expectedVisualRect, expectedTransformedRect, \
                       expectedTransformToAncestor,                            \
                       expectedClipInAncestorSpace, localPropertyTreeState,    \
                       ancestorPropertyTreeState)                              \
  do {                                                                         \
    EXPECT_RECT_EQ(                                                            \
        expectedVisualRect,                                                    \
        geometryMapper->localToAncestorVisualRect(                             \
            inputRect, localPropertyTreeState, ancestorPropertyTreeState));    \
    FloatRect mappedClip = geometryMapper->localToAncestorClipRect(            \
        localPropertyTreeState, ancestorPropertyTreeState);                    \
    EXPECT_RECT_EQ(expectedClipInAncestorSpace, mappedClip);                   \
    EXPECT_RECT_EQ(                                                            \
        expectedVisualRect,                                                    \
        geometryMapper->sourceToDestinationVisualRect(                         \
            inputRect, localPropertyTreeState, ancestorPropertyTreeState));    \
    EXPECT_RECT_EQ(expectedTransformedRect,                                    \
                   geometryMapper->localToAncestorRect(                        \
                       inputRect, localPropertyTreeState.transform(),          \
                       ancestorPropertyTreeState.transform()));                \
    EXPECT_RECT_EQ(expectedTransformedRect,                                    \
                   geometryMapper->sourceToDestinationRect(                    \
                       inputRect, localPropertyTreeState.transform(),          \
                       ancestorPropertyTreeState.transform()));                \
    if (ancestorPropertyTreeState.transform() !=                               \
        localPropertyTreeState.transform()) {                                  \
      EXPECT_EQ(                                                               \
          expectedTransformToAncestor,                                         \
          getPrecomputedDataForAncestor(ancestorPropertyTreeState)             \
              .toAncestorTransforms.get(localPropertyTreeState.transform()));  \
    }                                                                          \
    if (ancestorPropertyTreeState.clip() != localPropertyTreeState.clip()) {   \
      EXPECT_EQ(expectedClipInAncestorSpace,                                   \
                getPrecomputedDataForAncestor(ancestorPropertyTreeState)       \
                    .toAncestorClipRects.get(localPropertyTreeState.clip()));  \
    }                                                                          \
  } while (false)

TEST_F(GeometryMapperTest, Root) {
  FloatRect input(0, 0, 100, 100);

  CHECK_MAPPINGS(input, input, input,
                 TransformPaintPropertyNode::root()->matrix(),
                 ClipPaintPropertyNode::root()->clipRect().rect(),
                 rootPropertyTreeState(), rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, IdentityTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         TransformationMatrix(),
                                         FloatPoint3D());
  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);

  CHECK_MAPPINGS(input, input, input, transform->matrix(),
                 ClipPaintPropertyNode::root()->clipRect().rect(), localState,
                 rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, TranslationTransform) {
  TransformationMatrix transformMatrix;
  transformMatrix.translate(20, 10);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         transformMatrix, FloatPoint3D());
  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = transformMatrix.mapRect(input);

  CHECK_MAPPINGS(input, output, output, transform->matrix(),
                 ClipPaintPropertyNode::root()->clipRect().rect(), localState,
                 rootPropertyTreeState());

  EXPECT_RECT_EQ(input, geometryMapper->ancestorToLocalRect(
                            output, rootPropertyTreeState().transform(),
                            localState.transform()));
}

TEST_F(GeometryMapperTest, RotationAndScaleTransform) {
  TransformationMatrix transformMatrix;
  transformMatrix.rotate(45);
  transformMatrix.scale(2);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         transformMatrix,
                                         FloatPoint3D(0, 0, 0));
  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = transformMatrix.mapRect(input);

  CHECK_MAPPINGS(input, output, output, transformMatrix,
                 ClipPaintPropertyNode::root()->clipRect().rect(), localState,
                 rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, RotationAndScaleTransformWithTransformOrigin) {
  TransformationMatrix transformMatrix;
  transformMatrix.rotate(45);
  transformMatrix.scale(2);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         transformMatrix,
                                         FloatPoint3D(50, 50, 0));
  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  transformMatrix.applyTransformOrigin(50, 50, 0);
  FloatRect output = transformMatrix.mapRect(input);

  CHECK_MAPPINGS(input, output, output, transformMatrix,
                 ClipPaintPropertyNode::root()->clipRect().rect(), localState,
                 rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, NestedTransforms) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  TransformationMatrix scaleTransform;
  scaleTransform.scale(2);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, scaleTransform,
                                         FloatPoint3D());

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  TransformationMatrix final = rotateTransform * scaleTransform;
  FloatRect output = final.mapRect(input);

  CHECK_MAPPINGS(input, output, output, final,
                 ClipPaintPropertyNode::root()->clipRect().rect(), localState,
                 rootPropertyTreeState());

  // Check the cached matrix for the intermediate transform.
  EXPECT_EQ(rotateTransform,
            getPrecomputedDataForAncestor(rootPropertyTreeState())
                .toAncestorTransforms.get(transform1.get()));
}

TEST_F(GeometryMapperTest, NestedTransformsScaleAndTranslation) {
  TransformationMatrix scaleTransform;
  scaleTransform.scale(2);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         scaleTransform, FloatPoint3D());

  TransformationMatrix translateTransform;
  translateTransform.translate(100, 0);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, translateTransform,
                                         FloatPoint3D());

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  // Note: unlike NestedTransforms, the order of these transforms matters. This
  // tests correct order of matrix multiplication.
  TransformationMatrix final = scaleTransform * translateTransform;
  FloatRect output = final.mapRect(input);

  CHECK_MAPPINGS(input, output, output, final,
                 ClipPaintPropertyNode::root()->clipRect().rect(), localState,
                 rootPropertyTreeState());

  // Check the cached matrix for the intermediate transform.
  EXPECT_EQ(scaleTransform,
            getPrecomputedDataForAncestor(rootPropertyTreeState())
                .toAncestorTransforms.get(transform1.get()));
}

TEST_F(GeometryMapperTest, NestedTransformsIntermediateDestination) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  TransformationMatrix scaleTransform;
  scaleTransform.scale(2);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, scaleTransform,
                                         FloatPoint3D());

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform2.get());

  PropertyTreeState intermediateState = rootPropertyTreeState();
  intermediateState.setTransform(transform1.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = scaleTransform.mapRect(input);

  CHECK_MAPPINGS(input, output, output, scaleTransform,
                 ClipPaintPropertyNode::root()->clipRect().rect(), localState,
                 intermediateState);
}

TEST_F(GeometryMapperTest, SimpleClip) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setClip(clip.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(10, 10, 50, 50);

  CHECK_MAPPINGS(input,   // Input
                 output,  // Visual rect
                 input,   // Transformed rect (not clipped).
                 TransformPaintPropertyNode::root()
                     ->matrix(),           // Transform matrix to ancestor space
                 clip->clipRect().rect(),  // Clip rect in ancestor space
                 localState,
                 rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, ClipBeforeTransform) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform.get(),
      FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setClip(clip.get());
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(input);
  output.intersect(clip->clipRect().rect());
  output = rotateTransform.mapRect(output);

  CHECK_MAPPINGS(
      input,                           // Input
      output,                          // Visual rect
      rotateTransform.mapRect(input),  // Transformed rect (not clipped).
      rotateTransform,                 // Transform matrix to ancestor space
      rotateTransform.mapRect(
          clip->clipRect().rect()),  // Clip rect in ancestor space
      localState,
      rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, ClipAfterTransform) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 10, 200, 200));

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setClip(clip.get());
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(input);
  output = rotateTransform.mapRect(output);
  output.intersect(clip->clipRect().rect());

  CHECK_MAPPINGS(
      input,                           // Input
      output,                          // Visual rect
      rotateTransform.mapRect(input),  // Transformed rect (not clipped)
      rotateTransform,                 // Transform matrix to ancestor space
      clip->clipRect().rect(),         // Clip rect in ancestor space
      localState, rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, TwoClipsWithTransformBetween) {
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 10, 200, 200));

  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
      clip1, transform.get(), FloatRoundedRect(10, 10, 200, 200));

  FloatRect input(0, 0, 100, 100);

  {
    PropertyTreeState localState = rootPropertyTreeState();
    localState.setClip(clip1.get());
    localState.setTransform(transform.get());

    FloatRect output(input);
    output = rotateTransform.mapRect(output);
    output.intersect(clip1->clipRect().rect());

    CHECK_MAPPINGS(
        input,                           // Input
        output,                          // Visual rect
        rotateTransform.mapRect(input),  // Transformed rect (not clipped)
        rotateTransform,                 // Transform matrix to ancestor space
        clip1->clipRect().rect(),        // Clip rect in ancestor space
        localState, rootPropertyTreeState());
  }

  {
    PropertyTreeState localState = rootPropertyTreeState();
    localState.setClip(clip2.get());
    localState.setTransform(transform.get());

    FloatRect mappedClip = rotateTransform.mapRect(clip2->clipRect().rect());
    mappedClip.intersect(clip1->clipRect().rect());

    // All clips are performed in the space of the ancestor. In cases such as
    // this, this means the clip is a bit lossy.
    FloatRect output(input);
    // Map to transformed rect in ancestor space.
    output = rotateTransform.mapRect(output);
    // Intersect with all clips between local and ancestor, independently mapped
    // to ancestor space.
    output.intersect(mappedClip);

    CHECK_MAPPINGS(
        input,                           // Input
        output,                          // Visual rect
        rotateTransform.mapRect(input),  // Transformed rect (not clipped)
        rotateTransform,                 // Transform matrix to ancestor space
        mappedClip,                      // Clip rect in ancestor space
        localState, rootPropertyTreeState());
  }
}

TEST_F(GeometryMapperTest, SiblingTransforms) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  TransformationMatrix rotateTransform1;
  rotateTransform1.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform1, FloatPoint3D());

  TransformationMatrix rotateTransform2;
  rotateTransform2.rotate(-45);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform2, FloatPoint3D());

  PropertyTreeState transform1State = rootPropertyTreeState();
  transform1State.setTransform(transform1.get());
  PropertyTreeState transform2State = rootPropertyTreeState();
  transform2State.setTransform(transform2.get());

  bool success;
  FloatRect input(0, 0, 100, 100);
  FloatRect result = localToAncestorVisualRectInternal(
      input, transform1State, transform2State, success);
  // Fails, because the transform2state is not an ancestor of transform1State.
  EXPECT_FALSE(success);
  EXPECT_RECT_EQ(input, result);

  result = localToAncestorRectInternal(input, transform1.get(),
                                       transform2.get(), success);
  // Fails, because the transform2state is not an ancestor of transform1State.
  EXPECT_FALSE(success);
  EXPECT_RECT_EQ(input, result);

  result = localToAncestorVisualRectInternal(input, transform2State,
                                             transform1State, success);
  // Fails, because the transform1state is not an ancestor of transform2State.
  EXPECT_FALSE(success);
  EXPECT_RECT_EQ(input, result);

  result = localToAncestorRectInternal(input, transform2.get(),
                                       transform1.get(), success);
  // Fails, because the transform1state is not an ancestor of transform2State.
  EXPECT_FALSE(success);
  EXPECT_RECT_EQ(input, result);

  FloatRect expected =
      rotateTransform2.inverse().mapRect(rotateTransform1.mapRect(input));
  result = geometryMapper->sourceToDestinationVisualRect(input, transform1State,
                                                         transform2State);
  EXPECT_RECT_EQ(expected, result);

  result = geometryMapper->sourceToDestinationRect(input, transform1.get(),
                                                   transform2.get());
  EXPECT_RECT_EQ(expected, result);
}

TEST_F(GeometryMapperTest, SiblingTransformsWithClip) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  TransformationMatrix rotateTransform1;
  rotateTransform1.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform1, FloatPoint3D());

  TransformationMatrix rotateTransform2;
  rotateTransform2.rotate(-45);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform2, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      rootPropertyTreeState().clip(), transform2.get(),
      FloatRoundedRect(10, 10, 70, 70));

  PropertyTreeState transform1State = rootPropertyTreeState();
  transform1State.setTransform(transform1.get());
  PropertyTreeState transform2AndClipState = rootPropertyTreeState();
  transform2AndClipState.setTransform(transform2.get());
  transform2AndClipState.setClip(clip.get());

  bool success;
  FloatRect input(0, 0, 100, 100);

  // Test map from transform1State to transform2AndClipState.
  FloatRect expected =
      rotateTransform2.inverse().mapRect(rotateTransform1.mapRect(input));

  // sourceToDestinationVisualRect ignores clip from the common ancestor to
  // destination.
  FloatRect result = sourceToDestinationVisualRectInternal(
      input, transform1State, transform2AndClipState, success);
  // Fails, because the clip of the destination state is not an ancestor of the
  // clip of the source state.
  EXPECT_FALSE(success);

  // sourceToDestinationRect applies transforms only.
  result = geometryMapper->sourceToDestinationRect(input, transform1.get(),
                                                   transform2.get());
  EXPECT_RECT_EQ(expected, result);

  // Test map from transform2AndClipState to transform1State.
  FloatRect expectedUnclipped =
      rotateTransform1.inverse().mapRect(rotateTransform2.mapRect(input));
  FloatRect expectedClipped = rotateTransform1.inverse().mapRect(
      rotateTransform2.mapRect(FloatRect(10, 10, 70, 70)));

  // sourceToDestinationVisualRect ignores clip from the common ancestor to
  // destination.
  result = geometryMapper->sourceToDestinationVisualRect(
      input, transform2AndClipState, transform1State);
  EXPECT_RECT_EQ(expectedClipped, result);

  // sourceToDestinationRect applies transforms only.
  result = geometryMapper->sourceToDestinationRect(input, transform2.get(),
                                                   transform1.get());
  EXPECT_RECT_EQ(expectedUnclipped, result);
}

TEST_F(GeometryMapperTest, LowestCommonAncestor) {
  TransformationMatrix matrix;
  RefPtr<TransformPaintPropertyNode> child1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         matrix, FloatPoint3D());
  RefPtr<TransformPaintPropertyNode> child2 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         matrix, FloatPoint3D());

  RefPtr<TransformPaintPropertyNode> childOfChild1 =
      TransformPaintPropertyNode::create(child1, matrix, FloatPoint3D());
  RefPtr<TransformPaintPropertyNode> childOfChild2 =
      TransformPaintPropertyNode::create(child2, matrix, FloatPoint3D());

  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild1.get(), childOfChild2.get()));
  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild1.get(), child2.get()));
  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild1.get(),
                                 rootPropertyTreeState().transform()));
  EXPECT_EQ(child1, lowestCommonAncestor(childOfChild1.get(), child1.get()));

  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild2.get(), childOfChild1.get()));
  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild2.get(), child1.get()));
  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild2.get(),
                                 rootPropertyTreeState().transform()));
  EXPECT_EQ(child2, lowestCommonAncestor(childOfChild2.get(), child2.get()));

  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(child1.get(), child2.get()));
}

}  // namespace blink
