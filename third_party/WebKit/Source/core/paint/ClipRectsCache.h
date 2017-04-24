// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRectsCache_h
#define ClipRectsCache_h

#include "core/paint/ClipRects.h"

#if DCHECK_IS_ON()
#include "platform/scroll/ScrollTypes.h"  // For OverlayScrollbarClipBehavior.
#endif

namespace blink {

class PaintLayer;

enum ClipRectsCacheSlot {
  // Relative to the ancestor treated as the root (e.g. transformed layer).
  // Used for hit testing.
  RootRelativeClipRects,
  RootRelativeClipRectsIgnoringViewportClip,

  // Relative to the LayoutView's layer. Used for compositing overlap testing.
  AbsoluteClipRects,

  // Relative to painting ancestor. Used for painting.
  PaintingClipRects,
  PaintingClipRectsIgnoringOverflowClip,

  NumberOfClipRectsCacheSlots,
  UncachedClipRects,
};

class ClipRectsCache {
  USING_FAST_MALLOC(ClipRectsCache);

 public:
  struct Entry {
    Entry()
        : root(nullptr)
#if DCHECK_IS_ON()
          ,
          overlayScrollbarClipBehavior(IgnoreOverlayScrollbarSize)
#endif
    {
    }
    const PaintLayer* root;
    RefPtr<ClipRects> clipRects;
#if DCHECK_IS_ON()
    OverlayScrollbarClipBehavior overlayScrollbarClipBehavior;
#endif
  };
  Entry& get(ClipRectsCacheSlot slot) {
    DCHECK(slot < NumberOfClipRectsCacheSlots);
    return m_entries[slot];
  }
  void clear(ClipRectsCacheSlot slot) {
    DCHECK(slot < NumberOfClipRectsCacheSlots);
    m_entries[slot] = Entry();
  }

 private:
  Entry m_entries[NumberOfClipRectsCacheSlots];
};

}  // namespace blink

#endif  // ClipRectsCache_h
