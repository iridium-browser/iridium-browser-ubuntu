// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(
    NGWritingMode writing_mode,
    TextDirection direction,
    NGLogicalSize available_size,
    NGLogicalSize percentage_resolution_size,
    LayoutUnit fragmentainer_space_available,
    bool is_fixed_size_inline,
    bool is_fixed_size_block,
    bool is_shrink_to_fit,
    bool is_inline_direction_triggers_scrollbar,
    bool is_block_direction_triggers_scrollbar,
    NGFragmentationType block_direction_fragmentation_type,
    bool is_new_fc,
    const std::shared_ptr<NGExclusions>& exclusions_)
    : available_size_(available_size),
      percentage_resolution_size_(percentage_resolution_size),
      fragmentainer_space_available_(fragmentainer_space_available),
      is_fixed_size_inline_(is_fixed_size_inline),
      is_fixed_size_block_(is_fixed_size_block),
      is_shrink_to_fit_(is_shrink_to_fit),
      is_inline_direction_triggers_scrollbar_(
          is_inline_direction_triggers_scrollbar),
      is_block_direction_triggers_scrollbar_(
          is_block_direction_triggers_scrollbar),
      block_direction_fragmentation_type_(block_direction_fragmentation_type),
      is_new_fc_(is_new_fc),
      writing_mode_(writing_mode),
      direction_(static_cast<unsigned>(direction)),
      exclusions_(exclusions_) {}

NGConstraintSpace* NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box) {
  bool fixed_inline = false, fixed_block = false;
  // XXX for orthogonal writing mode this is not right
  LayoutUnit available_logical_width =
      std::max(LayoutUnit(), box.containingBlockLogicalWidthForContent());
  LayoutUnit available_logical_height;
  if (!box.parent()) {
    available_logical_height = box.view()->viewLogicalHeightForPercentages();
  } else if (box.containingBlock()) {
    available_logical_height =
        box.containingBlock()->availableLogicalHeightForPercentageComputation();
  }
  NGLogicalSize percentage_size = {available_logical_width,
                                   available_logical_height};
  NGLogicalSize available_size = percentage_size;
  // When we have an override size, the available_logical_{width,height} will be
  // used as the final size of the box, so it has to include border and
  // padding.
  if (box.hasOverrideLogicalContentWidth()) {
    available_size.inline_size =
        box.borderAndPaddingLogicalWidth() + box.overrideLogicalContentWidth();
    fixed_inline = true;
  }
  if (box.hasOverrideLogicalContentHeight()) {
    available_size.block_size = box.borderAndPaddingLogicalHeight() +
                                box.overrideLogicalContentHeight();
    fixed_block = true;
  }

  bool is_new_fc =
      box.isLayoutBlock() && toLayoutBlock(box).createsNewFormattingContext();

  auto writing_mode = FromPlatformWritingMode(box.styleRef().getWritingMode());
  return NGConstraintSpaceBuilder(writing_mode)
      .SetAvailableSize(available_size)
      .SetPercentageResolutionSize(percentage_size)
      .SetIsInlineDirectionTriggersScrollbar(
          box.styleRef().overflowInlineDirection() == EOverflow::Auto)
      .SetIsBlockDirectionTriggersScrollbar(
          box.styleRef().overflowBlockDirection() == EOverflow::Auto)
      .SetIsFixedSizeInline(fixed_inline)
      .SetIsFixedSizeBlock(fixed_block)
      .SetIsShrinkToFit(
          box.sizesLogicalWidthToFitContent(box.styleRef().logicalWidth()))
      .SetIsNewFormattingContext(is_new_fc)
      .SetTextDirection(box.styleRef().direction())
      .ToConstraintSpace();
}

void NGConstraintSpace::AddExclusion(const NGExclusion& exclusion) {
  exclusions_->Add(exclusion);
}

NGFragmentationType NGConstraintSpace::BlockFragmentationType() const {
  return static_cast<NGFragmentationType>(block_direction_fragmentation_type_);
}

void NGConstraintSpace::Subtract(const NGBoxFragment*) {
  // TODO(layout-ng): Implement.
}

NGLayoutOpportunityIterator* NGConstraintSpace::LayoutOpportunities(
    unsigned clear,
    bool for_inline_or_bfc) {
  NGLayoutOpportunityIterator* iterator = new NGLayoutOpportunityIterator(this);
  return iterator;
}

String NGConstraintSpace::ToString() const {
  return String::format("%s,%s %sx%s",
                        offset_.inline_offset.toString().ascii().data(),
                        offset_.block_offset.toString().ascii().data(),
                        AvailableSize().inline_size.toString().ascii().data(),
                        AvailableSize().block_size.toString().ascii().data());
}

}  // namespace blink
