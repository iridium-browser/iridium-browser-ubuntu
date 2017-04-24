/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutTextTrackContainer.h"

#include "core/frame/DeprecatedScheduleStyleRecalcDuringLayout.h"
#include "core/layout/LayoutVideo.h"

namespace blink {

LayoutTextTrackContainer::LayoutTextTrackContainer(Element* element)
    : LayoutBlockFlow(element), m_fontSize(0) {}

void LayoutTextTrackContainer::layout() {
  LayoutBlockFlow::layout();
  if (style()->display() == EDisplay::None)
    return;

  DeprecatedScheduleStyleRecalcDuringLayout marker(
      node()->document().lifecycle());

  LayoutObject* mediaLayoutObject = parent();
  if (!mediaLayoutObject || !mediaLayoutObject->isVideo())
    return;
  if (updateSizes(toLayoutVideo(*mediaLayoutObject)))
    toElement(node())->setInlineStyleProperty(
        CSSPropertyFontSize, m_fontSize, CSSPrimitiveValue::UnitType::Pixels);
}

bool LayoutTextTrackContainer::updateSizes(
    const LayoutVideo& videoLayoutObject) {
  // FIXME: The video size is used to calculate the font size (a workaround
  // for lack of per-spec vh/vw support) but the whole media element is used
  // for cue rendering. This is inconsistent. See also the somewhat related
  // spec bug: https://www.w3.org/Bugs/Public/show_bug.cgi?id=28105
  LayoutSize videoSize = videoLayoutObject.replacedContentRect().size();

  float smallestDimension =
      std::min(videoSize.height().toFloat(), videoSize.width().toFloat());

  float fontSize = smallestDimension * 0.05f;

  // Avoid excessive FP precision issue.
  // C11 5.2.4.2.2:9 requires assignment and cast to remove extra precision, but
  // the behavior is currently not portable. fontSize may have precision higher
  // than m_fontSize thus straight comparison can fail despite they cast to the
  // same float value.
  volatile float& currentFontSize = m_fontSize;
  float oldFontSize = currentFontSize;
  currentFontSize = fontSize;
  return currentFontSize != oldFontSize;
}

}  // namespace blink
