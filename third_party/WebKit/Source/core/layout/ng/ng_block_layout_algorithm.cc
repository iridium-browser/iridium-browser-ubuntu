// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_column_mapper.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"
#include "wtf/Optional.h"

namespace blink {
namespace {

// Adjusts content's offset to CSS "clear" property.
// TODO(glebl): Support margin collapsing edge cases, e.g. margin collapsing
// should not occur if "clear" is applied to non-floating blocks.
// TODO(layout-ng): the call to AdjustToClearance should be moved to
// CreateConstraintSpaceForChild once ConstraintSpaceBuilder is sharing the
// exclusion information between constraint spaces.
void AdjustToClearance(const NGConstraintSpace& space,
                       const ComputedStyle& style,
                       LayoutUnit* content_size) {
  const NGExclusion* right_exclusion = space.Exclusions()->last_right_float;
  const NGExclusion* left_exclusion = space.Exclusions()->last_left_float;

  // Calculates Left/Right block end offset from left/right float exclusions or
  // use the default content offset position.
  LayoutUnit left_block_end_offset =
      left_exclusion ? left_exclusion->rect.BlockEndOffset() : *content_size;
  LayoutUnit right_block_end_offset =
      right_exclusion ? right_exclusion->rect.BlockEndOffset() : *content_size;

  switch (style.clear()) {
    case EClear::ClearNone:
      return;  // nothing to do here.
    case EClear::ClearLeft:
      *content_size = left_block_end_offset;
      break;
    case EClear::ClearRight:
      *content_size = right_block_end_offset;
      break;
    case EClear::ClearBoth:
      *content_size = std::max(left_block_end_offset, right_block_end_offset);
      break;
    default:
      ASSERT_NOT_REACHED();
  }
}

LayoutUnit ComputeCollapsedMarginBlockStart(
    const NGMarginStrut& prev_margin_strut,
    const NGMarginStrut& curr_margin_strut) {
  return std::max(prev_margin_strut.margin_block_end,
                  curr_margin_strut.margin_block_start) -
         std::max(prev_margin_strut.negative_margin_block_end.abs(),
                  curr_margin_strut.negative_margin_block_start.abs());
}

// Creates an exclusion from the fragment that will be placed in the provided
// layout opportunity.
NGExclusion CreateExclusion(const NGFragment& fragment,
                            const NGLayoutOpportunity& opportunity,
                            LayoutUnit float_offset,
                            NGBoxStrut margins,
                            NGExclusion::Type exclusion_type) {
  NGExclusion exclusion;
  exclusion.type = exclusion_type;
  NGLogicalRect& rect = exclusion.rect;
  rect.offset = opportunity.offset;
  rect.offset.inline_offset += float_offset;

  rect.size.inline_size = fragment.InlineSize();
  rect.size.block_size = fragment.BlockSize();

  // Adjust to child's margin.
  rect.size.block_size += margins.BlockSum();
  rect.size.inline_size += margins.InlineSum();

  return exclusion;
}

// Finds a layout opportunity for the fragment.
// It iterates over all layout opportunities in the constraint space and returns
// the first layout opportunity that is wider than the fragment or returns the
// last one which is always the widest.
//
// @param space Constraint space that is used to find layout opportunity for
//              the fragment.
// @param fragment Fragment that needs to be placed.
// @param margins Margins of the fragment.
// @return Layout opportunity for the fragment.
const NGLayoutOpportunity FindLayoutOpportunityForFragment(
    NGConstraintSpace* space,
    const NGFragment& fragment,
    const NGBoxStrut& margins) {
  NGLayoutOpportunityIterator* opportunity_iter = space->LayoutOpportunities();
  NGLayoutOpportunity opportunity;
  NGLayoutOpportunity opportunity_candidate = opportunity_iter->Next();

  while (!opportunity_candidate.IsEmpty()) {
    opportunity = opportunity_candidate;
    // Checking opportunity's block size is not necessary as a float cannot be
    // positioned on top of another float inside of the same constraint space.
    auto fragment_inline_size = fragment.InlineSize() + margins.InlineSum();
    if (opportunity.size.inline_size > fragment_inline_size)
      break;

    opportunity_candidate = opportunity_iter->Next();
  }

  return opportunity;
}

// Calculates the logical offset for opportunity.
NGLogicalOffset CalculateLogicalOffsetForOpportunity(
    const NGLayoutOpportunity& opportunity,
    LayoutUnit float_offset,
    NGBoxStrut margins) {
  // Adjust to child's margin.
  LayoutUnit inline_offset = margins.inline_start;
  LayoutUnit block_offset = margins.block_start;

  // Offset from the opportunity's block/inline start.
  inline_offset += opportunity.offset.inline_offset;
  block_offset += opportunity.offset.block_offset;

  inline_offset += float_offset;

  return NGLogicalOffset(inline_offset, block_offset);
}

// Whether an in-flow block-level child creates a new formatting context.
//
// This will *NOT* check the following cases:
//  - The child is out-of-flow, e.g. floating or abs-pos.
//  - The child is a inline-level, e.g. "display: inline-block".
//  - The child establishes a new formatting context, but should be a child of
//    another layout algorithm, e.g. "display: table-caption" or flex-item.
bool IsNewFormattingContextForInFlowBlockLevelChild(
    const NGConstraintSpace& space,
    const ComputedStyle& style) {
  // TODO(layout-dev): This doesn't capture a few cases which can't be computed
  // directly from style yet:
  //  - The child is a <fieldset>.
  //  - "column-span: all" is set on the child (requires knowledge that we are
  //    in a multi-col formatting context).
  //    (https://drafts.csswg.org/css-multicol-1/#valdef-column-span-all)

  if (style.specifiesColumns() || style.containsPaint() ||
      style.containsLayout())
    return true;

  if (!style.isOverflowVisible())
    return true;

  EDisplay display = style.display();
  if (display == EDisplay::Grid || display == EDisplay::Flex ||
      display == EDisplay::WebkitBox)
    return true;

  if (space.WritingMode() != FromPlatformWritingMode(style.getWritingMode()))
    return true;

  return false;
}

}  // namespace

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGBlockNode* first_child,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : NGLayoutAlgorithm(kBlockLayoutAlgorithm),
      style_(style),
      first_child_(first_child),
      constraint_space_(constraint_space),
      break_token_(break_token),
      is_fragment_margin_strut_block_start_updated_(false) {
  DCHECK(style_);
}

bool NGBlockLayoutAlgorithm::ComputeMinAndMaxContentSizes(
    MinAndMaxContentSizes* sizes) {
  sizes->min_content = LayoutUnit();
  sizes->max_content = LayoutUnit();

  // Size-contained elements don't consider their contents for intrinsic sizing.
  if (Style().containsSize())
    return true;

  // TODO: handle floats & orthogonal children.
  for (NGBlockNode* node = first_child_; node; node = node->NextSibling()) {
    Optional<MinAndMaxContentSizes> child_minmax;
    if (NeedMinAndMaxContentSizesForContentContribution(*node->Style())) {
      child_minmax = node->ComputeMinAndMaxContentSizesSync();
    }

    MinAndMaxContentSizes child_sizes =
        ComputeMinAndMaxContentContribution(*node->Style(), child_minmax);

    sizes->min_content = std::max(sizes->min_content, child_sizes.min_content);
    sizes->max_content = std::max(sizes->max_content, child_sizes.max_content);
  }

  sizes->max_content = std::max(sizes->min_content, sizes->max_content);
  return true;
}

NGLayoutStatus NGBlockLayoutAlgorithm::Layout(
    NGPhysicalFragment* child_fragment,
    NGPhysicalFragment** fragment_out,
    NGLayoutAlgorithm** algorithm_out) {
  WTF::Optional<MinAndMaxContentSizes> sizes;
  if (NeedMinAndMaxContentSizes(ConstraintSpace(), Style())) {
    // TODO(ikilpatrick): Change ComputeMinAndMaxContentSizes to return
    // MinAndMaxContentSizes.
    sizes = MinAndMaxContentSizes();
    ComputeMinAndMaxContentSizes(&*sizes);
  }

  border_and_padding_ =
      ComputeBorders(Style()) + ComputePadding(ConstraintSpace(), Style());

  LayoutUnit inline_size =
      ComputeInlineSizeForFragment(ConstraintSpace(), Style(), sizes);
  LayoutUnit adjusted_inline_size =
      inline_size - border_and_padding_.InlineSum();
  // TODO(layout-ng): For quirks mode, should we pass blockSize instead of
  // -1?
  LayoutUnit block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), NGSizeIndefinite);
  LayoutUnit adjusted_block_size(block_size);
  // Our calculated block-axis size may be indefinite at this point.
  // If so, just leave the size as NGSizeIndefinite instead of subtracting
  // borders and padding.
  if (adjusted_block_size != NGSizeIndefinite)
    adjusted_block_size -= border_and_padding_.BlockSum();

  space_builder_ = new NGConstraintSpaceBuilder(constraint_space_);
  if (Style().specifiesColumns()) {
    space_builder_->SetFragmentationType(kFragmentColumn);
    adjusted_inline_size =
        ResolveUsedColumnInlineSize(adjusted_inline_size, Style());
    LayoutUnit inline_progression =
        adjusted_inline_size + ResolveUsedColumnGap(Style());
    fragmentainer_mapper_ =
        new NGColumnMapper(inline_progression, adjusted_block_size);
  }
  space_builder_->SetAvailableSize(
      NGLogicalSize(adjusted_inline_size, adjusted_block_size));
  space_builder_->SetPercentageResolutionSize(
      NGLogicalSize(adjusted_inline_size, adjusted_block_size));

  builder_ = new NGFragmentBuilder(NGPhysicalFragment::kFragmentBox);
  builder_->SetDirection(constraint_space_->Direction());
  builder_->SetWritingMode(constraint_space_->WritingMode());
  builder_->SetInlineSize(inline_size).SetBlockSize(block_size);

  if (NGBlockBreakToken* token = CurrentBlockBreakToken()) {
    // Resume after a previous break.
    content_size_ = token->BreakOffset();
    current_child_ = token->InputNode();
  } else {
    content_size_ = border_and_padding_.block_start;
    current_child_ = first_child_;
  }

  while (current_child_) {
    EPosition position = current_child_->Style()->position();
    if (position == AbsolutePosition || position == FixedPosition) {
      builder_->AddOutOfFlowChildCandidate(current_child_,
                                           GetChildSpaceOffset());
      current_child_ = current_child_->NextSibling();
      continue;
    }

    DCHECK(!ConstraintSpace().HasBlockFragmentation() ||
           SpaceAvailableForCurrentChild() > LayoutUnit());
    space_for_current_child_ = CreateConstraintSpaceForCurrentChild();

    NGFragment* fragment;
    current_child_->LayoutSync(space_for_current_child_, &fragment);
    NGPhysicalFragment* child_fragment = fragment->PhysicalFragment();

    // TODO(layout_ng): Seems like a giant hack to call this here.
    current_child_->UpdateLayoutBox(toNGPhysicalBoxFragment(child_fragment),
                                    space_for_current_child_);

    FinishCurrentChildLayout(new NGBoxFragment(
        ConstraintSpace().WritingMode(), ConstraintSpace().Direction(),
        toNGPhysicalBoxFragment(child_fragment)));

    if (!ProceedToNextUnfinishedSibling(child_fragment))
      break;
  }

  content_size_ += border_and_padding_.block_end;

  // Recompute the block-axis size now that we know our content size.
  block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), content_size_);
  builder_->SetBlockSize(block_size);

  // Out of flow setup.
  out_of_flow_layout_ = new NGOutOfFlowLayoutPart(&Style(), builder_->Size());
  builder_->GetAndClearOutOfFlowDescendantCandidates(
      &out_of_flow_candidates_, &out_of_flow_candidate_positions_);
  out_of_flow_candidate_positions_index_ = 0;
  current_child_ = nullptr;

  while (!LayoutOutOfFlowChild())
    continue;

  builder_->SetInlineOverflow(max_inline_size_).SetBlockOverflow(content_size_);

  if (ConstraintSpace().HasBlockFragmentation())
    FinalizeForFragmentation();

  *fragment_out = builder_->ToBoxFragment();
  return kNewFragment;
}

