/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/web/WebSearchableFormData.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/html/FormData.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "platform/network/FormDataEncoder.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebInputElement.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

using namespace HTMLNames;

namespace {

// Gets the encoding for the form.
// TODO(tkent): Use FormDataEncoder::encodingFromAcceptCharset().
void getFormEncoding(const HTMLFormElement& form, WTF::TextEncoding* encoding) {
  String str(form.fastGetAttribute(HTMLNames::accept_charsetAttr));
  str.replace(',', ' ');
  Vector<String> charsets;
  str.split(' ', charsets);
  for (const String& charset : charsets) {
    *encoding = WTF::TextEncoding(charset);
    if (encoding->isValid())
      return;
  }
  if (form.document().loader())
    *encoding = WTF::TextEncoding(form.document().encoding());
}

// If the form does not have an activated submit button, the first submit
// button is returned.
HTMLFormControlElement* buttonToActivate(const HTMLFormElement& form) {
  HTMLFormControlElement* firstSubmitButton = nullptr;
  for (auto& element : form.listedElements()) {
    if (!element->isFormControlElement())
      continue;
    HTMLFormControlElement* control = toHTMLFormControlElement(element);
    if (control->isActivatedSubmit()) {
      // There's a button that is already activated for submit, return
      // nullptr.
      return nullptr;
    }
    if (!firstSubmitButton && control->isSuccessfulSubmitButton())
      firstSubmitButton = control;
  }
  return firstSubmitButton;
}

// Returns true if the selected state of all the options matches the default
// selected state.
bool isSelectInDefaultState(const HTMLSelectElement& select) {
  if (select.isMultiple() || select.size() > 1) {
    for (const auto& optionElement : select.optionList()) {
      if (optionElement->selected() !=
          optionElement->fastHasAttribute(selectedAttr))
        return false;
    }
    return true;
  }

  // The select is rendered as a combobox (called menulist in WebKit). At
  // least one item is selected, determine which one.
  HTMLOptionElement* initialSelected = nullptr;
  for (const auto& optionElement : select.optionList()) {
    if (optionElement->fastHasAttribute(selectedAttr)) {
      // The page specified the option to select.
      initialSelected = optionElement;
      break;
    }
    if (!initialSelected)
      initialSelected = optionElement;
  }
  return !initialSelected || initialSelected->selected();
}

// Returns true if the form element is in its default state, false otherwise.
// The default state is the state of the form element on initial load of the
// page, and varies depending upon the form element. For example, a checkbox is
// in its default state if the checked state matches the state of the checked
// attribute.
bool isInDefaultState(const HTMLFormControlElement& formElement) {
  if (isHTMLInputElement(formElement)) {
    const HTMLInputElement& inputElement = toHTMLInputElement(formElement);
    if (inputElement.type() == InputTypeNames::checkbox ||
        inputElement.type() == InputTypeNames::radio)
      return inputElement.checked() ==
             inputElement.fastHasAttribute(checkedAttr);
  } else if (isHTMLSelectElement(formElement)) {
    return isSelectInDefaultState(toHTMLSelectElement(formElement));
  }
  return true;
}

// Look for a suitable search text field in a given HTMLFormElement
// Return nothing if one of those items are found:
//  - A text area field
//  - A file upload field
//  - A Password field
//  - More than one text field
HTMLInputElement* findSuitableSearchInputElement(const HTMLFormElement& form) {
  HTMLInputElement* textElement = nullptr;
  for (const auto& item : form.listedElements()) {
    if (!item->isFormControlElement())
      continue;

    HTMLFormControlElement& control = toHTMLFormControlElement(*item);

    if (control.isDisabledFormControl() || control.name().isNull())
      continue;

    if (!isInDefaultState(control) || isHTMLTextAreaElement(control))
      return nullptr;

    if (isHTMLInputElement(control) && control.willValidate()) {
      const HTMLInputElement& input = toHTMLInputElement(control);

      // Return nothing if a file upload field or a password field are
      // found.
      if (input.type() == InputTypeNames::file ||
          input.type() == InputTypeNames::password)
        return nullptr;

      if (input.isTextField()) {
        if (textElement) {
          // The auto-complete bar only knows how to fill in one
          // value.  This form has multiple fields; don't treat it as
          // searchable.
          return nullptr;
        }
        textElement = toHTMLInputElement(&control);
      }
    }
  }
  return textElement;
}

// Build a search string based on a given HTMLFormElement and HTMLInputElement
//
// Search string output example from www.google.com:
// "hl=en&source=hp&biw=1085&bih=854&q={searchTerms}&btnG=Google+Search&aq=f&aqi=&aql=&oq="
//
// Return false if the provided HTMLInputElement is not found in the form
bool buildSearchString(const HTMLFormElement& form,
                       Vector<char>* encodedString,
                       const WTF::TextEncoding& encoding,
                       const HTMLInputElement* textElement) {
  bool isElementFound = false;
  for (const auto& item : form.listedElements()) {
    if (!item->isFormControlElement())
      continue;

    HTMLFormControlElement& control = toHTMLFormControlElement(*item);
    if (control.isDisabledFormControl() || control.name().isNull())
      continue;

    FormData* formData = FormData::create(encoding);
    control.appendToFormData(*formData);

    for (const auto& entry : formData->entries()) {
      if (!encodedString->isEmpty())
        encodedString->push_back('&');
      FormDataEncoder::encodeStringAsFormData(*encodedString, entry->name(),
                                              FormDataEncoder::NormalizeCRLF);
      encodedString->push_back('=');
      if (&control == textElement) {
        encodedString->append("{searchTerms}", 13);
        isElementFound = true;
      } else {
        FormDataEncoder::encodeStringAsFormData(*encodedString, entry->value(),
                                                FormDataEncoder::NormalizeCRLF);
      }
    }
  }
  return isElementFound;
}

}  // namespace

