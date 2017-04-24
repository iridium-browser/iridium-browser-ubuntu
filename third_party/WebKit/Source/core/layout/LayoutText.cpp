/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/layout/LayoutText.h"

#include <algorithm>
#include "core/dom/AXObjectCache.h"
#include "core/dom/Text.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTextCombine.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/layout/line/AbstractInlineTextBox.h"
#include "core/layout/line/EllipsisBox.h"
#include "core/layout/line/GlyphOverflow.h"
#include "core/layout/line/InlineTextBox.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/Character.h"
#include "platform/text/Hyphenation.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextRunIterator.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringBuilder.h"

using namespace WTF;
using namespace Unicode;

namespace blink {

struct SameSizeAsLayoutText : public LayoutObject {
  uint32_t bitfields : 16;
  float widths[4];
  String text;
  void* pointers[2];
};

static_assert(sizeof(LayoutText) == sizeof(SameSizeAsLayoutText),
              "LayoutText should stay small");

class SecureTextTimer;
typedef HashMap<LayoutText*, SecureTextTimer*> SecureTextTimerMap;
static SecureTextTimerMap* gSecureTextTimers = nullptr;

class SecureTextTimer final : public TimerBase {
 public:
  SecureTextTimer(LayoutText* layoutText)
      : TimerBase(Platform::current()
                      ->currentThread()
                      ->scheduler()
                      ->timerTaskRunner()),
        m_layoutText(layoutText),
        m_lastTypedCharacterOffset(-1) {}

  void restartWithNewText(unsigned lastTypedCharacterOffset) {
    m_lastTypedCharacterOffset = lastTypedCharacterOffset;
    if (Settings* settings = m_layoutText->document().settings()) {
      startOneShot(settings->getPasswordEchoDurationInSeconds(),
                   BLINK_FROM_HERE);
    }
  }
  void invalidate() { m_lastTypedCharacterOffset = -1; }
  unsigned lastTypedCharacterOffset() { return m_lastTypedCharacterOffset; }

 private:
  void fired() override {
    ASSERT(gSecureTextTimers->contains(m_layoutText));
    m_layoutText->setText(
        m_layoutText->text().impl(),
        true /* forcing setting text as it may be masked later */);
  }

  LayoutText* m_layoutText;
  int m_lastTypedCharacterOffset;
};

static void makeCapitalized(String* string, UChar previous) {
  if (string->isNull())
    return;

  unsigned length = string->length();
  const StringImpl& input = *string->impl();

  if (length >= std::numeric_limits<unsigned>::max())
    CRASH();

  StringBuffer<UChar> stringWithPrevious(length + 1);
  stringWithPrevious[0] =
      previous == noBreakSpaceCharacter ? spaceCharacter : previous;
  for (unsigned i = 1; i < length + 1; i++) {
    // Replace &nbsp with a real space since ICU no longer treats &nbsp as a
    // word separator.
    if (input[i - 1] == noBreakSpaceCharacter)
      stringWithPrevious[i] = spaceCharacter;
    else
      stringWithPrevious[i] = input[i - 1];
  }

  TextBreakIterator* boundary =
      wordBreakIterator(stringWithPrevious.characters(), length + 1);
  if (!boundary)
    return;

  StringBuilder result;
  result.reserveCapacity(length);

  int32_t endOfWord;
  int32_t startOfWord = boundary->first();
  for (endOfWord = boundary->next(); endOfWord != TextBreakDone;
       startOfWord = endOfWord, endOfWord = boundary->next()) {
    if (startOfWord)  // Ignore first char of previous string
      result.append(input[startOfWord - 1] == noBreakSpaceCharacter
                        ? noBreakSpaceCharacter
                        : toTitleCase(stringWithPrevious[startOfWord]));
    for (int i = startOfWord + 1; i < endOfWord; i++)
      result.append(input[i - 1]);
  }

  *string = result.toString();
}

LayoutText::LayoutText(Node* node, PassRefPtr<StringImpl> str)
    : LayoutObject(!node || node->isDocumentNode() ? 0 : node),
      m_hasTab(false),
      m_linesDirty(false),
      m_containsReversedText(false),
      m_knownToHaveNoOverflowAndNoFallbackFonts(false),
      m_minWidth(-1),
      m_maxWidth(-1),
      m_firstLineMinWidth(0),
      m_lastLineLineMinWidth(0),
      m_text(std::move(str)),
      m_firstTextBox(nullptr),
      m_lastTextBox(nullptr) {
  ASSERT(m_text);
  // FIXME: Some clients of LayoutText (and subclasses) pass Document as node to
  // create anonymous layoutObject.
  // They should be switched to passing null and using setDocumentForAnonymous.
  if (node && node->isDocumentNode())
    setDocumentForAnonymous(toDocument(node));

  setIsText();

  view()->frameView()->incrementVisuallyNonEmptyCharacterCount(m_text.length());
}

#if DCHECK_IS_ON()

LayoutText::~LayoutText() {
  ASSERT(!m_firstTextBox);
  ASSERT(!m_lastTextBox);
}

#endif

bool LayoutText::isTextFragment() const {
  return false;
}

bool LayoutText::isWordBreak() const {
  return false;
}

void LayoutText::styleDidChange(StyleDifference diff,
                                const ComputedStyle* oldStyle) {
  // There is no need to ever schedule paint invalidations from a style change
  // of a text run, since we already did this for the parent of the text run.
  // We do have to schedule layouts, though, since a style change can force us
  // to need to relayout.
  if (diff.needsFullLayout()) {
    setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::StyleChange);
    m_knownToHaveNoOverflowAndNoFallbackFonts = false;
  }

  const ComputedStyle& newStyle = styleRef();
  ETextTransform oldTransform =
      oldStyle ? oldStyle->textTransform() : ETextTransform::kNone;
  ETextSecurity oldSecurity = oldStyle ? oldStyle->textSecurity() : TSNONE;
  if (oldTransform != newStyle.textTransform() ||
      oldSecurity != newStyle.textSecurity())
    transformText();

  // This is an optimization that kicks off font load before layout.
  if (!text().containsOnlyWhitespace())
    newStyle.font().willUseFontData(text());

  TextAutosizer* textAutosizer = document().textAutosizer();
  if (!oldStyle && textAutosizer)
    textAutosizer->record(this);
}

void LayoutText::removeAndDestroyTextBoxes() {
  if (!documentBeingDestroyed()) {
    if (firstTextBox()) {
      if (isBR()) {
        RootInlineBox* next = firstTextBox()->root().nextRootBox();
        if (next)
          next->markDirty();
      }
      for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        box->remove();
    } else if (parent()) {
      parent()->dirtyLinesFromChangedChild(this);
    }
  }
  deleteTextBoxes();
}

void LayoutText::willBeDestroyed() {
  if (SecureTextTimer* secureTextTimer =
          gSecureTextTimers ? gSecureTextTimers->take(this) : 0)
    delete secureTextTimer;

  removeAndDestroyTextBoxes();
  LayoutObject::willBeDestroyed();
}

void LayoutText::extractTextBox(InlineTextBox* box) {
  checkConsistency();

  m_lastTextBox = box->prevTextBox();
  if (box == m_firstTextBox)
    m_firstTextBox = nullptr;
  if (box->prevTextBox())
    box->prevTextBox()->setNextTextBox(nullptr);
  box->setPreviousTextBox(nullptr);
  for (InlineTextBox* curr = box; curr; curr = curr->nextTextBox())
    curr->setExtracted();

  checkConsistency();
}

void LayoutText::attachTextBox(InlineTextBox* box) {
  checkConsistency();

  if (m_lastTextBox) {
    m_lastTextBox->setNextTextBox(box);
    box->setPreviousTextBox(m_lastTextBox);
  } else {
    m_firstTextBox = box;
  }
  InlineTextBox* last = box;
  for (InlineTextBox* curr = box; curr; curr = curr->nextTextBox()) {
    curr->setExtracted(false);
    last = curr;
  }
  m_lastTextBox = last;

  checkConsistency();
}

void LayoutText::removeTextBox(InlineTextBox* box) {
  checkConsistency();

  if (box == m_firstTextBox)
    m_firstTextBox = box->nextTextBox();
  if (box == m_lastTextBox)
    m_lastTextBox = box->prevTextBox();
  if (box->nextTextBox())
    box->nextTextBox()->setPreviousTextBox(box->prevTextBox());
  if (box->prevTextBox())
    box->prevTextBox()->setNextTextBox(box->nextTextBox());

  checkConsistency();
}

void LayoutText::deleteTextBoxes() {
  if (firstTextBox()) {
    InlineTextBox* next;
    for (InlineTextBox* curr = firstTextBox(); curr; curr = next) {
      next = curr->nextTextBox();
      curr->destroy();
    }
    m_firstTextBox = m_lastTextBox = nullptr;
  }
}

PassRefPtr<StringImpl> LayoutText::originalText() const {
  Node* e = node();
  return (e && e->isTextNode()) ? toText(e)->dataImpl() : 0;
}