void NGBlockLayoutAlgorithm::FinishCurrentChildLayout(NGFragment* fragment) {
  NGBoxStrut child_margins = ComputeMargins(
      *space_for_current_child_, CurrentChildStyle(),
      constraint_space_->WritingMode(), constraint_space_->Direction());

  NGLogicalOffset fragment_offset;
  if (CurrentChildStyle().isFloating()) {
    fragment_offset = PositionFloatFragment(*fragment, child_margins);
  } else {
    ApplyAutoMargins(*space_for_current_child_, CurrentChildStyle(), *fragment,
                     &child_margins);
    fragment_offset = PositionFragment(*fragment, child_margins);
  }
  if (fragmentainer_mapper_)
    fragmentainer_mapper_->ToVisualOffset(fragment_offset);
  else
    fragment_offset.block_offset -= PreviousBreakOffset();
  builder_->AddChild(fragment, fragment_offset);
}

bool NGBlockLayoutAlgorithm::LayoutOutOfFlowChild() {
  if (out_of_flow_candidates_.isEmpty()) {
    out_of_flow_layout_ = nullptr;
    out_of_flow_candidate_positions_.clear();
    return true;
  }
  current_child_ = out_of_flow_candidates_.first();
  out_of_flow_candidates_.removeFirst();
  NGStaticPosition static_position = out_of_flow_candidate_positions_
      [out_of_flow_candidate_positions_index_++];

  if (IsContainingBlockForAbsoluteChild(Style(), *current_child_->Style())) {
    NGFragment* fragment;
    NGLogicalOffset offset;
    out_of_flow_layout_->Layout(*current_child_, static_position, &fragment,
                                &offset);
    // TODO(atotic) Need to adjust size of overflow rect per spec.
    builder_->AddChild(fragment, offset);
  } else {
    builder_->AddOutOfFlowDescendant(current_child_, static_position);
  }

  return false;
}

