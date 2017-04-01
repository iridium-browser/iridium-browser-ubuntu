/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@nypop.com>.
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSPrimitiveValueMappings_h
#define CSSPrimitiveValueMappings_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSReflectionDirection.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/LineClampValue.h"
#include "core/style/SVGComputedStyleDefs.h"
#include "platform/Length.h"
#include "platform/ThemeTypes.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/fonts/TextRenderingMode.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextRun.h"
#include "platform/text/UnicodeBidi.h"
#include "platform/text/WritingMode.h"
#include "public/platform/WebBlendMode.h"
#include "wtf/MathExtras.h"

namespace blink {

// TODO(sashab): Move these to CSSPrimitiveValue.h.
template <>
inline short CSSPrimitiveValue::convertTo() const {
  ASSERT(isNumber());
  return clampTo<short>(getDoubleValue());
}

template <>
inline unsigned short CSSPrimitiveValue::convertTo() const {
  ASSERT(isNumber());
  return clampTo<unsigned short>(getDoubleValue());
}

template <>
inline int CSSPrimitiveValue::convertTo() const {
  ASSERT(isNumber());
  return clampTo<int>(getDoubleValue());
}

template <>
inline unsigned CSSPrimitiveValue::convertTo() const {
  ASSERT(isNumber());
  return clampTo<unsigned>(getDoubleValue());
}

template <>
inline float CSSPrimitiveValue::convertTo() const {
  ASSERT(isNumber());
  return clampTo<float>(getDoubleValue());
}

template <>
inline CSSPrimitiveValue::CSSPrimitiveValue(LineClampValue i)
    : CSSValue(PrimitiveClass) {
  init(i.isPercentage() ? UnitType::Percentage : UnitType::Integer);
  m_value.num = static_cast<double>(i.value());
}

template <>
inline LineClampValue CSSPrimitiveValue::convertTo() const {
  if (type() == UnitType::Integer)
    return LineClampValue(clampTo<int>(m_value.num), LineClampLineCount);

  if (type() == UnitType::Percentage)
    return LineClampValue(clampTo<int>(m_value.num), LineClampPercentage);

  ASSERT_NOT_REACHED();
  return LineClampValue();
}

// TODO(sashab): Move these to CSSIdentifierValueMappings.h, and update to use
// the CSSValuePool.
template <>
inline CSSIdentifierValue::CSSIdentifierValue(CSSReflectionDirection e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ReflectionAbove:
      m_valueID = CSSValueAbove;
      break;
    case ReflectionBelow:
      m_valueID = CSSValueBelow;
      break;
    case ReflectionLeft:
      m_valueID = CSSValueLeft;
      break;
    case ReflectionRight:
      m_valueID = CSSValueRight;
  }
}

template <>
inline CSSReflectionDirection CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAbove:
      return ReflectionAbove;
    case CSSValueBelow:
      return ReflectionBelow;
    case CSSValueLeft:
      return ReflectionLeft;
    case CSSValueRight:
      return ReflectionRight;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return ReflectionBelow;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ColumnFill columnFill)
    : CSSValue(IdentifierClass) {
  switch (columnFill) {
    case ColumnFillAuto:
      m_valueID = CSSValueAuto;
      break;
    case ColumnFillBalance:
      m_valueID = CSSValueBalance;
      break;
  }
}

template <>
inline ColumnFill CSSIdentifierValue::convertTo() const {
  if (m_valueID == CSSValueBalance)
    return ColumnFillBalance;
  if (m_valueID == CSSValueAuto)
    return ColumnFillAuto;
  ASSERT_NOT_REACHED();
  return ColumnFillBalance;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ColumnSpan columnSpan)
    : CSSValue(IdentifierClass) {
  switch (columnSpan) {
    case ColumnSpanAll:
      m_valueID = CSSValueAll;
      break;
    case ColumnSpanNone:
      m_valueID = CSSValueNone;
      break;
  }
}

template <>
inline ColumnSpan CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAll:
      return ColumnSpanAll;
    default:
      ASSERT_NOT_REACHED();
    // fall-through
    case CSSValueNone:
      return ColumnSpanNone;
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EPrintColorAdjust value)
    : CSSValue(IdentifierClass) {
  switch (value) {
    case EPrintColorAdjust::kExact:
      m_valueID = CSSValueExact;
      break;
    case EPrintColorAdjust::kEconomy:
      m_valueID = CSSValueEconomy;
      break;
  }
}

template <>
inline EPrintColorAdjust CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueEconomy:
      return EPrintColorAdjust::kEconomy;
    case CSSValueExact:
      return EPrintColorAdjust::kExact;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EPrintColorAdjust::kEconomy;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBorderStyle e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case BorderStyleNone:
      m_valueID = CSSValueNone;
      break;
    case BorderStyleHidden:
      m_valueID = CSSValueHidden;
      break;
    case BorderStyleInset:
      m_valueID = CSSValueInset;
      break;
    case BorderStyleGroove:
      m_valueID = CSSValueGroove;
      break;
    case BorderStyleRidge:
      m_valueID = CSSValueRidge;
      break;
    case BorderStyleOutset:
      m_valueID = CSSValueOutset;
      break;
    case BorderStyleDotted:
      m_valueID = CSSValueDotted;
      break;
    case BorderStyleDashed:
      m_valueID = CSSValueDashed;
      break;
    case BorderStyleSolid:
      m_valueID = CSSValueSolid;
      break;
    case BorderStyleDouble:
      m_valueID = CSSValueDouble;
      break;
  }
}

template <>
inline EBorderStyle CSSIdentifierValue::convertTo() const {
  if (m_valueID == CSSValueAuto)  // Valid for CSS outline-style
    return BorderStyleDotted;
  return (EBorderStyle)(m_valueID - CSSValueNone);
}

template <>
inline OutlineIsAuto CSSIdentifierValue::convertTo() const {
  if (m_valueID == CSSValueAuto)
    return OutlineIsAutoOn;
  return OutlineIsAutoOff;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(CompositeOperator e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case CompositeClear:
      m_valueID = CSSValueClear;
      break;
    case CompositeCopy:
      m_valueID = CSSValueCopy;
      break;
    case CompositeSourceOver:
      m_valueID = CSSValueSourceOver;
      break;
    case CompositeSourceIn:
      m_valueID = CSSValueSourceIn;
      break;
    case CompositeSourceOut:
      m_valueID = CSSValueSourceOut;
      break;
    case CompositeSourceAtop:
      m_valueID = CSSValueSourceAtop;
      break;
    case CompositeDestinationOver:
      m_valueID = CSSValueDestinationOver;
      break;
    case CompositeDestinationIn:
      m_valueID = CSSValueDestinationIn;
      break;
    case CompositeDestinationOut:
      m_valueID = CSSValueDestinationOut;
      break;
    case CompositeDestinationAtop:
      m_valueID = CSSValueDestinationAtop;
      break;
    case CompositeXOR:
      m_valueID = CSSValueXor;
      break;
    case CompositePlusLighter:
      m_valueID = CSSValuePlusLighter;
      break;
    default:
      ASSERT_NOT_REACHED();
      break;
  }
}

template <>
inline CompositeOperator CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueClear:
      return CompositeClear;
    case CSSValueCopy:
      return CompositeCopy;
    case CSSValueSourceOver:
      return CompositeSourceOver;
    case CSSValueSourceIn:
      return CompositeSourceIn;
    case CSSValueSourceOut:
      return CompositeSourceOut;
    case CSSValueSourceAtop:
      return CompositeSourceAtop;
    case CSSValueDestinationOver:
      return CompositeDestinationOver;
    case CSSValueDestinationIn:
      return CompositeDestinationIn;
    case CSSValueDestinationOut:
      return CompositeDestinationOut;
    case CSSValueDestinationAtop:
      return CompositeDestinationAtop;
    case CSSValueXor:
      return CompositeXOR;
    case CSSValuePlusLighter:
      return CompositePlusLighter;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return CompositeClear;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ControlPart e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case NoControlPart:
      m_valueID = CSSValueNone;
      break;
    case CheckboxPart:
      m_valueID = CSSValueCheckbox;
      break;
    case RadioPart:
      m_valueID = CSSValueRadio;
      break;
    case PushButtonPart:
      m_valueID = CSSValuePushButton;
      break;
    case SquareButtonPart:
      m_valueID = CSSValueSquareButton;
      break;
    case ButtonPart:
      m_valueID = CSSValueButton;
      break;
    case ButtonBevelPart:
      m_valueID = CSSValueButtonBevel;
      break;
    case InnerSpinButtonPart:
      m_valueID = CSSValueInnerSpinButton;
      break;
    case ListboxPart:
      m_valueID = CSSValueListbox;
      break;
    case ListItemPart:
      m_valueID = CSSValueListitem;
      break;
    case MediaEnterFullscreenButtonPart:
      m_valueID = CSSValueMediaEnterFullscreenButton;
      break;
    case MediaExitFullscreenButtonPart:
      m_valueID = CSSValueMediaExitFullscreenButton;
      break;
    case MediaPlayButtonPart:
      m_valueID = CSSValueMediaPlayButton;
      break;
    case MediaOverlayPlayButtonPart:
      m_valueID = CSSValueMediaOverlayPlayButton;
      break;
    case MediaMuteButtonPart:
      m_valueID = CSSValueMediaMuteButton;
      break;
    case MediaToggleClosedCaptionsButtonPart:
      m_valueID = CSSValueMediaToggleClosedCaptionsButton;
      break;
    case MediaCastOffButtonPart:
      m_valueID = CSSValueInternalMediaCastOffButton;
      break;
    case MediaOverlayCastOffButtonPart:
      m_valueID = CSSValueInternalMediaOverlayCastOffButton;
      break;
    case MediaSliderPart:
      m_valueID = CSSValueMediaSlider;
      break;
    case MediaSliderThumbPart:
      m_valueID = CSSValueMediaSliderthumb;
      break;
    case MediaVolumeSliderContainerPart:
      m_valueID = CSSValueMediaVolumeSliderContainer;
      break;
    case MediaVolumeSliderPart:
      m_valueID = CSSValueMediaVolumeSlider;
      break;
    case MediaVolumeSliderThumbPart:
      m_valueID = CSSValueMediaVolumeSliderthumb;
      break;
    case MediaControlsBackgroundPart:
      m_valueID = CSSValueMediaControlsBackground;
      break;
    case MediaControlsFullscreenBackgroundPart:
      m_valueID = CSSValueMediaControlsFullscreenBackground;
      break;
    case MediaFullscreenVolumeSliderPart:
      m_valueID = CSSValueMediaFullscreenVolumeSlider;
      break;
    case MediaFullscreenVolumeSliderThumbPart:
      m_valueID = CSSValueMediaFullscreenVolumeSliderThumb;
      break;
    case MediaCurrentTimePart:
      m_valueID = CSSValueMediaCurrentTimeDisplay;
      break;
    case MediaTimeRemainingPart:
      m_valueID = CSSValueMediaTimeRemainingDisplay;
      break;
    case MediaTrackSelectionCheckmarkPart:
      m_valueID = CSSValueInternalMediaTrackSelectionCheckmark;
      break;
    case MediaClosedCaptionsIconPart:
      m_valueID = CSSValueInternalMediaClosedCaptionsIcon;
      break;
    case MediaSubtitlesIconPart:
      m_valueID = CSSValueInternalMediaSubtitlesIcon;
      break;
    case MediaOverflowMenuButtonPart:
      m_valueID = CSSValueInternalMediaOverflowButton;
      break;
    case MediaDownloadIconPart:
      m_valueID = CSSValueInternalMediaDownloadButton;
      break;
    case MenulistPart:
      m_valueID = CSSValueMenulist;
      break;
    case MenulistButtonPart:
      m_valueID = CSSValueMenulistButton;
      break;
    case MenulistTextPart:
      m_valueID = CSSValueMenulistText;
      break;
    case MenulistTextFieldPart:
      m_valueID = CSSValueMenulistTextfield;
      break;
    case MeterPart:
      m_valueID = CSSValueMeter;
      break;
    case ProgressBarPart:
      m_valueID = CSSValueProgressBar;
      break;
    case ProgressBarValuePart:
      m_valueID = CSSValueProgressBarValue;
      break;
    case SliderHorizontalPart:
      m_valueID = CSSValueSliderHorizontal;
      break;
    case SliderVerticalPart:
      m_valueID = CSSValueSliderVertical;
      break;
    case SliderThumbHorizontalPart:
      m_valueID = CSSValueSliderthumbHorizontal;
      break;
    case SliderThumbVerticalPart:
      m_valueID = CSSValueSliderthumbVertical;
      break;
    case CaretPart:
      m_valueID = CSSValueCaret;
      break;
    case SearchFieldPart:
      m_valueID = CSSValueSearchfield;
      break;
    case SearchFieldCancelButtonPart:
      m_valueID = CSSValueSearchfieldCancelButton;
      break;
    case TextFieldPart:
      m_valueID = CSSValueTextfield;
      break;
    case TextAreaPart:
      m_valueID = CSSValueTextarea;
      break;
    case CapsLockIndicatorPart:
      m_valueID = CSSValueCapsLockIndicator;
      break;
  }
}