String LayoutText::plainText() const {
  if (node())
    return blink::plainText(EphemeralRange::rangeOfContents(*node()));

  // FIXME: this is just a stopgap until TextIterator is adapted to support
  // generated text.
  StringBuilder plainTextBuilder;
  for (InlineTextBox* textBox = firstTextBox(); textBox;
       textBox = textBox->nextTextBox()) {
    String text = m_text.substring(textBox->start(), textBox->len())
                      .simplifyWhiteSpace(WTF::DoNotStripWhiteSpace);
    plainTextBuilder.append(text);
    if (textBox->nextTextBox() &&
        textBox->nextTextBox()->start() > textBox->end() && text.length() &&
        !text.right(1).containsOnlyWhitespace())
      plainTextBuilder.append(spaceCharacter);
  }
  return plainTextBuilder.toString();
}

void LayoutText::absoluteRects(Vector<IntRect>& rects,
                               const LayoutPoint& accumulatedOffset) const {
  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
    rects.push_back(enclosingIntRect(LayoutRect(
        LayoutPoint(accumulatedOffset) + box->location(), box->size())));
  }
}

static FloatRect localQuadForTextBox(InlineTextBox* box,
                                     unsigned start,
                                     unsigned end,
                                     bool useSelectionHeight) {
  unsigned realEnd = std::min(box->end() + 1, end);
  LayoutRect r = box->localSelectionRect(start, realEnd);
  if (r.height()) {
    if (!useSelectionHeight) {
      // Change the height and y position (or width and x for vertical text)
      // because selectionRect uses selection-specific values.
      if (box->isHorizontal()) {
        r.setHeight(box->height());
        r.setY(box->y());
      } else {
        r.setWidth(box->width());
        r.setX(box->x());
      }
    }
    return FloatRect(r);
  }
  return FloatRect();
}

void LayoutText::absoluteRectsForRange(Vector<IntRect>& rects,
                                       unsigned start,
                                       unsigned end,
                                       bool useSelectionHeight) {
  // Work around signed/unsigned issues. This function takes unsigneds, and is
  // often passed UINT_MAX to mean "all the way to the end". InlineTextBox
  // coordinates are unsigneds, so changing this function to take ints causes
  // various internal mismatches. But selectionRect takes ints, and passing
  // UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take
  // unsigneds, but that would cause many ripple effects, so for now we'll just
  // clamp our unsigned parameters to INT_MAX.
  ASSERT(end == UINT_MAX || end <= INT_MAX);
  ASSERT(start <= INT_MAX);
  start = std::min(start, static_cast<unsigned>(INT_MAX));
  end = std::min(end, static_cast<unsigned>(INT_MAX));

  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
    // Note: box->end() returns the index of the last character, not the index
    // past it
    if (start <= box->start() && box->end() < end) {
      FloatRect r(box->frameRect());
      if (useSelectionHeight) {
        LayoutRect selectionRect = box->localSelectionRect(start, end);
        if (box->isHorizontal()) {
          r.setHeight(selectionRect.height().toFloat());
          r.setY(selectionRect.y().toFloat());
        } else {
          r.setWidth(selectionRect.width().toFloat());
          r.setX(selectionRect.x().toFloat());
        }
      }
      rects.push_back(localToAbsoluteQuad(r).enclosingBoundingBox());
    } else {
      // FIXME: This code is wrong. It's converting local to absolute twice.
      // http://webkit.org/b/65722
      FloatRect rect = localQuadForTextBox(box, start, end, useSelectionHeight);
      if (!rect.isZero())
        rects.push_back(localToAbsoluteQuad(rect).enclosingBoundingBox());
    }
  }
}

static IntRect ellipsisRectForBox(InlineTextBox* box,
                                  unsigned startPos,
                                  unsigned endPos) {
  if (!box)
    return IntRect();

  unsigned short truncation = box->truncation();
  if (truncation == cNoTruncation)
    return IntRect();

  IntRect rect;
  if (EllipsisBox* ellipsis = box->root().ellipsisBox()) {
    int ellipsisStartPosition = std::max<int>(startPos - box->start(), 0);
    int ellipsisEndPosition = std::min<int>(endPos - box->start(), box->len());

    // The ellipsis should be considered to be selected if the end of the
    // selection is past the beginning of the truncation and the beginning of
    // the selection is before or at the beginning of the truncation.
    if (ellipsisEndPosition >= truncation &&
        ellipsisStartPosition <= truncation)
      return ellipsis->selectionRect();
  }

  return IntRect();
}

void LayoutText::quads(Vector<FloatQuad>& quads,
                       ClippingOption option,
                       LocalOrAbsoluteOption localOrAbsolute,
                       MapCoordinatesFlags mode) const {
  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
    FloatRect boundaries(box->frameRect());

    // Shorten the width of this text box if it ends in an ellipsis.
    // FIXME: ellipsisRectForBox should switch to return FloatRect soon with the
    // subpixellayout branch.
    IntRect ellipsisRect = (option == ClipToEllipsis)
                               ? ellipsisRectForBox(box, 0, textLength())
                               : IntRect();
    if (!ellipsisRect.isEmpty()) {
      if (style()->isHorizontalWritingMode())
        boundaries.setWidth(ellipsisRect.maxX() - boundaries.x());
      else
        boundaries.setHeight(ellipsisRect.maxY() - boundaries.y());
    }
    if (localOrAbsolute == AbsoluteQuads)
      quads.push_back(localToAbsoluteQuad(boundaries, mode));
    else
      quads.push_back(boundaries);
  }
}

void LayoutText::absoluteQuads(Vector<FloatQuad>& quads,
                               MapCoordinatesFlags mode) const {
  this->quads(quads, NoClipping, AbsoluteQuads, mode);
}

void LayoutText::absoluteQuadsForRange(Vector<FloatQuad>& quads,
                                       unsigned start,
                                       unsigned end,
                                       bool useSelectionHeight) {
  // Work around signed/unsigned issues. This function takes unsigneds, and is
  // often passed UINT_MAX to mean "all the way to the end". InlineTextBox
  // coordinates are unsigneds, so changing this function to take ints causes
  // various internal mismatches. But selectionRect takes ints, and passing
  // UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take
  // unsigneds, but that would cause many ripple effects, so for now we'll just
  // clamp our unsigned parameters to INT_MAX.
  ASSERT(end == UINT_MAX || end <= INT_MAX);
  ASSERT(start <= INT_MAX);
  start = std::min(start, static_cast<unsigned>(INT_MAX));
  end = std::min(end, static_cast<unsigned>(INT_MAX));

  const unsigned caretMinOffset = static_cast<unsigned>(this->caretMinOffset());
  const unsigned caretMaxOffset = static_cast<unsigned>(this->caretMaxOffset());

  // Narrows |start| and |end| into |caretMinOffset| and |careMaxOffset|
  // to ignore unrendered leading and trailing whitespaces.
  start = std::min(std::max(caretMinOffset, start), caretMaxOffset);
  end = std::min(std::max(caretMinOffset, end), caretMaxOffset);

  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
    // Note: box->end() returns the index of the last character, not the index
    // past it
    if (start <= box->start() && box->end() < end) {
      LayoutRect r(box->frameRect());
      if (useSelectionHeight) {
        LayoutRect selectionRect = box->localSelectionRect(start, end);
        if (box->isHorizontal()) {
          r.setHeight(selectionRect.height());
          r.setY(selectionRect.y());
        } else {
          r.setWidth(selectionRect.width());
          r.setX(selectionRect.x());
        }
      }
      quads.push_back(localToAbsoluteQuad(FloatRect(r)));
    } else {
      FloatRect rect = localQuadForTextBox(box, start, end, useSelectionHeight);
      if (!rect.isZero())
        quads.push_back(localToAbsoluteQuad(rect));
    }
  }
}

FloatRect LayoutText::localBoundingBoxRectForAccessibility() const {
  FloatRect result;
  Vector<FloatQuad> quads;
  this->quads(quads, LayoutText::ClipToEllipsis, LayoutText::LocalQuads);
  for (const FloatQuad& quad : quads)
    result.unite(quad.boundingBox());
  return result;
}

enum ShouldAffinityBeDownstream {
  AlwaysDownstream,
  AlwaysUpstream,
  UpstreamIfPositionIsNotAtStart
};

static bool lineDirectionPointFitsInBox(
    int pointLineDirection,
    InlineTextBox* box,
    ShouldAffinityBeDownstream& shouldAffinityBeDownstream) {
  shouldAffinityBeDownstream = AlwaysDownstream;

  // the x coordinate is equal to the left edge of this box the affinity must be
  // downstream so the position doesn't jump back to the previous line except
  // when box is the first box in the line
  if (pointLineDirection <= box->logicalLeft()) {
    shouldAffinityBeDownstream = !box->prevLeafChild()
                                     ? UpstreamIfPositionIsNotAtStart
                                     : AlwaysDownstream;
    return true;
  }

  // and the x coordinate is to the left of the right edge of this box
  // check to see if position goes in this box
  if (pointLineDirection < box->logicalRight()) {
    shouldAffinityBeDownstream = UpstreamIfPositionIsNotAtStart;
    return true;
  }

  // box is first on line
  // and the x coordinate is to the left of the first text box left edge
  if (!box->prevLeafChildIgnoringLineBreak() &&
      pointLineDirection < box->logicalLeft())
    return true;

  if (!box->nextLeafChildIgnoringLineBreak()) {
    // box is last on line and the x coordinate is to the right of the last text
    // box right edge generate VisiblePosition, use TextAffinity::Upstream
    // affinity if possible
    shouldAffinityBeDownstream = UpstreamIfPositionIsNotAtStart;
    return true;
  }

  return false;
}

