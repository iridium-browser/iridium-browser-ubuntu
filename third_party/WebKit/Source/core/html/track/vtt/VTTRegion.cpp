/*
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/track/vtt/VTTRegion.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ClientRect.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/track/vtt/VTTParser.h"
#include "core/html/track/vtt/VTTScanner.h"
#include "public/platform/Platform.h"
#include "wtf/MathExtras.h"

#define VTT_LOG_LEVEL 3

namespace blink {

// The following values default values are defined within the WebVTT Regions
// Spec.
// https://dvcs.w3.org/hg/text-tracks/raw-file/default/608toVTT/region.html

// The region occupies by default 100% of the width of the video viewport.
static const float defaultWidth = 100;

// The region has, by default, 3 lines of text.
static const int defaultHeightInLines = 3;

// The region and viewport are anchored in the bottom left corner.
static const float defaultAnchorPointX = 0;
static const float defaultAnchorPointY = 100;

// The region doesn't have scrolling text, by default.
static const bool defaultScroll = false;

// Default region line-height (vh units)
static const float lineHeight = 5.33;

// Default scrolling animation time period (s).
static const float scrollTime = 0.433;

static bool isNonPercentage(double value,
                            const char* method,
                            ExceptionState& exceptionState) {
  if (value < 0 || value > 100) {
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange(
            "value", value, 0.0, ExceptionMessages::InclusiveBound, 100.0,
            ExceptionMessages::InclusiveBound));
    return true;
  }
  return false;
}

VTTRegion::VTTRegion()
    : m_id(emptyString),
      m_width(defaultWidth),
      m_lines(defaultHeightInLines),
      m_regionAnchor(FloatPoint(defaultAnchorPointX, defaultAnchorPointY)),
      m_viewportAnchor(FloatPoint(defaultAnchorPointX, defaultAnchorPointY)),
      m_scroll(defaultScroll),
      m_currentTop(0),
      m_scrollTimer(Platform::current()->currentThread()->getWebTaskRunner(),
                    this,
                    &VTTRegion::scrollTimerFired) {}

VTTRegion::~VTTRegion() {}

void VTTRegion::setId(const String& id) {
  m_id = id;
}

void VTTRegion::setWidth(double value, ExceptionState& exceptionState) {
  if (isNonPercentage(value, "width", exceptionState))
    return;

  m_width = value;
}

void VTTRegion::setLines(int value, ExceptionState& exceptionState) {
  if (value < 0) {
    exceptionState.throwDOMException(
        IndexSizeError,
        "The height provided (" + String::number(value) + ") is negative.");
    return;
  }
  m_lines = value;
}

void VTTRegion::setRegionAnchorX(double value, ExceptionState& exceptionState) {
  if (isNonPercentage(value, "regionAnchorX", exceptionState))
    return;

  m_regionAnchor.setX(value);
}

void VTTRegion::setRegionAnchorY(double value, ExceptionState& exceptionState) {
  if (isNonPercentage(value, "regionAnchorY", exceptionState))
    return;

  m_regionAnchor.setY(value);
}

void VTTRegion::setViewportAnchorX(double value,
                                   ExceptionState& exceptionState) {
  if (isNonPercentage(value, "viewportAnchorX", exceptionState))
    return;

  m_viewportAnchor.setX(value);
}

void VTTRegion::setViewportAnchorY(double value,
                                   ExceptionState& exceptionState) {
  if (isNonPercentage(value, "viewportAnchorY", exceptionState))
    return;

  m_viewportAnchor.setY(value);
}

const AtomicString VTTRegion::scroll() const {
  DEFINE_STATIC_LOCAL(const AtomicString, upScrollValueKeyword, ("up"));
  return m_scroll ? upScrollValueKeyword : emptyAtom;
}

void VTTRegion::setScroll(const AtomicString& value) {
  DCHECK(value == "up" || value == emptyAtom);
  m_scroll = value != emptyAtom;
}

void VTTRegion::setRegionSettings(const String& inputString) {
  VTTScanner input(inputString);

  while (!input.isAtEnd()) {
    input.skipWhile<VTTParser::isValidSettingDelimiter>();

    if (input.isAtEnd())
      break;

    // Scan the name part.
    RegionSetting name = scanSettingName(input);

    // Verify that we're looking at a '='.
    if (name == None || !input.scan('=')) {
      input.skipUntil<VTTParser::isASpace>();
      continue;
    }

    // Scan the value part.
    parseSettingValue(name, input);
  }
}

VTTRegion::RegionSetting VTTRegion::scanSettingName(VTTScanner& input) {
  if (input.scan("id"))
    return Id;
  if (input.scan("height"))
    return Height;
  if (input.scan("width"))
    return Width;
  if (input.scan("viewportanchor"))
    return ViewportAnchor;
  if (input.scan("regionanchor"))
    return RegionAnchor;
  if (input.scan("scroll"))
    return Scroll;

  return None;
}

static inline bool parsedEntireRun(const VTTScanner& input,
                                   const VTTScanner::Run& run) {
  return input.isAt(run.end());
}

void VTTRegion::parseSettingValue(RegionSetting setting, VTTScanner& input) {
  DEFINE_STATIC_LOCAL(const AtomicString, scrollUpValueKeyword, ("up"));

  VTTScanner::Run valueRun = input.collectUntil<VTTParser::isASpace>();

  switch (setting) {
    case Id: {
      String stringValue = input.extractString(valueRun);
      if (stringValue.find("-->") == kNotFound)
        m_id = stringValue;
      break;
    }
    case Width: {
      float floatWidth;
      if (VTTParser::parseFloatPercentageValue(input, floatWidth) &&
          parsedEntireRun(input, valueRun))
        m_width = floatWidth;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid Width";
      break;
    }
    case Height: {
      int number;
      if (input.scanDigits(number) && parsedEntireRun(input, valueRun))
        m_lines = number;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid Height";
      break;
    }
    case RegionAnchor: {
      FloatPoint anchor;
      if (VTTParser::parseFloatPercentageValuePair(input, ',', anchor) &&
          parsedEntireRun(input, valueRun))
        m_regionAnchor = anchor;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid RegionAnchor";
      break;
    }
    case ViewportAnchor: {
      FloatPoint anchor;
      if (VTTParser::parseFloatPercentageValuePair(input, ',', anchor) &&
          parsedEntireRun(input, valueRun))
        m_viewportAnchor = anchor;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid ViewportAnchor";
      break;
    }
    case Scroll:
      if (input.scanRun(valueRun, scrollUpValueKeyword))
        m_scroll = true;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid Scroll";
      break;
    case None:
      break;
  }

  input.skipRun(valueRun);
}

const AtomicString& VTTRegion::textTrackCueContainerScrollingClass() {
  DEFINE_STATIC_LOCAL(const AtomicString, trackRegionCueContainerScrollingClass,
                      ("scrolling"));

  return trackRegionCueContainerScrollingClass;
}

HTMLDivElement* VTTRegion::getDisplayTree(Document& document) {
  if (!m_regionDisplayTree) {
    m_regionDisplayTree = HTMLDivElement::create(document);
    prepareRegionDisplayTree();
  }

  return m_regionDisplayTree;
}

void VTTRegion::willRemoveVTTCueBox(VTTCueBox* box) {
  DVLOG(VTT_LOG_LEVEL) << "willRemoveVTTCueBox";
  DCHECK(m_cueContainer->contains(box));

  double boxHeight = box->getBoundingClientRect()->height();

  m_cueContainer->classList().remove(textTrackCueContainerScrollingClass(),
                                     ASSERT_NO_EXCEPTION);

  m_currentTop += boxHeight;
  m_cueContainer->setInlineStyleProperty(CSSPropertyTop, m_currentTop,
                                         CSSPrimitiveValue::UnitType::Pixels);
}

void VTTRegion::appendVTTCueBox(VTTCueBox* displayBox) {
  DCHECK(m_cueContainer);

  if (m_cueContainer->contains(displayBox))
    return;

  m_cueContainer->appendChild(displayBox);
  displayLastVTTCueBox();
}

void VTTRegion::displayLastVTTCueBox() {
  DVLOG(VTT_LOG_LEVEL) << "displayLastVTTCueBox";
  DCHECK(m_cueContainer);

  // FIXME: This should not be causing recalc styles in a loop to set the "top"
  // css property to move elements. We should just scroll the text track cues on
  // the compositor with an animation.

  if (m_scrollTimer.isActive())
    return;

  // If it's a scrolling region, add the scrolling class.
  if (isScrollingRegion())
    m_cueContainer->classList().add(textTrackCueContainerScrollingClass(),
                                    ASSERT_NO_EXCEPTION);

  float regionBottom = m_regionDisplayTree->getBoundingClientRect()->bottom();

  // Find first cue that is not entirely displayed and scroll it upwards.
  for (Element& child : ElementTraversal::childrenOf(*m_cueContainer)) {
    ClientRect* clientRect = child.getBoundingClientRect();
    float childBottom = clientRect->bottom();

    if (regionBottom >= childBottom)
      continue;

    m_currentTop -= std::min(clientRect->height(), childBottom - regionBottom);
    m_cueContainer->setInlineStyleProperty(CSSPropertyTop, m_currentTop,
                                           CSSPrimitiveValue::UnitType::Pixels);

    startTimer();
    break;
  }
}

void VTTRegion::prepareRegionDisplayTree() {
  DCHECK(m_regionDisplayTree);

  // 7.2 Prepare region CSS boxes

  // FIXME: Change the code below to use viewport units when
  // http://crbug/244618 is fixed.

  // Let regionWidth be the text track region width.
  // Let width be 'regionWidth vw' ('vw' is a CSS unit)
  m_regionDisplayTree->setInlineStyleProperty(
      CSSPropertyWidth, m_width, CSSPrimitiveValue::UnitType::Percentage);

  // Let lineHeight be '0.0533vh' ('vh' is a CSS unit) and regionHeight be
  // the text track region height. Let height be 'lineHeight' multiplied
  // by regionHeight.
  double height = lineHeight * m_lines;
  m_regionDisplayTree->setInlineStyleProperty(
      CSSPropertyHeight, height, CSSPrimitiveValue::UnitType::ViewportHeight);

  // Let viewportAnchorX be the x dimension of the text track region viewport
  // anchor and regionAnchorX be the x dimension of the text track region
  // anchor. Let leftOffset be regionAnchorX multiplied by width divided by
  // 100.0. Let left be leftOffset subtracted from 'viewportAnchorX vw'.
  double leftOffset = m_regionAnchor.x() * m_width / 100;
  m_regionDisplayTree->setInlineStyleProperty(
      CSSPropertyLeft, m_viewportAnchor.x() - leftOffset,
      CSSPrimitiveValue::UnitType::Percentage);

  // Let viewportAnchorY be the y dimension of the text track region viewport
  // anchor and regionAnchorY be the y dimension of the text track region
  // anchor. Let topOffset be regionAnchorY multiplied by height divided by
  // 100.0. Let top be topOffset subtracted from 'viewportAnchorY vh'.
  double topOffset = m_regionAnchor.y() * height / 100;
  m_regionDisplayTree->setInlineStyleProperty(
      CSSPropertyTop, m_viewportAnchor.y() - topOffset,
      CSSPrimitiveValue::UnitType::Percentage);

  // The cue container is used to wrap the cues and it is the object which is
  // gradually scrolled out as multiple cues are appended to the region.
  m_cueContainer = HTMLDivElement::create(m_regionDisplayTree->document());
  m_cueContainer->setInlineStyleProperty(CSSPropertyTop, 0.0,
                                         CSSPrimitiveValue::UnitType::Pixels);

  m_cueContainer->setShadowPseudoId(
      AtomicString("-webkit-media-text-track-region-container"));
  m_regionDisplayTree->appendChild(m_cueContainer);

  // 7.5 Every WebVTT region object is initialised with the following CSS
  m_regionDisplayTree->setShadowPseudoId(
      AtomicString("-webkit-media-text-track-region"));
}

void VTTRegion::startTimer() {
  DVLOG(VTT_LOG_LEVEL) << "startTimer";

  if (m_scrollTimer.isActive())
    return;

  double duration = isScrollingRegion() ? scrollTime : 0;
  m_scrollTimer.startOneShot(duration, BLINK_FROM_HERE);
}

void VTTRegion::stopTimer() {
  DVLOG(VTT_LOG_LEVEL) << "stopTimer";
  m_scrollTimer.stop();
}

void VTTRegion::scrollTimerFired(TimerBase*) {
  DVLOG(VTT_LOG_LEVEL) << "scrollTimerFired";

  stopTimer();
  displayLastVTTCueBox();
}

DEFINE_TRACE(VTTRegion) {
  visitor->trace(m_cueContainer);
  visitor->trace(m_regionDisplayTree);
}

}  // namespace blink