template <>
inline ControlPart CSSIdentifierValue::convertTo() const {
  if (m_valueID == CSSValueNone)
    return NoControlPart;
  return ControlPart(m_valueID - CSSValueCheckbox + 1);
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBackfaceVisibility e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case BackfaceVisibilityVisible:
      m_valueID = CSSValueVisible;
      break;
    case BackfaceVisibilityHidden:
      m_valueID = CSSValueHidden;
      break;
  }
}

template <>
inline EBackfaceVisibility CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueVisible:
      return BackfaceVisibilityVisible;
    case CSSValueHidden:
      return BackfaceVisibilityHidden;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return BackfaceVisibilityHidden;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillAttachment e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ScrollBackgroundAttachment:
      m_valueID = CSSValueScroll;
      break;
    case LocalBackgroundAttachment:
      m_valueID = CSSValueLocal;
      break;
    case FixedBackgroundAttachment:
      m_valueID = CSSValueFixed;
      break;
  }
}

template <>
inline EFillAttachment CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueScroll:
      return ScrollBackgroundAttachment;
    case CSSValueLocal:
      return LocalBackgroundAttachment;
    case CSSValueFixed:
      return FixedBackgroundAttachment;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return ScrollBackgroundAttachment;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillBox e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case BorderFillBox:
      m_valueID = CSSValueBorderBox;
      break;
    case PaddingFillBox:
      m_valueID = CSSValuePaddingBox;
      break;
    case ContentFillBox:
      m_valueID = CSSValueContentBox;
      break;
    case TextFillBox:
      m_valueID = CSSValueText;
      break;
  }
}

template <>
inline EFillBox CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueBorder:
    case CSSValueBorderBox:
      return BorderFillBox;
    case CSSValuePadding:
    case CSSValuePaddingBox:
      return PaddingFillBox;
    case CSSValueContent:
    case CSSValueContentBox:
      return ContentFillBox;
    case CSSValueText:
      return TextFillBox;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return BorderFillBox;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillRepeat e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case RepeatFill:
      m_valueID = CSSValueRepeat;
      break;
    case NoRepeatFill:
      m_valueID = CSSValueNoRepeat;
      break;
    case RoundFill:
      m_valueID = CSSValueRound;
      break;
    case SpaceFill:
      m_valueID = CSSValueSpace;
      break;
  }
}

template <>
inline EFillRepeat CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueRepeat:
      return RepeatFill;
    case CSSValueNoRepeat:
      return NoRepeatFill;
    case CSSValueRound:
      return RoundFill;
    case CSSValueSpace:
      return SpaceFill;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return RepeatFill;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBoxPack e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case BoxPackStart:
      m_valueID = CSSValueStart;
      break;
    case BoxPackCenter:
      m_valueID = CSSValueCenter;
      break;
    case BoxPackEnd:
      m_valueID = CSSValueEnd;
      break;
    case BoxPackJustify:
      m_valueID = CSSValueJustify;
      break;
  }
}

template <>
inline EBoxPack CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueStart:
      return BoxPackStart;
    case CSSValueEnd:
      return BoxPackEnd;
    case CSSValueCenter:
      return BoxPackCenter;
    case CSSValueJustify:
      return BoxPackJustify;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return BoxPackJustify;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBoxAlignment e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case BSTRETCH:
      m_valueID = CSSValueStretch;
      break;
    case BSTART:
      m_valueID = CSSValueStart;
      break;
    case BCENTER:
      m_valueID = CSSValueCenter;
      break;
    case BEND:
      m_valueID = CSSValueEnd;
      break;
    case BBASELINE:
      m_valueID = CSSValueBaseline;
      break;
  }
}

template <>
inline EBoxAlignment CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueStretch:
      return BSTRETCH;
    case CSSValueStart:
      return BSTART;
    case CSSValueEnd:
      return BEND;
    case CSSValueCenter:
      return BCENTER;
    case CSSValueBaseline:
      return BBASELINE;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return BSTRETCH;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBoxDecorationBreak e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case BoxDecorationBreakSlice:
      m_valueID = CSSValueSlice;
      break;
    case BoxDecorationBreakClone:
      m_valueID = CSSValueClone;
      break;
  }
}

template <>
inline EBoxDecorationBreak CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueSlice:
      return BoxDecorationBreakSlice;
    case CSSValueClone:
      return BoxDecorationBreakClone;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return BoxDecorationBreakSlice;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(BackgroundEdgeOrigin e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TopEdge:
      m_valueID = CSSValueTop;
      break;
    case RightEdge:
      m_valueID = CSSValueRight;
      break;
    case BottomEdge:
      m_valueID = CSSValueBottom;
      break;
    case LeftEdge:
      m_valueID = CSSValueLeft;
      break;
  }
}

template <>
inline BackgroundEdgeOrigin CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueTop:
      return TopEdge;
    case CSSValueRight:
      return RightEdge;
    case CSSValueBottom:
      return BottomEdge;
    case CSSValueLeft:
      return LeftEdge;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TopEdge;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBoxSizing e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EBoxSizing::kBorderBox:
      m_valueID = CSSValueBorderBox;
      break;
    case EBoxSizing::kContentBox:
      m_valueID = CSSValueContentBox;
      break;
  }
}

template <>
inline EBoxSizing CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueBorderBox:
      return EBoxSizing::kBorderBox;
    case CSSValueContentBox:
      return EBoxSizing::kContentBox;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EBoxSizing::kBorderBox;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBoxDirection e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EBoxDirection::kNormal:
      m_valueID = CSSValueNormal;
      break;
    case EBoxDirection::kReverse:
      m_valueID = CSSValueReverse;
      break;
  }
}

template <>
inline EBoxDirection CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNormal:
      return EBoxDirection::kNormal;
    case CSSValueReverse:
      return EBoxDirection::kReverse;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EBoxDirection::kNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBoxLines e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case SINGLE:
      m_valueID = CSSValueSingle;
      break;
    case MULTIPLE:
      m_valueID = CSSValueMultiple;
      break;
  }
}

template <>
inline EBoxLines CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueSingle:
      return SINGLE;
    case CSSValueMultiple:
      return MULTIPLE;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return SINGLE;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBoxOrient e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case HORIZONTAL:
      m_valueID = CSSValueHorizontal;
      break;
    case VERTICAL:
      m_valueID = CSSValueVertical;
      break;
  }
}

template <>
inline EBoxOrient CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueHorizontal:
    case CSSValueInlineAxis:
      return HORIZONTAL;
    case CSSValueVertical:
    case CSSValueBlockAxis:
      return VERTICAL;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return HORIZONTAL;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ECaptionSide e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ECaptionSide::kLeft:
      m_valueID = CSSValueLeft;
      break;
    case ECaptionSide::kRight:
      m_valueID = CSSValueRight;
      break;
    case ECaptionSide::kTop:
      m_valueID = CSSValueTop;
      break;
    case ECaptionSide::kBottom:
      m_valueID = CSSValueBottom;
      break;
  }
}

template <>
inline ECaptionSide CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueLeft:
      return ECaptionSide::kLeft;
    case CSSValueRight:
      return ECaptionSide::kRight;
    case CSSValueTop:
      return ECaptionSide::kTop;
    case CSSValueBottom:
      return ECaptionSide::kBottom;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return ECaptionSide::kTop;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EClear e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ClearNone:
      m_valueID = CSSValueNone;
      break;
    case ClearLeft:
      m_valueID = CSSValueLeft;
      break;
    case ClearRight:
      m_valueID = CSSValueRight;
      break;
    case ClearBoth:
      m_valueID = CSSValueBoth;
      break;
  }
}

template <>
inline EClear CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return ClearNone;
    case CSSValueLeft:
      return ClearLeft;
    case CSSValueRight:
      return ClearRight;
    case CSSValueBoth:
      return ClearBoth;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return ClearNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ECursor e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ECursor::Auto:
      m_valueID = CSSValueAuto;
      break;
    case ECursor::Cross:
      m_valueID = CSSValueCrosshair;
      break;
    case ECursor::Default:
      m_valueID = CSSValueDefault;
      break;
    case ECursor::Pointer:
      m_valueID = CSSValuePointer;
      break;
    case ECursor::Move:
      m_valueID = CSSValueMove;
      break;
    case ECursor::Cell:
      m_valueID = CSSValueCell;
      break;
    case ECursor::VerticalText:
      m_valueID = CSSValueVerticalText;
      break;
    case ECursor::ContextMenu:
      m_valueID = CSSValueContextMenu;
      break;
    case ECursor::Alias:
      m_valueID = CSSValueAlias;
      break;
    case ECursor::Copy:
      m_valueID = CSSValueCopy;
      break;
    case ECursor::None:
      m_valueID = CSSValueNone;
      break;
    case ECursor::Progress:
      m_valueID = CSSValueProgress;
      break;
    case ECursor::NoDrop:
      m_valueID = CSSValueNoDrop;
      break;
    case ECursor::NotAllowed:
      m_valueID = CSSValueNotAllowed;
      break;
    case ECursor::ZoomIn:
      m_valueID = CSSValueZoomIn;
      break;
    case ECursor::ZoomOut:
      m_valueID = CSSValueZoomOut;
      break;
    case ECursor::EResize:
      m_valueID = CSSValueEResize;
      break;
    case ECursor::NeResize:
      m_valueID = CSSValueNeResize;
      break;
    case ECursor::NwResize:
      m_valueID = CSSValueNwResize;
      break;
    case ECursor::NResize:
      m_valueID = CSSValueNResize;
      break;
    case ECursor::SeResize:
      m_valueID = CSSValueSeResize;
      break;
    case ECursor::SwResize:
      m_valueID = CSSValueSwResize;
      break;
    case ECursor::SResize:
      m_valueID = CSSValueSResize;
      break;
    case ECursor::WResize:
      m_valueID = CSSValueWResize;
      break;
    case ECursor::EwResize:
      m_valueID = CSSValueEwResize;
      break;
    case ECursor::NsResize:
      m_valueID = CSSValueNsResize;
      break;
    case ECursor::NeswResize:
      m_valueID = CSSValueNeswResize;
      break;
    case ECursor::NwseResize:
      m_valueID = CSSValueNwseResize;
      break;
    case ECursor::ColResize:
      m_valueID = CSSValueColResize;
      break;
    case ECursor::RowResize:
      m_valueID = CSSValueRowResize;
      break;
    case ECursor::Text:
      m_valueID = CSSValueText;
      break;
    case ECursor::Wait:
      m_valueID = CSSValueWait;
      break;
    case ECursor::Help:
      m_valueID = CSSValueHelp;
      break;
    case ECursor::AllScroll:
      m_valueID = CSSValueAllScroll;
      break;
    case ECursor::WebkitGrab:
      m_valueID = CSSValueWebkitGrab;
      break;
    case ECursor::WebkitGrabbing:
      m_valueID = CSSValueWebkitGrabbing;
      break;
  }
}