static PositionWithAffinity createPositionWithAffinityForBox(
    const InlineBox* box,
    int offset,
    ShouldAffinityBeDownstream shouldAffinityBeDownstream) {
  TextAffinity affinity = VP_DEFAULT_AFFINITY;
  switch (shouldAffinityBeDownstream) {
    case AlwaysDownstream:
      affinity = TextAffinity::Downstream;
      break;
    case AlwaysUpstream:
      affinity = VP_UPSTREAM_IF_POSSIBLE;
      break;
    case UpstreamIfPositionIsNotAtStart:
      affinity = offset > box->caretMinOffset() ? VP_UPSTREAM_IF_POSSIBLE
                                                : TextAffinity::Downstream;
      break;
  }
  int textStartOffset =
      box->getLineLayoutItem().isText()
          ? LineLayoutText(box->getLineLayoutItem()).textStartOffset()
          : 0;
  return box->getLineLayoutItem().createPositionWithAffinity(
      offset + textStartOffset, affinity);
}

static PositionWithAffinity
createPositionWithAffinityForBoxAfterAdjustingOffsetForBiDi(
    const InlineTextBox* box,
    int offset,
    ShouldAffinityBeDownstream shouldAffinityBeDownstream) {
  ASSERT(box);
  ASSERT(offset >= 0);

  if (offset && static_cast<unsigned>(offset) < box->len())
    return createPositionWithAffinityForBox(box, box->start() + offset,
                                            shouldAffinityBeDownstream);

  bool positionIsAtStartOfBox = !offset;
  if (positionIsAtStartOfBox == box->isLeftToRightDirection()) {
    // offset is on the left edge

    const InlineBox* prevBox = box->prevLeafChildIgnoringLineBreak();
    if ((prevBox && prevBox->bidiLevel() == box->bidiLevel()) ||
        box->getLineLayoutItem().containingBlock().style()->direction() ==
            box->direction())  // FIXME: left on 12CBA
      return createPositionWithAffinityForBox(box, box->caretLeftmostOffset(),
                                              shouldAffinityBeDownstream);

    if (prevBox && prevBox->bidiLevel() > box->bidiLevel()) {
      // e.g. left of B in aDC12BAb
      const InlineBox* leftmostBox;
      do {
        leftmostBox = prevBox;
        prevBox = leftmostBox->prevLeafChildIgnoringLineBreak();
      } while (prevBox && prevBox->bidiLevel() > box->bidiLevel());
      return createPositionWithAffinityForBox(
          leftmostBox, leftmostBox->caretRightmostOffset(),
          shouldAffinityBeDownstream);
    }

    if (!prevBox || prevBox->bidiLevel() < box->bidiLevel()) {
      // e.g. left of D in aDC12BAb
      const InlineBox* rightmostBox;
      const InlineBox* nextBox = box;
      do {
        rightmostBox = nextBox;
        nextBox = rightmostBox->nextLeafChildIgnoringLineBreak();
      } while (nextBox && nextBox->bidiLevel() >= box->bidiLevel());
      return createPositionWithAffinityForBox(
          rightmostBox,
          box->isLeftToRightDirection() ? rightmostBox->caretMaxOffset()
                                        : rightmostBox->caretMinOffset(),
          shouldAffinityBeDownstream);
    }

    return createPositionWithAffinityForBox(box, box->caretRightmostOffset(),
                                            shouldAffinityBeDownstream);
  }

  const InlineBox* nextBox = box->nextLeafChildIgnoringLineBreak();
  if ((nextBox && nextBox->bidiLevel() == box->bidiLevel()) ||
      box->getLineLayoutItem().containingBlock().style()->direction() ==
          box->direction())
    return createPositionWithAffinityForBox(box, box->caretRightmostOffset(),
                                            shouldAffinityBeDownstream);

  // offset is on the right edge
  if (nextBox && nextBox->bidiLevel() > box->bidiLevel()) {
    // e.g. right of C in aDC12BAb
    const InlineBox* rightmostBox;
    do {
      rightmostBox = nextBox;
      nextBox = rightmostBox->nextLeafChildIgnoringLineBreak();
    } while (nextBox && nextBox->bidiLevel() > box->bidiLevel());
    return createPositionWithAffinityForBox(rightmostBox,
                                            rightmostBox->caretLeftmostOffset(),
                                            shouldAffinityBeDownstream);
  }

  if (!nextBox || nextBox->bidiLevel() < box->bidiLevel()) {
    // e.g. right of A in aDC12BAb
    const InlineBox* leftmostBox;
    const InlineBox* prevBox = box;
    do {
      leftmostBox = prevBox;
      prevBox = leftmostBox->prevLeafChildIgnoringLineBreak();
    } while (prevBox && prevBox->bidiLevel() >= box->bidiLevel());
    return createPositionWithAffinityForBox(leftmostBox,
                                            box->isLeftToRightDirection()
                                                ? leftmostBox->caretMinOffset()
                                                : leftmostBox->caretMaxOffset(),
                                            shouldAffinityBeDownstream);
  }

  return createPositionWithAffinityForBox(box, box->caretLeftmostOffset(),
                                          shouldAffinityBeDownstream);
}

PositionWithAffinity LayoutText::positionForPoint(const LayoutPoint& point) {
  if (!firstTextBox() || textLength() == 0)
    return createPositionWithAffinity(0);

  LayoutUnit pointLineDirection =
      firstTextBox()->isHorizontal() ? point.x() : point.y();
  LayoutUnit pointBlockDirection =
      firstTextBox()->isHorizontal() ? point.y() : point.x();
  bool blocksAreFlipped = style()->isFlippedBlocksWritingMode();

  InlineTextBox* lastBox = nullptr;
  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
    if (box->isLineBreak() && !box->prevLeafChild() && box->nextLeafChild() &&
        !box->nextLeafChild()->isLineBreak())
      box = box->nextTextBox();

    RootInlineBox& rootBox = box->root();
    LayoutUnit top = std::min(rootBox.selectionTop(), rootBox.lineTop());
    if (pointBlockDirection > top ||
        (!blocksAreFlipped && pointBlockDirection == top)) {
      LayoutUnit bottom = rootBox.selectionBottom();
      if (rootBox.nextRootBox())
        bottom = std::min(bottom, rootBox.nextRootBox()->lineTop());

      if (pointBlockDirection < bottom ||
          (blocksAreFlipped && pointBlockDirection == bottom)) {
        ShouldAffinityBeDownstream shouldAffinityBeDownstream;
        if (lineDirectionPointFitsInBox(pointLineDirection.toInt(), box,
                                        shouldAffinityBeDownstream))
          return createPositionWithAffinityForBoxAfterAdjustingOffsetForBiDi(
              box, box->offsetForPosition(pointLineDirection),
              shouldAffinityBeDownstream);
      }
    }
    lastBox = box;
  }

  if (lastBox) {
    ShouldAffinityBeDownstream shouldAffinityBeDownstream;
    lineDirectionPointFitsInBox(pointLineDirection.toInt(), lastBox,
                                shouldAffinityBeDownstream);
    return createPositionWithAffinityForBoxAfterAdjustingOffsetForBiDi(
        lastBox,
        lastBox->offsetForPosition(pointLineDirection) + lastBox->start(),
        shouldAffinityBeDownstream);
  }
  return createPositionWithAffinity(0);
}

LayoutRect LayoutText::localCaretRect(InlineBox* inlineBox,
                                      int caretOffset,
                                      LayoutUnit* extraWidthToEndOfLine) {
  if (!inlineBox)
    return LayoutRect();

  ASSERT(inlineBox->isInlineTextBox());
  if (!inlineBox->isInlineTextBox())
    return LayoutRect();

  InlineTextBox* box = toInlineTextBox(inlineBox);
  // Find an InlineBox before caret position, which is used to get caret height.
  InlineBox* caretBox = box;
  if (box->getLineLayoutItem().style(box->isFirstLineStyle())->direction() ==
      TextDirection::kLtr) {
    if (box->prevLeafChild() && caretOffset == 0)
      caretBox = box->prevLeafChild();
  } else {
    if (box->nextLeafChild() && caretOffset == 0)
      caretBox = box->nextLeafChild();
  }

  // Get caret height from a font of character.
  const ComputedStyle* styleToUse =
      caretBox->getLineLayoutItem().style(caretBox->isFirstLineStyle());
  if (!styleToUse->font().primaryFont())
    return LayoutRect();

  int height = styleToUse->font().primaryFont()->getFontMetrics().height();
  int top = caretBox->logicalTop().toInt();

  // Go ahead and round left to snap it to the nearest pixel.
  LayoutUnit left = box->positionForOffset(caretOffset);
  LayoutUnit caretWidth = frameView()->caretWidth();

  // Distribute the caret's width to either side of the offset.
  LayoutUnit caretWidthLeftOfOffset = caretWidth / 2;
  left -= caretWidthLeftOfOffset;
  LayoutUnit caretWidthRightOfOffset = caretWidth - caretWidthLeftOfOffset;

  left = LayoutUnit(left.round());

  LayoutUnit rootLeft = box->root().logicalLeft();
  LayoutUnit rootRight = box->root().logicalRight();

  // FIXME: should we use the width of the root inline box or the
  // width of the containing block for this?
  if (extraWidthToEndOfLine)
    *extraWidthToEndOfLine =
        (box->root().logicalWidth() + rootLeft) - (left + 1);

  LayoutBlock* cb = containingBlock();
  const ComputedStyle& cbStyle = cb->styleRef();

  LayoutUnit leftEdge;
  LayoutUnit rightEdge;
  leftEdge = std::min(LayoutUnit(), rootLeft);
  rightEdge = std::max(cb->logicalWidth(), rootRight);

  bool rightAligned = false;
  switch (cbStyle.textAlign()) {
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      rightAligned = true;
      break;
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      break;
    case ETextAlign::kJustify:
    case ETextAlign::kStart:
      rightAligned = !cbStyle.isLeftToRightDirection();
      break;
    case ETextAlign::kEnd:
      rightAligned = cbStyle.isLeftToRightDirection();
      break;
  }

  // for unicode-bidi: plaintext, use inlineBoxBidiLevel() to test the correct
  // direction for the cursor.
  if (rightAligned && style()->getUnicodeBidi() == UnicodeBidi::kPlaintext) {
    if (inlineBox->bidiLevel() % 2 != 1)
      rightAligned = false;
  }

  if (rightAligned) {
    left = std::max(left, leftEdge);
    left = std::min(left, rootRight - caretWidth);
  } else {
    left = std::min(left, rightEdge - caretWidthRightOfOffset);
    left = std::max(left, rootLeft);
  }

  return LayoutRect(
      style()->isHorizontalWritingMode()
          ? IntRect(left.toInt(), top, caretWidth.toInt(), height)
          : IntRect(top, left.toInt(), height, caretWidth.toInt()));
}

