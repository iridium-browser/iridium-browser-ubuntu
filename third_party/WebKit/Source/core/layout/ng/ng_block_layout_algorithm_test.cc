// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_layout_coordinator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

NGConstraintSpace* ConstructConstraintSpace(NGWritingMode writing_mode,
                                            TextDirection direction,
                                            NGLogicalSize size,
                                            bool shrink_to_fit = false) {
  return NGConstraintSpaceBuilder(writing_mode)
      .SetAvailableSize(size)
      .SetPercentageResolutionSize(size)
      .SetTextDirection(direction)
      .SetWritingMode(writing_mode)
      .SetIsShrinkToFit(shrink_to_fit)
      .ToConstraintSpace();
}

class NGBlockLayoutAlgorithmTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::create(); }

  NGPhysicalBoxFragment* RunBlockLayoutAlgorithm(NGConstraintSpace* space,
                                                 NGBlockNode* first_child) {
    NGBlockNode parent(style_.get());
    parent.SetFirstChild(first_child);

    NGBlockLayoutAlgorithm algorithm(style_.get(), first_child, space);

    NGPhysicalFragment* fragment;
    NGLayoutAlgorithm* not_used;
    EXPECT_EQ(kNewFragment, algorithm.Layout(nullptr, &fragment, &not_used));

    return toNGPhysicalBoxFragment(fragment);
  }

  MinAndMaxContentSizes RunComputeMinAndMax(NGBlockNode* first_child) {
    // The constraint space is not used for min/max computation, but we need
    // it to create the algorithm.
    NGConstraintSpace* space =
        ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                                 NGLogicalSize(LayoutUnit(), LayoutUnit()));
    NGBlockLayoutAlgorithm algorithm(style_.get(), first_child, space);
    MinAndMaxContentSizes sizes;
    EXPECT_TRUE(algorithm.ComputeMinAndMaxContentSizes(&sizes));
    return sizes;
  }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGBlockLayoutAlgorithmTest, FixedSize) {
  style_->setWidth(Length(30, Fixed));
  style_->setHeight(Length(40, Fixed));

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, nullptr);

  EXPECT_EQ(LayoutUnit(30), frag->Width());
  EXPECT_EQ(LayoutUnit(40), frag->Height());
}