template <>
inline ECursor CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return ECursor::Auto;
    case CSSValueCrosshair:
      return ECursor::Cross;
    case CSSValueDefault:
      return ECursor::Default;
    case CSSValuePointer:
      return ECursor::Pointer;
    case CSSValueMove:
      return ECursor::Move;
    case CSSValueCell:
      return ECursor::Cell;
    case CSSValueVerticalText:
      return ECursor::VerticalText;
    case CSSValueContextMenu:
      return ECursor::ContextMenu;
    case CSSValueAlias:
      return ECursor::Alias;
    case CSSValueCopy:
      return ECursor::Copy;
    case CSSValueNone:
      return ECursor::None;
    case CSSValueProgress:
      return ECursor::Progress;
    case CSSValueNoDrop:
      return ECursor::NoDrop;
    case CSSValueNotAllowed:
      return ECursor::NotAllowed;
    case CSSValueZoomIn:
    case CSSValueWebkitZoomIn:
      return ECursor::ZoomIn;
    case CSSValueZoomOut:
    case CSSValueWebkitZoomOut:
      return ECursor::ZoomOut;
    case CSSValueEResize:
      return ECursor::EResize;
    case CSSValueNeResize:
      return ECursor::NeResize;
    case CSSValueNwResize:
      return ECursor::NwResize;
    case CSSValueNResize:
      return ECursor::NResize;
    case CSSValueSeResize:
      return ECursor::SeResize;
    case CSSValueSwResize:
      return ECursor::SwResize;
    case CSSValueSResize:
      return ECursor::SResize;
    case CSSValueWResize:
      return ECursor::WResize;
    case CSSValueEwResize:
      return ECursor::EwResize;
    case CSSValueNsResize:
      return ECursor::NsResize;
    case CSSValueNeswResize:
      return ECursor::NeswResize;
    case CSSValueNwseResize:
      return ECursor::NwseResize;
    case CSSValueColResize:
      return ECursor::ColResize;
    case CSSValueRowResize:
      return ECursor::RowResize;
    case CSSValueText:
      return ECursor::Text;
    case CSSValueWait:
      return ECursor::Wait;
    case CSSValueHelp:
      return ECursor::Help;
    case CSSValueAllScroll:
      return ECursor::AllScroll;
    case CSSValueWebkitGrab:
      return ECursor::WebkitGrab;
    case CSSValueWebkitGrabbing:
      return ECursor::WebkitGrabbing;
    default:
      NOTREACHED();
      return ECursor::Auto;
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EDisplay e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EDisplay::Inline:
      m_valueID = CSSValueInline;
      break;
    case EDisplay::Block:
      m_valueID = CSSValueBlock;
      break;
    case EDisplay::ListItem:
      m_valueID = CSSValueListItem;
      break;
    case EDisplay::InlineBlock:
      m_valueID = CSSValueInlineBlock;
      break;
    case EDisplay::Table:
      m_valueID = CSSValueTable;
      break;
    case EDisplay::InlineTable:
      m_valueID = CSSValueInlineTable;
      break;
    case EDisplay::TableRowGroup:
      m_valueID = CSSValueTableRowGroup;
      break;
    case EDisplay::TableHeaderGroup:
      m_valueID = CSSValueTableHeaderGroup;
      break;
    case EDisplay::TableFooterGroup:
      m_valueID = CSSValueTableFooterGroup;
      break;
    case EDisplay::TableRow:
      m_valueID = CSSValueTableRow;
      break;
    case EDisplay::TableColumnGroup:
      m_valueID = CSSValueTableColumnGroup;
      break;
    case EDisplay::TableColumn:
      m_valueID = CSSValueTableColumn;
      break;
    case EDisplay::TableCell:
      m_valueID = CSSValueTableCell;
      break;
    case EDisplay::TableCaption:
      m_valueID = CSSValueTableCaption;
      break;
    case EDisplay::WebkitBox:
      m_valueID = CSSValueWebkitBox;
      break;
    case EDisplay::WebkitInlineBox:
      m_valueID = CSSValueWebkitInlineBox;
      break;
    case EDisplay::Flex:
      m_valueID = CSSValueFlex;
      break;
    case EDisplay::InlineFlex:
      m_valueID = CSSValueInlineFlex;
      break;
    case EDisplay::Grid:
      m_valueID = CSSValueGrid;
      break;
    case EDisplay::InlineGrid:
      m_valueID = CSSValueInlineGrid;
      break;
    case EDisplay::Contents:
      m_valueID = CSSValueContents;
      break;
    case EDisplay::None:
      m_valueID = CSSValueNone;
      break;
  }
}

template <>
inline EDisplay CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueInline:
      return EDisplay::Inline;
    case CSSValueBlock:
      return EDisplay::Block;
    case CSSValueListItem:
      return EDisplay::ListItem;
    case CSSValueInlineBlock:
      return EDisplay::InlineBlock;
    case CSSValueTable:
      return EDisplay::Table;
    case CSSValueInlineTable:
      return EDisplay::InlineTable;
    case CSSValueTableRowGroup:
      return EDisplay::TableRowGroup;
    case CSSValueTableHeaderGroup:
      return EDisplay::TableHeaderGroup;
    case CSSValueTableFooterGroup:
      return EDisplay::TableFooterGroup;
    case CSSValueTableRow:
      return EDisplay::TableRow;
    case CSSValueTableColumnGroup:
      return EDisplay::TableColumnGroup;
    case CSSValueTableColumn:
      return EDisplay::TableColumn;
    case CSSValueTableCell:
      return EDisplay::TableCell;
    case CSSValueTableCaption:
      return EDisplay::TableCaption;
    case CSSValueWebkitBox:
      return EDisplay::WebkitBox;
    case CSSValueWebkitInlineBox:
      return EDisplay::WebkitInlineBox;
    case CSSValueFlex:
    case CSSValueWebkitFlex:
      return EDisplay::Flex;
    case CSSValueInlineFlex:
    case CSSValueWebkitInlineFlex:
      return EDisplay::InlineFlex;
    case CSSValueGrid:
      return EDisplay::Grid;
    case CSSValueInlineGrid:
      return EDisplay::InlineGrid;
    case CSSValueContents:
      return EDisplay::Contents;
    case CSSValueNone:
      return EDisplay::None;
      break;
    default:
      NOTREACHED();
      return EDisplay::None;
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EEmptyCells e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EEmptyCells::kShow:
      m_valueID = CSSValueShow;
      break;
    case EEmptyCells::kHide:
      m_valueID = CSSValueHide;
      break;
  }
}

template <>
inline EEmptyCells CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueShow:
      return EEmptyCells::kShow;
    case CSSValueHide:
      return EEmptyCells::kHide;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EEmptyCells::kShow;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFlexDirection e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case FlowRow:
      m_valueID = CSSValueRow;
      break;
    case FlowRowReverse:
      m_valueID = CSSValueRowReverse;
      break;
    case FlowColumn:
      m_valueID = CSSValueColumn;
      break;
    case FlowColumnReverse:
      m_valueID = CSSValueColumnReverse;
      break;
  }
}

template <>
inline EFlexDirection CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueRow:
      return FlowRow;
    case CSSValueRowReverse:
      return FlowRowReverse;
    case CSSValueColumn:
      return FlowColumn;
    case CSSValueColumnReverse:
      return FlowColumnReverse;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return FlowRow;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFlexWrap e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case FlexNoWrap:
      m_valueID = CSSValueNowrap;
      break;
    case FlexWrap:
      m_valueID = CSSValueWrap;
      break;
    case FlexWrapReverse:
      m_valueID = CSSValueWrapReverse;
      break;
  }
}

template <>
inline EFlexWrap CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNowrap:
      return FlexNoWrap;
    case CSSValueWrap:
      return FlexWrap;
    case CSSValueWrapReverse:
      return FlexWrapReverse;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return FlexNoWrap;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFloat e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EFloat::kNone:
      m_valueID = CSSValueNone;
      break;
    case EFloat::kLeft:
      m_valueID = CSSValueLeft;
      break;
    case EFloat::kRight:
      m_valueID = CSSValueRight;
      break;
  }
}

template <>
inline EFloat CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueLeft:
      return EFloat::kLeft;
    case CSSValueRight:
      return EFloat::kRight;
    case CSSValueNone:
      return EFloat::kNone;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EFloat::kNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(Hyphens e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case HyphensAuto:
      m_valueID = CSSValueAuto;
      break;
    case HyphensManual:
      m_valueID = CSSValueManual;
      break;
    case HyphensNone:
      m_valueID = CSSValueNone;
      break;
  }
}

template <>
inline Hyphens CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return HyphensAuto;
    case CSSValueManual:
      return HyphensManual;
    case CSSValueNone:
      return HyphensNone;
    default:
      break;
  }

  NOTREACHED();
  return HyphensManual;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(LineBreak e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case LineBreakAuto:
      m_valueID = CSSValueAuto;
      break;
    case LineBreakLoose:
      m_valueID = CSSValueLoose;
      break;
    case LineBreakNormal:
      m_valueID = CSSValueNormal;
      break;
    case LineBreakStrict:
      m_valueID = CSSValueStrict;
      break;
    case LineBreakAfterWhiteSpace:
      m_valueID = CSSValueAfterWhiteSpace;
      break;
  }
}

template <>
inline LineBreak CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return LineBreakAuto;
    case CSSValueLoose:
      return LineBreakLoose;
    case CSSValueNormal:
      return LineBreakNormal;
    case CSSValueStrict:
      return LineBreakStrict;
    case CSSValueAfterWhiteSpace:
      return LineBreakAfterWhiteSpace;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return LineBreakAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EListStylePosition e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EListStylePosition::kOutside:
      m_valueID = CSSValueOutside;
      break;
    case EListStylePosition::kInside:
      m_valueID = CSSValueInside;
      break;
  }
}

template <>
inline EListStylePosition CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueOutside:
      return EListStylePosition::kOutside;
    case CSSValueInside:
      return EListStylePosition::kInside;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EListStylePosition::kOutside;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EListStyleType e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EListStyleType::kArabicIndic:
      m_valueID = CSSValueArabicIndic;
      break;
    case EListStyleType::kArmenian:
      m_valueID = CSSValueArmenian;
      break;
    case EListStyleType::kBengali:
      m_valueID = CSSValueBengali;
      break;
    case EListStyleType::kCambodian:
      m_valueID = CSSValueCambodian;
      break;
    case EListStyleType::kCircle:
      m_valueID = CSSValueCircle;
      break;
    case EListStyleType::kCjkEarthlyBranch:
      m_valueID = CSSValueCjkEarthlyBranch;
      break;
    case EListStyleType::kCjkHeavenlyStem:
      m_valueID = CSSValueCjkHeavenlyStem;
      break;
    case EListStyleType::kCjkIdeographic:
      m_valueID = CSSValueCjkIdeographic;
      break;
    case EListStyleType::kDecimalLeadingZero:
      m_valueID = CSSValueDecimalLeadingZero;
      break;
    case EListStyleType::kDecimal:
      m_valueID = CSSValueDecimal;
      break;
    case EListStyleType::kDevanagari:
      m_valueID = CSSValueDevanagari;
      break;
    case EListStyleType::kDisc:
      m_valueID = CSSValueDisc;
      break;
    case EListStyleType::kEthiopicHalehame:
      m_valueID = CSSValueEthiopicHalehame;
      break;
    case EListStyleType::kEthiopicHalehameAm:
      m_valueID = CSSValueEthiopicHalehameAm;
      break;
    case EListStyleType::kEthiopicHalehameTiEt:
      m_valueID = CSSValueEthiopicHalehameTiEt;
      break;
    case EListStyleType::kEthiopicHalehameTiEr:
      m_valueID = CSSValueEthiopicHalehameTiEr;
      break;
    case EListStyleType::kGeorgian:
      m_valueID = CSSValueGeorgian;
      break;
    case EListStyleType::kGujarati:
      m_valueID = CSSValueGujarati;
      break;
    case EListStyleType::kGurmukhi:
      m_valueID = CSSValueGurmukhi;
      break;
    case EListStyleType::kHangul:
      m_valueID = CSSValueHangul;
      break;
    case EListStyleType::kHangulConsonant:
      m_valueID = CSSValueHangulConsonant;
      break;
    case EListStyleType::kKoreanHangulFormal:
      m_valueID = CSSValueKoreanHangulFormal;
      break;
    case EListStyleType::kKoreanHanjaFormal:
      m_valueID = CSSValueKoreanHanjaFormal;
      break;
    case EListStyleType::kKoreanHanjaInformal:
      m_valueID = CSSValueKoreanHanjaInformal;
      break;
    case EListStyleType::kHebrew:
      m_valueID = CSSValueHebrew;
      break;
    case EListStyleType::kHiragana:
      m_valueID = CSSValueHiragana;
      break;
    case EListStyleType::kHiraganaIroha:
      m_valueID = CSSValueHiraganaIroha;
      break;
    case EListStyleType::kKannada:
      m_valueID = CSSValueKannada;
      break;
    case EListStyleType::kKatakana:
      m_valueID = CSSValueKatakana;
      break;
    case EListStyleType::kKatakanaIroha:
      m_valueID = CSSValueKatakanaIroha;
      break;
    case EListStyleType::kKhmer:
      m_valueID = CSSValueKhmer;
      break;
    case EListStyleType::kLao:
      m_valueID = CSSValueLao;
      break;
    case EListStyleType::kLowerAlpha:
      m_valueID = CSSValueLowerAlpha;
      break;
    case EListStyleType::kLowerArmenian:
      m_valueID = CSSValueLowerArmenian;
      break;
    case EListStyleType::kLowerGreek:
      m_valueID = CSSValueLowerGreek;
      break;
    case EListStyleType::kLowerLatin:
      m_valueID = CSSValueLowerLatin;
      break;
    case EListStyleType::kLowerRoman:
      m_valueID = CSSValueLowerRoman;
      break;
    case EListStyleType::kMalayalam:
      m_valueID = CSSValueMalayalam;
      break;
    case EListStyleType::kMongolian:
      m_valueID = CSSValueMongolian;
      break;
    case EListStyleType::kMyanmar:
      m_valueID = CSSValueMyanmar;
      break;
    case EListStyleType::kNone:
      m_valueID = CSSValueNone;
      break;
    case EListStyleType::kOriya:
      m_valueID = CSSValueOriya;
      break;
    case EListStyleType::kPersian:
      m_valueID = CSSValuePersian;
      break;
    case EListStyleType::kSimpChineseFormal:
      m_valueID = CSSValueSimpChineseFormal;
      break;
    case EListStyleType::kSimpChineseInformal:
      m_valueID = CSSValueSimpChineseInformal;
      break;
    case EListStyleType::kSquare:
      m_valueID = CSSValueSquare;
      break;
    case EListStyleType::kTelugu:
      m_valueID = CSSValueTelugu;
      break;
    case EListStyleType::kThai:
      m_valueID = CSSValueThai;
      break;
    case EListStyleType::kTibetan:
      m_valueID = CSSValueTibetan;
      break;
    case EListStyleType::kTradChineseFormal:
      m_valueID = CSSValueTradChineseFormal;
      break;
    case EListStyleType::kTradChineseInformal:
      m_valueID = CSSValueTradChineseInformal;
      break;
    case EListStyleType::kUpperAlpha:
      m_valueID = CSSValueUpperAlpha;
      break;
    case EListStyleType::kUpperArmenian:
      m_valueID = CSSValueUpperArmenian;
      break;
    case EListStyleType::kUpperLatin:
      m_valueID = CSSValueUpperLatin;
      break;
    case EListStyleType::kUpperRoman:
      m_valueID = CSSValueUpperRoman;
      break;
    case EListStyleType::kUrdu:
      m_valueID = CSSValueUrdu;
      break;
  }
}

