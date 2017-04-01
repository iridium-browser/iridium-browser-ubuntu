// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineLayoutAlgorithm_h
#define NGInlineLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGBreakToken;
class NGConstraintSpace;
class NGFragmentBuilder;
class NGInlineNode;
class NGLineBuilder;

// A class for inline layout (e.g. a anonymous block with inline-level children
// only).
//
// This algorithm may at some point be merged with NGBlockLayoutAlgorithm in
// the future. Currently it exists as its own class to simplify the LayoutNG
// transition period.
class CORE_EXPORT NGInlineLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param style Style reference of the block that is being laid out.
  // @param first_child Our first child; the algorithm will use its NextSibling
  //                    method to access all the children.
  // @param space The constraint space which the algorithm should generate a
  //              fragment within.
  NGInlineLayoutAlgorithm(PassRefPtr<const ComputedStyle>,
                          NGInlineNode* first_child,
                          NGConstraintSpace* space,
                          NGBreakToken* break_token = nullptr);

  NGLayoutStatus Layout(NGPhysicalFragment*,
                        NGPhysicalFragment**,
                        NGLayoutAlgorithm**) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  // Read-only Getters.
  const ComputedStyle& Style() const { return *style_; }

  bool LayoutCurrentChild();
  NGConstraintSpace* CreateConstraintSpaceForCurrentChild() const;

  enum State { kStateInit, kStateChildLayout, kStateFinalize };
  State state_ = kStateInit;

  RefPtr<const ComputedStyle> style_;
  Member<NGInlineNode> first_child_;
  Member<NGConstraintSpace> constraint_space_;
  Member<NGBreakToken> break_token_;
  Member<NGFragmentBuilder> builder_;
  Member<NGConstraintSpace> space_for_current_child_;
  Member<NGInlineNode> current_child_;
  Member<NGLineBuilder> line_builder_;
};

}  // namespace blink

#endif  // NGInlineLayoutAlgorithm_h
