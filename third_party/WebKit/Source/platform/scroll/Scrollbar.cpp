/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#include "platform/scroll/Scrollbar.h"

#include <algorithm>
#include "platform/HostWindow.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/WebGestureEvent.h"
#include "public/platform/WebMouseEvent.h"

namespace blink {

Scrollbar::Scrollbar(ScrollableArea* scrollableArea,
                     ScrollbarOrientation orientation,
                     ScrollbarControlSize controlSize,
                     HostWindow* hostWindow,
                     ScrollbarTheme* theme)
    : m_scrollableArea(scrollableArea),
      m_orientation(orientation),
      m_controlSize(controlSize),
      m_theme(theme ? *theme : ScrollbarTheme::theme()),
      m_hostWindow(hostWindow),
      m_visibleSize(0),
      m_totalSize(0),
      m_currentPos(0),
      m_dragOrigin(0),
      m_hoveredPart(NoPart),
      m_pressedPart(NoPart),
      m_pressedPos(0),
      m_scrollPos(0),
      m_draggingDocument(false),
      m_documentDragPos(0),
      m_enabled(true),
      m_scrollTimer(scrollableArea->getTimerTaskRunner(),
                    this,
                    &Scrollbar::autoscrollTimerFired),
      m_elasticOverscroll(0),
      m_trackNeedsRepaint(true),
      m_thumbNeedsRepaint(true) {
  m_theme.registerScrollbar(*this);

  // FIXME: This is ugly and would not be necessary if we fix cross-platform
  // code to actually query for scrollbar thickness and use it when sizing
  // scrollbars (rather than leaving one dimension of the scrollbar alone when
  // sizing).
  int thickness = m_theme.scrollbarThickness(controlSize);
  m_themeScrollbarThickness = thickness;
  if (m_hostWindow)
    thickness = m_hostWindow->windowToViewportScalar(thickness);
  FrameViewBase::setFrameRect(IntRect(0, 0, thickness, thickness));

  m_currentPos = scrollableAreaCurrentPos();
}

Scrollbar::~Scrollbar() {
  m_theme.unregisterScrollbar(*this);
}

DEFINE_TRACE(Scrollbar) {
  visitor->trace(m_scrollableArea);
  visitor->trace(m_hostWindow);
  FrameViewBase::trace(visitor);
}

void Scrollbar::setFrameRect(const IntRect& frameRect) {
  if (frameRect == this->frameRect())
    return;

  FrameViewBase::setFrameRect(frameRect);
  setNeedsPaintInvalidation(AllParts);
  if (m_scrollableArea)
    m_scrollableArea->scrollbarFrameRectChanged();
}

ScrollbarOverlayColorTheme Scrollbar::getScrollbarOverlayColorTheme() const {
  return m_scrollableArea ? m_scrollableArea->getScrollbarOverlayColorTheme()
                          : ScrollbarOverlayColorThemeDark;
}

void Scrollbar::getTickmarks(Vector<IntRect>& tickmarks) const {
  if (m_scrollableArea)
    m_scrollableArea->getTickmarks(tickmarks);
}

bool Scrollbar::isScrollableAreaActive() const {
  return m_scrollableArea && m_scrollableArea->isActive();
}

bool Scrollbar::isLeftSideVerticalScrollbar() const {
  if (m_orientation == VerticalScrollbar && m_scrollableArea)
    return m_scrollableArea->shouldPlaceVerticalScrollbarOnLeft();
  return false;
}

void Scrollbar::offsetDidChange() {
  ASSERT(m_scrollableArea);

  float position = scrollableAreaCurrentPos();
  if (position == m_currentPos)
    return;

  float oldPosition = m_currentPos;
  int oldThumbPosition = theme().thumbPosition(*this);
  m_currentPos = position;

  ScrollbarPart invalidParts =
      theme().invalidateOnThumbPositionChange(*this, oldPosition, position);
  setNeedsPaintInvalidation(invalidParts);

  if (m_pressedPart == ThumbPart)
    setPressedPos(m_pressedPos + theme().thumbPosition(*this) -
                  oldThumbPosition);
}

void Scrollbar::disconnectFromScrollableArea() {
  m_scrollableArea = nullptr;
}

void Scrollbar::setProportion(int visibleSize, int totalSize) {
  if (visibleSize == m_visibleSize && totalSize == m_totalSize)
    return;

  m_visibleSize = visibleSize;
  m_totalSize = totalSize;

  setNeedsPaintInvalidation(AllParts);
}

void Scrollbar::paint(GraphicsContext& context,
                      const CullRect& cullRect) const {
  if (!cullRect.intersectsCullRect(frameRect()))
    return;

  if (!theme().paint(*this, context, cullRect))
    FrameViewBase::paint(context, cullRect);
}

void Scrollbar::autoscrollTimerFired(TimerBase*) {
  autoscrollPressedPart(theme().autoscrollTimerDelay());
}

bool Scrollbar::thumbWillBeUnderMouse() const {
  int thumbPos = theme().trackPosition(*this) +
                 theme().thumbPosition(*this, scrollableAreaTargetPos());
  int thumbLength = theme().thumbLength(*this);
  return pressedPos() >= thumbPos && pressedPos() < thumbPos + thumbLength;
}

void Scrollbar::autoscrollPressedPart(double delay) {
  // Don't do anything for the thumb or if nothing was pressed.
  if (m_pressedPart == ThumbPart || m_pressedPart == NoPart)
    return;

  // Handle the track.
  if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) &&
      thumbWillBeUnderMouse()) {
    setHoveredPart(ThumbPart);
    return;
  }

  // Handle the arrows and track.
  if (m_scrollableArea &&
      m_scrollableArea
          ->userScroll(pressedPartScrollGranularity(),
                       toScrollDelta(pressedPartScrollDirectionPhysical(), 1))
          .didScroll())
    startTimerIfNeeded(delay);
}