bool NGBlockLayoutAlgorithm::ProceedToNextUnfinishedSibling(
    NGPhysicalFragment* child_fragment) {
  DCHECK(current_child_);
  NGBlockNode* finished_child = current_child_;
  current_child_ = current_child_->NextSibling();
  if (!ConstraintSpace().HasBlockFragmentation() && !fragmentainer_mapper_)
    return true;
  // If we're resuming layout after a fragmentainer break, we need to skip
  // siblings that we're done with. We may have been able to fully lay out some
  // node(s) preceding a node that we had to break inside (and therefore were
  // not able to fully lay out). This happens when we have parallel flows [1],
  // which are caused by floats, overflow, etc.
  //
  // [1] https://drafts.csswg.org/css-break/#parallel-flows
  if (CurrentBlockBreakToken()) {
    // TODO(layout-ng): Figure out if we need a better way to determine if the
    // node is finished. Maybe something to encode in a break token?
    while (current_child_ && current_child_->IsLayoutFinished())
      current_child_ = current_child_->NextSibling();
  }
  LayoutUnit break_offset = NextBreakOffset();
  bool is_out_of_space = content_size_ - PreviousBreakOffset() >= break_offset;
  if (!HasPendingBreakToken()) {
    bool child_broke = child_fragment->BreakToken();
    // This block needs to break if the child broke, or if we're out of space
    // and there's more content waiting to be laid out. Otherwise, just bail
    // now.
    if (!child_broke && (!is_out_of_space || !current_child_))
      return true;
    // Prepare a break token for this block, so that we know where to resume
    // when the time comes for that. We may not be able to abort layout of this
    // block right away, due to the posibility of parallel flows. We can only
    // abort when we're out of space, or when there are no siblings left to
    // process.
    NGBlockBreakToken* token;
    if (child_broke) {
      // The child we just laid out was the first one to break. So that is
      // where we need to resume.
      token = new NGBlockBreakToken(finished_child, break_offset);
    } else {
      // Resume layout at the next sibling that needs layout.
      DCHECK(current_child_);
      token = new NGBlockBreakToken(current_child_, break_offset);
    }
    SetPendingBreakToken(token);
  }

  if (!fragmentainer_mapper_) {
    if (!is_out_of_space)
      return true;
    // We have run out of space in this flow, so there's no work left to do for
    // this block in this fragmentainer. We should finalize the fragment and get
    // back to the remaining content when laying out the next fragmentainer(s).
    return false;
  }

  if (is_out_of_space || !current_child_) {
    NGBlockBreakToken* token = fragmentainer_mapper_->Advance();
    DCHECK(token || !is_out_of_space);
    if (token) {
      break_token_ = token;
      content_size_ = token->BreakOffset();
      current_child_ = token->InputNode();
    }
  }
  return true;
}