ALWAYS_INLINE float LayoutText::widthFromFont(
    const Font& f,
    int start,
    int len,
    float leadWidth,
    float textWidthSoFar,
    TextDirection textDirection,
    HashSet<const SimpleFontData*>* fallbackFonts,
    FloatRect* glyphBoundsAccumulation) const {
  if (style()->hasTextCombine() && isCombineText()) {
    const LayoutTextCombine* combineText = toLayoutTextCombine(this);
    if (combineText->isCombined())
      return combineText->combinedTextWidth(f);
  }

  TextRun run =
      constructTextRun(f, this, start, len, styleRef(), textDirection);
  run.setCharactersLength(textLength() - start);
  ASSERT(run.charactersLength() >= run.length());
  run.setTabSize(!style()->collapseWhiteSpace(), style()->getTabSize());
  run.setXPos(leadWidth + textWidthSoFar);

  FloatRect newGlyphBounds;
  float result = f.width(run, fallbackFonts,
                         glyphBoundsAccumulation ? &newGlyphBounds : nullptr);
  if (glyphBoundsAccumulation) {
    newGlyphBounds.move(textWidthSoFar, 0);
    glyphBoundsAccumulation->unite(newGlyphBounds);
  }
  return result;
}

void LayoutText::trimmedPrefWidths(LayoutUnit leadWidthLayoutUnit,
                                   LayoutUnit& firstLineMinWidth,
                                   bool& hasBreakableStart,
                                   LayoutUnit& lastLineMinWidth,
                                   bool& hasBreakableEnd,
                                   bool& hasBreakableChar,
                                   bool& hasBreak,
                                   LayoutUnit& firstLineMaxWidth,
                                   LayoutUnit& lastLineMaxWidth,
                                   LayoutUnit& minWidth,
                                   LayoutUnit& maxWidth,
                                   bool& stripFrontSpaces,
                                   TextDirection direction) {
  float floatMinWidth = 0.0f, floatMaxWidth = 0.0f;

  // Convert leadWidth to a float here, to avoid multiple implict conversions
  // below.
  float leadWidth = leadWidthLayoutUnit.toFloat();

  bool collapseWhiteSpace = style()->collapseWhiteSpace();
  if (!collapseWhiteSpace)
    stripFrontSpaces = false;

  if (m_hasTab || preferredLogicalWidthsDirty())
    computePreferredLogicalWidths(leadWidth);

  hasBreakableStart = !stripFrontSpaces && m_hasBreakableStart;
  hasBreakableEnd = m_hasBreakableEnd;

  int len = textLength();

  if (!len || (stripFrontSpaces && text().impl()->containsOnlyWhitespace())) {
    firstLineMinWidth = LayoutUnit();
    lastLineMinWidth = LayoutUnit();
    firstLineMaxWidth = LayoutUnit();
    lastLineMaxWidth = LayoutUnit();
    minWidth = LayoutUnit();
    maxWidth = LayoutUnit();
    hasBreak = false;
    return;
  }

  floatMinWidth = m_minWidth;
  floatMaxWidth = m_maxWidth;

  firstLineMinWidth = LayoutUnit(m_firstLineMinWidth);
  lastLineMinWidth = LayoutUnit(m_lastLineLineMinWidth);

  hasBreakableChar = m_hasBreakableChar;
  hasBreak = m_hasBreak;

  ASSERT(m_text);
  StringImpl& text = *m_text.impl();
  if (text[0] == spaceCharacter ||
      (text[0] == newlineCharacter && !style()->preserveNewline()) ||
      text[0] == tabulationCharacter) {
    const Font& font = style()->font();  // FIXME: This ignores first-line.
    if (stripFrontSpaces) {
      const UChar spaceChar = spaceCharacter;
      TextRun run =
          constructTextRun(font, &spaceChar, 1, styleRef(), direction);
      float spaceWidth = font.width(run);
      floatMaxWidth -= spaceWidth;
    } else {
      floatMaxWidth += font.getFontDescription().wordSpacing();
    }
  }

  stripFrontSpaces = collapseWhiteSpace && m_hasEndWhiteSpace;

  if (!style()->autoWrap() || floatMinWidth > floatMaxWidth)
    floatMinWidth = floatMaxWidth;

  // Compute our max widths by scanning the string for newlines.
  if (hasBreak) {
    const Font& f = style()->font();  // FIXME: This ignores first-line.
    bool firstLine = true;
    firstLineMaxWidth = LayoutUnit(floatMaxWidth);
    lastLineMaxWidth = LayoutUnit(floatMaxWidth);
    for (int i = 0; i < len; i++) {
      int linelen = 0;
      while (i + linelen < len && text[i + linelen] != newlineCharacter)
        linelen++;

      if (linelen) {
        lastLineMaxWidth = LayoutUnit(
            widthFromFont(f, i, linelen, leadWidth, lastLineMaxWidth.toFloat(),
                          direction, nullptr, nullptr));
        if (firstLine) {
          firstLine = false;
          leadWidth = 0.f;
          firstLineMaxWidth = lastLineMaxWidth;
        }
        i += linelen;
      } else if (firstLine) {
        firstLineMaxWidth = LayoutUnit();
        firstLine = false;
        leadWidth = 0.f;
      }

      if (i == len - 1) {
        // A <pre> run that ends with a newline, as in, e.g.,
        // <pre>Some text\n\n<span>More text</pre>
        lastLineMaxWidth = LayoutUnit();
      }
    }
  }

  minWidth = LayoutUnit::fromFloatCeil(floatMinWidth);
  maxWidth = LayoutUnit::fromFloatCeil(floatMaxWidth);
}

float LayoutText::minLogicalWidth() const {
  if (preferredLogicalWidthsDirty())
    const_cast<LayoutText*>(this)->computePreferredLogicalWidths(0);

  return m_minWidth;
}

float LayoutText::maxLogicalWidth() const {
  if (preferredLogicalWidthsDirty())
    const_cast<LayoutText*>(this)->computePreferredLogicalWidths(0);

  return m_maxWidth;
}

void LayoutText::computePreferredLogicalWidths(float leadWidth) {
  HashSet<const SimpleFontData*> fallbackFonts;
  FloatRect glyphBounds;
  computePreferredLogicalWidths(leadWidth, fallbackFonts, glyphBounds);
}

static float minWordFragmentWidthForBreakAll(LayoutText* layoutText,
                                             const ComputedStyle& style,
                                             const Font& font,
                                             TextDirection textDirection,
                                             int start,
                                             int length,
                                             EWordBreak breakAllOrBreakWord) {
  DCHECK_GT(length, 0);
  LazyLineBreakIterator breakIterator(layoutText->text(),
                                      localeForLineBreakIterator(style));
  int nextBreakable = -1;
  float min = std::numeric_limits<float>::max();
  int end = start + length;
  for (int i = start; i < end;) {
    int fragmentLength;
    if (breakAllOrBreakWord == EWordBreak::BreakAllWordBreak) {
      breakIterator.isBreakable(i + 1, nextBreakable, LineBreakType::BreakAll);
      fragmentLength = (nextBreakable > i ? nextBreakable : length) - i;
    } else {
      fragmentLength = U16_LENGTH(layoutText->codepointAt(i));
    }

    // Ensure that malformed surrogate pairs don't cause us to read
    // past the end of the string.
    int textLength = layoutText->textLength();
    if (i + fragmentLength > textLength)
      fragmentLength = std::max(textLength - i, 0);

    // The correct behavior is to measure width without re-shaping, but we
    // reshape each fragment here because a) the current line breaker does not
    // support it, b) getCharacterRange() can reshape if the text is too long
    // to fit in the cache, and c) each fragment here is almost 1 char and thus
    // reshape is fast.
    TextRun run = constructTextRun(font, layoutText, i, fragmentLength, style,
                                   textDirection);
    float fragmentWidth = font.width(run);
    min = std::min(min, fragmentWidth);
    i += fragmentLength;
  }
  return min;
}

