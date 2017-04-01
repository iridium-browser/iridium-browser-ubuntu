/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/layout/LayoutMedia.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/shadow/MediaControls.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"

namespace blink {

LayoutMedia::LayoutMedia(HTMLMediaElement* video) : LayoutImage(video) {
  setImageResource(LayoutImageResource::create());
}

LayoutMedia::~LayoutMedia() {}

HTMLMediaElement* LayoutMedia::mediaElement() const {
  return toHTMLMediaElement(node());
}

void LayoutMedia::layout() {
  LayoutSize oldSize = contentBoxRect().size();

  LayoutImage::layout();

  LayoutRect newRect = contentBoxRect();

  LayoutState state(*this);

  Optional<LayoutUnit> newPanelWidth;

// Iterate the children in reverse order so that the media controls are laid
// out before the text track container. This is to ensure that the text
// track rendering has an up-to-date position of the media controls for
// overlap checking, see LayoutVTTCue.
#if DCHECK_IS_ON()
  bool seenTextTrackContainer = false;
#endif
  for (LayoutObject* child = m_children.lastChild(); child;
       child = child->previousSibling()) {
#if DCHECK_IS_ON()
    if (child->node()->isMediaControls())
      ASSERT(!seenTextTrackContainer);
    else if (child->node()->isTextTrackContainer())
      seenTextTrackContainer = true;
    else
      ASSERT_NOT_REACHED();
#endif

    // TODO(mlamouri): we miss some layouts because needsLayout returns false in
    // some cases where we want to change the width of the controls because the
    // visible viewport has changed for example.
    if (newRect.size() == oldSize && !child->needsLayout())
      continue;

    LayoutUnit width = newRect.width();
    if (child->node()->isMediaControls()) {
      width = computePanelWidth(newRect);
      newPanelWidth = width;
    }

    LayoutBox* layoutBox = toLayoutBox(child);
    layoutBox->setLocation(newRect.location());
    // TODO(foolip): Remove the mutableStyleRef() and depend on CSS
    // width/height: inherit to match the media element size.
    layoutBox->mutableStyleRef().setHeight(Length(newRect.height(), Fixed));
    layoutBox->mutableStyleRef().setWidth(Length(width, Fixed));

    layoutBox->forceLayout();
  }

  clearNeedsLayout();

  // Notify our MediaControls that a layout has happened.
  if (mediaElement() && mediaElement()->mediaControls() &&
      newPanelWidth.has_value()) {
    if (!m_lastReportedPanelWidth.has_value() ||
        m_lastReportedPanelWidth.value() != newPanelWidth.value()) {
      mediaElement()->mediaControls()->notifyPanelWidthChanged(
          newPanelWidth.value());
      // Store the last value we reported, so we know if it has changed.
      m_lastReportedPanelWidth = newPanelWidth.value();
    }
  }
}

bool LayoutMedia::isChildAllowed(LayoutObject* child,
                                 const ComputedStyle&) const {
  // Two types of child layout objects are allowed: media controls
  // and the text track container. Filter children by node type.
  ASSERT(child->node());

  // The user agent stylesheet (mediaControls.css) has
  // ::-webkit-media-controls { display: flex; }. If author style
  // sets display: inline we would get an inline layoutObject as a child
  // of replaced content, which is not supposed to be possible. This
  // check can be removed if ::-webkit-media-controls is made
  // internal.
  if (child->node()->isMediaControls())
    return child->isFlexibleBox();

  if (child->node()->isTextTrackContainer())
    return true;

  return false;
}

void LayoutMedia::paintReplaced(const PaintInfo&, const LayoutPoint&) const {}

LayoutUnit LayoutMedia::computePanelWidth(const LayoutRect& mediaRect) const {
  // TODO(mlamouri): we don't know if the main frame has an horizontal scrollbar
  // if it is out of process. See https://crbug.com/662480
  if (document().page()->mainFrame()->isRemoteFrame())
    return mediaRect.width();

  // TODO(foolip): when going fullscreen, the animation sometimes does not clear
  // up properly and the last `absoluteXOffset` received is incorrect. This is
  // a shortcut that we could ideally avoid. See https://crbug.com/663680
  if (mediaElement() && mediaElement()->isFullscreen())
    return mediaRect.width();

  FrameHost* frameHost = document().frameHost();
  LocalFrame* mainFrame = document().page()->deprecatedLocalMainFrame();
  FrameView* pageView = mainFrame ? mainFrame->view() : nullptr;
  if (!frameHost || !mainFrame || !pageView)
    return mediaRect.width();

  if (pageView->horizontalScrollbarMode() != ScrollbarAlwaysOff)
    return mediaRect.width();

  // On desktop, this will include scrollbars when they stay visible.
  const LayoutUnit visibleWidth(frameHost->visualViewport().visibleWidth());
  const LayoutUnit absoluteXOffset(
      localToAbsolute(
          FloatPoint(mediaRect.location()),
          UseTransforms | ApplyContainerFlip | TraverseDocumentBoundaries)
          .x());
  const LayoutUnit newWidth = visibleWidth - absoluteXOffset;

  if (newWidth < 0)
    return mediaRect.width();

  return std::min(mediaRect.width(), visibleWidth - absoluteXOffset);
}

}  // namespace blink
