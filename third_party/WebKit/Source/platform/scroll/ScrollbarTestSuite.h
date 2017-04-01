// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollbarTestSuite_h
#define ScrollbarTestSuite_h

#include "platform/heap/GarbageCollected.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/scroll/ScrollbarThemeMock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class MockScrollableArea : public GarbageCollectedFinalized<MockScrollableArea>,
                           public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(MockScrollableArea);

 public:
  static MockScrollableArea* create() { return new MockScrollableArea(); }

  static MockScrollableArea* create(const ScrollOffset& maximumScrollOffset) {
    MockScrollableArea* mock = create();
    mock->setMaximumScrollOffset(maximumScrollOffset);
    return mock;
  }

  MOCK_CONST_METHOD0(visualRectForScrollbarParts, LayoutRect());
  MOCK_CONST_METHOD0(isActive, bool());
  MOCK_CONST_METHOD1(scrollSize, int(ScrollbarOrientation));
  MOCK_CONST_METHOD0(isScrollCornerVisible, bool());
  MOCK_CONST_METHOD0(scrollCornerRect, IntRect());
  MOCK_CONST_METHOD0(enclosingScrollableArea, ScrollableArea*());
  MOCK_CONST_METHOD1(visibleContentRect, IntRect(IncludeScrollbarsInRect));
  MOCK_CONST_METHOD0(contentsSize, IntSize());
  MOCK_CONST_METHOD0(scrollableAreaBoundingBox, IntRect());
  MOCK_CONST_METHOD0(layerForHorizontalScrollbar, GraphicsLayer*());
  MOCK_CONST_METHOD0(layerForVerticalScrollbar, GraphicsLayer*());
  MOCK_CONST_METHOD0(horizontalScrollbar, Scrollbar*());
  MOCK_CONST_METHOD0(verticalScrollbar, Scrollbar*());

  bool userInputScrollable(ScrollbarOrientation) const override { return true; }
  bool scrollbarsCanBeActive() const override { return true; }
  bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
  void updateScrollOffset(const ScrollOffset& offset, ScrollType) override {
    m_scrollOffset = offset.shrunkTo(m_maximumScrollOffset);
  }
  IntSize scrollOffsetInt() const override {
    return flooredIntSize(m_scrollOffset);
  }
  IntSize minimumScrollOffsetInt() const override { return IntSize(); }
  IntSize maximumScrollOffsetInt() const override {
    return expandedIntSize(m_maximumScrollOffset);
  }
  int visibleHeight() const override { return 768; }
  int visibleWidth() const override { return 1024; }
  bool scrollAnimatorEnabled() const override { return false; }
  int pageStep(ScrollbarOrientation) const override { return 0; }
  void scrollControlWasSetNeedsPaintInvalidation() {}

  using ScrollableArea::horizontalScrollbarNeedsPaintInvalidation;
  using ScrollableArea::verticalScrollbarNeedsPaintInvalidation;
  using ScrollableArea::clearNeedsPaintInvalidationForScrollControls;

  DEFINE_INLINE_VIRTUAL_TRACE() { ScrollableArea::trace(visitor); }

 protected:
  explicit MockScrollableArea() : m_maximumScrollOffset(ScrollOffset(0, 100)) {}
  explicit MockScrollableArea(const ScrollOffset& offset)
      : m_maximumScrollOffset(offset) {}

 private:
  void setMaximumScrollOffset(const ScrollOffset& maximumScrollOffset) {
    m_maximumScrollOffset = maximumScrollOffset;
  }

  ScrollOffset m_scrollOffset;
  ScrollOffset m_maximumScrollOffset;
};

}  // namespace blink

#endif