static float maxWordFragmentWidth(LayoutText* layoutText,
                                  const ComputedStyle& style,
                                  const Font& font,
                                  TextDirection textDirection,
                                  Hyphenation& hyphenation,
                                  unsigned wordOffset,
                                  unsigned wordLength,
                                  int& suffixStart) {
  suffixStart = 0;
  if (wordLength <= Hyphenation::minimumSuffixLength)
    return 0;

  Vector<size_t, 8> hyphenLocations = hyphenation.hyphenLocations(
      StringView(layoutText->text(), wordOffset, wordLength));
  if (hyphenLocations.isEmpty())
    return 0;

  float minimumFragmentWidthToConsider = Hyphenation::minimumPrefixWidth(font);
  float maxFragmentWidth = 0;
  TextRun run = constructTextRun(font, layoutText, wordOffset, wordLength,
                                 style, textDirection);
  size_t end = wordLength;
  for (size_t start : hyphenLocations) {
    float fragmentWidth = font.getCharacterRange(run, start, end).width();

    if (fragmentWidth <= minimumFragmentWidthToConsider)
      continue;

    maxFragmentWidth = std::max(maxFragmentWidth, fragmentWidth);
    end = start;
  }
  suffixStart = hyphenLocations.front();
  return maxFragmentWidth + layoutText->hyphenWidth(font, textDirection);
}

AtomicString localeForLineBreakIterator(const ComputedStyle& style) {
  LineBreakIteratorMode mode = LineBreakIteratorMode::Default;
  switch (style.getLineBreak()) {
    default:
      NOTREACHED();
    // Fall through.
    case LineBreakAuto:
    case LineBreakAfterWhiteSpace:
      return style.locale();
    case LineBreakNormal:
      mode = LineBreakIteratorMode::Normal;
      break;
    case LineBreakStrict:
      mode = LineBreakIteratorMode::Strict;
      break;
    case LineBreakLoose:
      mode = LineBreakIteratorMode::Loose;
      break;
  }
  if (const LayoutLocale* locale = style.getFontDescription().locale())
    return locale->localeWithBreakKeyword(mode);
  return style.locale();
}

void LayoutText::computePreferredLogicalWidths(
    float leadWidth,
    HashSet<const SimpleFontData*>& fallbackFonts,
    FloatRect& glyphBounds) {
  ASSERT(m_hasTab || preferredLogicalWidthsDirty() ||
         !m_knownToHaveNoOverflowAndNoFallbackFonts);

  m_minWidth = 0;
  m_maxWidth = 0;
  m_firstLineMinWidth = 0;
  m_lastLineLineMinWidth = 0;

  if (isBR())
    return;

  float currMinWidth = 0;
  float currMaxWidth = 0;
  m_hasBreakableChar = false;
  m_hasBreak = false;
  m_hasTab = false;
  m_hasBreakableStart = false;
  m_hasBreakableEnd = false;
  m_hasEndWhiteSpace = false;

  const ComputedStyle& styleToUse = styleRef();
  const Font& f = styleToUse.font();  // FIXME: This ignores first-line.
  float wordSpacing = styleToUse.wordSpacing();
  int len = textLength();
  LazyLineBreakIterator breakIterator(m_text,
                                      localeForLineBreakIterator(styleToUse));
  bool needsWordSpacing = false;
  bool ignoringSpaces = false;
  bool isSpace = false;
  bool firstWord = true;
  bool firstLine = true;
  int nextBreakable = -1;
  int lastWordBoundary = 0;
  float cachedWordTrailingSpaceWidth[2] = {0, 0};  // LTR, RTL

  EWordBreak breakAllOrBreakWord = EWordBreak::NormalWordBreak;
  LineBreakType lineBreakType = LineBreakType::Normal;
  if (styleToUse.autoWrap()) {
    if (styleToUse.wordBreak() == BreakAllWordBreak ||
        styleToUse.wordBreak() == BreakWordBreak) {
      breakAllOrBreakWord = styleToUse.wordBreak();
    } else if (styleToUse.wordBreak() == KeepAllWordBreak) {
      lineBreakType = LineBreakType::KeepAll;
    }
  }

  Hyphenation* hyphenation =
      styleToUse.autoWrap() ? styleToUse.getHyphenation() : nullptr;
  bool disableSoftHyphen = styleToUse.getHyphens() == HyphensNone;
  float maxWordWidth = 0;
  if (!hyphenation)
    maxWordWidth = std::numeric_limits<float>::infinity();

  BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
  BidiCharacterRun* run;
  TextDirection textDirection = styleToUse.direction();
  if ((is8Bit() && textDirection == TextDirection::kLtr) ||
      isOverride(styleToUse.getUnicodeBidi())) {
    run = 0;
  } else {
    TextRun textRun(text());
    BidiStatus status(textDirection, false);
    bidiResolver.setStatus(status);
    bidiResolver.setPositionIgnoringNestedIsolates(
        TextRunIterator(&textRun, 0));
    bool hardLineBreak = false;
    bool reorderRuns = false;
    bidiResolver.createBidiRunsForLine(
        TextRunIterator(&textRun, textRun.length()), NoVisualOverride,
        hardLineBreak, reorderRuns);
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    run = bidiRuns.firstRun();
  }

  for (int i = 0; i < len; i++) {
    UChar c = uncheckedCharacterAt(i);

    if (run) {
      // Treat adjacent runs with the same resolved directionality
      // (TextDirection as opposed to WTF::Unicode::Direction) as belonging
      // to the same run to avoid breaking unnecessarily.
      while (i >= run->stop() ||
             (run->next() && run->next()->direction() == run->direction()))
        run = run->next();

      ASSERT(run);
      ASSERT(i <= run->stop());
      textDirection = run->direction();
    }

    bool previousCharacterIsSpace = isSpace;
    bool isNewline = false;
    if (c == newlineCharacter) {
      if (styleToUse.preserveNewline()) {
        m_hasBreak = true;
        isNewline = true;
        isSpace = false;
      } else {
        isSpace = true;
      }
    } else if (c == tabulationCharacter) {
      if (!styleToUse.collapseWhiteSpace()) {
        m_hasTab = true;
        isSpace = false;
      } else {
        isSpace = true;
      }
    } else {
      isSpace = c == spaceCharacter;
    }

    bool isBreakableLocation = isNewline || (isSpace && styleToUse.autoWrap());
    if (!i)
      m_hasBreakableStart = isBreakableLocation;
    if (i == len - 1) {
      m_hasBreakableEnd = isBreakableLocation;
      m_hasEndWhiteSpace = isNewline || isSpace;
    }

    if (!ignoringSpaces && styleToUse.collapseWhiteSpace() &&
        previousCharacterIsSpace && isSpace)
      ignoringSpaces = true;

    if (ignoringSpaces && !isSpace)
      ignoringSpaces = false;

    // Ignore spaces and soft hyphens
    if (ignoringSpaces) {
      ASSERT(lastWordBoundary == i);
      lastWordBoundary++;
      continue;
    }
    if (c == softHyphenCharacter && !disableSoftHyphen) {
      currMaxWidth += widthFromFont(f, lastWordBoundary, i - lastWordBoundary,
                                    leadWidth, currMaxWidth, textDirection,
                                    &fallbackFonts, &glyphBounds);
      lastWordBoundary = i + 1;
      continue;
    }

    bool hasBreak = breakIterator.isBreakable(i, nextBreakable, lineBreakType);
    bool betweenWords = true;
    int j = i;
    while (c != newlineCharacter && c != spaceCharacter &&
           c != tabulationCharacter &&
           (c != softHyphenCharacter || disableSoftHyphen)) {
      j++;
      if (j == len)
        break;
      c = uncheckedCharacterAt(j);
      if (breakIterator.isBreakable(j, nextBreakable) &&
          characterAt(j - 1) != softHyphenCharacter)
        break;
    }

    // Terminate word boundary at bidi run boundary.
    if (run)
      j = std::min(j, run->stop() + 1);
    int wordLen = j - i;
    if (wordLen) {
      bool isSpace = (j < len) && c == spaceCharacter;

      // Non-zero only when kerning is enabled, in which case we measure words
      // with their trailing space, then subtract its width.
      float wordTrailingSpaceWidth = 0;
      if (isSpace &&
          (f.getFontDescription().getTypesettingFeatures() & Kerning)) {
        const unsigned textDirectionIndex =
            static_cast<unsigned>(textDirection);
        DCHECK_GE(textDirectionIndex, 0U);
        DCHECK_LE(textDirectionIndex, 1U);
        if (!cachedWordTrailingSpaceWidth[textDirectionIndex])
          cachedWordTrailingSpaceWidth[textDirectionIndex] =
              f.width(constructTextRun(f, &spaceCharacter, 1, styleToUse,
                                       textDirection)) +
              wordSpacing;
        wordTrailingSpaceWidth =
            cachedWordTrailingSpaceWidth[textDirectionIndex];
      }

      float w;
      if (wordTrailingSpaceWidth && isSpace) {
        w = widthFromFont(f, i, wordLen + 1, leadWidth, currMaxWidth,
                          textDirection, &fallbackFonts, &glyphBounds) -
            wordTrailingSpaceWidth;
      } else {
        w = widthFromFont(f, i, wordLen, leadWidth, currMaxWidth, textDirection,
                          &fallbackFonts, &glyphBounds);
        if (c == softHyphenCharacter && !disableSoftHyphen)
          currMinWidth += hyphenWidth(f, textDirection);
      }

      if (w > maxWordWidth) {
        DCHECK(hyphenation);
        int suffixStart;
        float maxFragmentWidth =
            maxWordFragmentWidth(this, styleToUse, f, textDirection,
                                 *hyphenation, i, wordLen, suffixStart);
        if (suffixStart) {
          float suffixWidth;
          if (wordTrailingSpaceWidth && isSpace)
            suffixWidth =
                widthFromFont(f, i + suffixStart, wordLen - suffixStart + 1,
                              leadWidth, currMaxWidth, textDirection,
                              &fallbackFonts, &glyphBounds) -
                wordTrailingSpaceWidth;
          else
            suffixWidth = widthFromFont(
                f, i + suffixStart, wordLen - suffixStart, leadWidth,
                currMaxWidth, textDirection, &fallbackFonts, &glyphBounds);
          maxFragmentWidth = std::max(maxFragmentWidth, suffixWidth);
          currMinWidth += maxFragmentWidth - w;
          maxWordWidth = std::max(maxWordWidth, maxFragmentWidth);
        } else {
          maxWordWidth = w;
        }
      }

      if (breakAllOrBreakWord != EWordBreak::NormalWordBreak) {
        // Because sum of character widths may not be equal to the word width,
        // we need to measure twice; once with normal break for max width,
        // another with break-all for min width.
        currMinWidth =
            minWordFragmentWidthForBreakAll(this, styleToUse, f, textDirection,
                                            i, wordLen, breakAllOrBreakWord);
      } else {
        currMinWidth += w;
      }
      if (betweenWords) {
        if (lastWordBoundary == i)
          currMaxWidth += w;
        else
          currMaxWidth += widthFromFont(
              f, lastWordBoundary, j - lastWordBoundary, leadWidth,
              currMaxWidth, textDirection, &fallbackFonts, &glyphBounds);
        lastWordBoundary = j;
      }

      bool isCollapsibleWhiteSpace =
          (j < len) && styleToUse.isCollapsibleWhiteSpace(c);
      if (j < len && styleToUse.autoWrap())
        m_hasBreakableChar = true;

      // Add in wordSpacing to our currMaxWidth, but not if this is the last
      // word on a line or the
      // last word in the run.
      if (wordSpacing && (isSpace || isCollapsibleWhiteSpace) &&
          !containsOnlyWhitespace(j, len - j))
        currMaxWidth += wordSpacing;

      if (firstWord) {
        firstWord = false;
        // If the first character in the run is breakable, then we consider
        // ourselves to have a beginning minimum width of 0, since a break could
        // occur right before our run starts, preventing us from ever being
        // appended to a previous text run when considering the total minimum
        // width of the containing block.
        if (hasBreak)
          m_hasBreakableChar = true;
        m_firstLineMinWidth = hasBreak ? 0 : currMinWidth;
      }
      m_lastLineLineMinWidth = currMinWidth;

      if (currMinWidth > m_minWidth)
        m_minWidth = currMinWidth;
      currMinWidth = 0;

      i += wordLen - 1;
    } else {
      // Nowrap can never be broken, so don't bother setting the breakable
      // character boolean. Pre can only be broken if we encounter a newline.
      if (style()->autoWrap() || isNewline)
        m_hasBreakableChar = true;

      if (currMinWidth > m_minWidth)
        m_minWidth = currMinWidth;
      currMinWidth = 0;

      // Only set if preserveNewline was true and we saw a newline.
      if (isNewline) {
        if (firstLine) {
          firstLine = false;
          leadWidth = 0;
          if (!styleToUse.autoWrap())
            m_firstLineMinWidth = currMaxWidth;
        }

        if (currMaxWidth > m_maxWidth)
          m_maxWidth = currMaxWidth;
        currMaxWidth = 0;
      } else {
        TextRun run =
            constructTextRun(f, this, i, 1, styleToUse, textDirection);
        run.setCharactersLength(len - i);
        ASSERT(run.charactersLength() >= run.length());
        run.setTabSize(!style()->collapseWhiteSpace(), style()->getTabSize());
        run.setXPos(leadWidth + currMaxWidth);

        currMaxWidth += f.width(run);
        needsWordSpacing = isSpace && !previousCharacterIsSpace && i == len - 1;
      }
      ASSERT(lastWordBoundary == i);
      lastWordBoundary++;
    }
  }
  if (run)
    bidiResolver.runs().deleteRuns();

  if ((needsWordSpacing && len > 1) || (ignoringSpaces && !firstWord))
    currMaxWidth += wordSpacing;

  m_minWidth = std::max(currMinWidth, m_minWidth);
  m_maxWidth = std::max(currMaxWidth, m_maxWidth);

  if (!styleToUse.autoWrap())
    m_minWidth = m_maxWidth;

  if (styleToUse.whiteSpace() == EWhiteSpace::kPre) {
    if (firstLine)
      m_firstLineMinWidth = m_maxWidth;
    m_lastLineLineMinWidth = currMaxWidth;
  }

  const SimpleFontData* fontData = f.primaryFont();
  DCHECK(fontData);

  GlyphOverflow glyphOverflow;
  if (fontData) {
    glyphOverflow.setFromBounds(
        glyphBounds, fontData->getFontMetrics().floatAscent(),
        fontData->getFontMetrics().floatDescent(), m_maxWidth);
  }
  // We shouldn't change our mind once we "know".
  ASSERT(!m_knownToHaveNoOverflowAndNoFallbackFonts ||
         (fallbackFonts.isEmpty() && glyphOverflow.isApproximatelyZero()));
  m_knownToHaveNoOverflowAndNoFallbackFonts =
      fallbackFonts.isEmpty() && glyphOverflow.isApproximatelyZero();

  clearPreferredLogicalWidthsDirty();
}

