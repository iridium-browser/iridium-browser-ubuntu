// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_layout_algorithm.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_line_builder.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGInlineNode* first_child,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : NGLayoutAlgorithm(kInlineLayoutAlgorithm),
      style_(style),
      first_child_(first_child),
      constraint_space_(constraint_space),
      break_token_(break_token) {
  DCHECK(style_);
}

NGLayoutStatus NGInlineLayoutAlgorithm::Layout(
    NGPhysicalFragment*,
    NGPhysicalFragment** fragment_out,
    NGLayoutAlgorithm**) {
  // TODO(kojii): Implement sizing and child constraint spaces. Share common
  // logic with NGBlockLayoutAlgorithm using composition.
  switch (state_) {
    case kStateInit: {
      builder_ = new NGFragmentBuilder(NGPhysicalFragment::kFragmentBox);
      builder_->SetWritingMode(constraint_space_->WritingMode());
      builder_->SetDirection(constraint_space_->Direction());
      current_child_ = first_child_;
      if (current_child_) {
        space_for_current_child_ = CreateConstraintSpaceForCurrentChild();
        line_builder_ =
            new NGLineBuilder(current_child_, space_for_current_child_);
      }

      state_ = kStateChildLayout;
      return kNotFinished;
    }
    case kStateChildLayout: {
      if (current_child_) {
        if (!LayoutCurrentChild())
          return kNotFinished;
        current_child_ = current_child_->NextSibling();
        if (current_child_) {
          // TODO(kojii): Since line_builder_ is bound to NGLayoutInlineItem
          // in current_child_, changing current_child_ needs more work.
          ASSERT_NOT_REACHED();
          space_for_current_child_ = CreateConstraintSpaceForCurrentChild();
          return kNotFinished;
        }
      }
      state_ = kStateFinalize;
      return kNotFinished;
    }
    case kStateFinalize:
      line_builder_->CreateFragments(builder_);
      *fragment_out = builder_->ToBoxFragment();
      line_builder_->CopyFragmentDataToLayoutBlockFlow();
      state_ = kStateInit;
      return kNewFragment;
  };
  NOTREACHED();
  *fragment_out = nullptr;
  return kNewFragment;
}

bool NGInlineLayoutAlgorithm::LayoutCurrentChild() {
  return current_child_->LayoutInline(space_for_current_child_, line_builder_);
}

NGConstraintSpace*
NGInlineLayoutAlgorithm::CreateConstraintSpaceForCurrentChild() const {
  DCHECK(current_child_);
  // TODO(kojii): Implement child constraint space.
  NGConstraintSpace* child_space =
      NGConstraintSpaceBuilder(constraint_space_->WritingMode())
          .SetTextDirection(constraint_space_->Direction())
          .ToConstraintSpace();
  return child_space;
}

DEFINE_TRACE(NGInlineLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(first_child_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
  visitor->trace(builder_);
  visitor->trace(space_for_current_child_);
  visitor->trace(current_child_);
  visitor->trace(line_builder_);
}

}  // namespace blink