void NGBlockLayoutAlgorithm::SetPendingBreakToken(NGBlockBreakToken* token) {
  if (fragmentainer_mapper_)
    fragmentainer_mapper_->SetBreakToken(token);
  else
    builder_->SetBreakToken(token);
}

bool NGBlockLayoutAlgorithm::HasPendingBreakToken() const {
  if (fragmentainer_mapper_)
    return fragmentainer_mapper_->HasBreakToken();
  return builder_->HasBreakToken();
}

void NGBlockLayoutAlgorithm::FinalizeForFragmentation() {
  LayoutUnit block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), content_size_);
  LayoutUnit previous_break_offset = PreviousBreakOffset();
  block_size -= previous_break_offset;
  block_size = std::max(LayoutUnit(), block_size);
  LayoutUnit space_left = ConstraintSpace().FragmentainerSpaceAvailable();
  DCHECK_GE(space_left, LayoutUnit());
  if (builder_->HasBreakToken()) {
    // A break token is ready, which means that we're going to break
    // before or inside a block-level child.
    builder_->SetBlockSize(std::min(space_left, block_size));
    builder_->SetBlockOverflow(space_left);
    return;
  }
  if (block_size > space_left) {
    // Need a break inside this block.
    builder_->SetBreakToken(new NGBlockBreakToken(nullptr, NextBreakOffset()));
    builder_->SetBlockSize(space_left);
    builder_->SetBlockOverflow(space_left);
    return;
  }
  // The end of the block fits in the current fragmentainer.
  builder_->SetBlockSize(block_size);
  builder_->SetBlockOverflow(content_size_ - previous_break_offset);
}

