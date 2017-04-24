/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameViewBase_h
#define FrameViewBase_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFocusType.h"
#include "wtf/Forward.h"

namespace blink {

class CullRect;
class Event;
class GraphicsContext;

// The FrameViewBase class serves as a base class for FrameView, Scrollbar, and
// PluginView.
//
// FrameViewBases are connected in a hierarchy, with the restriction that
// plugins and scrollbars are always leaves of the tree. Only FrameView can have
// children (and therefore the FrameViewBase class has no concept of children).
class PLATFORM_EXPORT FrameViewBase
    : public GarbageCollectedFinalized<FrameViewBase> {
 public:
  FrameViewBase();
  virtual ~FrameViewBase();

  int x() const { return frameRect().x(); }
  int y() const { return frameRect().y(); }
  int width() const { return frameRect().width(); }
  int height() const { return frameRect().height(); }
  IntSize size() const { return frameRect().size(); }
  IntPoint location() const { return frameRect().location(); }

  virtual void setFrameRect(const IntRect& frameRect) {
    m_frameRect = frameRect;
  }
  const IntRect& frameRect() const { return m_frameRect; }
  IntRect boundsRect() const { return IntRect(0, 0, width(), height()); }

  void resize(int w, int h) { setFrameRect(IntRect(x(), y(), w, h)); }
  void resize(const IntSize& s) { setFrameRect(IntRect(location(), s)); }

  virtual void paint(GraphicsContext&, const CullRect&) const {}
  void invalidate() { invalidateRect(boundsRect()); }
  virtual void invalidateRect(const IntRect&) = 0;

  virtual void setFocused(bool, WebFocusType) {}

  virtual void show() {}
  virtual void hide() {}
  bool isSelfVisible() const {
    return m_selfVisible;
  }  // Whether or not we have been explicitly marked as visible or not.
  bool isParentVisible() const {
    return m_parentVisible;
  }  // Whether or not our parent is visible.
  bool isVisible() const {
    return m_selfVisible && m_parentVisible;
  }  // Whether or not we are actually visible.
  virtual void setParentVisible(bool visible) { m_parentVisible = visible; }
  void setSelfVisible(bool v) { m_selfVisible = v; }

  virtual bool isFrameView() const { return false; }
  virtual bool isRemoteFrameView() const { return false; }
  virtual bool isPluginView() const { return false; }
  virtual bool isPluginContainer() const { return false; }
  virtual bool isScrollbar() const { return false; }

  virtual void setParent(FrameViewBase*);
  FrameViewBase* parent() const { return m_parent; }
  FrameViewBase* root() const;

  virtual void handleEvent(Event*) {}

  IntRect convertToRootFrame(const IntRect&) const;
  IntRect convertFromRootFrame(const IntRect&) const;

  IntPoint convertToRootFrame(const IntPoint&) const;
  IntPoint convertFromRootFrame(const IntPoint&) const;
  FloatPoint convertFromRootFrame(const FloatPoint&) const;

  virtual void frameRectsChanged() {}

  virtual void widgetGeometryMayHaveChanged() {}

  virtual IntRect convertToContainingWidget(const IntRect&) const;
  virtual IntRect convertFromContainingWidget(const IntRect&) const;
  virtual IntPoint convertToContainingWidget(const IntPoint&) const;
  virtual IntPoint convertFromContainingWidget(const IntPoint&) const;

  // Virtual methods to convert points to/from child frameviewbases.
  virtual IntPoint convertChildToSelf(const FrameViewBase*,
                                      const IntPoint&) const;
  virtual IntPoint convertSelfToChild(const FrameViewBase*,
                                      const IntPoint&) const;

  // Notifies this frameviewbase that it will no longer be receiving events.
  virtual void eventListenersRemoved() {}

  DECLARE_VIRTUAL_TRACE();
  virtual void dispose() {}

 private:
  Member<FrameViewBase> m_parent;
  IntRect m_frameRect;
  bool m_selfVisible;
  bool m_parentVisible;
};

}  // namespace blink

#endif  // FrameViewBase_h