template <>
inline EListStyleType CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return EListStyleType::kNone;
    case CSSValueArabicIndic:
      return EListStyleType::kArabicIndic;
    case CSSValueArmenian:
      return EListStyleType::kArmenian;
    case CSSValueBengali:
      return EListStyleType::kBengali;
    case CSSValueCambodian:
      return EListStyleType::kCambodian;
    case CSSValueCircle:
      return EListStyleType::kCircle;
    case CSSValueCjkEarthlyBranch:
      return EListStyleType::kCjkEarthlyBranch;
    case CSSValueCjkHeavenlyStem:
      return EListStyleType::kCjkHeavenlyStem;
    case CSSValueCjkIdeographic:
      return EListStyleType::kCjkIdeographic;
    case CSSValueDecimalLeadingZero:
      return EListStyleType::kDecimalLeadingZero;
    case CSSValueDecimal:
      return EListStyleType::kDecimal;
    case CSSValueDevanagari:
      return EListStyleType::kDevanagari;
    case CSSValueDisc:
      return EListStyleType::kDisc;
    case CSSValueEthiopicHalehame:
      return EListStyleType::kEthiopicHalehame;
    case CSSValueEthiopicHalehameAm:
      return EListStyleType::kEthiopicHalehameAm;
    case CSSValueEthiopicHalehameTiEt:
      return EListStyleType::kEthiopicHalehameTiEt;
    case CSSValueEthiopicHalehameTiEr:
      return EListStyleType::kEthiopicHalehameTiEr;
    case CSSValueGeorgian:
      return EListStyleType::kGeorgian;
    case CSSValueGujarati:
      return EListStyleType::kGujarati;
    case CSSValueGurmukhi:
      return EListStyleType::kGurmukhi;
    case CSSValueHangul:
      return EListStyleType::kHangul;
    case CSSValueHangulConsonant:
      return EListStyleType::kHangulConsonant;
    case CSSValueKoreanHangulFormal:
      return EListStyleType::kKoreanHangulFormal;
    case CSSValueKoreanHanjaFormal:
      return EListStyleType::kKoreanHanjaFormal;
    case CSSValueKoreanHanjaInformal:
      return EListStyleType::kKoreanHanjaInformal;
    case CSSValueHebrew:
      return EListStyleType::kHebrew;
    case CSSValueHiragana:
      return EListStyleType::kHiragana;
    case CSSValueHiraganaIroha:
      return EListStyleType::kHiraganaIroha;
    case CSSValueKannada:
      return EListStyleType::kKannada;
    case CSSValueKatakana:
      return EListStyleType::kKatakana;
    case CSSValueKatakanaIroha:
      return EListStyleType::kKatakanaIroha;
    case CSSValueKhmer:
      return EListStyleType::kKhmer;
    case CSSValueLao:
      return EListStyleType::kLao;
    case CSSValueLowerAlpha:
      return EListStyleType::kLowerAlpha;
    case CSSValueLowerArmenian:
      return EListStyleType::kLowerArmenian;
    case CSSValueLowerGreek:
      return EListStyleType::kLowerGreek;
    case CSSValueLowerLatin:
      return EListStyleType::kLowerLatin;
    case CSSValueLowerRoman:
      return EListStyleType::kLowerRoman;
    case CSSValueMalayalam:
      return EListStyleType::kMalayalam;
    case CSSValueMongolian:
      return EListStyleType::kMongolian;
    case CSSValueMyanmar:
      return EListStyleType::kMyanmar;
    case CSSValueOriya:
      return EListStyleType::kOriya;
    case CSSValuePersian:
      return EListStyleType::kPersian;
    case CSSValueSimpChineseFormal:
      return EListStyleType::kSimpChineseFormal;
    case CSSValueSimpChineseInformal:
      return EListStyleType::kSimpChineseInformal;
    case CSSValueSquare:
      return EListStyleType::kSquare;
    case CSSValueTelugu:
      return EListStyleType::kTelugu;
    case CSSValueThai:
      return EListStyleType::kThai;
    case CSSValueTibetan:
      return EListStyleType::kTibetan;
    case CSSValueTradChineseFormal:
      return EListStyleType::kTradChineseFormal;
    case CSSValueTradChineseInformal:
      return EListStyleType::kTradChineseInformal;
    case CSSValueUpperAlpha:
      return EListStyleType::kUpperAlpha;
    case CSSValueUpperArmenian:
      return EListStyleType::kUpperArmenian;
    case CSSValueUpperLatin:
      return EListStyleType::kUpperLatin;
    case CSSValueUpperRoman:
      return EListStyleType::kUpperRoman;
    case CSSValueUrdu:
      return EListStyleType::kUrdu;
    default:
      break;
  }

  NOTREACHED();
  return EListStyleType::kNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EMarginCollapse e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case MarginCollapseCollapse:
      m_valueID = CSSValueCollapse;
      break;
    case MarginCollapseSeparate:
      m_valueID = CSSValueSeparate;
      break;
    case MarginCollapseDiscard:
      m_valueID = CSSValueDiscard;
      break;
  }
}

template <>
inline EMarginCollapse CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueCollapse:
      return MarginCollapseCollapse;
    case CSSValueSeparate:
      return MarginCollapseSeparate;
    case CSSValueDiscard:
      return MarginCollapseDiscard;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return MarginCollapseCollapse;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EOverflow e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EOverflow::Visible:
      m_valueID = CSSValueVisible;
      break;
    case EOverflow::Hidden:
      m_valueID = CSSValueHidden;
      break;
    case EOverflow::Scroll:
      m_valueID = CSSValueScroll;
      break;
    case EOverflow::Auto:
      m_valueID = CSSValueAuto;
      break;
    case EOverflow::Overlay:
      m_valueID = CSSValueOverlay;
      break;
    case EOverflow::PagedX:
      m_valueID = CSSValueWebkitPagedX;
      break;
    case EOverflow::PagedY:
      m_valueID = CSSValueWebkitPagedY;
      break;
  }
}

template <>
inline EOverflow CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueVisible:
      return EOverflow::Visible;
    case CSSValueHidden:
      return EOverflow::Hidden;
    case CSSValueScroll:
      return EOverflow::Scroll;
    case CSSValueAuto:
      return EOverflow::Auto;
    case CSSValueOverlay:
      return EOverflow::Overlay;
    case CSSValueWebkitPagedX:
      return EOverflow::PagedX;
    case CSSValueWebkitPagedY:
      return EOverflow::PagedY;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EOverflow::Visible;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBreak e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    default:
      ASSERT_NOT_REACHED();
    case BreakAuto:
      m_valueID = CSSValueAuto;
      break;
    case BreakAlways:
      m_valueID = CSSValueAlways;
      break;
    case BreakAvoid:
      m_valueID = CSSValueAvoid;
      break;
    case BreakAvoidPage:
      m_valueID = CSSValueAvoidPage;
      break;
    case BreakPage:
      m_valueID = CSSValuePage;
      break;
    case BreakLeft:
      m_valueID = CSSValueLeft;
      break;
    case BreakRight:
      m_valueID = CSSValueRight;
      break;
    case BreakRecto:
      m_valueID = CSSValueRecto;
      break;
    case BreakVerso:
      m_valueID = CSSValueVerso;
      break;
    case BreakAvoidColumn:
      m_valueID = CSSValueAvoidColumn;
      break;
    case BreakColumn:
      m_valueID = CSSValueColumn;
      break;
  }
}

template <>
inline EBreak CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    default:
      ASSERT_NOT_REACHED();
    case CSSValueAuto:
      return BreakAuto;
    case CSSValueAvoid:
      return BreakAvoid;
    case CSSValueAlways:
      return BreakAlways;
    case CSSValueAvoidPage:
      return BreakAvoidPage;
    case CSSValuePage:
      return BreakPage;
    case CSSValueLeft:
      return BreakLeft;
    case CSSValueRight:
      return BreakRight;
    case CSSValueRecto:
      return BreakRecto;
    case CSSValueVerso:
      return BreakVerso;
    case CSSValueAvoidColumn:
      return BreakAvoidColumn;
    case CSSValueColumn:
      return BreakColumn;
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EPosition e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case StaticPosition:
      m_valueID = CSSValueStatic;
      break;
    case RelativePosition:
      m_valueID = CSSValueRelative;
      break;
    case AbsolutePosition:
      m_valueID = CSSValueAbsolute;
      break;
    case FixedPosition:
      m_valueID = CSSValueFixed;
      break;
    case StickyPosition:
      m_valueID = CSSValueSticky;
      break;
  }
}

template <>
inline EPosition CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueStatic:
      return StaticPosition;
    case CSSValueRelative:
      return RelativePosition;
    case CSSValueAbsolute:
      return AbsolutePosition;
    case CSSValueFixed:
      return FixedPosition;
    case CSSValueSticky:
      return StickyPosition;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return StaticPosition;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EResize e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case RESIZE_BOTH:
      m_valueID = CSSValueBoth;
      break;
    case RESIZE_HORIZONTAL:
      m_valueID = CSSValueHorizontal;
      break;
    case RESIZE_VERTICAL:
      m_valueID = CSSValueVertical;
      break;
    case RESIZE_NONE:
      m_valueID = CSSValueNone;
      break;
  }
}

template <>
inline EResize CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueBoth:
      return RESIZE_BOTH;
    case CSSValueHorizontal:
      return RESIZE_HORIZONTAL;
    case CSSValueVertical:
      return RESIZE_VERTICAL;
    case CSSValueAuto:
      // Depends on settings, thus should be handled by the caller.
      NOTREACHED();
      return RESIZE_NONE;
    case CSSValueNone:
      return RESIZE_NONE;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return RESIZE_NONE;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ETableLayout e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TableLayoutAuto:
      m_valueID = CSSValueAuto;
      break;
    case TableLayoutFixed:
      m_valueID = CSSValueFixed;
      break;
  }
}

template <>
inline ETableLayout CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueFixed:
      return TableLayoutFixed;
    case CSSValueAuto:
      return TableLayoutAuto;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TableLayoutAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ETextAlign e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ETextAlign::kStart:
      m_valueID = CSSValueStart;
      break;
    case ETextAlign::kEnd:
      m_valueID = CSSValueEnd;
      break;
    case ETextAlign::kLeft:
      m_valueID = CSSValueLeft;
      break;
    case ETextAlign::kRight:
      m_valueID = CSSValueRight;
      break;
    case ETextAlign::kCenter:
      m_valueID = CSSValueCenter;
      break;
    case ETextAlign::kJustify:
      m_valueID = CSSValueJustify;
      break;
    case ETextAlign::kWebkitLeft:
      m_valueID = CSSValueWebkitLeft;
      break;
    case ETextAlign::kWebkitRight:
      m_valueID = CSSValueWebkitRight;
      break;
    case ETextAlign::kWebkitCenter:
      m_valueID = CSSValueWebkitCenter;
      break;
  }
}

template <>
inline ETextAlign CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueWebkitAuto:  // Legacy -webkit-auto. Eqiuvalent to start.
    case CSSValueStart:
      return ETextAlign::kStart;
    case CSSValueEnd:
      return ETextAlign::kEnd;
    case CSSValueCenter:
    case CSSValueInternalCenter:
      return ETextAlign::kCenter;
    case CSSValueLeft:
      return ETextAlign::kLeft;
    case CSSValueRight:
      return ETextAlign::kRight;
    case CSSValueJustify:
      return ETextAlign::kJustify;
    case CSSValueWebkitLeft:
      return ETextAlign::kWebkitLeft;
    case CSSValueWebkitRight:
      return ETextAlign::kWebkitRight;
    case CSSValueWebkitCenter:
      return ETextAlign::kWebkitCenter;
    default:
      NOTREACHED();
      return ETextAlign::kLeft;
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextAlignLast e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TextAlignLastStart:
      m_valueID = CSSValueStart;
      break;
    case TextAlignLastEnd:
      m_valueID = CSSValueEnd;
      break;
    case TextAlignLastLeft:
      m_valueID = CSSValueLeft;
      break;
    case TextAlignLastRight:
      m_valueID = CSSValueRight;
      break;
    case TextAlignLastCenter:
      m_valueID = CSSValueCenter;
      break;
    case TextAlignLastJustify:
      m_valueID = CSSValueJustify;
      break;
    case TextAlignLastAuto:
      m_valueID = CSSValueAuto;
      break;
  }
}