// Verifies that two children are laid out with the correct size and position.
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildren) {
  const int kWidth = 30;
  const int kHeight1 = 20;
  const int kHeight2 = 30;
  const int kMarginTop = 5;
  const int kMarginBottom = 20;
  style_->setWidth(Length(kWidth, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setHeight(Length(kHeight1, Fixed));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setHeight(Length(kHeight2, Fixed));
  second_style->setMarginTop(Length(kMarginTop, Fixed));
  second_style->setMarginBottom(Length(kMarginBottom, Fixed));
  NGBlockNode* second_child = new NGBlockNode(second_style.get());

  first_child->SetNextSibling(second_child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(kWidth), frag->Width());
  EXPECT_EQ(LayoutUnit(kHeight1 + kHeight2 + kMarginTop), frag->Height());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(kHeight1, child->Height());
  EXPECT_EQ(0, child->TopOffset());

  child = frag->Children()[1];
  EXPECT_EQ(kHeight2, child->Height());
  EXPECT_EQ(kHeight1 + kMarginTop, child->TopOffset());
}

// Verifies that a child is laid out correctly if it's writing mode is different
// from the parent's one.
//
// Test case's HTML representation:
// <div style="writing-mode: vertical-lr;">
//   <div style="width:50px;
//       height: 50px; margin-left: 100px;
//       writing-mode: horizontal-tb;"></div>
// </div>
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildrenWithWritingMode) {
  const int kWidth = 50;
  const int kHeight = 50;
  const int kMarginLeft = 100;

  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWritingMode(WritingMode::kVerticalLr);
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setHeight(Length(kHeight, Fixed));
  div2_style->setWidth(Length(kWidth, Fixed));
  div1_style->setWritingMode(WritingMode::kHorizontalTb);
  div2_style->setMarginLeft(Length(kMarginLeft, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space =
      ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  const NGPhysicalFragment* child = frag->Children()[0];
  // DIV2
  child = static_cast<const NGPhysicalBoxFragment*>(child)->Children()[0];

  EXPECT_EQ(kHeight, child->Height());
  EXPECT_EQ(0, child->TopOffset());
  EXPECT_EQ(kMarginLeft, child->LeftOffset());
}

// Verifies the collapsing margins case for the next pair:
// - top margin of a box and top margin of its first in-flow child.
//
// Test case's HTML representation:
// <div style="margin-top: 20px; height: 50px;">  <!-- DIV1 -->
//    <div style="margin-top: 10px"></div>        <!-- DIV2 -->
// </div>
//
// Expected:
// - Empty margin strut of the fragment that establishes new formatting context
// - Margins are collapsed resulting a single margin 20px = max(20px, 10px)
// - The top offset of DIV2 == 20px
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase1) {
  const int kHeight = 50;
  const int kDiv1MarginTop = 20;
  const int kDiv2MarginTop = 10;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setMarginTop(Length(kDiv1MarginTop, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setMarginTop(Length(kDiv2MarginTop, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space =
      NGConstraintSpaceBuilder(kHorizontalTopBottom)
          .SetAvailableSize(NGLogicalSize(LayoutUnit(100), NGSizeIndefinite))
          .SetPercentageResolutionSize(
              NGLogicalSize(LayoutUnit(100), NGSizeIndefinite))
          .SetTextDirection(TextDirection::kLtr)
          .SetIsNewFormattingContext(true)
          .ToConstraintSpace();
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  EXPECT_TRUE(frag->MarginStrut().IsEmpty());
  ASSERT_EQ(frag->Children().size(), 1UL);
  const NGPhysicalBoxFragment* div2_fragment =
      static_cast<const NGPhysicalBoxFragment*>(frag->Children()[0].get());
  EXPECT_EQ(NGMarginStrut({LayoutUnit(kDiv2MarginTop)}),
            div2_fragment->MarginStrut());
  EXPECT_EQ(kDiv1MarginTop, div2_fragment->TopOffset());
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of box and top margin of its next in-flow following sibling.
//
// Test case's HTML representation:
// <div style="margin-bottom: 20px; height: 50px;">  <!-- DIV1 -->
//    <div style="margin-bottom: -15px"></div>       <!-- DIV2 -->
//    <div></div>                                    <!-- DIV3 -->
// </div>
// <div></div>                                       <!-- DIV4 -->
// <div style="margin-top: 10px; height: 50px;">     <!-- DIV5 -->
//    <div></div>                                    <!-- DIV6 -->
//    <div style="margin-top: -30px"></div>          <!-- DIV7 -->
// </div>
//
// Expected:
//   Margins are collapsed resulting an overlap
//   -10px = max(20px, 10px) - max(abs(-15px), abs(-30px))
//   between DIV2 and DIV3. Zero-height blocks are ignored.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase2) {
  const int kHeight = 50;
  const int kDiv1MarginBottom = 20;
  const int kDiv2MarginBottom = -15;
  const int kDiv5MarginTop = 10;
  const int kDiv7MarginTop = -30;
  const int kExpectedCollapsedMargin = -10;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setMarginBottom(Length(kDiv1MarginBottom, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setMarginBottom(Length(kDiv2MarginBottom, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  // Empty DIVs: DIV3, DIV4, DIV6
  NGBlockNode* div3 = new NGBlockNode(ComputedStyle::create().get());
  NGBlockNode* div4 = new NGBlockNode(ComputedStyle::create().get());
  NGBlockNode* div6 = new NGBlockNode(ComputedStyle::create().get());

  // DIV5
  RefPtr<ComputedStyle> div5_style = ComputedStyle::create();
  div5_style->setHeight(Length(kHeight, Fixed));
  div5_style->setMarginTop(Length(kDiv5MarginTop, Fixed));
  NGBlockNode* div5 = new NGBlockNode(div5_style.get());

  // DIV7
  RefPtr<ComputedStyle> div7_style = ComputedStyle::create();
  div7_style->setMarginTop(Length(kDiv7MarginTop, Fixed));
  NGBlockNode* div7 = new NGBlockNode(div7_style.get());

  div1->SetFirstChild(div2);
  div2->SetNextSibling(div3);
  div1->SetNextSibling(div4);
  div4->SetNextSibling(div5);
  div5->SetFirstChild(div6);
  div6->SetNextSibling(div7);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 3UL);

  // DIV1
  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(kHeight, child->Height());
  EXPECT_EQ(0, child->TopOffset());

  // DIV5
  child = frag->Children()[2];
  EXPECT_EQ(kHeight, child->Height());
  EXPECT_EQ(kHeight + kExpectedCollapsedMargin, child->TopOffset());
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of a last in-flow child and bottom margin of its parent if
//   the parent has 'auto' computed height
//
// Test case's HTML representation:
// <div style="margin-bottom: 20px; height: 50px;">            <!-- DIV1 -->
//   <div style="margin-bottom: 200px; height: 50px;"/>        <!-- DIV2 -->
// </div>
//
// Expected:
//   1) Margins are collapsed with the result = std::max(20, 200)
//      if DIV1.height == auto
//   2) Margins are NOT collapsed if DIV1.height != auto
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase3) {
  const int kHeight = 50;
  const int kDiv1MarginBottom = 20;
  const int kDiv2MarginBottom = 200;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setMarginBottom(Length(kDiv1MarginBottom, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setHeight(Length(kHeight, Fixed));
  div2_style->setMarginBottom(Length(kDiv2MarginBottom, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  // Verify that margins are collapsed.
  EXPECT_EQ(NGMarginStrut({LayoutUnit(0), LayoutUnit(kDiv2MarginBottom)}),
            frag->MarginStrut());

  // Verify that margins are NOT collapsed.
  div1_style->setHeight(Length(kHeight, Fixed));
  frag = RunBlockLayoutAlgorithm(space, div1);
  EXPECT_EQ(NGMarginStrut({LayoutUnit(0), LayoutUnit(kDiv1MarginBottom)}),
            frag->MarginStrut());
}

// Verifies that 2 adjoining margins are not collapsed if there is padding or
// border that separates them.
//
// Test case's HTML representation:
// <div style="margin: 30px 0px; padding: 20px 0px;">    <!-- DIV1 -->
//   <div style="margin: 200px 0px; height: 50px;"/>     <!-- DIV2 -->
// </div>
//
// Expected:
// Margins do NOT collapse if there is an interfering padding or border.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase4) {
  const int kHeight = 50;
  const int kDiv1Margin = 30;
  const int kDiv1Padding = 20;
  const int kDiv2Margin = 200;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setMarginTop(Length(kDiv1Margin, Fixed));
  div1_style->setMarginBottom(Length(kDiv1Margin, Fixed));
  div1_style->setPaddingTop(Length(kDiv1Padding, Fixed));
  div1_style->setPaddingBottom(Length(kDiv1Padding, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setHeight(Length(kHeight, Fixed));
  div2_style->setMarginTop(Length(kDiv2Margin, Fixed));
  div2_style->setMarginBottom(Length(kDiv2Margin, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  // Verify that margins do NOT collapse.
  frag = RunBlockLayoutAlgorithm(space, div1);
  EXPECT_EQ(NGMarginStrut({LayoutUnit(kDiv1Margin), LayoutUnit(kDiv1Margin)}),
            frag->MarginStrut());
  ASSERT_EQ(frag->Children().size(), 1UL);

  EXPECT_EQ(NGMarginStrut({LayoutUnit(kDiv2Margin), LayoutUnit(kDiv2Margin)}),
            static_cast<const NGPhysicalBoxFragment*>(frag->Children()[0].get())
                ->MarginStrut());

  // Reset padding and verify that margins DO collapse.
  div1_style->setPaddingTop(Length(0, Fixed));
  div1_style->setPaddingBottom(Length(0, Fixed));
  frag = RunBlockLayoutAlgorithm(space, div1);
  EXPECT_EQ(NGMarginStrut({LayoutUnit(kDiv2Margin), LayoutUnit(kDiv2Margin)}),
            frag->MarginStrut());
}

// Verifies that margins of 2 adjoining blocks with different writing modes
// get collapsed.
//
// Test case's HTML representation:
//   <div style="writing-mode: vertical-lr;">
//     <div style="margin-right: 60px; width: 60px;">vertical</div>
//     <div style="margin-left: 100px; writing-mode: horizontal-tb;">
//       horizontal
//     </div>
//   </div>
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase5) {
  const int kVerticalDivMarginRight = 60;
  const int kVerticalDivWidth = 50;
  const int kHorizontalDivMarginLeft = 100;

  style_->setWidth(Length(500, Fixed));
  style_->setHeight(Length(500, Fixed));
  style_->setWritingMode(WritingMode::kVerticalLr);

  // Vertical DIV
  RefPtr<ComputedStyle> vertical_style = ComputedStyle::create();
  vertical_style->setMarginRight(Length(kVerticalDivMarginRight, Fixed));
  vertical_style->setWidth(Length(kVerticalDivWidth, Fixed));
  NGBlockNode* vertical_div = new NGBlockNode(vertical_style.get());

  // Horizontal DIV
  RefPtr<ComputedStyle> horizontal_style = ComputedStyle::create();
  horizontal_style->setMarginLeft(Length(kHorizontalDivMarginLeft, Fixed));
  horizontal_style->setWritingMode(WritingMode::kHorizontalTb);
  NGBlockNode* horizontal_div = new NGBlockNode(horizontal_style.get());

  vertical_div->SetNextSibling(horizontal_div);

  auto* space =
      ConstructConstraintSpace(kVerticalLeftRight, TextDirection::kLtr,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, vertical_div);

  ASSERT_EQ(frag->Children().size(), 2UL);
  const NGPhysicalFragment* child = frag->Children()[1];
  // Horizontal div
  EXPECT_EQ(0, child->TopOffset());
  EXPECT_EQ(kVerticalDivWidth + kHorizontalDivMarginLeft, child->LeftOffset());
}

// Verifies that the margin strut of a child with a different writing mode does
// not get used in the collapsing margins calculation.
//
// Test case's HTML representation:
//   <style>
//     #div1 { margin-bottom: 10px; height: 60px; writing-mode: vertical-rl; }
//     #div2 { margin-left: -20px; width: 10px; }
//     #div3 { margin-top: 40px; height: 60px; }
//   </style>
//   <div id="div1">
//      <div id="div2">vertical</div>
//   </div>
//   <div id="div3"></div>
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase6) {
  const int kHeight = 60;
  const int kWidth = 10;
  const int kMarginBottom = 10;
  const int kMarginLeft = -20;
  const int kMarginTop = 40;

  style_->setWidth(Length(500, Fixed));
  style_->setHeight(Length(500, Fixed));

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWidth(Length(kWidth, Fixed));
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setWritingMode(WritingMode::kVerticalRl);
  div1_style->setMarginBottom(Length(kMarginBottom, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setWidth(Length(kWidth, Fixed));
  div2_style->setMarginLeft(Length(kMarginLeft, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  // DIV3
  RefPtr<ComputedStyle> div3_style = ComputedStyle::create();
  div3_style->setHeight(Length(kHeight, Fixed));
  div3_style->setMarginTop(Length(kMarginTop, Fixed));
  NGBlockNode* div3 = new NGBlockNode(div3_style.get());

  div1->SetFirstChild(div2);
  div1->SetNextSibling(div3);

  auto* space =
      ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragment* child1 = frag->Children()[0];
  EXPECT_EQ(0, child1->TopOffset());
  EXPECT_EQ(kHeight, child1->Height());

  const NGPhysicalFragment* child2 = frag->Children()[1];
  EXPECT_EQ(kHeight + std::max(kMarginBottom, kMarginTop), child2->TopOffset());
}

// Verifies that a box's size includes its borders and padding, and that
// children are positioned inside the content box.
//
// Test case's HTML representation:
// <style>
//   #div1 { width:100px; height:100px; }
//   #div1 { border-style:solid; border-width:1px 2px 3px 4px; }
//   #div1 { padding:5px 6px 7px 8px; }
// </style>
// <div id="div1">
//    <div id="div2"></div>
// </div>
TEST_F(NGBlockLayoutAlgorithmTest, BorderAndPadding) {
  const int kWidth = 100;
  const int kHeight = 100;
  const int kBorderTop = 1;
  const int kBorderRight = 2;
  const int kBorderBottom = 3;
  const int kBorderLeft = 4;
  const int kPaddingTop = 5;
  const int kPaddingRight = 6;
  const int kPaddingBottom = 7;
  const int kPaddingLeft = 8;
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();

  div1_style->setWidth(Length(kWidth, Fixed));
  div1_style->setHeight(Length(kHeight, Fixed));

  div1_style->setBorderTopWidth(kBorderTop);
  div1_style->setBorderTopStyle(BorderStyleSolid);
  div1_style->setBorderRightWidth(kBorderRight);
  div1_style->setBorderRightStyle(BorderStyleSolid);
  div1_style->setBorderBottomWidth(kBorderBottom);
  div1_style->setBorderBottomStyle(BorderStyleSolid);
  div1_style->setBorderLeftWidth(kBorderLeft);
  div1_style->setBorderLeftStyle(BorderStyleSolid);

  div1_style->setPaddingTop(Length(kPaddingTop, Fixed));
  div1_style->setPaddingRight(Length(kPaddingRight, Fixed));
  div1_style->setPaddingBottom(Length(kPaddingBottom, Fixed));
  div1_style->setPaddingLeft(Length(kPaddingLeft, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 1UL);

  // div1
  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(kBorderLeft + kPaddingLeft + kWidth + kPaddingRight + kBorderRight,
            child->Width());
  EXPECT_EQ(kBorderTop + kPaddingTop + kHeight + kPaddingBottom + kBorderBottom,
            child->Height());

  ASSERT_TRUE(child->Type() == NGPhysicalFragment::kFragmentBox);
  ASSERT_EQ(static_cast<const NGPhysicalBoxFragment*>(child)->Children().size(),
            1UL);

  // div2
  child = static_cast<const NGPhysicalBoxFragment*>(child)->Children()[0];
  EXPECT_EQ(kBorderTop + kPaddingTop, child->TopOffset());
  EXPECT_EQ(kBorderLeft + kPaddingLeft, child->LeftOffset());
}

TEST_F(NGBlockLayoutAlgorithmTest, PercentageResolutionSize) {
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  style_->setWidth(Length(kWidth, Fixed));
  style_->setPaddingLeft(Length(kPaddingLeft, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setWidth(Length(40, Percent));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Width());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 1UL);

  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(LayoutUnit(12), child->Width());
}

// A very simple auto margin case. We rely on the tests in ng_length_utils_test
// for the more complex cases; just make sure we handle auto at all here.
TEST_F(NGBlockLayoutAlgorithmTest, AutoMargin) {
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  style_->setWidth(Length(kWidth, Fixed));
  style_->setPaddingLeft(Length(kPaddingLeft, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  const int kChildWidth = 10;
  first_style->setWidth(Length(kChildWidth, Fixed));
  first_style->setMarginLeft(Length(Auto));
  first_style->setMarginRight(Length(Auto));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Width());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->WidthOverflow());
  ASSERT_EQ(1UL, frag->Children().size());

  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(LayoutUnit(kChildWidth), child->Width());
  EXPECT_EQ(LayoutUnit(kPaddingLeft + 10), child->LeftOffset());
  EXPECT_EQ(LayoutUnit(0), child->TopOffset());
}

// Verifies that 3 Left/Right float fragments and one regular block fragment
// are correctly positioned by the algorithm.
//
// Test case's HTML representation:
//  <div id="parent" style="width: 200px; height: 200px;">
//    <div style="float:left; width: 30px; height: 30px;
//        margin-top: 10px;"/>   <!-- DIV1 -->
//    <div style="width: 30px; height: 30px;"/>   <!-- DIV2 -->
//    <div style="float:right; width: 50px; height: 50px;"/>  <!-- DIV3 -->
//    <div style="float:left; width: 120px; height: 120px;
//        margin-left: 30px;"/>  <!-- DIV4 -->
//  </div>
//
// Expected:
// - Left float(DIV1) is positioned at the left.
// - Regular block (DIV2) is positioned behind DIV1.
// - Right float(DIV3) is positioned at the right below DIV2
// - Left float(DIV4) is positioned at the left below DIV3.
TEST_F(NGBlockLayoutAlgorithmTest, PositionFloatFragments) {
  const int kParentLeftPadding = 10;
  const int kDiv1TopMargin = 10;
  const int kParentSize = 200;
  const int kDiv1Size = 30;
  const int kDiv2Size = 30;
  const int kDiv3Size = 50;
  const int kDiv4Size = kParentSize - kDiv3Size;
  const int kDiv4LeftMargin = kDiv1Size;

  style_->setHeight(Length(kParentSize, Fixed));
  style_->setWidth(Length(kParentSize, Fixed));
  style_->setPaddingLeft(Length(kParentLeftPadding, Fixed));

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWidth(Length(kDiv1Size, Fixed));
  div1_style->setHeight(Length(kDiv1Size, Fixed));
  div1_style->setFloating(EFloat::kLeft);
  div1_style->setMarginTop(Length(kDiv1TopMargin, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setWidth(Length(kDiv2Size, Fixed));
  div2_style->setHeight(Length(kDiv2Size, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  // DIV3
  RefPtr<ComputedStyle> div3_style = ComputedStyle::create();
  div3_style->setWidth(Length(kDiv3Size, Fixed));
  div3_style->setHeight(Length(kDiv3Size, Fixed));
  div3_style->setFloating(EFloat::kRight);
  NGBlockNode* div3 = new NGBlockNode(div3_style.get());

  // DIV4
  RefPtr<ComputedStyle> div4_style = ComputedStyle::create();
  div4_style->setWidth(Length(kDiv4Size, Fixed));
  div4_style->setHeight(Length(kDiv4Size, Fixed));
  div4_style->setMarginLeft(Length(kDiv4LeftMargin, Fixed));
  div4_style->setFloating(EFloat::kLeft);
  NGBlockNode* div4 = new NGBlockNode(div4_style.get());

  div1->SetNextSibling(div2);
  div2->SetNextSibling(div3);
  div3->SetNextSibling(div4);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);
  ASSERT_EQ(frag->Children().size(), 4UL);

  // DIV1
  const NGPhysicalFragment* child1 = frag->Children()[0];
  EXPECT_EQ(kDiv1TopMargin, child1->TopOffset());
  EXPECT_EQ(kParentLeftPadding, child1->LeftOffset());

  // DIV2
  const NGPhysicalFragment* child2 = frag->Children()[1];
  EXPECT_EQ(0, child2->TopOffset());
  EXPECT_EQ(kParentLeftPadding, child2->LeftOffset());

  // DIV3
  const NGPhysicalFragment* child3 = frag->Children()[2];
  EXPECT_EQ(kDiv2Size, child3->TopOffset());
  EXPECT_EQ(kParentLeftPadding + kParentSize - kDiv3Size, child3->LeftOffset());

  // DIV4
  const NGPhysicalFragment* child4 = frag->Children()[3];
  EXPECT_EQ(kDiv2Size + kDiv3Size, child4->TopOffset());
  EXPECT_EQ(kParentLeftPadding + kDiv4LeftMargin, child4->LeftOffset());
}

// Verifies that NG block layout algorithm respects "clear" CSS property.
//
// Test case's HTML representation:
//  <div id="parent" style="width: 200px; height: 200px;">
//    <div style="float: left; width: 30px; height: 30px;"/>   <!-- DIV1 -->
//    <div style="float: right; width: 40px; height: 40px;
//        clear: left;"/>  <!-- DIV2 -->
//    <div style="clear: ...; width: 50px; height: 50px;"/>    <!-- DIV3 -->
//  </div>
//
// Expected:
// - DIV2 is positioned below DIV1 because it has clear: left;
// - DIV3 is positioned below DIV1 if clear: left;
// - DIV3 is positioned below DIV2 if clear: right;
// - DIV3 is positioned below DIV2 if clear: both;
TEST_F(NGBlockLayoutAlgorithmTest, PositionFragmentsWithClear) {
  const int kParentSize = 200;
  const int kDiv1Size = 30;
  const int kDiv2Size = 40;
  const int kDiv3Size = 50;

  style_->setHeight(Length(kParentSize, Fixed));
  style_->setWidth(Length(kParentSize, Fixed));

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWidth(Length(kDiv1Size, Fixed));
  div1_style->setHeight(Length(kDiv1Size, Fixed));
  div1_style->setFloating(EFloat::kLeft);
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setWidth(Length(kDiv2Size, Fixed));
  div2_style->setHeight(Length(kDiv2Size, Fixed));
  div2_style->setClear(EClear::ClearLeft);
  div2_style->setFloating(EFloat::kRight);
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  // DIV3
  RefPtr<ComputedStyle> div3_style = ComputedStyle::create();
  div3_style->setWidth(Length(kDiv3Size, Fixed));
  div3_style->setHeight(Length(kDiv3Size, Fixed));
  NGBlockNode* div3 = new NGBlockNode(div3_style.get());

  div1->SetNextSibling(div2);
  div2->SetNextSibling(div3);

  // clear: left;
  div3_style->setClear(EClear::ClearLeft);
  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);
  const NGPhysicalFragment* child3 = frag->Children()[2];
  EXPECT_EQ(kDiv1Size, child3->TopOffset());

  // clear: right;
  div3_style->setClear(EClear::ClearRight);
  space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  frag = RunBlockLayoutAlgorithm(space, div1);
  child3 = frag->Children()[2];
  EXPECT_EQ(kDiv1Size + kDiv2Size, child3->TopOffset());

  // clear: both;
  div3_style->setClear(EClear::ClearBoth);
  space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  frag = RunBlockLayoutAlgorithm(space, div1);
  space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  child3 = frag->Children()[2];
  EXPECT_EQ(kDiv1Size + kDiv2Size, child3->TopOffset());
}

// Verifies that we compute the right min and max-content size.
TEST_F(NGBlockLayoutAlgorithmTest, ComputeMinMaxContent) {
  const int kWidth = 50;
  const int kWidthChild1 = 20;
  const int kWidthChild2 = 30;

  // This should have no impact on the min/max content size.
  style_->setWidth(Length(kWidth, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setWidth(Length(kWidthChild1, Fixed));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setWidth(Length(kWidthChild2, Fixed));
  NGBlockNode* second_child = new NGBlockNode(second_style.get());

  first_child->SetNextSibling(second_child);

  MinAndMaxContentSizes sizes = RunComputeMinAndMax(first_child);
  EXPECT_EQ(kWidthChild2, sizes.min_content);
  EXPECT_EQ(kWidthChild2, sizes.max_content);
}

// Tests that we correctly handle shrink-to-fit
TEST_F(NGBlockLayoutAlgorithmTest, ShrinkToFit) {
  const int kWidthChild1 = 20;
  const int kWidthChild2 = 30;

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setWidth(Length(kWidthChild1, Fixed));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setWidth(Length(kWidthChild2, Fixed));
  NGBlockNode* second_child = new NGBlockNode(second_style.get());

  first_child->SetNextSibling(second_child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite), true);
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(30), frag->Width());
}

class FragmentChildIterator
    : public GarbageCollectedFinalized<FragmentChildIterator> {
 public:
  FragmentChildIterator() {}
  FragmentChildIterator(const NGPhysicalBoxFragment* parent) {
    SetParent(parent);
  }
  void SetParent(const NGPhysicalBoxFragment* parent) {
    parent_ = parent;
    index_ = 0;
  }

  const NGPhysicalBoxFragment* NextChild() {
    if (!parent_)
      return nullptr;
    if (index_ >= parent_->Children().size())
      return nullptr;
    while (parent_->Children()[index_]->Type() !=
           NGPhysicalFragment::kFragmentBox) {
      ++index_;
      if (index_ >= parent_->Children().size())
        return nullptr;
    }
    return toNGPhysicalBoxFragment(parent_->Children()[index_++]);
  }

  DEFINE_INLINE_TRACE() { visitor->trace(parent_); }

 private:
  Member<const NGPhysicalBoxFragment> parent_;
  unsigned index_;
};

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:210px; height:100px;">
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, EmptyMulticol) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(210, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);
  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  // There should be nothing inside the multicol container.
  EXPECT_FALSE(FragmentChildIterator(fragment).NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:210px; height:100px;">
//    <div id="child"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, EmptyBlock) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(210, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);
  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  ASSERT_TRUE(fragment);
  EXPECT_FALSE(iterator.NextChild());
  iterator.SetParent(fragment);

  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(100), fragment->Width());
  EXPECT_EQ(LayoutUnit(), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:310px; height:100px;">
//    <div id="child" style="width:60%; height:100px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, BlockInOneColumn) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(310, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setWidth(Length(60, Percent));
  child_style->setHeight(Length(100, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(310), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());
  iterator.SetParent(fragment);

  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(90), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:210px; height:100px;">
//    <div id="child" style="width:75%; height:150px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, BlockInTwoColumns) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(210, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setWidth(Length(75, Percent));
  child_style->setHeight(Length(150, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child" style="width:75%; height:250px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, BlockInThreeColumns) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setWidth(Length(75, Percent));
  child_style->setHeight(Length(250, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());

  // #child fragment in third column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(220), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:210px; height:100px;">
//    <div id="child" style="width:1px; height:250px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, ActualColumnCountGreaterThanSpecified) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(210, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setWidth(Length(1, Fixed));
  child_style->setHeight(Length(250, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(1), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(1), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());

  // #child fragment in third column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(220), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(1), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child1" style="width:75%; height:60px;"></div>
//    <div id="child2" style="width:85%; height:60px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, TwoBlocksInTwoColumns) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child1
  RefPtr<ComputedStyle> child1_style = ComputedStyle::create();
  child1_style->setWidth(Length(75, Percent));
  child1_style->setHeight(Length(60, Fixed));
  NGBlockNode* child1 = new NGBlockNode(child1_style.get());

  // child2
  RefPtr<ComputedStyle> child2_style = ComputedStyle::create();
  child2_style->setWidth(Length(85, Percent));
  child2_style->setHeight(Length(60, Fixed));
  NGBlockNode* child2 = new NGBlockNode(child2_style.get());

  parent->SetFirstChild(child1);
  child1->SetNextSibling(child2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(60), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(60), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(85), fragment->Width());
  EXPECT_EQ(LayoutUnit(40), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child2 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(85), fragment->Width());
  EXPECT_EQ(LayoutUnit(20), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child1" style="width:75%; height:60px;">
//      <div id="grandchild1" style="width:50px; height:120px;"></div>
//      <div id="grandchild2" style="width:40px; height:20px;"></div>
//    </div>
//    <div id="child2" style="width:85%; height:10px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, OverflowedBlock) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child1
  RefPtr<ComputedStyle> child1_style = ComputedStyle::create();
  child1_style->setWidth(Length(75, Percent));
  child1_style->setHeight(Length(60, Fixed));
  NGBlockNode* child1 = new NGBlockNode(child1_style.get());

  // grandchild1
  RefPtr<ComputedStyle> grandchild1_style = ComputedStyle::create();
  grandchild1_style->setWidth(Length(50, Fixed));
  grandchild1_style->setHeight(Length(120, Fixed));
  NGBlockNode* grandchild1 = new NGBlockNode(grandchild1_style.get());

  // grandchild2
  RefPtr<ComputedStyle> grandchild2_style = ComputedStyle::create();
  grandchild2_style->setWidth(Length(40, Fixed));
  grandchild2_style->setHeight(Length(20, Fixed));
  NGBlockNode* grandchild2 = new NGBlockNode(grandchild2_style.get());

  // child2
  RefPtr<ComputedStyle> child2_style = ComputedStyle::create();
  child2_style->setWidth(Length(85, Percent));
  child2_style->setHeight(Length(10, Fixed));
  NGBlockNode* child2 = new NGBlockNode(child2_style.get());

  parent->SetFirstChild(child1);
  child1->SetNextSibling(child2);
  child1->SetFirstChild(grandchild1);
  grandchild1->SetNextSibling(grandchild2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(60), fragment->Height());
  FragmentChildIterator grandchild_iterator(fragment);
  // #grandchild1 fragment in first column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(50), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(grandchild_iterator.NextChild());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(60), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(85), fragment->Width());
  EXPECT_EQ(LayoutUnit(10), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child1 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(), fragment->Height());
  grandchild_iterator.SetParent(fragment);
  // #grandchild1 fragment in second column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(50), fragment->Width());
  EXPECT_EQ(LayoutUnit(20), fragment->Height());
  // #grandchild2 fragment in second column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(20), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(40), fragment->Width());
  EXPECT_EQ(LayoutUnit(20), fragment->Height());
  EXPECT_FALSE(grandchild_iterator.NextChild());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child" style="float:left; width:75%; height:100px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, FloatInOneColumn) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setFloating(EFloat::kLeft);
  child_style->setWidth(Length(75, Percent));
  child_style->setHeight(Length(100, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child1" style="float:left; width:15%; height:100px;"></div>
//    <div id="child2" style="float:right; width:16%; height:100px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, TwoFloatsInOneColumn) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child1
  RefPtr<ComputedStyle> child1_style = ComputedStyle::create();
  child1_style->setFloating(EFloat::kLeft);
  child1_style->setWidth(Length(15, Percent));
  child1_style->setHeight(Length(100, Fixed));
  NGBlockNode* child1 = new NGBlockNode(child1_style.get());

  // child2
  RefPtr<ComputedStyle> child2_style = ComputedStyle::create();
  child2_style->setFloating(EFloat::kRight);
  child2_style->setWidth(Length(16, Percent));
  child2_style->setHeight(Length(100, Fixed));
  NGBlockNode* child2 = new NGBlockNode(child2_style.get());

  parent->SetFirstChild(child1);
  child1->SetNextSibling(child2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(15), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(84), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(16), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child1" style="float:left; width:15%; height:150px;"></div>
//    <div id="child2" style="float:right; width:16%; height:150px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, TwoFloatsInTwoColumns) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child1
  RefPtr<ComputedStyle> child1_style = ComputedStyle::create();
  child1_style->setFloating(EFloat::kLeft);
  child1_style->setWidth(Length(15, Percent));
  child1_style->setHeight(Length(150, Fixed));
  NGBlockNode* child1 = new NGBlockNode(child1_style.get());

  // child2
  RefPtr<ComputedStyle> child2_style = ComputedStyle::create();
  child2_style->setFloating(EFloat::kRight);
  child2_style->setWidth(Length(16, Percent));
  child2_style->setHeight(Length(150, Fixed));
  NGBlockNode* child2 = new NGBlockNode(child2_style.get());

  parent->SetFirstChild(child1);
  child1->SetNextSibling(child2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(15), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(84), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(16), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child1 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(15), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(194), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(16), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

}  // namespace
}  // namespace blink