bool LayoutText::isAllCollapsibleWhitespace() const {
  unsigned length = textLength();
  if (is8Bit()) {
    for (unsigned i = 0; i < length; ++i) {
      if (!style()->isCollapsibleWhiteSpace(characters8()[i]))
        return false;
    }
    return true;
  }
  for (unsigned i = 0; i < length; ++i) {
    if (!style()->isCollapsibleWhiteSpace(characters16()[i]))
      return false;
  }
  return true;
}

bool LayoutText::isRenderedCharacter(int offsetInNode) const {
  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
    if (offsetInNode < static_cast<int>(box->start()) &&
        !containsReversedText()) {
      // The offset we're looking for is before this node this means the offset
      // must be in content that is not laid out. Return false.
      return false;
    }
    if (offsetInNode >= static_cast<int>(box->start()) &&
        offsetInNode < static_cast<int>(box->start() + box->len()))
      return true;
  }

  return false;
}

bool LayoutText::containsOnlyWhitespace(unsigned from, unsigned len) const {
  ASSERT(m_text);
  StringImpl& text = *m_text.impl();
  unsigned currPos;
  for (currPos = from;
       currPos < from + len &&
       (text[currPos] == newlineCharacter || text[currPos] == spaceCharacter ||
        text[currPos] == tabulationCharacter);
       currPos++) {
  }
  return currPos >= (from + len);
}

FloatPoint LayoutText::firstRunOrigin() const {
  return IntPoint(firstRunX(), firstRunY());
}

float LayoutText::firstRunX() const {
  return m_firstTextBox ? m_firstTextBox->x().toFloat() : 0;
}

float LayoutText::firstRunY() const {
  return m_firstTextBox ? m_firstTextBox->y().toFloat() : 0;
}

void LayoutText::setSelectionState(SelectionState state) {
  LayoutObject::setSelectionState(state);

  if (canUpdateSelectionOnRootLineBoxes()) {
    if (state == SelectionStart || state == SelectionEnd ||
        state == SelectionBoth) {
      int startPos, endPos;
      selectionStartEnd(startPos, endPos);
      if (getSelectionState() == SelectionStart) {
        endPos = textLength();

        // to handle selection from end of text to end of line
        if (startPos && startPos == endPos)
          startPos = endPos - 1;
      } else if (getSelectionState() == SelectionEnd) {
        startPos = 0;
      }

      for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        if (box->isSelected(startPos, endPos)) {
          box->root().setHasSelectedChildren(true);
        }
      }
    } else {
      for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        box->root().setHasSelectedChildren(state == SelectionInside);
      }
    }
  }

  // The containing block can be null in case of an orphaned tree.
  LayoutBlock* containingBlock = this->containingBlock();
  if (containingBlock && !containingBlock->isLayoutView())
    containingBlock->setSelectionState(state);
}

