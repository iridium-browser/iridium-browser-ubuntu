/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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
 */

#include "core/html/HTMLMetaElement.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/HttpEquiv.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/text/StringToNumber.h"

namespace blink {

using namespace HTMLNames;

inline HTMLMetaElement::HTMLMetaElement(Document& document)
    : HTMLElement(metaTag, document) {}

DEFINE_NODE_FACTORY(HTMLMetaElement)

static bool isInvalidSeparator(UChar c) {
  return c == ';';
}

// Though isspace() considers \t and \v to be whitespace, Win IE doesn't.
static bool isSeparator(UChar c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' ||
         c == ',' || c == '\0';
}

void HTMLMetaElement::parseContentAttribute(const String& content,
                                            void* data,
                                            Document* document,
                                            bool viewportMetaZeroValuesQuirk) {
  bool hasInvalidSeparator = false;

  // Tread lightly in this code -- it was specifically designed to mimic Win
  // IE's parsing behavior.
  unsigned keyBegin, keyEnd;
  unsigned valueBegin, valueEnd;

  String buffer = content.lower();
  unsigned length = buffer.length();
  for (unsigned i = 0; i < length; /* no increment here */) {
    // skip to first non-separator, but don't skip past the end of the string
    while (isSeparator(buffer[i])) {
      if (i >= length)
        break;
      i++;
    }
    keyBegin = i;

    // skip to first separator
    while (!isSeparator(buffer[i])) {
      hasInvalidSeparator |= isInvalidSeparator(buffer[i]);
      if (i >= length)
        break;
      i++;
    }
    keyEnd = i;

    // skip to first '=', but don't skip past a ',' or the end of the string
    while (buffer[i] != '=') {
      hasInvalidSeparator |= isInvalidSeparator(buffer[i]);
      if (buffer[i] == ',' || i >= length)
        break;
      i++;
    }

    // Skip to first non-separator, but don't skip past a ',' or the end of the
    // string.
    while (isSeparator(buffer[i])) {
      if (buffer[i] == ',' || i >= length)
        break;
      i++;
    }
    valueBegin = i;

    // skip to first separator
    while (!isSeparator(buffer[i])) {
      hasInvalidSeparator |= isInvalidSeparator(buffer[i]);
      if (i >= length)
        break;
      i++;
    }
    valueEnd = i;

    SECURITY_DCHECK(i <= length);

    String keyString = buffer.substring(keyBegin, keyEnd - keyBegin);
    String valueString = buffer.substring(valueBegin, valueEnd - valueBegin);
    processViewportKeyValuePair(document, !hasInvalidSeparator, keyString,
                                valueString, viewportMetaZeroValuesQuirk, data);
  }
  if (hasInvalidSeparator && document) {
    String message =
        "Error parsing a meta element's content: ';' is not a valid key-value "
        "pair separator. Please use ',' instead.";
    document->addConsoleMessage(ConsoleMessage::create(
        RenderingMessageSource, WarningMessageLevel, message));
  }
}

static inline float clampLengthValue(float value) {
  // Limits as defined in the css-device-adapt spec.
  if (value != ViewportDescription::ValueAuto)
    return std::min(float(10000), std::max(value, float(1)));
  return value;
}

static inline float clampScaleValue(float value) {
  // Limits as defined in the css-device-adapt spec.
  if (value != ViewportDescription::ValueAuto)
    return std::min(float(10), std::max(value, float(0.1)));
  return value;
}

float HTMLMetaElement::parsePositiveNumber(Document* document,
                                           bool reportWarnings,
                                           const String& keyString,
                                           const String& valueString,
                                           bool* ok) {
  size_t parsedLength;
  float value;
  if (valueString.is8Bit())
    value = charactersToFloat(valueString.characters8(), valueString.length(),
                              parsedLength);
  else
    value = charactersToFloat(valueString.characters16(), valueString.length(),
                              parsedLength);
  if (!parsedLength) {
    if (reportWarnings)
      reportViewportWarning(document, UnrecognizedViewportArgumentValueError,
                            valueString, keyString);
    if (ok)
      *ok = false;
    return 0;
  }
  if (parsedLength < valueString.length() && reportWarnings)
    reportViewportWarning(document, TruncatedViewportArgumentValueError,
                          valueString, keyString);
  if (ok)
    *ok = true;
  return value;
}

Length HTMLMetaElement::parseViewportValueAsLength(Document* document,
                                                   bool reportWarnings,
                                                   const String& keyString,
                                                   const String& valueString) {
  // 1) Non-negative number values are translated to px lengths.
  // 2) Negative number values are translated to auto.
  // 3) device-width and device-height are used as keywords.
  // 4) Other keywords and unknown values translate to 0.0.

  if (equalIgnoringCase(valueString, "device-width"))
    return Length(DeviceWidth);
  if (equalIgnoringCase(valueString, "device-height"))
    return Length(DeviceHeight);

  float value =
      parsePositiveNumber(document, reportWarnings, keyString, valueString);

  if (value < 0)
    return Length();  // auto

  return Length(clampLengthValue(value), Fixed);
}

float HTMLMetaElement::parseViewportValueAsZoom(
    Document* document,
    bool reportWarnings,
    const String& keyString,
    const String& valueString,
    bool& computedValueMatchesParsedValue,
    bool viewportMetaZeroValuesQuirk) {
  // 1) Non-negative number values are translated to <number> values.
  // 2) Negative number values are translated to auto.
  // 3) yes is translated to 1.0.
  // 4) device-width and device-height are translated to 10.0.
  // 5) no and unknown values are translated to 0.0

  computedValueMatchesParsedValue = false;
  if (equalIgnoringCase(valueString, "yes"))
    return 1;
  if (equalIgnoringCase(valueString, "no"))
    return 0;
  if (equalIgnoringCase(valueString, "device-width"))
    return 10;
  if (equalIgnoringCase(valueString, "device-height"))
    return 10;

  float value =
      parsePositiveNumber(document, reportWarnings, keyString, valueString);

  if (value < 0)
    return ViewportDescription::ValueAuto;

  if (value > 10.0 && reportWarnings)
    reportViewportWarning(document, MaximumScaleTooLargeError, String(),
                          String());

  if (!value && viewportMetaZeroValuesQuirk)
    return ViewportDescription::ValueAuto;

  float clampedValue = clampScaleValue(value);
  if (clampedValue == value)
    computedValueMatchesParsedValue = true;

  return clampedValue;
}

bool HTMLMetaElement::parseViewportValueAsUserZoom(
    Document* document,
    bool reportWarnings,
    const String& keyString,
    const String& valueString,
    bool& computedValueMatchesParsedValue) {
  // yes and no are used as keywords.
  // Numbers >= 1, numbers <= -1, device-width and device-height are mapped to
  // yes.
  // Numbers in the range <-1, 1>, and unknown values, are mapped to no.

  computedValueMatchesParsedValue = false;
  if (equalIgnoringCase(valueString, "yes")) {
    computedValueMatchesParsedValue = true;
    return true;
  }
  if (equalIgnoringCase(valueString, "no")) {
    computedValueMatchesParsedValue = true;
    return false;
  }
  if (equalIgnoringCase(valueString, "device-width"))
    return true;
  if (equalIgnoringCase(valueString, "device-height"))
    return true;

  float value =
      parsePositiveNumber(document, reportWarnings, keyString, valueString);
  if (fabs(value) < 1)
    return false;

  return true;
}

float HTMLMetaElement::parseViewportValueAsDPI(Document* document,
                                               bool reportWarnings,
                                               const String& keyString,
                                               const String& valueString) {
  if (equalIgnoringCase(valueString, "device-dpi"))
    return ViewportDescription::ValueDeviceDPI;
  if (equalIgnoringCase(valueString, "low-dpi"))
    return ViewportDescription::ValueLowDPI;
  if (equalIgnoringCase(valueString, "medium-dpi"))
    return ViewportDescription::ValueMediumDPI;
  if (equalIgnoringCase(valueString, "high-dpi"))
    return ViewportDescription::ValueHighDPI;

  bool ok;
  float value = parsePositiveNumber(document, reportWarnings, keyString,
                                    valueString, &ok);
  if (!ok || value < 70 || value > 400)
    return ViewportDescription::ValueAuto;

  return value;
}

void HTMLMetaElement::processViewportKeyValuePair(
    Document* document,
    bool reportWarnings,
    const String& keyString,
    const String& valueString,
    bool viewportMetaZeroValuesQuirk,
    void* data) {
  ViewportDescription* description = static_cast<ViewportDescription*>(data);

  if (keyString == "width") {
    const Length& width = parseViewportValueAsLength(document, reportWarnings,
                                                     keyString, valueString);
    if (!width.isAuto()) {
      description->minWidth = Length(ExtendToZoom);
      description->maxWidth = width;
    }
  } else if (keyString == "height") {
    const Length& height = parseViewportValueAsLength(document, reportWarnings,
                                                      keyString, valueString);
    if (!height.isAuto()) {
      description->minHeight = Length(ExtendToZoom);
      description->maxHeight = height;
    }
  } else if (keyString == "initial-scale") {
    description->zoom = parseViewportValueAsZoom(
        document, reportWarnings, keyString, valueString,
        description->zoomIsExplicit, viewportMetaZeroValuesQuirk);
  } else if (keyString == "minimum-scale") {
    description->minZoom = parseViewportValueAsZoom(
        document, reportWarnings, keyString, valueString,
        description->minZoomIsExplicit, viewportMetaZeroValuesQuirk);
  } else if (keyString == "maximum-scale") {
    description->maxZoom = parseViewportValueAsZoom(
        document, reportWarnings, keyString, valueString,
        description->maxZoomIsExplicit, viewportMetaZeroValuesQuirk);
  } else if (keyString == "user-scalable") {
    description->userZoom = parseViewportValueAsUserZoom(
        document, reportWarnings, keyString, valueString,
        description->userZoomIsExplicit);
  } else if (keyString == "target-densitydpi") {
    description->deprecatedTargetDensityDPI = parseViewportValueAsDPI(
        document, reportWarnings, keyString, valueString);
    if (reportWarnings)
      reportViewportWarning(document, TargetDensityDpiUnsupported, String(),
                            String());
  } else if (keyString == "minimal-ui") {
    // Ignore vendor-specific argument.
  } else if (keyString == "shrink-to-fit") {
    // Ignore vendor-specific argument.
  } else if (reportWarnings) {
    reportViewportWarning(document, UnrecognizedViewportArgumentKeyError,
                          keyString, String());
  }
}

static const char* viewportErrorMessageTemplate(ViewportErrorCode errorCode) {
  static const char* const errors[] = {
      "The key \"%replacement1\" is not recognized and ignored.",
      "The value \"%replacement1\" for key \"%replacement2\" is invalid, and "
      "has been ignored.",
      "The value \"%replacement1\" for key \"%replacement2\" was truncated to "
      "its numeric prefix.",
      "The value for key \"maximum-scale\" is out of bounds and the value has "
      "been clamped.",
      "The key \"target-densitydpi\" is not supported.",
  };

  return errors[errorCode];
}

static MessageLevel viewportErrorMessageLevel(ViewportErrorCode errorCode) {
  switch (errorCode) {
    case TruncatedViewportArgumentValueError:
    case TargetDensityDpiUnsupported:
    case UnrecognizedViewportArgumentKeyError:
    case UnrecognizedViewportArgumentValueError:
    case MaximumScaleTooLargeError:
      return WarningMessageLevel;
  }

  NOTREACHED();
  return ErrorMessageLevel;
}

void HTMLMetaElement::reportViewportWarning(Document* document,
                                            ViewportErrorCode errorCode,
                                            const String& replacement1,
                                            const String& replacement2) {
  if (!document || !document->frame())
    return;

  String message = viewportErrorMessageTemplate(errorCode);
  if (!replacement1.isNull())
    message.replace("%replacement1", replacement1);
  if (!replacement2.isNull())
    message.replace("%replacement2", replacement2);

  // FIXME: This message should be moved off the console once a solution to
  // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
  document->addConsoleMessage(ConsoleMessage::create(
      RenderingMessageSource, viewportErrorMessageLevel(errorCode), message));
}

void HTMLMetaElement::getViewportDescriptionFromContentAttribute(
    const String& content,
    ViewportDescription& description,
    Document* document,
    bool viewportMetaZeroValuesQuirk) {
  parseContentAttribute(content, (void*)&description, document,
                        viewportMetaZeroValuesQuirk);

  if (description.minZoom == ViewportDescription::ValueAuto)
    description.minZoom = 0.25;

  if (description.maxZoom == ViewportDescription::ValueAuto) {
    description.maxZoom = 5;
    description.minZoom = std::min(description.minZoom, float(5));
  }
}
void HTMLMetaElement::processViewportContentAttribute(
    const String& content,
    ViewportDescription::Type origin) {
  DCHECK(!content.isNull());

  if (!document().shouldOverrideLegacyDescription(origin))
    return;

  ViewportDescription descriptionFromLegacyTag(origin);
  if (document().shouldMergeWithLegacyDescription(origin))
    descriptionFromLegacyTag = document().viewportDescription();

  getViewportDescriptionFromContentAttribute(
      content, descriptionFromLegacyTag, &document(),
      document().settings() &&
          document().settings()->getViewportMetaZeroValuesQuirk());

  document().setViewportDescription(descriptionFromLegacyTag);
}

void HTMLMetaElement::parseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == http_equivAttr || params.name == contentAttr) {
    process();
    return;
  }

  if (params.name != nameAttr)
    HTMLElement::parseAttribute(params);
}

