// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBuilder_h
#define NGFragmentBuilder_h

#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

class NGFragment;
class NGInlineNode;
class NGPhysicalBoxFragment;
class NGPhysicalTextFragment;

class CORE_EXPORT NGFragmentBuilder final
    : public GarbageCollectedFinalized<NGFragmentBuilder> {
 public:
  NGFragmentBuilder(NGPhysicalFragment::NGFragmentType);

  using WeakBoxList = HeapLinkedHashSet<WeakMember<NGBlockNode>>;

  NGFragmentBuilder& SetWritingMode(NGWritingMode);
  NGFragmentBuilder& SetDirection(TextDirection);

  NGFragmentBuilder& SetInlineSize(LayoutUnit);
  NGFragmentBuilder& SetBlockSize(LayoutUnit);
  NGLogicalSize Size() const { return size_; }

  NGFragmentBuilder& SetInlineOverflow(LayoutUnit);
  NGFragmentBuilder& SetBlockOverflow(LayoutUnit);

  NGFragmentBuilder& AddChild(NGFragment*, const NGLogicalOffset&);

  // Builder has non-trivial out-of-flow descendant methods.
  // These methods are building blocks for implementation of
  // out-of-flow descendants by layout algorithms.
  //
  // They are intended to be used by layout algorithm like this:
  //
  // Part 1: layout algorithm positions in-flow children.
  //   out-of-flow children, and out-of-flow descendants of fragments
  //   are stored inside builder.
  //
  // for (child : children)
  //   if (child->position == (Absolute or Fixed))
  //     builder->AddOutOfFlowChildCandidate(child);
  //   else
  //     fragment = child->Layout()
  //     builder->AddChild(fragment)
  // end
  //
  // Part 2: layout algorithm positions out-of-flow descendants.
  //
  // builder->SetInlineSize/SetBlockSize
  // builder->GetAndClearOutOfFlowDescendantCandidates(oof_candidates);
  // NGOutOfFlowLayoutPart out_of_flow_layout(container_style,
  //                                          builder->Size());
  // while (oof_candidates.size() > 0)
  // {
  //   candidate = oof_candidates.shift();
  //   if (IsContainingBlockForAbsoluteChild(style, candidate_style)) {
  //     NGFragmentBase* fragment;
  //     NGLogicalOffset* fragment_offset;
  //     out_of_flow_layout.Layout(candidate, &fragment, &offset);
  //     builder->AddChild(fragment);
  //     builder->GetAndClearOutOfFlowDescendantCandidates(child_oof_candidates);
  //     oof_candidates.prepend(child_oof_candidates);
  //   } else {
  //     builder->AddOutOfFlowDescendant();
  //   }
  // }
  NGFragmentBuilder& AddOutOfFlowChildCandidate(NGBlockNode*, NGLogicalOffset);

  void GetAndClearOutOfFlowDescendantCandidates(WeakBoxList*,
                                                Vector<NGStaticPosition>*);

  NGFragmentBuilder& AddOutOfFlowDescendant(NGBlockNode*,
                                            const NGStaticPosition&);

  void SetBreakToken(NGBreakToken* token) {
    DCHECK(!break_token_);
    break_token_ = token;
  }
  bool HasBreakToken() const { return break_token_; }

  // Sets MarginStrut for the resultant fragment.
  NGFragmentBuilder& SetMarginStrutBlockStart(const NGMarginStrut& from);
  NGFragmentBuilder& SetMarginStrutBlockEnd(const NGMarginStrut& from);

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  NGPhysicalBoxFragment* ToBoxFragment();
  NGPhysicalTextFragment* ToTextFragment(NGInlineNode*,
                                         unsigned start_index,
                                         unsigned end_index);

  DECLARE_VIRTUAL_TRACE();

 private:
  // Out-of-flow descendant placement information.
  // The generated fragment must compute NGStaticPosition for all
  // out-of-flow descendants.
  // The resulting NGStaticPosition gets derived from:
  // 1. The offset of fragment's child.
  // 2. The static position of descendant wrt child.
  //
  // A child can be:
  // 1. A descendant itself. In this case, descendant position is (0,0).
  // 2. A fragment containing a descendant.
  //
  // child_offset is stored as NGLogicalOffset because physical offset cannot
  // be computed until we know fragment's size.
  struct OutOfFlowPlacement {
    NGLogicalOffset child_offset;
    NGStaticPosition descendant_position;
  };

  NGPhysicalFragment::NGFragmentType type_;
  NGWritingMode writing_mode_;
  TextDirection direction_;

  NGLogicalSize size_;
  NGLogicalSize overflow_;

  NGMarginStrut margin_strut_;

  HeapVector<Member<NGPhysicalFragment>> children_;
  Vector<NGLogicalOffset> offsets_;

  WeakBoxList out_of_flow_descendant_candidates_;
  Vector<OutOfFlowPlacement> out_of_flow_candidate_placements_;

  WeakBoxList out_of_flow_descendants_;
  Vector<NGStaticPosition> out_of_flow_positions_;

  Member<NGBreakToken> break_token_;
};

}  // namespace blink

#endif  // NGFragmentBuilder