void Scrollbar::startTimerIfNeeded(double delay) {
  // Don't do anything for the thumb.
  if (m_pressedPart == ThumbPart)
    return;

  // Handle the track.  We halt track scrolling once the thumb is level
  // with us.
  if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) &&
      thumbWillBeUnderMouse()) {
    setHoveredPart(ThumbPart);
    return;
  }

  // We can't scroll if we've hit the beginning or end.
  ScrollDirectionPhysical dir = pressedPartScrollDirectionPhysical();
  if (dir == ScrollUp || dir == ScrollLeft) {
    if (m_currentPos == 0)
      return;
  } else {
    if (m_currentPos == maximum())
      return;
  }

  m_scrollTimer.startOneShot(delay, BLINK_FROM_HERE);
}

void Scrollbar::stopTimerIfNeeded() {
  m_scrollTimer.stop();
}

ScrollDirectionPhysical Scrollbar::pressedPartScrollDirectionPhysical() {
  if (m_orientation == HorizontalScrollbar) {
    if (m_pressedPart == BackButtonStartPart ||
        m_pressedPart == BackButtonEndPart || m_pressedPart == BackTrackPart)
      return ScrollLeft;
    return ScrollRight;
  } else {
    if (m_pressedPart == BackButtonStartPart ||
        m_pressedPart == BackButtonEndPart || m_pressedPart == BackTrackPart)
      return ScrollUp;
    return ScrollDown;
  }
}

ScrollGranularity Scrollbar::pressedPartScrollGranularity() {
  if (m_pressedPart == BackButtonStartPart ||
      m_pressedPart == BackButtonEndPart ||
      m_pressedPart == ForwardButtonStartPart ||
      m_pressedPart == ForwardButtonEndPart)
    return ScrollByLine;
  return ScrollByPage;
}