Node::InsertionNotificationRequest HTMLMetaElement::insertedInto(
    ContainerNode* insertionPoint) {
  HTMLElement::insertedInto(insertionPoint);
  return InsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLMetaElement::didNotifySubtreeInsertionsToDocument() {
  process();
}

static bool inDocumentHead(HTMLMetaElement* element) {
  if (!element->isConnected())
    return false;

  return Traversal<HTMLHeadElement>::firstAncestor(*element);
}

void HTMLMetaElement::process() {
  if (!isInDocumentTree())
    return;

  // All below situations require a content attribute (which can be the empty
  // string).
  const AtomicString& contentValue = fastGetAttribute(contentAttr);
  if (contentValue.isNull())
    return;

  const AtomicString& nameValue = fastGetAttribute(nameAttr);
  if (!nameValue.isEmpty()) {
    if (equalIgnoringCase(nameValue, "viewport"))
      processViewportContentAttribute(contentValue,
                                      ViewportDescription::ViewportMeta);
    else if (equalIgnoringCase(nameValue, "referrer"))
      document().parseAndSetReferrerPolicy(contentValue,
                                           true /* support legacy keywords */);
    else if (equalIgnoringCase(nameValue, "handheldfriendly") &&
             equalIgnoringCase(contentValue, "true"))
      processViewportContentAttribute(
          "width=device-width", ViewportDescription::HandheldFriendlyMeta);
    else if (equalIgnoringCase(nameValue, "mobileoptimized"))
      processViewportContentAttribute("width=device-width, initial-scale=1",
                                      ViewportDescription::MobileOptimizedMeta);
    else if (equalIgnoringCase(nameValue, "theme-color") && document().frame())
      document().frame()->loader().client()->dispatchDidChangeThemeColor();
  }

  // Get the document to process the tag, but only if we're actually part of DOM
  // tree (changing a meta tag while it's not in the tree shouldn't have any
  // effect on the document).

  const AtomicString& httpEquivValue = fastGetAttribute(http_equivAttr);
  if (httpEquivValue.isEmpty())
    return;

  HttpEquiv::process(document(), httpEquivValue, contentValue,
                     inDocumentHead(this), this);
}

WTF::TextEncoding HTMLMetaElement::computeEncoding() const {
  HTMLAttributeList attributeList;
  for (const Attribute& attr : attributes())
    attributeList.push_back(
        std::make_pair(attr.name().localName(), attr.value().getString()));
  return encodingFromMetaAttributes(attributeList);
}

const AtomicString& HTMLMetaElement::content() const {
  return getAttribute(contentAttr);
}

const AtomicString& HTMLMetaElement::httpEquiv() const {
  return getAttribute(http_equivAttr);
}

const AtomicString& HTMLMetaElement::name() const {
  return getNameAttribute();
}
}