template <>
inline TextAlignLast CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return TextAlignLastAuto;
    case CSSValueStart:
      return TextAlignLastStart;
    case CSSValueEnd:
      return TextAlignLastEnd;
    case CSSValueLeft:
      return TextAlignLastLeft;
    case CSSValueRight:
      return TextAlignLastRight;
    case CSSValueCenter:
      return TextAlignLastCenter;
    case CSSValueJustify:
      return TextAlignLastJustify;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextAlignLastAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextJustify e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TextJustifyAuto:
      m_valueID = CSSValueAuto;
      break;
    case TextJustifyNone:
      m_valueID = CSSValueNone;
      break;
    case TextJustifyInterWord:
      m_valueID = CSSValueInterWord;
      break;
    case TextJustifyDistribute:
      m_valueID = CSSValueDistribute;
      break;
  }
}

template <>
inline TextJustify CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return TextJustifyAuto;
    case CSSValueNone:
      return TextJustifyNone;
    case CSSValueInterWord:
      return TextJustifyInterWord;
    case CSSValueDistribute:
      return TextJustifyDistribute;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextJustifyAuto;
}

template <>
inline TextDecoration CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return TextDecorationNone;
    case CSSValueUnderline:
      return TextDecorationUnderline;
    case CSSValueOverline:
      return TextDecorationOverline;
    case CSSValueLineThrough:
      return TextDecorationLineThrough;
    case CSSValueBlink:
      return TextDecorationBlink;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextDecorationNone;
}

template <>
inline TextDecorationStyle CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueSolid:
      return TextDecorationStyleSolid;
    case CSSValueDouble:
      return TextDecorationStyleDouble;
    case CSSValueDotted:
      return TextDecorationStyleDotted;
    case CSSValueDashed:
      return TextDecorationStyleDashed;
    case CSSValueWavy:
      return TextDecorationStyleWavy;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextDecorationStyleSolid;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextUnderlinePosition e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TextUnderlinePositionAuto:
      m_valueID = CSSValueAuto;
      break;
    case TextUnderlinePositionUnder:
      m_valueID = CSSValueUnder;
      break;
  }

  // FIXME: Implement support for 'under left' and 'under right' values.
}

template <>
inline TextUnderlinePosition CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return TextUnderlinePositionAuto;
    case CSSValueUnder:
      return TextUnderlinePositionUnder;
    default:
      break;
  }

  // FIXME: Implement support for 'under left' and 'under right' values.

  ASSERT_NOT_REACHED();
  return TextUnderlinePositionAuto;
}

template <>
inline TextDecorationSkip CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueObjects:
      return TextDecorationSkipObjects;
    case CSSValueInk:
      return TextDecorationSkipInk;
    default:
      break;
  }

  NOTREACHED();
  return TextDecorationSkipObjects;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ETextSecurity e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TSNONE:
      m_valueID = CSSValueNone;
      break;
    case TSDISC:
      m_valueID = CSSValueDisc;
      break;
    case TSCIRCLE:
      m_valueID = CSSValueCircle;
      break;
    case TSSQUARE:
      m_valueID = CSSValueSquare;
      break;
  }
}

template <>
inline ETextSecurity CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return TSNONE;
    case CSSValueDisc:
      return TSDISC;
    case CSSValueCircle:
      return TSCIRCLE;
    case CSSValueSquare:
      return TSSQUARE;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TSNONE;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ETextTransform e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ETextTransform::kCapitalize:
      m_valueID = CSSValueCapitalize;
      break;
    case ETextTransform::kUppercase:
      m_valueID = CSSValueUppercase;
      break;
    case ETextTransform::kLowercase:
      m_valueID = CSSValueLowercase;
      break;
    case ETextTransform::kNone:
      m_valueID = CSSValueNone;
      break;
  }
}

template <>
inline ETextTransform CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueCapitalize:
      return ETextTransform::kCapitalize;
    case CSSValueUppercase:
      return ETextTransform::kUppercase;
    case CSSValueLowercase:
      return ETextTransform::kLowercase;
    case CSSValueNone:
      return ETextTransform::kNone;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return ETextTransform::kNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(UnicodeBidi e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case UnicodeBidi::kNormal:
      m_valueID = CSSValueNormal;
      break;
    case UnicodeBidi::kEmbed:
      m_valueID = CSSValueEmbed;
      break;
    case UnicodeBidi::kBidiOverride:
      m_valueID = CSSValueBidiOverride;
      break;
    case UnicodeBidi::kIsolate:
      m_valueID = CSSValueIsolate;
      break;
    case UnicodeBidi::kIsolateOverride:
      m_valueID = CSSValueIsolateOverride;
      break;
    case UnicodeBidi::kPlaintext:
      m_valueID = CSSValuePlaintext;
      break;
  }
}

template <>
inline UnicodeBidi CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNormal:
      return UnicodeBidi::kNormal;
    case CSSValueEmbed:
      return UnicodeBidi::kEmbed;
    case CSSValueBidiOverride:
      return UnicodeBidi::kBidiOverride;
    case CSSValueIsolate:
    case CSSValueWebkitIsolate:
      return UnicodeBidi::kIsolate;
    case CSSValueIsolateOverride:
    case CSSValueWebkitIsolateOverride:
      return UnicodeBidi::kIsolateOverride;
    case CSSValuePlaintext:
    case CSSValueWebkitPlaintext:
      return UnicodeBidi::kPlaintext;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return UnicodeBidi::kNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EUserDrag e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case DRAG_AUTO:
      m_valueID = CSSValueAuto;
      break;
    case DRAG_NONE:
      m_valueID = CSSValueNone;
      break;
    case DRAG_ELEMENT:
      m_valueID = CSSValueElement;
      break;
    default:
      break;
  }
}

template <>
inline EUserDrag CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return DRAG_AUTO;
    case CSSValueNone:
      return DRAG_NONE;
    case CSSValueElement:
      return DRAG_ELEMENT;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return DRAG_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EUserModify e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case READ_ONLY:
      m_valueID = CSSValueReadOnly;
      break;
    case READ_WRITE:
      m_valueID = CSSValueReadWrite;
      break;
    case READ_WRITE_PLAINTEXT_ONLY:
      m_valueID = CSSValueReadWritePlaintextOnly;
      break;
  }
}

template <>
inline EUserModify CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueReadOnly:
      return READ_ONLY;
    case CSSValueReadWrite:
      return READ_WRITE;
    case CSSValueReadWritePlaintextOnly:
      return READ_WRITE_PLAINTEXT_ONLY;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return READ_ONLY;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EUserSelect e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case SELECT_NONE:
      m_valueID = CSSValueNone;
      break;
    case SELECT_TEXT:
      m_valueID = CSSValueText;
      break;
    case SELECT_ALL:
      m_valueID = CSSValueAll;
      break;
  }
}

template <>
inline EUserSelect CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return SELECT_TEXT;
    case CSSValueNone:
      return SELECT_NONE;
    case CSSValueText:
      return SELECT_TEXT;
    case CSSValueAll:
      return SELECT_ALL;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return SELECT_TEXT;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EVerticalAlign a)
    : CSSValue(IdentifierClass) {
  switch (a) {
    case EVerticalAlign::Top:
      m_valueID = CSSValueTop;
      break;
    case EVerticalAlign::Bottom:
      m_valueID = CSSValueBottom;
      break;
    case EVerticalAlign::Middle:
      m_valueID = CSSValueMiddle;
      break;
    case EVerticalAlign::Baseline:
      m_valueID = CSSValueBaseline;
      break;
    case EVerticalAlign::TextBottom:
      m_valueID = CSSValueTextBottom;
      break;
    case EVerticalAlign::TextTop:
      m_valueID = CSSValueTextTop;
      break;
    case EVerticalAlign::Sub:
      m_valueID = CSSValueSub;
      break;
    case EVerticalAlign::Super:
      m_valueID = CSSValueSuper;
      break;
    case EVerticalAlign::BaselineMiddle:
      m_valueID = CSSValueWebkitBaselineMiddle;
      break;
    case EVerticalAlign::Length:
      m_valueID = CSSValueInvalid;
  }
}

template <>
inline EVerticalAlign CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueTop:
      return EVerticalAlign::Top;
    case CSSValueBottom:
      return EVerticalAlign::Bottom;
    case CSSValueMiddle:
      return EVerticalAlign::Middle;
    case CSSValueBaseline:
      return EVerticalAlign::Baseline;
    case CSSValueTextBottom:
      return EVerticalAlign::TextBottom;
    case CSSValueTextTop:
      return EVerticalAlign::TextTop;
    case CSSValueSub:
      return EVerticalAlign::Sub;
    case CSSValueSuper:
      return EVerticalAlign::Super;
    case CSSValueWebkitBaselineMiddle:
      return EVerticalAlign::BaselineMiddle;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EVerticalAlign::Top;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EVisibility e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EVisibility::kVisible:
      m_valueID = CSSValueVisible;
      break;
    case EVisibility::kHidden:
      m_valueID = CSSValueHidden;
      break;
    case EVisibility::kCollapse:
      m_valueID = CSSValueCollapse;
      break;
  }
}

template <>
inline EVisibility CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueHidden:
      return EVisibility::kHidden;
    case CSSValueVisible:
      return EVisibility::kVisible;
    case CSSValueCollapse:
      return EVisibility::kCollapse;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EVisibility::kVisible;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EWhiteSpace e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EWhiteSpace::kNormal:
      m_valueID = CSSValueNormal;
      break;
    case EWhiteSpace::kPre:
      m_valueID = CSSValuePre;
      break;
    case EWhiteSpace::kPreWrap:
      m_valueID = CSSValuePreWrap;
      break;
    case EWhiteSpace::kPreLine:
      m_valueID = CSSValuePreLine;
      break;
    case EWhiteSpace::kNowrap:
      m_valueID = CSSValueNowrap;
      break;
    case EWhiteSpace::kWebkitNowrap:
      m_valueID = CSSValueWebkitNowrap;
      break;
  }
}

template <>
inline EWhiteSpace CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueWebkitNowrap:
      return EWhiteSpace::kWebkitNowrap;
    case CSSValueNowrap:
      return EWhiteSpace::kNowrap;
    case CSSValuePre:
      return EWhiteSpace::kPre;
    case CSSValuePreWrap:
      return EWhiteSpace::kPreWrap;
    case CSSValuePreLine:
      return EWhiteSpace::kPreLine;
    case CSSValueNormal:
      return EWhiteSpace::kNormal;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EWhiteSpace::kNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EWordBreak e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case NormalWordBreak:
      m_valueID = CSSValueNormal;
      break;
    case BreakAllWordBreak:
      m_valueID = CSSValueBreakAll;
      break;
    case BreakWordBreak:
      m_valueID = CSSValueBreakWord;
      break;
    case KeepAllWordBreak:
      m_valueID = CSSValueKeepAll;
      break;
  }
}

template <>
inline EWordBreak CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueBreakAll:
      return BreakAllWordBreak;
    case CSSValueBreakWord:
      return BreakWordBreak;
    case CSSValueNormal:
      return NormalWordBreak;
    case CSSValueKeepAll:
      return KeepAllWordBreak;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return NormalWordBreak;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EOverflowAnchor e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EOverflowAnchor::Visible:
      m_valueID = CSSValueVisible;
      break;
    case EOverflowAnchor::None:
      m_valueID = CSSValueNone;
      break;
    case EOverflowAnchor::Auto:
      m_valueID = CSSValueAuto;
      break;
  }
}

template <>
inline EOverflowAnchor CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueVisible:
      return EOverflowAnchor::Visible;
    case CSSValueNone:
      return EOverflowAnchor::None;
    case CSSValueAuto:
      return EOverflowAnchor::Auto;
    default:
      break;
  }

  NOTREACHED();
  return EOverflowAnchor::None;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EOverflowWrap e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case NormalOverflowWrap:
      m_valueID = CSSValueNormal;
      break;
    case BreakOverflowWrap:
      m_valueID = CSSValueBreakWord;
      break;
  }
}

template <>
inline EOverflowWrap CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueBreakWord:
      return BreakOverflowWrap;
    case CSSValueNormal:
      return NormalOverflowWrap;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return NormalOverflowWrap;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextDirection e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TextDirection::kLtr:
      m_valueID = CSSValueLtr;
      break;
    case TextDirection::kRtl:
      m_valueID = CSSValueRtl;
      break;
  }
}

template <>
inline TextDirection CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueLtr:
      return TextDirection::kLtr;
    case CSSValueRtl:
      return TextDirection::kRtl;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextDirection::kLtr;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(WritingMode e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case WritingMode::kHorizontalTb:
      m_valueID = CSSValueHorizontalTb;
      break;
    case WritingMode::kVerticalRl:
      m_valueID = CSSValueVerticalRl;
      break;
    case WritingMode::kVerticalLr:
      m_valueID = CSSValueVerticalLr;
      break;
  }
}

template <>
inline WritingMode CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueHorizontalTb:
    case CSSValueLr:
    case CSSValueLrTb:
    case CSSValueRl:
    case CSSValueRlTb:
      return WritingMode::kHorizontalTb;
    case CSSValueVerticalRl:
    case CSSValueTb:
    case CSSValueTbRl:
      return WritingMode::kVerticalRl;
    case CSSValueVerticalLr:
      return WritingMode::kVerticalLr;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return WritingMode::kHorizontalTb;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextCombine e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TextCombineNone:
      m_valueID = CSSValueNone;
      break;
    case TextCombineAll:
      m_valueID = CSSValueAll;
      break;
  }
}

