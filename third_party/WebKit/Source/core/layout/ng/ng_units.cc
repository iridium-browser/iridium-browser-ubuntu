// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_units.h"

namespace blink {

LayoutUnit MinAndMaxContentSizes::ShrinkToFit(LayoutUnit available_size) const {
  DCHECK_GE(max_content, min_content);
  return std::min(max_content, std::max(min_content, available_size));
}

bool MinAndMaxContentSizes::operator==(
    const MinAndMaxContentSizes& other) const {
  return min_content == other.min_content && max_content == other.max_content;
}

NGPhysicalSize NGLogicalSize::ConvertToPhysical(NGWritingMode mode) const {
  return mode == kHorizontalTopBottom ? NGPhysicalSize(inline_size, block_size)
                                      : NGPhysicalSize(block_size, inline_size);
}

bool NGLogicalSize::operator==(const NGLogicalSize& other) const {
  return std::tie(other.inline_size, other.block_size) ==
         std::tie(inline_size, block_size);
}

NGLogicalSize NGPhysicalSize::ConvertToLogical(NGWritingMode mode) const {
  return mode == kHorizontalTopBottom ? NGLogicalSize(width, height)
                                      : NGLogicalSize(height, width);
}

bool NGLogicalRect::IsEmpty() const {
  // TODO(layout-dev): equality check shouldn't allocate an object each time.
  return *this == NGLogicalRect();
}

bool NGLogicalRect::IsContained(const NGLogicalRect& other) const {
  return !(InlineEndOffset() <= other.InlineStartOffset() ||
           BlockEndOffset() <= other.BlockStartOffset() ||
           InlineStartOffset() >= other.InlineEndOffset() ||
           BlockStartOffset() >= other.BlockEndOffset());
}

bool NGLogicalRect::operator==(const NGLogicalRect& other) const {
  return std::tie(other.offset, other.size) == std::tie(offset, size);
}

String NGLogicalRect::ToString() const {
  return String::format("%s,%s %sx%s",
                        offset.inline_offset.toString().ascii().data(),
                        offset.block_offset.toString().ascii().data(),
                        size.inline_size.toString().ascii().data(),
                        size.block_size.toString().ascii().data());
}

NGPhysicalOffset NGLogicalOffset::ConvertToPhysical(
    NGWritingMode mode,
    TextDirection direction,
    NGPhysicalSize outer_size,
    NGPhysicalSize inner_size) const {
  switch (mode) {
    case kHorizontalTopBottom:
      if (direction == TextDirection::kLtr)
        return NGPhysicalOffset(inline_offset, block_offset);
      else
        return NGPhysicalOffset(
            outer_size.width - inline_offset - inner_size.width, block_offset);
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      if (direction == TextDirection::kLtr)
        return NGPhysicalOffset(
            outer_size.width - block_offset - inner_size.width, inline_offset);
      else
        return NGPhysicalOffset(
            outer_size.width - block_offset - inner_size.width,
            outer_size.height - inline_offset - inner_size.height);
    case kVerticalLeftRight:
      if (direction == TextDirection::kLtr)
        return NGPhysicalOffset(block_offset, inline_offset);
      else
        return NGPhysicalOffset(
            block_offset,
            outer_size.height - inline_offset - inner_size.height);
    case kSidewaysLeftRight:
      if (direction == TextDirection::kLtr)
        return NGPhysicalOffset(
            block_offset,
            outer_size.height - inline_offset - inner_size.height);
      else
        return NGPhysicalOffset(block_offset, inline_offset);
    default:
      ASSERT_NOT_REACHED();
      return NGPhysicalOffset();
  }
}

bool NGLogicalOffset::operator==(const NGLogicalOffset& other) const {
  return std::tie(other.inline_offset, other.block_offset) ==
         std::tie(inline_offset, block_offset);
}

NGLogicalOffset NGLogicalOffset::operator+(const NGLogicalOffset& other) const {
  NGLogicalOffset result;
  result.inline_offset = this->inline_offset + other.inline_offset;
  result.block_offset = this->block_offset + other.block_offset;
  return result;
}

NGLogicalOffset& NGLogicalOffset::operator+=(const NGLogicalOffset& other) {
  *this = *this + other;
  return *this;
}

bool NGLogicalOffset::operator>(const NGLogicalOffset& other) const {
  return inline_offset > other.inline_offset &&
         block_offset > other.block_offset;
}

bool NGLogicalOffset::operator>=(const NGLogicalOffset& other) const {
  return inline_offset >= other.inline_offset &&
         block_offset >= other.block_offset;
}

bool NGLogicalOffset::operator<(const NGLogicalOffset& other) const {
  return inline_offset < other.inline_offset &&
         block_offset < other.block_offset;
}

bool NGLogicalOffset::operator<=(const NGLogicalOffset& other) const {
  return inline_offset <= other.inline_offset &&
         block_offset <= other.block_offset;
}

String NGLogicalOffset::ToString() const {
  return String::format("%dx%d", inline_offset.toInt(), block_offset.toInt());
}

NGPhysicalOffset NGPhysicalOffset::operator+(
    const NGPhysicalOffset& other) const {
  return NGPhysicalOffset{this->left + other.left, this->top + other.top};
}

NGPhysicalOffset& NGPhysicalOffset::operator+=(const NGPhysicalOffset& other) {
  *this = *this + other;
  return *this;
}

NGPhysicalOffset NGPhysicalOffset::operator-(
    const NGPhysicalOffset& other) const {
  return NGPhysicalOffset{this->left - other.left, this->top - other.top};
}

NGPhysicalOffset& NGPhysicalOffset::operator-=(const NGPhysicalOffset& other) {
  *this = *this - other;
  return *this;
}

bool NGBoxStrut::IsEmpty() const {
  return *this == NGBoxStrut();
}

bool NGBoxStrut::operator==(const NGBoxStrut& other) const {
  return std::tie(other.inline_start, other.inline_end, other.block_start,
                  other.block_end) ==
         std::tie(inline_start, inline_end, block_start, block_end);
}

// Converts physical dimensions to logical ones per
// https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
NGBoxStrut NGPhysicalBoxStrut::ConvertToLogical(NGWritingMode writing_mode,
                                                TextDirection direction) const {
  NGBoxStrut strut;
  switch (writing_mode) {
    case kHorizontalTopBottom:
      strut = {left, right, top, bottom};
      break;
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      strut = {top, bottom, right, left};
      break;
    case kVerticalLeftRight:
      strut = {top, bottom, left, right};
      break;
    case kSidewaysLeftRight:
      strut = {bottom, top, left, right};
      break;
  }
  if (direction == TextDirection::kRtl)
    std::swap(strut.inline_start, strut.inline_end);
  return strut;
}

LayoutUnit NGMarginStrut::BlockEndSum() const {
  return margin_block_end + negative_margin_block_end;
}

void NGMarginStrut::AppendMarginBlockStart(const LayoutUnit& value) {
  if (value < 0) {
    negative_margin_block_start =
        -std::max(value.abs(), negative_margin_block_start.abs());
  } else {
    margin_block_start = std::max(value, margin_block_start);
  }
}

void NGMarginStrut::AppendMarginBlockEnd(const LayoutUnit& value) {
  if (value < 0) {
    negative_margin_block_end =
        -std::max(value.abs(), negative_margin_block_end.abs());
  } else {
    margin_block_end = std::max(value, margin_block_end);
  }
}

void NGMarginStrut::SetMarginBlockStart(const LayoutUnit& value) {
  if (value < 0) {
    negative_margin_block_start = value;
  } else {
    margin_block_start = value;
  }
}

void NGMarginStrut::SetMarginBlockEnd(const LayoutUnit& value) {
  if (value < 0) {
    negative_margin_block_end = value;
  } else {
    margin_block_end = value;
  }
}

String NGMarginStrut::ToString() const {
  return String::format("Start: (%d %d) End: (%d %d)",
                        margin_block_start.toInt(), margin_block_end.toInt(),
                        negative_margin_block_start.toInt(),
                        negative_margin_block_end.toInt());
}

bool NGMarginStrut::IsEmpty() const {
  return *this == NGMarginStrut();
}

bool NGMarginStrut::operator==(const NGMarginStrut& other) const {
  return std::tie(other.margin_block_start, other.margin_block_end,
                  other.negative_margin_block_start,
                  other.negative_margin_block_end) ==
         std::tie(margin_block_start, margin_block_end,
                  negative_margin_block_start, negative_margin_block_end);
}

NGExclusions::NGExclusions()
    : last_left_float(nullptr), last_right_float(nullptr) {}

NGExclusions::NGExclusions(const NGExclusions& other) {
  for (const auto& exclusion : other.storage)
    Add(*exclusion);
}

void NGExclusions::Add(const NGExclusion& exclusion) {
  storage.push_back(WTF::makeUnique<NGExclusion>(exclusion));
  if (exclusion.type == NGExclusion::kFloatLeft) {
    last_left_float = storage.rbegin()->get();
  } else if (exclusion.type == NGExclusion::kFloatRight) {
    last_right_float = storage.rbegin()->get();
  }
}

inline NGExclusions& NGExclusions::operator=(const NGExclusions& other) {
  storage.clear();
  last_left_float = nullptr;
  last_right_float = nullptr;
  for (const auto& exclusion : other.storage)
    Add(*exclusion);
  return *this;
}

NGStaticPosition NGStaticPosition::Create(NGWritingMode writing_mode,
                                          TextDirection direction,
                                          NGPhysicalOffset offset) {
  NGStaticPosition position;
  position.offset = offset;
  switch (writing_mode) {
    case kHorizontalTopBottom:
      position.type = (direction == TextDirection::kLtr) ? kTopLeft : kTopRight;
      break;
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      position.type =
          (direction == TextDirection::kLtr) ? kTopRight : kBottomRight;
      break;
    case kVerticalLeftRight:
      position.type =
          (direction == TextDirection::kLtr) ? kTopLeft : kBottomLeft;
      break;
    case kSidewaysLeftRight:
      position.type =
          (direction == TextDirection::kLtr) ? kBottomLeft : kTopLeft;
      break;
  }
  return position;
}

}  // namespace blink