void LayoutText::setTextWithOffset(PassRefPtr<StringImpl> text,
                                   unsigned offset,
                                   unsigned len,
                                   bool force) {
  if (!force && equal(m_text.impl(), text.get()))
    return;

  unsigned oldLen = textLength();
  unsigned newLen = text->length();
  int delta = newLen - oldLen;
  unsigned end = len ? offset + len - 1 : offset;

  RootInlineBox* firstRootBox = nullptr;
  RootInlineBox* lastRootBox = nullptr;

  bool dirtiedLines = false;

  // Dirty all text boxes that include characters in between offset and
  // offset+len.
  for (InlineTextBox* curr = firstTextBox(); curr; curr = curr->nextTextBox()) {
    // FIXME: This shouldn't rely on the end of a dirty line box. See
    // https://bugs.webkit.org/show_bug.cgi?id=97264
    // Text run is entirely before the affected range.
    if (curr->end() < offset)
      continue;

    // Text run is entirely after the affected range.
    if (curr->start() > end) {
      curr->offsetRun(delta);
      RootInlineBox* root = &curr->root();
      if (!firstRootBox) {
        firstRootBox = root;
        // The affected area was in between two runs. Go ahead and mark the root
        // box of the run after the affected area as dirty.
        firstRootBox->markDirty();
        dirtiedLines = true;
      }
      lastRootBox = root;
    } else if (curr->end() >= offset && curr->end() <= end) {
      // Text run overlaps with the left end of the affected range.
      curr->dirtyLineBoxes();
      dirtiedLines = true;
    } else if (curr->start() <= offset && curr->end() >= end) {
      // Text run subsumes the affected range.
      curr->dirtyLineBoxes();
      dirtiedLines = true;
    } else if (curr->start() <= end && curr->end() >= end) {
      // Text run overlaps with right end of the affected range.
      curr->dirtyLineBoxes();
      dirtiedLines = true;
    }
  }

  // Now we have to walk all of the clean lines and adjust their cached line
  // break information to reflect our updated offsets.
  if (lastRootBox)
    lastRootBox = lastRootBox->nextRootBox();
  if (firstRootBox) {
    RootInlineBox* prev = firstRootBox->prevRootBox();
    if (prev)
      firstRootBox = prev;
  } else if (lastTextBox()) {
    ASSERT(!lastRootBox);
    firstRootBox = &lastTextBox()->root();
    firstRootBox->markDirty();
    dirtiedLines = true;
  }
  for (RootInlineBox* curr = firstRootBox; curr && curr != lastRootBox;
       curr = curr->nextRootBox()) {
    if (curr->lineBreakObj().isEqual(this) && curr->lineBreakPos() > end)
      curr->setLineBreakPos(clampTo<int>(curr->lineBreakPos() + delta));
  }

  // If the text node is empty, dirty the line where new text will be inserted.
  if (!firstTextBox() && parent()) {
    parent()->dirtyLinesFromChangedChild(this);
    dirtiedLines = true;
  }

  m_linesDirty = dirtiedLines;
  setText(std::move(text), force || dirtiedLines);
}

void LayoutText::transformText() {
  if (RefPtr<StringImpl> textToTransform = originalText())
    setText(std::move(textToTransform), true);
}

static inline bool isInlineFlowOrEmptyText(const LayoutObject* o) {
  if (o->isLayoutInline())
    return true;
  if (!o->isText())
    return false;
  return toLayoutText(o)->text().isEmpty();
}

UChar LayoutText::previousCharacter() const {
  // find previous text layoutObject if one exists
  const LayoutObject* previousText = previousInPreOrder();
  for (; previousText; previousText = previousText->previousInPreOrder()) {
    if (!isInlineFlowOrEmptyText(previousText))
      break;
  }
  UChar prev = spaceCharacter;
  if (previousText && previousText->isText()) {
    if (StringImpl* previousString = toLayoutText(previousText)->text().impl())
      prev = (*previousString)[previousString->length() - 1];
  }
  return prev;
}

void LayoutText::addLayerHitTestRects(LayerHitTestRects&,
                                      const PaintLayer* currentLayer,
                                      const LayoutPoint& layerOffset,
                                      const LayoutRect& containerRect) const {
  // Text nodes aren't event targets, so don't descend any further.
}

void applyTextTransform(const ComputedStyle* style,
                        String& text,
                        UChar previousCharacter) {
  if (!style)
    return;

  switch (style->textTransform()) {
    case ETextTransform::kNone:
      break;
    case ETextTransform::kCapitalize:
      makeCapitalized(&text, previousCharacter);
      break;
    case ETextTransform::kUppercase:
      text = text.upper(style->locale());
      break;
    case ETextTransform::kLowercase:
      text = text.lower(style->locale());
      break;
  }
}

void LayoutText::setTextInternal(PassRefPtr<StringImpl> text) {
  ASSERT(text);
  m_text = std::move(text);

  if (style()) {
    applyTextTransform(style(), m_text, previousCharacter());

    // We use the same characters here as for list markers.
    // See the listMarkerText function in LayoutListMarker.cpp.
    switch (style()->textSecurity()) {
      case TSNONE:
        break;
      case TSCIRCLE:
        secureText(whiteBulletCharacter);
        break;
      case TSDISC:
        secureText(bulletCharacter);
        break;
      case TSSQUARE:
        secureText(blackSquareCharacter);
    }
  }

  ASSERT(m_text);
  ASSERT(!isBR() || (textLength() == 1 && m_text[0] == newlineCharacter));
}

void LayoutText::secureText(UChar mask) {
  if (!m_text.length())
    return;

  int lastTypedCharacterOffsetToReveal = -1;
  UChar revealedText;
  SecureTextTimer* secureTextTimer =
      gSecureTextTimers ? gSecureTextTimers->at(this) : 0;
  if (secureTextTimer && secureTextTimer->isActive()) {
    lastTypedCharacterOffsetToReveal =
        secureTextTimer->lastTypedCharacterOffset();
    if (lastTypedCharacterOffsetToReveal >= 0)
      revealedText = m_text[lastTypedCharacterOffsetToReveal];
  }

  m_text.fill(mask);
  if (lastTypedCharacterOffsetToReveal >= 0) {
    m_text.replace(lastTypedCharacterOffsetToReveal, 1,
                   String(&revealedText, 1));
    // m_text may be updated later before timer fires. We invalidate the
    // lastTypedCharacterOffset to avoid inconsistency.
    secureTextTimer->invalidate();
  }
}

void LayoutText::setText(PassRefPtr<StringImpl> text, bool force) {
  ASSERT(text);

  if (!force && equal(m_text.impl(), text.get()))
    return;

  setTextInternal(std::move(text));
  // If preferredLogicalWidthsDirty() of an orphan child is true,
  // LayoutObjectChildList::insertChildNode() fails to set true to owner.
  // To avoid that, we call setNeedsLayoutAndPrefWidthsRecalc() only if this
  // LayoutText has parent.
  if (parent())
    setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        LayoutInvalidationReason::TextChanged);
  m_knownToHaveNoOverflowAndNoFallbackFonts = false;

  if (AXObjectCache* cache = document().existingAXObjectCache())
    cache->textChanged(this);

  TextAutosizer* textAutosizer = document().textAutosizer();
  if (textAutosizer)
    textAutosizer->record(this);
}

void LayoutText::dirtyOrDeleteLineBoxesIfNeeded(bool fullLayout) {
  if (fullLayout)
    deleteTextBoxes();
  else if (!m_linesDirty)
    dirtyLineBoxes();
  m_linesDirty = false;
}

void LayoutText::dirtyLineBoxes() {
  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
    box->dirtyLineBoxes();
  m_linesDirty = false;
}

InlineTextBox* LayoutText::createTextBox(int start, unsigned short length) {
  return new InlineTextBox(LineLayoutItem(this), start, length);
}

InlineTextBox* LayoutText::createInlineTextBox(int start,
                                               unsigned short length) {
  InlineTextBox* textBox = createTextBox(start, length);
  if (!m_firstTextBox) {
    m_firstTextBox = m_lastTextBox = textBox;
  } else {
    m_lastTextBox->setNextTextBox(textBox);
    textBox->setPreviousTextBox(m_lastTextBox);
    m_lastTextBox = textBox;
  }
  return textBox;
}

void LayoutText::positionLineBox(InlineBox* box) {
  InlineTextBox* s = toInlineTextBox(box);

  // FIXME: should not be needed!!!
  if (!s->len()) {
    // We want the box to be destroyed.
    s->remove(DontMarkLineBoxes);
    if (m_firstTextBox == s)
      m_firstTextBox = s->nextTextBox();
    else
      s->prevTextBox()->setNextTextBox(s->nextTextBox());
    if (m_lastTextBox == s)
      m_lastTextBox = s->prevTextBox();
    else
      s->nextTextBox()->setPreviousTextBox(s->prevTextBox());
    s->destroy();
    return;
  }

  m_containsReversedText |= !s->isLeftToRightDirection();
}

float LayoutText::width(unsigned from,
                        unsigned len,
                        LayoutUnit xPos,
                        TextDirection textDirection,
                        bool firstLine,
                        HashSet<const SimpleFontData*>* fallbackFonts,
                        FloatRect* glyphBounds) const {
  if (from >= textLength())
    return 0;

  if (len > textLength() || from + len > textLength())
    len = textLength() - from;

  return width(from, len, style(firstLine)->font(), xPos, textDirection,
               fallbackFonts, glyphBounds);
}