NGBlockBreakToken* NGBlockLayoutAlgorithm::CurrentBlockBreakToken() const {
  NGBreakToken* token = break_token_;
  if (!token || token->Type() != NGBreakToken::kBlockBreakToken)
    return nullptr;
  return toNGBlockBreakToken(token);
}

LayoutUnit NGBlockLayoutAlgorithm::PreviousBreakOffset() const {
  const NGBlockBreakToken* token = CurrentBlockBreakToken();
  return token ? token->BreakOffset() : LayoutUnit();
}

LayoutUnit NGBlockLayoutAlgorithm::NextBreakOffset() const {
  if (fragmentainer_mapper_)
    return fragmentainer_mapper_->NextBreakOffset();
  DCHECK(ConstraintSpace().HasBlockFragmentation());
  return PreviousBreakOffset() +
         ConstraintSpace().FragmentainerSpaceAvailable();
}

LayoutUnit NGBlockLayoutAlgorithm::SpaceAvailableForCurrentChild() const {
  LayoutUnit space_left;
  if (fragmentainer_mapper_)
    space_left = fragmentainer_mapper_->BlockSize();
  else if (ConstraintSpace().HasBlockFragmentation())
    space_left = ConstraintSpace().FragmentainerSpaceAvailable();
  else
    return NGSizeIndefinite;
  space_left -= BorderEdgeForCurrentChild() - PreviousBreakOffset();
  return space_left;
}