template <>
inline TextCombine CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return TextCombineNone;
    case CSSValueAll:
    case CSSValueHorizontal:  // -webkit-text-combine
      return TextCombineAll;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextCombineNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(RubyPosition position)
    : CSSValue(IdentifierClass) {
  switch (position) {
    case RubyPositionBefore:
      m_valueID = CSSValueBefore;
      break;
    case RubyPositionAfter:
      m_valueID = CSSValueAfter;
      break;
  }
}

template <>
inline RubyPosition CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueBefore:
      return RubyPositionBefore;
    case CSSValueAfter:
      return RubyPositionAfter;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return RubyPositionBefore;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextEmphasisPosition position)
    : CSSValue(IdentifierClass) {
  switch (position) {
    case TextEmphasisPositionOver:
      m_valueID = CSSValueOver;
      break;
    case TextEmphasisPositionUnder:
      m_valueID = CSSValueUnder;
      break;
  }
}

template <>
inline TextEmphasisPosition CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueOver:
      return TextEmphasisPositionOver;
    case CSSValueUnder:
      return TextEmphasisPositionUnder;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextEmphasisPositionOver;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextOverflow overflow)
    : CSSValue(IdentifierClass) {
  switch (overflow) {
    case TextOverflowClip:
      m_valueID = CSSValueClip;
      break;
    case TextOverflowEllipsis:
      m_valueID = CSSValueEllipsis;
      break;
  }
}

template <>
inline TextOverflow CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueClip:
      return TextOverflowClip;
    case CSSValueEllipsis:
      return TextOverflowEllipsis;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextOverflowClip;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextEmphasisFill fill)
    : CSSValue(IdentifierClass) {
  switch (fill) {
    case TextEmphasisFillFilled:
      m_valueID = CSSValueFilled;
      break;
    case TextEmphasisFillOpen:
      m_valueID = CSSValueOpen;
      break;
  }
}

template <>
inline TextEmphasisFill CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueFilled:
      return TextEmphasisFillFilled;
    case CSSValueOpen:
      return TextEmphasisFillOpen;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextEmphasisFillFilled;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextEmphasisMark mark)
    : CSSValue(IdentifierClass) {
  switch (mark) {
    case TextEmphasisMarkDot:
      m_valueID = CSSValueDot;
      break;
    case TextEmphasisMarkCircle:
      m_valueID = CSSValueCircle;
      break;
    case TextEmphasisMarkDoubleCircle:
      m_valueID = CSSValueDoubleCircle;
      break;
    case TextEmphasisMarkTriangle:
      m_valueID = CSSValueTriangle;
      break;
    case TextEmphasisMarkSesame:
      m_valueID = CSSValueSesame;
      break;
    case TextEmphasisMarkNone:
    case TextEmphasisMarkAuto:
    case TextEmphasisMarkCustom:
      ASSERT_NOT_REACHED();
      m_valueID = CSSValueNone;
      break;
  }
}

template <>
inline TextEmphasisMark CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return TextEmphasisMarkNone;
    case CSSValueDot:
      return TextEmphasisMarkDot;
    case CSSValueCircle:
      return TextEmphasisMarkCircle;
    case CSSValueDoubleCircle:
      return TextEmphasisMarkDoubleCircle;
    case CSSValueTriangle:
      return TextEmphasisMarkTriangle;
    case CSSValueSesame:
      return TextEmphasisMarkSesame;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextEmphasisMarkNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextOrientation e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TextOrientationSideways:
      m_valueID = CSSValueSideways;
      break;
    case TextOrientationMixed:
      m_valueID = CSSValueMixed;
      break;
    case TextOrientationUpright:
      m_valueID = CSSValueUpright;
      break;
  }
}

template <>
inline TextOrientation CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueSideways:
    case CSSValueSidewaysRight:
      return TextOrientationSideways;
    case CSSValueMixed:
    case CSSValueVerticalRight:  // -webkit-text-orientation
      return TextOrientationMixed;
    case CSSValueUpright:
      return TextOrientationUpright;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TextOrientationMixed;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EPointerEvents e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EPointerEvents::kNone:
      m_valueID = CSSValueNone;
      break;
    case EPointerEvents::kStroke:
      m_valueID = CSSValueStroke;
      break;
    case EPointerEvents::kFill:
      m_valueID = CSSValueFill;
      break;
    case EPointerEvents::kPainted:
      m_valueID = CSSValuePainted;
      break;
    case EPointerEvents::kVisible:
      m_valueID = CSSValueVisible;
      break;
    case EPointerEvents::kVisibleStroke:
      m_valueID = CSSValueVisibleStroke;
      break;
    case EPointerEvents::kVisibleFill:
      m_valueID = CSSValueVisibleFill;
      break;
    case EPointerEvents::kVisiblePainted:
      m_valueID = CSSValueVisiblePainted;
      break;
    case EPointerEvents::kAuto:
      m_valueID = CSSValueAuto;
      break;
    case EPointerEvents::kAll:
      m_valueID = CSSValueAll;
      break;
    case EPointerEvents::kBoundingBox:
      m_valueID = CSSValueBoundingBox;
      break;
  }
}

template <>
inline EPointerEvents CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAll:
      return EPointerEvents::kAll;
    case CSSValueAuto:
      return EPointerEvents::kAuto;
    case CSSValueNone:
      return EPointerEvents::kNone;
    case CSSValueVisiblePainted:
      return EPointerEvents::kVisiblePainted;
    case CSSValueVisibleFill:
      return EPointerEvents::kVisibleFill;
    case CSSValueVisibleStroke:
      return EPointerEvents::kVisibleStroke;
    case CSSValueVisible:
      return EPointerEvents::kVisible;
    case CSSValuePainted:
      return EPointerEvents::kPainted;
    case CSSValueFill:
      return EPointerEvents::kFill;
    case CSSValueStroke:
      return EPointerEvents::kStroke;
    case CSSValueBoundingBox:
      return EPointerEvents::kBoundingBox;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EPointerEvents::kAll;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(FontDescription::Kerning kerning)
    : CSSValue(IdentifierClass) {
  switch (kerning) {
    case FontDescription::AutoKerning:
      m_valueID = CSSValueAuto;
      return;
    case FontDescription::NormalKerning:
      m_valueID = CSSValueNormal;
      return;
    case FontDescription::NoneKerning:
      m_valueID = CSSValueNone;
      return;
  }

  ASSERT_NOT_REACHED();
  m_valueID = CSSValueAuto;
}

template <>
inline FontDescription::Kerning CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return FontDescription::AutoKerning;
    case CSSValueNormal:
      return FontDescription::NormalKerning;
    case CSSValueNone:
      return FontDescription::NoneKerning;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return FontDescription::AutoKerning;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ObjectFit fit)
    : CSSValue(IdentifierClass) {
  switch (fit) {
    case ObjectFitFill:
      m_valueID = CSSValueFill;
      break;
    case ObjectFitContain:
      m_valueID = CSSValueContain;
      break;
    case ObjectFitCover:
      m_valueID = CSSValueCover;
      break;
    case ObjectFitNone:
      m_valueID = CSSValueNone;
      break;
    case ObjectFitScaleDown:
      m_valueID = CSSValueScaleDown;
      break;
  }
}

template <>
inline ObjectFit CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueFill:
      return ObjectFitFill;
    case CSSValueContain:
      return ObjectFitContain;
    case CSSValueCover:
      return ObjectFitCover;
    case CSSValueNone:
      return ObjectFitNone;
    case CSSValueScaleDown:
      return ObjectFitScaleDown;
    default:
      ASSERT_NOT_REACHED();
      return ObjectFitFill;
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillSizeType fillSize)
    : CSSValue(IdentifierClass) {
  switch (fillSize) {
    case Contain:
      m_valueID = CSSValueContain;
      break;
    case Cover:
      m_valueID = CSSValueCover;
      break;
    case SizeNone:
      m_valueID = CSSValueNone;
      break;
    case SizeLength:
    default:
      ASSERT_NOT_REACHED();
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(FontSmoothingMode smoothing)
    : CSSValue(IdentifierClass) {
  switch (smoothing) {
    case AutoSmoothing:
      m_valueID = CSSValueAuto;
      return;
    case NoSmoothing:
      m_valueID = CSSValueNone;
      return;
    case Antialiased:
      m_valueID = CSSValueAntialiased;
      return;
    case SubpixelAntialiased:
      m_valueID = CSSValueSubpixelAntialiased;
      return;
  }

  ASSERT_NOT_REACHED();
  m_valueID = CSSValueAuto;
}

template <>
inline FontSmoothingMode CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return AutoSmoothing;
    case CSSValueNone:
      return NoSmoothing;
    case CSSValueAntialiased:
      return Antialiased;
    case CSSValueSubpixelAntialiased:
      return SubpixelAntialiased;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return AutoSmoothing;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(FontWeight weight)
    : CSSValue(IdentifierClass) {
  switch (weight) {
    case FontWeight900:
      m_valueID = CSSValue900;
      return;
    case FontWeight800:
      m_valueID = CSSValue800;
      return;
    case FontWeight700:
      m_valueID = CSSValueBold;
      return;
    case FontWeight600:
      m_valueID = CSSValue600;
      return;
    case FontWeight500:
      m_valueID = CSSValue500;
      return;
    case FontWeight400:
      m_valueID = CSSValueNormal;
      return;
    case FontWeight300:
      m_valueID = CSSValue300;
      return;
    case FontWeight200:
      m_valueID = CSSValue200;
      return;
    case FontWeight100:
      m_valueID = CSSValue100;
      return;
  }

  ASSERT_NOT_REACHED();
  m_valueID = CSSValueNormal;
}

template <>
inline FontWeight CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueBold:
      return FontWeightBold;
    case CSSValueNormal:
      return FontWeightNormal;
    case CSSValue900:
      return FontWeight900;
    case CSSValue800:
      return FontWeight800;
    case CSSValue700:
      return FontWeight700;
    case CSSValue600:
      return FontWeight600;
    case CSSValue500:
      return FontWeight500;
    case CSSValue400:
      return FontWeight400;
    case CSSValue300:
      return FontWeight300;
    case CSSValue200:
      return FontWeight200;
    case CSSValue100:
      return FontWeight100;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return FontWeightNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(FontStyle italic)
    : CSSValue(IdentifierClass) {
  switch (italic) {
    case FontStyleNormal:
      m_valueID = CSSValueNormal;
      return;
    case FontStyleOblique:
      m_valueID = CSSValueOblique;
      return;
    case FontStyleItalic:
      m_valueID = CSSValueItalic;
      return;
  }

  ASSERT_NOT_REACHED();
  m_valueID = CSSValueNormal;
}

template <>
inline FontStyle CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueOblique:
      return FontStyleOblique;
    case CSSValueItalic:
      return FontStyleItalic;
    case CSSValueNormal:
      return FontStyleNormal;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return FontStyleNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(FontStretch stretch)
    : CSSValue(IdentifierClass) {
  switch (stretch) {
    case FontStretchUltraCondensed:
      m_valueID = CSSValueUltraCondensed;
      return;
    case FontStretchExtraCondensed:
      m_valueID = CSSValueExtraCondensed;
      return;
    case FontStretchCondensed:
      m_valueID = CSSValueCondensed;
      return;
    case FontStretchSemiCondensed:
      m_valueID = CSSValueSemiCondensed;
      return;
    case FontStretchNormal:
      m_valueID = CSSValueNormal;
      return;
    case FontStretchSemiExpanded:
      m_valueID = CSSValueSemiExpanded;
      return;
    case FontStretchExpanded:
      m_valueID = CSSValueExpanded;
      return;
    case FontStretchExtraExpanded:
      m_valueID = CSSValueExtraExpanded;
      return;
    case FontStretchUltraExpanded:
      m_valueID = CSSValueUltraExpanded;
      return;
  }

  ASSERT_NOT_REACHED();
  m_valueID = CSSValueNormal;
}

template <>
inline FontStretch CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueUltraCondensed:
      return FontStretchUltraCondensed;
    case CSSValueExtraCondensed:
      return FontStretchExtraCondensed;
    case CSSValueCondensed:
      return FontStretchCondensed;
    case CSSValueSemiCondensed:
      return FontStretchSemiCondensed;
    case CSSValueNormal:
      return FontStretchNormal;
    case CSSValueSemiExpanded:
      return FontStretchSemiExpanded;
    case CSSValueExpanded:
      return FontStretchExpanded;
    case CSSValueExtraExpanded:
      return FontStretchExtraExpanded;
    case CSSValueUltraExpanded:
      return FontStretchUltraExpanded;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return FontStretchNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextRenderingMode e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case AutoTextRendering:
      m_valueID = CSSValueAuto;
      break;
    case OptimizeSpeed:
      m_valueID = CSSValueOptimizeSpeed;
      break;
    case OptimizeLegibility:
      m_valueID = CSSValueOptimizeLegibility;
      break;
    case GeometricPrecision:
      m_valueID = CSSValueGeometricPrecision;
      break;
  }
}