void Scrollbar::moveThumb(int pos, bool draggingDocument) {
  if (!m_scrollableArea)
    return;

  int delta = pos - m_pressedPos;

  if (draggingDocument) {
    if (m_draggingDocument)
      delta = pos - m_documentDragPos;
    m_draggingDocument = true;
    ScrollOffset currentPosition =
        m_scrollableArea->scrollAnimator().currentOffset();
    float destinationPosition =
        (m_orientation == HorizontalScrollbar ? currentPosition.width()
                                              : currentPosition.height()) +
        delta;
    destinationPosition =
        m_scrollableArea->clampScrollOffset(m_orientation, destinationPosition);
    m_scrollableArea->setScrollOffsetSingleAxis(
        m_orientation, destinationPosition, UserScroll);
    m_documentDragPos = pos;
    return;
  }

  if (m_draggingDocument) {
    delta += m_pressedPos - m_documentDragPos;
    m_draggingDocument = false;
  }

  // Drag the thumb.
  int thumbPos = theme().thumbPosition(*this);
  int thumbLen = theme().thumbLength(*this);
  int trackLen = theme().trackLength(*this);
  ASSERT(thumbLen <= trackLen);
  if (thumbLen == trackLen)
    return;

  if (delta > 0)
    delta = std::min(trackLen - thumbLen - thumbPos, delta);
  else if (delta < 0)
    delta = std::max(-thumbPos, delta);

  float minOffset = m_scrollableArea->minimumScrollOffset(m_orientation);
  float maxOffset = m_scrollableArea->maximumScrollOffset(m_orientation);
  if (delta) {
    float newOffset = static_cast<float>(thumbPos + delta) *
                          (maxOffset - minOffset) / (trackLen - thumbLen) +
                      minOffset;
    m_scrollableArea->setScrollOffsetSingleAxis(m_orientation, newOffset,
                                                UserScroll);
  }
}

void Scrollbar::setHoveredPart(ScrollbarPart part) {
  if (part == m_hoveredPart)
    return;

  if (((m_hoveredPart == NoPart || part == NoPart) &&
       theme().invalidateOnMouseEnterExit())
      // When there's a pressed part, we don't draw a hovered state, so there's
      // no reason to invalidate.
      || m_pressedPart == NoPart)
    setNeedsPaintInvalidation(static_cast<ScrollbarPart>(m_hoveredPart | part));

  m_hoveredPart = part;
}

void Scrollbar::setPressedPart(ScrollbarPart part) {
  if (m_pressedPart != NoPart
      // When we no longer have a pressed part, we can start drawing a hovered
      // state on the hovered part.
      || m_hoveredPart != NoPart)
    setNeedsPaintInvalidation(
        static_cast<ScrollbarPart>(m_pressedPart | m_hoveredPart | part));

  if (getScrollableArea())
    getScrollableArea()->didScrollWithScrollbar(part, orientation());

  m_pressedPart = part;
}

bool Scrollbar::gestureEvent(const WebGestureEvent& evt,
                             bool* shouldUpdateCapture) {
  DCHECK(shouldUpdateCapture);
  switch (evt.type()) {
    case WebInputEvent::GestureTapDown: {
      IntPoint position = flooredIntPoint(evt.positionInRootFrame());
      setPressedPart(theme().hitTest(*this, position));
      m_pressedPos = orientation() == HorizontalScrollbar
                         ? convertFromRootFrame(position).x()
                         : convertFromRootFrame(position).y();
      *shouldUpdateCapture = true;
      return true;
    }
    case WebInputEvent::GestureTapCancel:
      if (m_pressedPart != ThumbPart)
        return false;
      m_scrollPos = m_pressedPos;
      return true;
    case WebInputEvent::GestureScrollBegin:
      switch (evt.sourceDevice) {
        case WebGestureDeviceTouchpad:
          // Update the state on GSB for touchpad since GestureTapDown
          // is not generated by that device. Touchscreen uses the tap down
          // gesture since the scrollbar enters a visual active state.
          *shouldUpdateCapture = true;
          setPressedPart(NoPart);
          m_pressedPos = 0;
          return true;
        case WebGestureDeviceTouchscreen:
          if (m_pressedPart != ThumbPart)
            return false;
          m_scrollPos = m_pressedPos;
          return true;
        default:
          ASSERT_NOT_REACHED();
          return true;
      }
      break;
    case WebInputEvent::GestureScrollUpdate:
      switch (evt.sourceDevice) {
        case WebGestureDeviceTouchpad: {
          FloatSize delta(-evt.deltaXInRootFrame(), -evt.deltaYInRootFrame());
          if (m_scrollableArea &&
              m_scrollableArea
                  ->userScroll(toPlatformScrollGranularity(evt.deltaUnits()),
                               delta)
                  .didScroll()) {
            return true;
          }
          return false;
        }
        case WebGestureDeviceTouchscreen:
          if (m_pressedPart != ThumbPart)
            return false;
          m_scrollPos += orientation() == HorizontalScrollbar
                             ? evt.deltaXInRootFrame()
                             : evt.deltaYInRootFrame();
          moveThumb(m_scrollPos, false);
          return true;
        default:
          ASSERT_NOT_REACHED();
          return true;
      }
      break;
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureFlingStart:
      m_scrollPos = 0;
      m_pressedPos = 0;
      setPressedPart(NoPart);
      return false;
    case WebInputEvent::GestureTap: {
      if (m_pressedPart != ThumbPart && m_pressedPart != NoPart &&
          m_scrollableArea &&
          m_scrollableArea
              ->userScroll(
                  pressedPartScrollGranularity(),
                  toScrollDelta(pressedPartScrollDirectionPhysical(), 1))
              .didScroll()) {
        return true;
      }
      m_scrollPos = 0;
      m_pressedPos = 0;
      setPressedPart(NoPart);
      return false;
    }
    default:
      // By default, we assume that gestures don't deselect the scrollbar.
      return true;
  }
}