float LayoutText::width(unsigned from,
                        unsigned len,
                        const Font& f,
                        LayoutUnit xPos,
                        TextDirection textDirection,
                        HashSet<const SimpleFontData*>* fallbackFonts,
                        FloatRect* glyphBounds) const {
  ASSERT(from + len <= textLength());
  if (!textLength())
    return 0;

  const SimpleFontData* fontData = f.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return 0;

  float w;
  if (&f == &style()->font()) {
    if (!style()->preserveNewline() && !from && len == textLength()) {
      if (fallbackFonts) {
        ASSERT(glyphBounds);
        if (preferredLogicalWidthsDirty() ||
            !m_knownToHaveNoOverflowAndNoFallbackFonts)
          const_cast<LayoutText*>(this)->computePreferredLogicalWidths(
              0, *fallbackFonts, *glyphBounds);
        else
          *glyphBounds =
              FloatRect(0, -fontData->getFontMetrics().floatAscent(),
                        m_maxWidth, fontData->getFontMetrics().floatHeight());
        w = m_maxWidth;
      } else {
        w = maxLogicalWidth();
      }
    } else {
      w = widthFromFont(f, from, len, xPos.toFloat(), 0, textDirection,
                        fallbackFonts, glyphBounds);
    }
  } else {
    TextRun run =
        constructTextRun(f, this, from, len, styleRef(), textDirection);
    run.setCharactersLength(textLength() - from);
    ASSERT(run.charactersLength() >= run.length());

    run.setTabSize(!style()->collapseWhiteSpace(), style()->getTabSize());
    run.setXPos(xPos.toFloat());
    w = f.width(run, fallbackFonts, glyphBounds);
  }

  return w;
}

LayoutRect LayoutText::linesBoundingBox() const {
  LayoutRect result;

  ASSERT(!firstTextBox() ==
         !lastTextBox());  // Either both are null or both exist.
  if (firstTextBox() && lastTextBox()) {
    // Return the width of the minimal left side and the maximal right side.
    float logicalLeftSide = 0;
    float logicalRightSide = 0;
    for (InlineTextBox* curr = firstTextBox(); curr;
         curr = curr->nextTextBox()) {
      if (curr == firstTextBox() || curr->logicalLeft() < logicalLeftSide)
        logicalLeftSide = curr->logicalLeft().toFloat();
      if (curr == firstTextBox() || curr->logicalRight() > logicalRightSide)
        logicalRightSide = curr->logicalRight().toFloat();
    }

    bool isHorizontal = style()->isHorizontalWritingMode();

    float x = isHorizontal ? logicalLeftSide : firstTextBox()->x().toFloat();
    float y = isHorizontal ? firstTextBox()->y().toFloat() : logicalLeftSide;
    float width = isHorizontal ? logicalRightSide - logicalLeftSide
                               : lastTextBox()->logicalBottom() - x;
    float height = isHorizontal ? lastTextBox()->logicalBottom() - y
                                : logicalRightSide - logicalLeftSide;
    result = enclosingLayoutRect(FloatRect(x, y, width, height));
  }

  return result;
}

LayoutRect LayoutText::visualOverflowRect() const {
  if (!firstTextBox())
    return LayoutRect();

  // Return the width of the minimal left side and the maximal right side.
  LayoutUnit logicalLeftSide = LayoutUnit::max();
  LayoutUnit logicalRightSide = LayoutUnit::min();
  for (InlineTextBox* curr = firstTextBox(); curr; curr = curr->nextTextBox()) {
    LayoutRect logicalVisualOverflow = curr->logicalOverflowRect();
    logicalLeftSide = std::min(logicalLeftSide, logicalVisualOverflow.x());
    logicalRightSide = std::max(logicalRightSide, logicalVisualOverflow.maxX());
  }

  LayoutUnit logicalTop = firstTextBox()->logicalTopVisualOverflow();
  LayoutUnit logicalWidth = logicalRightSide - logicalLeftSide;
  LayoutUnit logicalHeight =
      lastTextBox()->logicalBottomVisualOverflow() - logicalTop;

  // Inflate visual overflow if we have adjusted ascent/descent causing the
  // painted glyphs to overflow the layout geometries based on the adjusted
  // ascent/descent.
  unsigned inflation_for_ascent = 0;
  unsigned inflation_for_descent = 0;
  const auto* font_data =
      styleRef(firstTextBox()->isFirstLineStyle()).font().primaryFont();
  if (font_data)
    inflation_for_ascent = font_data->VisualOverflowInflationForAscent();
  if (lastTextBox()->isFirstLineStyle() != firstTextBox()->isFirstLineStyle()) {
    font_data =
        styleRef(lastTextBox()->isFirstLineStyle()).font().primaryFont();
  }
  if (font_data)
    inflation_for_descent = font_data->VisualOverflowInflationForDescent();
  logicalTop -= LayoutUnit(inflation_for_ascent);
  logicalHeight += LayoutUnit(inflation_for_ascent + inflation_for_descent);

  LayoutRect rect(logicalLeftSide, logicalTop, logicalWidth, logicalHeight);
  if (!style()->isHorizontalWritingMode())
    rect = rect.transposedRect();
  return rect;
}

LayoutRect LayoutText::localVisualRect() const {
  if (style()->visibility() != EVisibility::kVisible)
    return LayoutRect();

  return unionRect(visualOverflowRect(), localSelectionRect());
}

LayoutRect LayoutText::localSelectionRect() const {
  ASSERT(!needsLayout());

  if (getSelectionState() == SelectionNone)
    return LayoutRect();
  LayoutBlock* cb = containingBlock();
  if (!cb)
    return LayoutRect();

  // Now calculate startPos and endPos for painting selection.
  // We include a selection while endPos > 0
  int startPos, endPos;
  if (getSelectionState() == SelectionInside) {
    // We are fully selected.
    startPos = 0;
    endPos = textLength();
  } else {
    selectionStartEnd(startPos, endPos);
    if (getSelectionState() == SelectionStart)
      endPos = textLength();
    else if (getSelectionState() == SelectionEnd)
      startPos = 0;
  }

  LayoutRect rect;

  if (startPos == endPos)
    return rect;

  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
    rect.unite(box->localSelectionRect(startPos, endPos));
    rect.unite(LayoutRect(ellipsisRectForBox(box, startPos, endPos)));
  }

  return rect;
}

int LayoutText::caretMinOffset() const {
  InlineTextBox* box = firstTextBox();
  if (!box)
    return 0;
  int minOffset = box->start();
  for (box = box->nextTextBox(); box; box = box->nextTextBox())
    minOffset = std::min<int>(minOffset, box->start());
  return minOffset;
}

int LayoutText::caretMaxOffset() const {
  InlineTextBox* box = lastTextBox();
  if (!lastTextBox())
    return textLength();

  int maxOffset = box->start() + box->len();
  for (box = box->prevTextBox(); box; box = box->prevTextBox())
    maxOffset = std::max<int>(maxOffset, box->start() + box->len());
  return maxOffset;
}

unsigned LayoutText::resolvedTextLength() const {
  int len = 0;
  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
    len += box->len();
  return len;
}

#if DCHECK_IS_ON()

void LayoutText::checkConsistency() const {
#ifdef CHECK_CONSISTENCY
  const InlineTextBox* prev = nullptr;
  for (const InlineTextBox* child = m_firstTextBox; child;
       child = child->nextTextBox()) {
    ASSERT(child->getLineLayoutItem().isEqual(this));
    ASSERT(child->prevTextBox() == prev);
    prev = child;
  }
  ASSERT(prev == m_lastTextBox);
#endif
}

#endif

void LayoutText::momentarilyRevealLastTypedCharacter(
    unsigned lastTypedCharacterOffset) {
  if (!gSecureTextTimers)
    gSecureTextTimers = new SecureTextTimerMap;

  SecureTextTimer* secureTextTimer = gSecureTextTimers->at(this);
  if (!secureTextTimer) {
    secureTextTimer = new SecureTextTimer(this);
    gSecureTextTimers->insert(this, secureTextTimer);
  }
  secureTextTimer->restartWithNewText(lastTypedCharacterOffset);
}

PassRefPtr<AbstractInlineTextBox> LayoutText::firstAbstractInlineTextBox() {
  return AbstractInlineTextBox::getOrCreate(LineLayoutText(this),
                                            m_firstTextBox);
}

void LayoutText::invalidateDisplayItemClients(
    PaintInvalidationReason invalidationReason) const {
  ObjectPaintInvalidator paintInvalidator(*this);
  paintInvalidator.invalidateDisplayItemClient(*this, invalidationReason);

  for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
    paintInvalidator.invalidateDisplayItemClient(*box, invalidationReason);
    if (box->truncation() != cNoTruncation) {
      if (EllipsisBox* ellipsisBox = box->root().ellipsisBox())
        paintInvalidator.invalidateDisplayItemClient(*ellipsisBox,
                                                     invalidationReason);
    }
  }
}

// TODO(lunalu): Would be better to dump the bounding box x and y rather than
// the first run's x and y, but that would involve updating many test results.
LayoutRect LayoutText::debugRect() const {
  IntRect linesBox = enclosingIntRect(linesBoundingBox());
  LayoutRect rect = LayoutRect(
      IntRect(firstRunX(), firstRunY(), linesBox.width(), linesBox.height()));
  LayoutBlock* block = containingBlock();
  if (block && hasTextBoxes())
    block->adjustChildDebugRect(rect);

  return rect;
}

}  // namespace blink
