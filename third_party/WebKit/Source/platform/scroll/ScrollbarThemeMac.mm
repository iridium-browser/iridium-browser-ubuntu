/*
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
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

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/scroll/ScrollbarThemeMac.h"

#include <Carbon/Carbon.h>
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/mac/ColorMac.h"
#include "platform/mac/LocalCurrentGraphicsContext.h"
#include "platform/mac/NSScrollerImpDetails.h"
#include "platform/mac/ScrollAnimatorMac.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "public/platform/WebMouseEvent.h"
#include "public/platform/WebThemeEngine.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "skia/ext/skia_utils_mac.h"
#include "wtf/HashSet.h"
#include "wtf/RetainPtr.h"
#include "wtf/StdLibExtras.h"

// FIXME: There are repainting problems due to Aqua scroll bar buttons' visual
// overflow.

using namespace blink;

@interface NSColor (WebNSColorDetails)
+ (NSImage*)_linenPatternImage;
@end

@interface BlinkScrollbarObserver : NSObject {
  blink::ScrollbarThemeClient* _scrollbar;
  RetainPtr<ScrollbarPainter> _scrollbarPainter;
}
- (id)initWithScrollbar:(blink::ScrollbarThemeClient*)scrollbar
                painter:(const RetainPtr<ScrollbarPainter>&)painter;
@end

@implementation BlinkScrollbarObserver

- (id)initWithScrollbar:(blink::ScrollbarThemeClient*)scrollbar
                painter:(const RetainPtr<ScrollbarPainter>&)painter {
  if (!(self = [super init]))
    return nil;
  _scrollbar = scrollbar;
  _scrollbarPainter = painter;
  [_scrollbarPainter.get() addObserver:self
                            forKeyPath:@"knobAlpha"
                               options:0
                               context:nil];
  return self;
}

- (id)painter {
  return _scrollbarPainter.get();
}

- (void)dealloc {
  [_scrollbarPainter.get() removeObserver:self forKeyPath:@"knobAlpha"];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"knobAlpha"]) {
    BOOL visible = [_scrollbarPainter.get() knobAlpha] > 0;
    _scrollbar->setScrollbarsHidden(!visible);
  }
}

@end

namespace blink {

typedef HashSet<ScrollbarThemeClient*> ScrollbarSet;

static ScrollbarSet& scrollbarSet() {
  DEFINE_STATIC_LOCAL(ScrollbarSet, set, ());
  return set;
}

static float gInitialButtonDelay = 0.5f;
static float gAutoscrollButtonDelay = 0.05f;
static NSScrollerStyle gPreferredScrollerStyle = NSScrollerStyleLegacy;

typedef HashMap<ScrollbarThemeClient*, RetainPtr<BlinkScrollbarObserver>>
    ScrollbarPainterMap;

static ScrollbarPainterMap& scrollbarPainterMap() {
  static ScrollbarPainterMap* map = new ScrollbarPainterMap;
  return *map;
}

static bool supportsExpandedScrollbars() {
  // FIXME: This is temporary until all platforms that support ScrollbarPainter
  // support this part of the API.
  static bool globalSupportsExpandedScrollbars =
      [NSClassFromString(@"NSScrollerImp")
          instancesRespondToSelector:@selector(setExpanded:)];
  return globalSupportsExpandedScrollbars;
}

ScrollbarTheme& ScrollbarTheme::nativeTheme() {
  DEFINE_STATIC_LOCAL(ScrollbarThemeMac, overlayTheme, ());
  return overlayTheme;
}

void ScrollbarThemeMac::paintTickmarks(GraphicsContext& context,
                                       const Scrollbar& scrollbar,
                                       const IntRect& rect) {
  IntRect tickmarkTrackRect = rect;
  tickmarkTrackRect.setX(tickmarkTrackRect.x() + 1);
  tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 1);
  ScrollbarTheme::paintTickmarks(context, scrollbar, tickmarkTrackRect);
}

ScrollbarThemeMac::~ScrollbarThemeMac() {}

void ScrollbarThemeMac::preferencesChanged(
    float initialButtonDelay,
    float autoscrollButtonDelay,
    NSScrollerStyle preferredScrollerStyle,
    bool redraw,
    WebScrollbarButtonsPlacement buttonPlacement) {
  updateButtonPlacement(buttonPlacement);
  gInitialButtonDelay = initialButtonDelay;
  gAutoscrollButtonDelay = autoscrollButtonDelay;
  gPreferredScrollerStyle = preferredScrollerStyle;
  if (redraw && !scrollbarSet().isEmpty()) {
    ScrollbarSet::iterator end = scrollbarSet().end();
    for (ScrollbarSet::iterator it = scrollbarSet().begin(); it != end; ++it) {
      (*it)->styleChanged();
      (*it)->invalidate();
    }
  }
}

double ScrollbarThemeMac::initialAutoscrollTimerDelay() {
  return gInitialButtonDelay;
}

double ScrollbarThemeMac::autoscrollTimerDelay() {
  return gAutoscrollButtonDelay;
}

bool ScrollbarThemeMac::shouldDragDocumentInsteadOfThumb(
    const ScrollbarThemeClient&,
    const WebMouseEvent& event) {
  return (event.modifiers() & WebInputEvent::Modifiers::AltKey) != 0;
}

int ScrollbarThemeMac::scrollbarPartToHIPressedState(ScrollbarPart part) {
  switch (part) {
    case BackButtonStartPart:
      return kThemeTopOutsideArrowPressed;
    case BackButtonEndPart:
      // This does not make much sense.  For some reason the outside constant
      // is required.
      return kThemeTopOutsideArrowPressed;
    case ForwardButtonStartPart:
      return kThemeTopInsideArrowPressed;
    case ForwardButtonEndPart:
      return kThemeBottomOutsideArrowPressed;
    case ThumbPart:
      return kThemeThumbPressed;
    default:
      return 0;
  }
}

ScrollbarPart ScrollbarThemeMac::invalidateOnThumbPositionChange(
    const ScrollbarThemeClient& scrollbar,
    float oldPosition,
    float newPosition) const {
  // ScrollAnimatorMac will invalidate scrollbar parts if necessary.
  return NoPart;
}

void ScrollbarThemeMac::registerScrollbar(ScrollbarThemeClient& scrollbar) {
  scrollbarSet().insert(&scrollbar);

  bool isHorizontal = scrollbar.orientation() == HorizontalScrollbar;
  RetainPtr<ScrollbarPainter> scrollbarPainter(
      AdoptNS, [[NSClassFromString(@"NSScrollerImp")
                   scrollerImpWithStyle:recommendedScrollerStyle()
                            controlSize:(NSControlSize)scrollbar.controlSize()
                             horizontal:isHorizontal
                   replacingScrollerImp:nil] retain]);
  RetainPtr<BlinkScrollbarObserver> observer(
      AdoptNS,
      [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar
                                                painter:scrollbarPainter]);

  scrollbarPainterMap().insert(&scrollbar, observer);
  updateEnabledState(scrollbar);
  updateScrollbarOverlayColorTheme(scrollbar);
}

void ScrollbarThemeMac::unregisterScrollbar(ScrollbarThemeClient& scrollbar) {
  scrollbarPainterMap().remove(&scrollbar);
  scrollbarSet().erase(&scrollbar);
}

void ScrollbarThemeMac::setNewPainterForScrollbar(
    ScrollbarThemeClient& scrollbar,
    ScrollbarPainter newPainter) {
  RetainPtr<ScrollbarPainter> scrollbarPainter(AdoptNS, [newPainter retain]);
  RetainPtr<BlinkScrollbarObserver> observer(
      AdoptNS,
      [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar
                                                painter:scrollbarPainter]);
  scrollbarPainterMap().set(&scrollbar, observer);
  updateEnabledState(scrollbar);
  updateScrollbarOverlayColorTheme(scrollbar);
}

ScrollbarPainter ScrollbarThemeMac::painterForScrollbar(
    const ScrollbarThemeClient& scrollbar) const {
  return [scrollbarPainterMap()
              .at(const_cast<ScrollbarThemeClient*>(&scrollbar))
              .get() painter];
}

void ScrollbarThemeMac::paintTrackBackground(GraphicsContext& context,
                                             const Scrollbar& scrollbar,
                                             const IntRect& rect) {
  if (DrawingRecorder::useCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarTrackBackground))
    return;

  DrawingRecorder recorder(context, scrollbar,
                           DisplayItem::kScrollbarTrackBackground, rect);

  GraphicsContextStateSaver stateSaver(context);
  context.translate(rect.x(), rect.y());
  LocalCurrentGraphicsContext localContext(context,
                                           IntRect(IntPoint(), rect.size()));

  CGRect frameRect = scrollbar.frameRect();
  ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
  [scrollbarPainter setEnabled:scrollbar.enabled()];
  [scrollbarPainter setBoundsSize:NSSizeFromCGSize(frameRect.size)];
  NSRect trackRect =
      NSMakeRect(0, 0, frameRect.size.width, frameRect.size.height);
  [scrollbarPainter drawKnobSlotInRect:trackRect highlight:NO];
}

void ScrollbarThemeMac::paintThumb(GraphicsContext& context,
                                   const Scrollbar& scrollbar,
                                   const IntRect& rect) {
  if (DrawingRecorder::useCachedDrawingIfPossible(context, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  // Expand dirty rect to allow for scroll thumb anti-aliasing in minimum thumb
  // size case.
  IntRect dirtyRect = IntRect(rect);
  dirtyRect.inflate(1);
  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb,
                           dirtyRect);

  GraphicsContextStateSaver stateSaver(context);
  context.translate(rect.x(), rect.y());
  LocalCurrentGraphicsContext localContext(context,
                                           IntRect(IntPoint(), rect.size()));

  ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
  [scrollbarPainter setEnabled:scrollbar.enabled()];
  // drawKnob aligns the thumb to right side of the draw rect.
  // If the vertical overlay scrollbar is on the left, use trackWidth instead
  // of scrollbar width, to avoid the gap on the left side of the thumb.
  IntRect drawRect = IntRect(rect);
  if (usesOverlayScrollbars() && scrollbar.isLeftSideVerticalScrollbar()) {
    int thumbWidth = [scrollbarPainter trackWidth];
    drawRect.setWidth(thumbWidth);
  }
  [scrollbarPainter setBoundsSize:NSSizeFromCGSize(drawRect.size())];

  [scrollbarPainter setDoubleValue:0];
  [scrollbarPainter setKnobProportion:1];

  CGFloat oldKnobAlpha = [scrollbarPainter knobAlpha];
  [scrollbarPainter setKnobAlpha:1];

  if (scrollbar.enabled())
    [scrollbarPainter drawKnob];

  // If this state is not set, then moving the cursor over the scrollbar area
  // will only cause the scrollbar to engorge when moved over the top of the
  // scrollbar area.
  [scrollbarPainter
      setBoundsSize:NSSizeFromCGSize(scrollbar.frameRect().size())];
  [scrollbarPainter setKnobAlpha:oldKnobAlpha];
}

int ScrollbarThemeMac::scrollbarThickness(ScrollbarControlSize controlSize) {
  NSControlSize nsControlSize = static_cast<NSControlSize>(controlSize);
  ScrollbarPainter scrollbarPainter = [NSClassFromString(@"NSScrollerImp")
      scrollerImpWithStyle:recommendedScrollerStyle()
               controlSize:nsControlSize
                horizontal:NO
      replacingScrollerImp:nil];
  BOOL wasExpanded = NO;
  if (supportsExpandedScrollbars()) {
    wasExpanded = [scrollbarPainter isExpanded];
    [scrollbarPainter setExpanded:YES];
  }
  int thickness = [scrollbarPainter trackBoxWidth];
  if (supportsExpandedScrollbars())
    [scrollbarPainter setExpanded:wasExpanded];
  return thickness;
}

bool ScrollbarThemeMac::usesOverlayScrollbars() const {
  return recommendedScrollerStyle() == NSScrollerStyleOverlay;
}

void ScrollbarThemeMac::updateScrollbarOverlayColorTheme(
    const ScrollbarThemeClient& scrollbar) {
  ScrollbarPainter painter = painterForScrollbar(scrollbar);
  switch (scrollbar.getScrollbarOverlayColorTheme()) {
    case ScrollbarOverlayColorThemeDark:
      [painter setKnobStyle:NSScrollerKnobStyleDark];
      break;
    case ScrollbarOverlayColorThemeLight:
      [painter setKnobStyle:NSScrollerKnobStyleLight];
      break;
  }
}

WebScrollbarButtonsPlacement ScrollbarThemeMac::buttonsPlacement() const {
  return WebScrollbarButtonsPlacementNone;
}

bool ScrollbarThemeMac::hasThumb(const ScrollbarThemeClient& scrollbar) {
  ScrollbarPainter painter = painterForScrollbar(scrollbar);
  int minLengthForThumb =
      [painter knobMinLength] + [painter trackOverlapEndInset] +
      [painter knobOverlapEndInset] +
      2 * ([painter trackEndInset] + [painter knobEndInset]);
  return scrollbar.enabled() &&
         (scrollbar.orientation() == HorizontalScrollbar
              ? scrollbar.width()
              : scrollbar.height()) >= minLengthForThumb;
}

IntRect ScrollbarThemeMac::backButtonRect(const ScrollbarThemeClient& scrollbar,
                                          ScrollbarPart part,
                                          bool painting) {
  ASSERT(buttonsPlacement() == WebScrollbarButtonsPlacementNone);
  return IntRect();
}

IntRect ScrollbarThemeMac::forwardButtonRect(
    const ScrollbarThemeClient& scrollbar,
    ScrollbarPart part,
    bool painting) {
  ASSERT(buttonsPlacement() == WebScrollbarButtonsPlacementNone);
  return IntRect();
}

IntRect ScrollbarThemeMac::trackRect(const ScrollbarThemeClient& scrollbar,
                                     bool painting) {
  ASSERT(!hasButtons(scrollbar));
  return scrollbar.frameRect();
}

int ScrollbarThemeMac::minimumThumbLength(
    const ScrollbarThemeClient& scrollbar) {
  return [painterForScrollbar(scrollbar) knobMinLength];
}

void ScrollbarThemeMac::updateEnabledState(
    const ScrollbarThemeClient& scrollbar) {
  [painterForScrollbar(scrollbar) setEnabled:scrollbar.enabled()];
}

float ScrollbarThemeMac::thumbOpacity(
    const ScrollbarThemeClient& scrollbar) const {
  ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
  return [scrollbarPainter knobAlpha];
}

// static
NSScrollerStyle ScrollbarThemeMac::recommendedScrollerStyle() {
  if (RuntimeEnabledFeatures::overlayScrollbarsEnabled())
    return NSScrollerStyleOverlay;
  return gPreferredScrollerStyle;
}

}  // namespace blink