void Scrollbar::mouseMoved(const WebMouseEvent& evt) {
  IntPoint position = flooredIntPoint(evt.positionInRootFrame());
  if (m_pressedPart == ThumbPart) {
    if (theme().shouldSnapBackToDragOrigin(*this, evt)) {
      if (m_scrollableArea) {
        m_scrollableArea->setScrollOffsetSingleAxis(
            m_orientation,
            m_dragOrigin + m_scrollableArea->minimumScrollOffset(m_orientation),
            UserScroll);
      }
    } else {
      moveThumb(m_orientation == HorizontalScrollbar
                    ? convertFromRootFrame(position).x()
                    : convertFromRootFrame(position).y(),
                theme().shouldDragDocumentInsteadOfThumb(*this, evt));
    }
    return;
  }

  if (m_pressedPart != NoPart) {
    m_pressedPos = orientation() == HorizontalScrollbar
                       ? convertFromRootFrame(position).x()
                       : convertFromRootFrame(position).y();
  }

  ScrollbarPart part = theme().hitTest(*this, position);
  if (part != m_hoveredPart) {
    if (m_pressedPart != NoPart) {
      if (part == m_pressedPart) {
        // The mouse is moving back over the pressed part.  We
        // need to start up the timer action again.
        startTimerIfNeeded(theme().autoscrollTimerDelay());
      } else if (m_hoveredPart == m_pressedPart) {
        // The mouse is leaving the pressed part.  Kill our timer
        // if needed.
        stopTimerIfNeeded();
      }
    }

    setHoveredPart(part);
  }

  return;
}

void Scrollbar::mouseEntered() {
  if (m_scrollableArea)
    m_scrollableArea->mouseEnteredScrollbar(*this);
}

void Scrollbar::mouseExited() {
  if (m_scrollableArea)
    m_scrollableArea->mouseExitedScrollbar(*this);
  setHoveredPart(NoPart);
}

void Scrollbar::mouseUp(const WebMouseEvent& mouseEvent) {
  bool isCaptured = m_pressedPart == ThumbPart;
  setPressedPart(NoPart);
  m_pressedPos = 0;
  m_draggingDocument = false;
  stopTimerIfNeeded();

  if (m_scrollableArea) {
    if (isCaptured)
      m_scrollableArea->mouseReleasedScrollbar();

    ScrollbarPart part = theme().hitTest(
        *this, flooredIntPoint(mouseEvent.positionInRootFrame()));
    if (part == NoPart) {
      setHoveredPart(NoPart);
      m_scrollableArea->mouseExitedScrollbar(*this);
    }
  }
}

void Scrollbar::mouseDown(const WebMouseEvent& evt) {
  // Early exit for right click
  if (evt.button == WebPointerProperties::Button::Right)
    return;

  IntPoint position = flooredIntPoint(evt.positionInRootFrame());
  setPressedPart(theme().hitTest(*this, position));
  int pressedPos = orientation() == HorizontalScrollbar
                       ? convertFromRootFrame(position).x()
                       : convertFromRootFrame(position).y();

  if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) &&
      theme().shouldCenterOnThumb(*this, evt)) {
    setHoveredPart(ThumbPart);
    setPressedPart(ThumbPart);
    m_dragOrigin = m_currentPos;
    int thumbLen = theme().thumbLength(*this);
    int desiredPos = pressedPos;
    // Set the pressed position to the middle of the thumb so that when we do
    // the move, the delta will be from the current pixel position of the thumb
    // to the new desired position for the thumb.
    m_pressedPos = theme().trackPosition(*this) + theme().thumbPosition(*this) +
                   thumbLen / 2;
    moveThumb(desiredPos);
    return;
  }
  if (m_pressedPart == ThumbPart) {
    m_dragOrigin = m_currentPos;
    if (m_scrollableArea)
      m_scrollableArea->mouseCapturedScrollbar();
  }

  m_pressedPos = pressedPos;

  autoscrollPressedPart(theme().initialAutoscrollTimerDelay());
}