WebSearchableFormData::WebSearchableFormData(
    const WebFormElement& form,
    const WebInputElement& selectedInputElement) {
  HTMLFormElement* formElement = static_cast<HTMLFormElement*>(form);
  HTMLInputElement* inputElement =
      static_cast<HTMLInputElement*>(selectedInputElement);

  // Only consider forms that GET data.
  if (equalIgnoringASCIICase(formElement->getAttribute(methodAttr), "post"))
    return;

  WTF::TextEncoding encoding;
  getFormEncoding(*formElement, &encoding);
  if (!encoding.isValid()) {
    // Need a valid encoding to encode the form elements.
    // If the encoding isn't found webkit ends up replacing the params with
    // empty strings. So, we don't try to do anything here.
    return;
  }

  // Look for a suitable search text field in the form when a
  // selectedInputElement is not provided.
  if (!inputElement) {
    inputElement = findSuitableSearchInputElement(*formElement);

    // Return if no suitable text element has been found.
    if (!inputElement)
      return;
  }

  HTMLFormControlElement* firstSubmitButton = buttonToActivate(*formElement);
  if (firstSubmitButton) {
    // The form does not have an active submit button, make the first button
    // active. We need to do this, otherwise the URL will not contain the
    // name of the submit button.
    firstSubmitButton->setActivatedSubmit(true);
  }

  Vector<char> encodedString;
  bool isValidSearchString =
      buildSearchString(*formElement, &encodedString, encoding, inputElement);

  if (firstSubmitButton)
    firstSubmitButton->setActivatedSubmit(false);

  // Return if the search string is not valid.
  if (!isValidSearchString)
    return;

  String action(formElement->action());
  KURL url(formElement->document().completeURL(action.isNull() ? "" : action));
  RefPtr<EncodedFormData> formData = EncodedFormData::create(encodedString);
  url.setQuery(formData->flattenToString());
  m_url = url;
  m_encoding = String(encoding.name());
}

}  // namespace blink