NGBoxStrut NGBlockLayoutAlgorithm::CollapseMargins(
    const NGBoxStrut& margins,
    const NGBoxFragment& fragment) {
  bool is_zero_height_box = !fragment.BlockSize() && margins.IsEmpty() &&
                            fragment.MarginStrut().IsEmpty();
  // Create the current child's margin strut from its children's margin strut or
  // use margin strut from the the last non-empty child.
  NGMarginStrut curr_margin_strut =
      is_zero_height_box ? prev_child_margin_strut_ : fragment.MarginStrut();

  // Calculate borders and padding for the current child.
  NGBoxStrut border_and_padding =
      ComputeBorders(CurrentChildStyle()) +
      ComputePadding(ConstraintSpace(), CurrentChildStyle());

  // Collapse BLOCK-START margins if there is no padding or border between
  // parent (current child) and its first in-flow child.
  if (border_and_padding.block_start) {
    curr_margin_strut.SetMarginBlockStart(margins.block_start);
  } else {
    curr_margin_strut.AppendMarginBlockStart(margins.block_start);
  }

  // Collapse BLOCK-END margins if
  // 1) there is no padding or border between parent (current child) and its
  //    first/last in-flow child
  // 2) parent's logical height is auto.
  if (CurrentChildStyle().logicalHeight().isAuto() &&
      !border_and_padding.block_end) {
    curr_margin_strut.AppendMarginBlockEnd(margins.block_end);
  } else {
    curr_margin_strut.SetMarginBlockEnd(margins.block_end);
  }

  NGBoxStrut result_margins;
  // Margins of the newly established formatting context do not participate
  // in Collapsing Margins:
  // - Compute margins block start for adjoining blocks *including* 1st block.
  // - Compute margins block end for the last block.
  // - Do not set the computed margins to the parent fragment.
  if (constraint_space_->IsNewFormattingContext()) {
    result_margins.block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);
    bool is_last_child = !current_child_->NextSibling();
    if (is_last_child)
      result_margins.block_end = curr_margin_strut.BlockEndSum();
    return result_margins;
  }

  // Zero-height boxes are ignored and do not participate in margin collapsing.
  if (is_zero_height_box)
    return result_margins;

  // Compute the margin block start for adjoining blocks *excluding* 1st block
  if (is_fragment_margin_strut_block_start_updated_) {
    result_margins.block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);
  }

  // Update the parent fragment's margin strut
  UpdateMarginStrut(curr_margin_strut);

  prev_child_margin_strut_ = curr_margin_strut;
  return result_margins;
}

NGLogicalOffset NGBlockLayoutAlgorithm::PositionFragment(
    const NGFragment& fragment,
    const NGBoxStrut& child_margins) {
  const NGBoxStrut collapsed_margins =
      CollapseMargins(child_margins, toNGBoxFragment(fragment));

  AdjustToClearance(ConstraintSpace(), CurrentChildStyle(), &content_size_);

  LayoutUnit inline_offset =
      border_and_padding_.inline_start + child_margins.inline_start;
  LayoutUnit block_offset = content_size_ + collapsed_margins.block_start;

  content_size_ += fragment.BlockSize() + collapsed_margins.BlockSum();
  max_inline_size_ = std::max(
      max_inline_size_, fragment.InlineSize() + child_margins.InlineSum() +
                            border_and_padding_.InlineSum());
  return NGLogicalOffset(inline_offset, block_offset);
}