void Scrollbar::setScrollbarsHidden(bool hidden) {
  if (m_scrollableArea)
    m_scrollableArea->setScrollbarsHidden(hidden);
}

void Scrollbar::setEnabled(bool e) {
  if (m_enabled == e)
    return;
  m_enabled = e;
  theme().updateEnabledState(*this);

  ScrollbarPart invalidParts = theme().invalidateOnEnabledChange();
  setNeedsPaintInvalidation(invalidParts);
}

int Scrollbar::scrollbarThickness() const {
  int thickness = orientation() == HorizontalScrollbar ? height() : width();
  if (!thickness || !m_hostWindow)
    return thickness;
  return m_hostWindow->windowToViewportScalar(m_themeScrollbarThickness);
}

bool Scrollbar::isOverlayScrollbar() const {
  return m_theme.usesOverlayScrollbars();
}

bool Scrollbar::shouldParticipateInHitTesting() {
  // Non-overlay scrollbars should always participate in hit testing.
  if (!isOverlayScrollbar())
    return true;
  return !m_scrollableArea->scrollbarsHidden();
}

bool Scrollbar::isWindowActive() const {
  return m_scrollableArea && m_scrollableArea->isActive();
}

IntRect Scrollbar::convertToContainingWidget(const IntRect& localRect) const {
  if (m_scrollableArea)
    return m_scrollableArea->convertFromScrollbarToContainingWidget(*this,
                                                                    localRect);

  return FrameViewBase::convertToContainingWidget(localRect);
}

IntRect Scrollbar::convertFromContainingWidget(
    const IntRect& parentRect) const {
  if (m_scrollableArea)
    return m_scrollableArea->convertFromContainingWidgetToScrollbar(*this,
                                                                    parentRect);

  return FrameViewBase::convertFromContainingWidget(parentRect);
}

IntPoint Scrollbar::convertToContainingWidget(
    const IntPoint& localPoint) const {
  if (m_scrollableArea)
    return m_scrollableArea->convertFromScrollbarToContainingWidget(*this,
                                                                    localPoint);

  return FrameViewBase::convertToContainingWidget(localPoint);
}

IntPoint Scrollbar::convertFromContainingWidget(
    const IntPoint& parentPoint) const {
  if (m_scrollableArea)
    return m_scrollableArea->convertFromContainingWidgetToScrollbar(
        *this, parentPoint);

  return FrameViewBase::convertFromContainingWidget(parentPoint);
}

float Scrollbar::scrollableAreaCurrentPos() const {
  if (!m_scrollableArea)
    return 0;

  if (m_orientation == HorizontalScrollbar) {
    return m_scrollableArea->getScrollOffset().width() -
           m_scrollableArea->minimumScrollOffset().width();
  }

  return m_scrollableArea->getScrollOffset().height() -
         m_scrollableArea->minimumScrollOffset().height();
}

float Scrollbar::scrollableAreaTargetPos() const {
  if (!m_scrollableArea)
    return 0;

  if (m_orientation == HorizontalScrollbar) {
    return m_scrollableArea->scrollAnimator().desiredTargetOffset().width() -
           m_scrollableArea->minimumScrollOffset().width();
  }

  return m_scrollableArea->scrollAnimator().desiredTargetOffset().height() -
         m_scrollableArea->minimumScrollOffset().height();
}

void Scrollbar::setNeedsPaintInvalidation(ScrollbarPart invalidParts) {
  if (m_theme.shouldRepaintAllPartsOnInvalidation())
    invalidParts = AllParts;
  if (invalidParts & ~ThumbPart)
    m_trackNeedsRepaint = true;
  if (invalidParts & ThumbPart)
    m_thumbNeedsRepaint = true;
  if (m_scrollableArea)
    m_scrollableArea->setScrollbarNeedsPaintInvalidation(orientation());
}

}  // namespace blink