template <>
inline TextRenderingMode CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return AutoTextRendering;
    case CSSValueOptimizeSpeed:
      return OptimizeSpeed;
    case CSSValueOptimizeLegibility:
      return OptimizeLegibility;
    case CSSValueGeometricPrecision:
      return GeometricPrecision;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return AutoTextRendering;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ESpeak e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case SpeakNone:
      m_valueID = CSSValueNone;
      break;
    case SpeakNormal:
      m_valueID = CSSValueNormal;
      break;
    case SpeakSpellOut:
      m_valueID = CSSValueSpellOut;
      break;
    case SpeakDigits:
      m_valueID = CSSValueDigits;
      break;
    case SpeakLiteralPunctuation:
      m_valueID = CSSValueLiteralPunctuation;
      break;
    case SpeakNoPunctuation:
      m_valueID = CSSValueNoPunctuation;
      break;
  }
}

template <>
inline EOrder CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueLogical:
      return EOrder::kLogical;
    case CSSValueVisual:
      return EOrder::kVisual;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EOrder::kLogical;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EOrder e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EOrder::kLogical:
      m_valueID = CSSValueLogical;
      break;
    case EOrder::kVisual:
      m_valueID = CSSValueVisual;
      break;
  }
}

template <>
inline ESpeak CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return SpeakNone;
    case CSSValueNormal:
      return SpeakNormal;
    case CSSValueSpellOut:
      return SpeakSpellOut;
    case CSSValueDigits:
      return SpeakDigits;
    case CSSValueLiteralPunctuation:
      return SpeakLiteralPunctuation;
    case CSSValueNoPunctuation:
      return SpeakNoPunctuation;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return SpeakNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(WebBlendMode blendMode)
    : CSSValue(IdentifierClass) {
  switch (blendMode) {
    case WebBlendModeNormal:
      m_valueID = CSSValueNormal;
      break;
    case WebBlendModeMultiply:
      m_valueID = CSSValueMultiply;
      break;
    case WebBlendModeScreen:
      m_valueID = CSSValueScreen;
      break;
    case WebBlendModeOverlay:
      m_valueID = CSSValueOverlay;
      break;
    case WebBlendModeDarken:
      m_valueID = CSSValueDarken;
      break;
    case WebBlendModeLighten:
      m_valueID = CSSValueLighten;
      break;
    case WebBlendModeColorDodge:
      m_valueID = CSSValueColorDodge;
      break;
    case WebBlendModeColorBurn:
      m_valueID = CSSValueColorBurn;
      break;
    case WebBlendModeHardLight:
      m_valueID = CSSValueHardLight;
      break;
    case WebBlendModeSoftLight:
      m_valueID = CSSValueSoftLight;
      break;
    case WebBlendModeDifference:
      m_valueID = CSSValueDifference;
      break;
    case WebBlendModeExclusion:
      m_valueID = CSSValueExclusion;
      break;
    case WebBlendModeHue:
      m_valueID = CSSValueHue;
      break;
    case WebBlendModeSaturation:
      m_valueID = CSSValueSaturation;
      break;
    case WebBlendModeColor:
      m_valueID = CSSValueColor;
      break;
    case WebBlendModeLuminosity:
      m_valueID = CSSValueLuminosity;
      break;
  }
}

template <>
inline WebBlendMode CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNormal:
      return WebBlendModeNormal;
    case CSSValueMultiply:
      return WebBlendModeMultiply;
    case CSSValueScreen:
      return WebBlendModeScreen;
    case CSSValueOverlay:
      return WebBlendModeOverlay;
    case CSSValueDarken:
      return WebBlendModeDarken;
    case CSSValueLighten:
      return WebBlendModeLighten;
    case CSSValueColorDodge:
      return WebBlendModeColorDodge;
    case CSSValueColorBurn:
      return WebBlendModeColorBurn;
    case CSSValueHardLight:
      return WebBlendModeHardLight;
    case CSSValueSoftLight:
      return WebBlendModeSoftLight;
    case CSSValueDifference:
      return WebBlendModeDifference;
    case CSSValueExclusion:
      return WebBlendModeExclusion;
    case CSSValueHue:
      return WebBlendModeHue;
    case CSSValueSaturation:
      return WebBlendModeSaturation;
    case CSSValueColor:
      return WebBlendModeColor;
    case CSSValueLuminosity:
      return WebBlendModeLuminosity;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return WebBlendModeNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(LineCap e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ButtCap:
      m_valueID = CSSValueButt;
      break;
    case RoundCap:
      m_valueID = CSSValueRound;
      break;
    case SquareCap:
      m_valueID = CSSValueSquare;
      break;
  }
}

template <>
inline LineCap CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueButt:
      return ButtCap;
    case CSSValueRound:
      return RoundCap;
    case CSSValueSquare:
      return SquareCap;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return ButtCap;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(LineJoin e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case MiterJoin:
      m_valueID = CSSValueMiter;
      break;
    case RoundJoin:
      m_valueID = CSSValueRound;
      break;
    case BevelJoin:
      m_valueID = CSSValueBevel;
      break;
  }
}

template <>
inline LineJoin CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueMiter:
      return MiterJoin;
    case CSSValueRound:
      return RoundJoin;
    case CSSValueBevel:
      return BevelJoin;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return MiterJoin;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(WindRule e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case RULE_NONZERO:
      m_valueID = CSSValueNonzero;
      break;
    case RULE_EVENODD:
      m_valueID = CSSValueEvenodd;
      break;
  }
}

template <>
inline WindRule CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNonzero:
      return RULE_NONZERO;
    case CSSValueEvenodd:
      return RULE_EVENODD;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return RULE_NONZERO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EAlignmentBaseline e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case AB_AUTO:
      m_valueID = CSSValueAuto;
      break;
    case AB_BASELINE:
      m_valueID = CSSValueBaseline;
      break;
    case AB_BEFORE_EDGE:
      m_valueID = CSSValueBeforeEdge;
      break;
    case AB_TEXT_BEFORE_EDGE:
      m_valueID = CSSValueTextBeforeEdge;
      break;
    case AB_MIDDLE:
      m_valueID = CSSValueMiddle;
      break;
    case AB_CENTRAL:
      m_valueID = CSSValueCentral;
      break;
    case AB_AFTER_EDGE:
      m_valueID = CSSValueAfterEdge;
      break;
    case AB_TEXT_AFTER_EDGE:
      m_valueID = CSSValueTextAfterEdge;
      break;
    case AB_IDEOGRAPHIC:
      m_valueID = CSSValueIdeographic;
      break;
    case AB_ALPHABETIC:
      m_valueID = CSSValueAlphabetic;
      break;
    case AB_HANGING:
      m_valueID = CSSValueHanging;
      break;
    case AB_MATHEMATICAL:
      m_valueID = CSSValueMathematical;
      break;
  }
}

template <>
inline EAlignmentBaseline CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return AB_AUTO;
    case CSSValueBaseline:
      return AB_BASELINE;
    case CSSValueBeforeEdge:
      return AB_BEFORE_EDGE;
    case CSSValueTextBeforeEdge:
      return AB_TEXT_BEFORE_EDGE;
    case CSSValueMiddle:
      return AB_MIDDLE;
    case CSSValueCentral:
      return AB_CENTRAL;
    case CSSValueAfterEdge:
      return AB_AFTER_EDGE;
    case CSSValueTextAfterEdge:
      return AB_TEXT_AFTER_EDGE;
    case CSSValueIdeographic:
      return AB_IDEOGRAPHIC;
    case CSSValueAlphabetic:
      return AB_ALPHABETIC;
    case CSSValueHanging:
      return AB_HANGING;
    case CSSValueMathematical:
      return AB_MATHEMATICAL;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return AB_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBorderCollapse e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case EBorderCollapse::kSeparate:
      m_valueID = CSSValueSeparate;
      break;
    case EBorderCollapse::kCollapse:
      m_valueID = CSSValueCollapse;
      break;
  }
}

template <>
inline EBorderCollapse CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueSeparate:
      return EBorderCollapse::kSeparate;
    case CSSValueCollapse:
      return EBorderCollapse::kCollapse;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return EBorderCollapse::kSeparate;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EImageRendering e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case ImageRenderingAuto:
      m_valueID = CSSValueAuto;
      break;
    case ImageRenderingOptimizeSpeed:
      m_valueID = CSSValueOptimizeSpeed;
      break;
    case ImageRenderingOptimizeQuality:
      m_valueID = CSSValueOptimizeQuality;
      break;
    case ImageRenderingPixelated:
      m_valueID = CSSValuePixelated;
      break;
    case ImageRenderingOptimizeContrast:
      m_valueID = CSSValueWebkitOptimizeContrast;
      break;
  }
}

template <>
inline EImageRendering CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return ImageRenderingAuto;
    case CSSValueOptimizeSpeed:
      return ImageRenderingOptimizeSpeed;
    case CSSValueOptimizeQuality:
      return ImageRenderingOptimizeQuality;
    case CSSValuePixelated:
      return ImageRenderingPixelated;
    case CSSValueWebkitOptimizeContrast:
      return ImageRenderingOptimizeContrast;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return ImageRenderingAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ETransformStyle3D e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TransformStyle3DFlat:
      m_valueID = CSSValueFlat;
      break;
    case TransformStyle3DPreserve3D:
      m_valueID = CSSValuePreserve3d;
      break;
  }
}

template <>
inline ETransformStyle3D CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueFlat:
      return TransformStyle3DFlat;
    case CSSValuePreserve3d:
      return TransformStyle3DPreserve3D;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TransformStyle3DFlat;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBufferedRendering e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case BR_AUTO:
      m_valueID = CSSValueAuto;
      break;
    case BR_DYNAMIC:
      m_valueID = CSSValueDynamic;
      break;
    case BR_STATIC:
      m_valueID = CSSValueStatic;
      break;
  }
}

template <>
inline EBufferedRendering CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return BR_AUTO;
    case CSSValueDynamic:
      return BR_DYNAMIC;
    case CSSValueStatic:
      return BR_STATIC;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return BR_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EColorInterpolation e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case CI_AUTO:
      m_valueID = CSSValueAuto;
      break;
    case CI_SRGB:
      m_valueID = CSSValueSRGB;
      break;
    case CI_LINEARRGB:
      m_valueID = CSSValueLinearRGB;
      break;
  }
}

template <>
inline EColorInterpolation CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueSRGB:
      return CI_SRGB;
    case CSSValueLinearRGB:
      return CI_LINEARRGB;
    case CSSValueAuto:
      return CI_AUTO;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return CI_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EColorRendering e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case CR_AUTO:
      m_valueID = CSSValueAuto;
      break;
    case CR_OPTIMIZESPEED:
      m_valueID = CSSValueOptimizeSpeed;
      break;
    case CR_OPTIMIZEQUALITY:
      m_valueID = CSSValueOptimizeQuality;
      break;
  }
}

template <>
inline EColorRendering CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueOptimizeSpeed:
      return CR_OPTIMIZESPEED;
    case CSSValueOptimizeQuality:
      return CR_OPTIMIZEQUALITY;
    case CSSValueAuto:
      return CR_AUTO;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return CR_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EDominantBaseline e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case DB_AUTO:
      m_valueID = CSSValueAuto;
      break;
    case DB_USE_SCRIPT:
      m_valueID = CSSValueUseScript;
      break;
    case DB_NO_CHANGE:
      m_valueID = CSSValueNoChange;
      break;
    case DB_RESET_SIZE:
      m_valueID = CSSValueResetSize;
      break;
    case DB_CENTRAL:
      m_valueID = CSSValueCentral;
      break;
    case DB_MIDDLE:
      m_valueID = CSSValueMiddle;
      break;
    case DB_TEXT_BEFORE_EDGE:
      m_valueID = CSSValueTextBeforeEdge;
      break;
    case DB_TEXT_AFTER_EDGE:
      m_valueID = CSSValueTextAfterEdge;
      break;
    case DB_IDEOGRAPHIC:
      m_valueID = CSSValueIdeographic;
      break;
    case DB_ALPHABETIC:
      m_valueID = CSSValueAlphabetic;
      break;
    case DB_HANGING:
      m_valueID = CSSValueHanging;
      break;
    case DB_MATHEMATICAL:
      m_valueID = CSSValueMathematical;
      break;
  }
}

template <>
inline EDominantBaseline CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return DB_AUTO;
    case CSSValueUseScript:
      return DB_USE_SCRIPT;
    case CSSValueNoChange:
      return DB_NO_CHANGE;
    case CSSValueResetSize:
      return DB_RESET_SIZE;
    case CSSValueIdeographic:
      return DB_IDEOGRAPHIC;
    case CSSValueAlphabetic:
      return DB_ALPHABETIC;
    case CSSValueHanging:
      return DB_HANGING;
    case CSSValueMathematical:
      return DB_MATHEMATICAL;
    case CSSValueCentral:
      return DB_CENTRAL;
    case CSSValueMiddle:
      return DB_MIDDLE;
    case CSSValueTextAfterEdge:
      return DB_TEXT_AFTER_EDGE;
    case CSSValueTextBeforeEdge:
      return DB_TEXT_BEFORE_EDGE;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return DB_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EShapeRendering e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case SR_AUTO:
      m_valueID = CSSValueAuto;
      break;
    case SR_OPTIMIZESPEED:
      m_valueID = CSSValueOptimizeSpeed;
      break;
    case SR_CRISPEDGES:
      m_valueID = CSSValueCrispEdges;
      break;
    case SR_GEOMETRICPRECISION:
      m_valueID = CSSValueGeometricPrecision;
      break;
  }
}