NGLogicalOffset NGBlockLayoutAlgorithm::PositionFloatFragment(
    const NGFragment& fragment,
    const NGBoxStrut& margins) {
  // TODO(glebl@chromium.org): Support the top edge alignment rule.
  // Find a layout opportunity that will fit our float.

  // Update offset if there is a clearance.
  NGLogicalOffset offset = space_for_current_child_->Offset();
  AdjustToClearance(ConstraintSpace(), CurrentChildStyle(),
                    &offset.block_offset);
  space_for_current_child_->SetOffset(offset);

  const NGLayoutOpportunity opportunity = FindLayoutOpportunityForFragment(
      space_for_current_child_, fragment, margins);
  DCHECK(!opportunity.IsEmpty()) << "Opportunity is empty but it shouldn't be";

  NGExclusion::Type exclusion_type = NGExclusion::kFloatLeft;
  // Calculate the float offset if needed.
  LayoutUnit float_offset;
  if (CurrentChildStyle().floating() == EFloat::kRight) {
    float_offset = opportunity.size.inline_size - fragment.InlineSize();
    exclusion_type = NGExclusion::kFloatRight;
  }

  // Add the float as an exclusion.
  const NGExclusion exclusion = CreateExclusion(
      fragment, opportunity, float_offset, margins, exclusion_type);
  constraint_space_->AddExclusion(exclusion);

  return CalculateLogicalOffsetForOpportunity(opportunity, float_offset,
                                              margins);
}

void NGBlockLayoutAlgorithm::UpdateMarginStrut(const NGMarginStrut& from) {
  if (!is_fragment_margin_strut_block_start_updated_) {
    builder_->SetMarginStrutBlockStart(from);
    is_fragment_margin_strut_block_start_updated_ = true;
  }
  builder_->SetMarginStrutBlockEnd(from);
}

NGConstraintSpace*
NGBlockLayoutAlgorithm::CreateConstraintSpaceForCurrentChild() const {
  // TODO(layout-ng): Orthogonal children should also shrink to fit (in *their*
  // inline axis)
  // We have to keep this commented out for now until we correctly compute
  // min/max content sizes in Layout().
  bool shrink_to_fit = CurrentChildStyle().display() == EDisplay::InlineBlock ||
                       CurrentChildStyle().isFloating();
  DCHECK(current_child_);
  space_builder_
      ->SetIsNewFormattingContext(
          IsNewFormattingContextForInFlowBlockLevelChild(ConstraintSpace(),
                                                         CurrentChildStyle()))
      .SetIsShrinkToFit(shrink_to_fit)
      .SetWritingMode(
          FromPlatformWritingMode(CurrentChildStyle().getWritingMode()))
      .SetTextDirection(CurrentChildStyle().direction());
  LayoutUnit space_available = SpaceAvailableForCurrentChild();
  space_builder_->SetFragmentainerSpaceAvailable(space_available);
  NGConstraintSpace* child_space = space_builder_->ToConstraintSpace();

  // TODO(layout-ng): Set offset through the space builder.
  child_space->SetOffset(GetChildSpaceOffset());
  return child_space;
}

DEFINE_TRACE(NGBlockLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(first_child_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
  visitor->trace(builder_);
  visitor->trace(space_builder_);
  visitor->trace(space_for_current_child_);
  visitor->trace(current_child_);
  visitor->trace(out_of_flow_layout_);
  visitor->trace(out_of_flow_candidates_);
  visitor->trace(fragmentainer_mapper_);
}

}  // namespace blink