template <>
inline EShapeRendering CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return SR_AUTO;
    case CSSValueOptimizeSpeed:
      return SR_OPTIMIZESPEED;
    case CSSValueCrispEdges:
      return SR_CRISPEDGES;
    case CSSValueGeometricPrecision:
      return SR_GEOMETRICPRECISION;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return SR_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ETextAnchor e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case TA_START:
      m_valueID = CSSValueStart;
      break;
    case TA_MIDDLE:
      m_valueID = CSSValueMiddle;
      break;
    case TA_END:
      m_valueID = CSSValueEnd;
      break;
  }
}

template <>
inline ETextAnchor CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueStart:
      return TA_START;
    case CSSValueMiddle:
      return TA_MIDDLE;
    case CSSValueEnd:
      return TA_END;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TA_START;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EVectorEffect e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case VE_NONE:
      m_valueID = CSSValueNone;
      break;
    case VE_NON_SCALING_STROKE:
      m_valueID = CSSValueNonScalingStroke;
      break;
  }
}

template <>
inline EVectorEffect CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return VE_NONE;
    case CSSValueNonScalingStroke:
      return VE_NON_SCALING_STROKE;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return VE_NONE;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EPaintOrderType e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case PT_FILL:
      m_valueID = CSSValueFill;
      break;
    case PT_STROKE:
      m_valueID = CSSValueStroke;
      break;
    case PT_MARKERS:
      m_valueID = CSSValueMarkers;
      break;
    default:
      ASSERT_NOT_REACHED();
      m_valueID = CSSValueFill;
      break;
  }
}

template <>
inline EPaintOrderType CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueFill:
      return PT_FILL;
    case CSSValueStroke:
      return PT_STROKE;
    case CSSValueMarkers:
      return PT_MARKERS;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return PT_NONE;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EMaskType e)
    : CSSValue(IdentifierClass) {
  switch (e) {
    case MT_LUMINANCE:
      m_valueID = CSSValueLuminance;
      break;
    case MT_ALPHA:
      m_valueID = CSSValueAlpha;
      break;
  }
}

template <>
inline EMaskType CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueLuminance:
      return MT_LUMINANCE;
    case CSSValueAlpha:
      return MT_ALPHA;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return MT_LUMINANCE;
}

template <>
inline TouchAction CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNone:
      return TouchActionNone;
    case CSSValueAuto:
      return TouchActionAuto;
    case CSSValuePanLeft:
      return TouchActionPanLeft;
    case CSSValuePanRight:
      return TouchActionPanRight;
    case CSSValuePanX:
      return TouchActionPanX;
    case CSSValuePanUp:
      return TouchActionPanUp;
    case CSSValuePanDown:
      return TouchActionPanDown;
    case CSSValuePanY:
      return TouchActionPanY;
    case CSSValueManipulation:
      return TouchActionManipulation;
    case CSSValuePinchZoom:
      return TouchActionPinchZoom;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return TouchActionNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EIsolation i)
    : CSSValue(IdentifierClass) {
  switch (i) {
    case IsolationAuto:
      m_valueID = CSSValueAuto;
      break;
    case IsolationIsolate:
      m_valueID = CSSValueIsolate;
      break;
  }
}

template <>
inline EIsolation CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return IsolationAuto;
    case CSSValueIsolate:
      return IsolationIsolate;
    default:
      break;
  }

  ASSERT_NOT_REACHED();
  return IsolationAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(CSSBoxType cssBox)
    : CSSValue(IdentifierClass) {
  switch (cssBox) {
    case MarginBox:
      m_valueID = CSSValueMarginBox;
      break;
    case BorderBox:
      m_valueID = CSSValueBorderBox;
      break;
    case PaddingBox:
      m_valueID = CSSValuePaddingBox;
      break;
    case ContentBox:
      m_valueID = CSSValueContentBox;
      break;
    case BoxMissing:
      // The missing box should convert to a null primitive value.
      ASSERT_NOT_REACHED();
  }
}

template <>
inline CSSBoxType CSSIdentifierValue::convertTo() const {
  switch (getValueID()) {
    case CSSValueMarginBox:
      return MarginBox;
    case CSSValueBorderBox:
      return BorderBox;
    case CSSValuePaddingBox:
      return PaddingBox;
    case CSSValueContentBox:
      return ContentBox;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return ContentBox;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ItemPosition itemPosition)
    : CSSValue(IdentifierClass) {
  switch (itemPosition) {
    case ItemPositionAuto:
      // The 'auto' values might have been already resolved.
      NOTREACHED();
      m_valueID = CSSValueNormal;
      break;
    case ItemPositionNormal:
      m_valueID = CSSValueNormal;
      break;
    case ItemPositionStretch:
      m_valueID = CSSValueStretch;
      break;
    case ItemPositionBaseline:
      m_valueID = CSSValueBaseline;
      break;
    case ItemPositionLastBaseline:
      m_valueID = CSSValueLastBaseline;
      break;
    case ItemPositionCenter:
      m_valueID = CSSValueCenter;
      break;
    case ItemPositionStart:
      m_valueID = CSSValueStart;
      break;
    case ItemPositionEnd:
      m_valueID = CSSValueEnd;
      break;
    case ItemPositionSelfStart:
      m_valueID = CSSValueSelfStart;
      break;
    case ItemPositionSelfEnd:
      m_valueID = CSSValueSelfEnd;
      break;
    case ItemPositionFlexStart:
      m_valueID = CSSValueFlexStart;
      break;
    case ItemPositionFlexEnd:
      m_valueID = CSSValueFlexEnd;
      break;
    case ItemPositionLeft:
      m_valueID = CSSValueLeft;
      break;
    case ItemPositionRight:
      m_valueID = CSSValueRight;
      break;
  }
}

template <>
inline ItemPosition CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueAuto:
      return ItemPositionAuto;
    case CSSValueNormal:
      return ItemPositionNormal;
    case CSSValueStretch:
      return ItemPositionStretch;
    case CSSValueBaseline:
      return ItemPositionBaseline;
    case CSSValueLastBaseline:
      return ItemPositionLastBaseline;
    case CSSValueCenter:
      return ItemPositionCenter;
    case CSSValueStart:
      return ItemPositionStart;
    case CSSValueEnd:
      return ItemPositionEnd;
    case CSSValueSelfStart:
      return ItemPositionSelfStart;
    case CSSValueSelfEnd:
      return ItemPositionSelfEnd;
    case CSSValueFlexStart:
      return ItemPositionFlexStart;
    case CSSValueFlexEnd:
      return ItemPositionFlexEnd;
    case CSSValueLeft:
      return ItemPositionLeft;
    case CSSValueRight:
      return ItemPositionRight;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return ItemPositionAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ContentPosition contentPosition)
    : CSSValue(IdentifierClass) {
  switch (contentPosition) {
    case ContentPositionNormal:
      m_valueID = CSSValueNormal;
      break;
    case ContentPositionBaseline:
      m_valueID = CSSValueBaseline;
      break;
    case ContentPositionLastBaseline:
      m_valueID = CSSValueLastBaseline;
      break;
    case ContentPositionCenter:
      m_valueID = CSSValueCenter;
      break;
    case ContentPositionStart:
      m_valueID = CSSValueStart;
      break;
    case ContentPositionEnd:
      m_valueID = CSSValueEnd;
      break;
    case ContentPositionFlexStart:
      m_valueID = CSSValueFlexStart;
      break;
    case ContentPositionFlexEnd:
      m_valueID = CSSValueFlexEnd;
      break;
    case ContentPositionLeft:
      m_valueID = CSSValueLeft;
      break;
    case ContentPositionRight:
      m_valueID = CSSValueRight;
      break;
  }
}

template <>
inline ContentPosition CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueNormal:
      return ContentPositionNormal;
    case CSSValueBaseline:
      return ContentPositionBaseline;
    case CSSValueLastBaseline:
      return ContentPositionLastBaseline;
    case CSSValueCenter:
      return ContentPositionCenter;
    case CSSValueStart:
      return ContentPositionStart;
    case CSSValueEnd:
      return ContentPositionEnd;
    case CSSValueFlexStart:
      return ContentPositionFlexStart;
    case CSSValueFlexEnd:
      return ContentPositionFlexEnd;
    case CSSValueLeft:
      return ContentPositionLeft;
    case CSSValueRight:
      return ContentPositionRight;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return ContentPositionNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(
    ContentDistributionType contentDistribution)
    : CSSValue(IdentifierClass) {
  switch (contentDistribution) {
    case ContentDistributionDefault:
      m_valueID = CSSValueDefault;
      break;
    case ContentDistributionSpaceBetween:
      m_valueID = CSSValueSpaceBetween;
      break;
    case ContentDistributionSpaceAround:
      m_valueID = CSSValueSpaceAround;
      break;
    case ContentDistributionSpaceEvenly:
      m_valueID = CSSValueSpaceEvenly;
      break;
    case ContentDistributionStretch:
      m_valueID = CSSValueStretch;
      break;
  }
}

template <>
inline ContentDistributionType CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueSpaceBetween:
      return ContentDistributionSpaceBetween;
    case CSSValueSpaceAround:
      return ContentDistributionSpaceAround;
    case CSSValueSpaceEvenly:
      return ContentDistributionSpaceEvenly;
    case CSSValueStretch:
      return ContentDistributionStretch;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return ContentDistributionStretch;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(
    OverflowAlignment overflowAlignment)
    : CSSValue(IdentifierClass) {
  switch (overflowAlignment) {
    case OverflowAlignmentDefault:
      m_valueID = CSSValueDefault;
      break;
    case OverflowAlignmentUnsafe:
      m_valueID = CSSValueUnsafe;
      break;
    case OverflowAlignmentSafe:
      m_valueID = CSSValueSafe;
      break;
  }
}

template <>
inline OverflowAlignment CSSIdentifierValue::convertTo() const {
  switch (m_valueID) {
    case CSSValueUnsafe:
      return OverflowAlignmentUnsafe;
    case CSSValueSafe:
      return OverflowAlignmentSafe;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return OverflowAlignmentUnsafe;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ScrollBehavior behavior)
    : CSSValue(IdentifierClass) {
  switch (behavior) {
    case ScrollBehaviorAuto:
      m_valueID = CSSValueAuto;
      break;
    case ScrollBehaviorSmooth:
      m_valueID = CSSValueSmooth;
      break;
    case ScrollBehaviorInstant:
      // Behavior 'instant' is only allowed in ScrollOptions arguments passed to
      // CSSOM scroll APIs.
      ASSERT_NOT_REACHED();
  }
}

template <>
inline ScrollBehavior CSSIdentifierValue::convertTo() const {
  switch (getValueID()) {
    case CSSValueAuto:
      return ScrollBehaviorAuto;
    case CSSValueSmooth:
      return ScrollBehaviorSmooth;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return ScrollBehaviorAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ScrollSnapType snapType)
    : CSSValue(IdentifierClass) {
  switch (snapType) {
    case ScrollSnapTypeNone:
      m_valueID = CSSValueNone;
      break;
    case ScrollSnapTypeMandatory:
      m_valueID = CSSValueMandatory;
      break;
    case ScrollSnapTypeProximity:
      m_valueID = CSSValueProximity;
      break;
  }
}

template <>
inline ScrollSnapType CSSIdentifierValue::convertTo() const {
  switch (getValueID()) {
    case CSSValueNone:
      return ScrollSnapTypeNone;
    case CSSValueMandatory:
      return ScrollSnapTypeMandatory;
    case CSSValueProximity:
      return ScrollSnapTypeProximity;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return ScrollSnapTypeNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(Containment snapType)
    : CSSValue(IdentifierClass) {
  switch (snapType) {
    case ContainsNone:
      m_valueID = CSSValueNone;
      break;
    case ContainsStrict:
      m_valueID = CSSValueStrict;
      break;
    case ContainsContent:
      m_valueID = CSSValueContent;
      break;
    case ContainsPaint:
      m_valueID = CSSValuePaint;
      break;
    case ContainsStyle:
      m_valueID = CSSValueStyle;
      break;
    case ContainsLayout:
      m_valueID = CSSValueLayout;
      break;
    case ContainsSize:
      m_valueID = CSSValueSize;
      break;
  }
}

template <>
inline Containment CSSIdentifierValue::convertTo() const {
  switch (getValueID()) {
    case CSSValueNone:
      return ContainsNone;
    case CSSValueStrict:
      return ContainsStrict;
    case CSSValueContent:
      return ContainsContent;
    case CSSValuePaint:
      return ContainsPaint;
    case CSSValueStyle:
      return ContainsStyle;
    case CSSValueLayout:
      return ContainsLayout;
    case CSSValueSize:
      return ContainsSize;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return ContainsNone;
}

}  // namespace blink

#endif
