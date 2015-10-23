// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Installs Autofill management functions on the |__gCrWeb| object.
//
// It scans the DOM, extracting and storing forms and returns a JSON string
// representing an array of objects, each of which represents an Autofill form
// with information about a form to be filled and/or submitted and it can be
// translated to struct FormData
// (chromium/src/components/autofill/core/common/form_data.h) for further
// processing.

/** @typedef {HTMLInputElement|HTMLTextAreaElement|HTMLSelectElement} */
var FormControlElement;

/**
  * @typedef {{
  *   name: string,
  *   form_control_type: string,
  *   autocomplete_attributes: string,
  *   is_autofilled: boolean,
  *   should_autocomplete: boolean,
  *   max_length: number,
  *   is_checkable: boolean,
  *   value: string
  * }}
  */
var AutofillFormFieldData;

/**
  * @typedef {{
  *   name: string,
  *   method: string,
  *   origin: string,
  *   action: string,
  *   fields: Array<AutofillFormFieldData>
  * }}
  */
var AutofillFormData;

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb['autofill'] = {};

/**
 * The maximum length allowed for form data.
 *
 * This variable is from AutofillTable::kMaxDataLength in
 * chromium/src/components/autofill/core/browser/webdata/autofill_table.h
 *
 * @const {number}
 */
__gCrWeb.autofill.MAX_DATA_LENGTH = 1024;

/**
 * The maximum number of form fields we are willing to parse, due to
 * computational costs. Several examples of forms with lots of fields that are
 * not relevant to Autofill: (1) the Netflix queue; (2) the Amazon wishlist;
 * (3) router configuration pages; and (4) other configuration pages, e.g. for
 * Google code project settings.
 *
 * This variable is |kMaxParseableFields| from
 * chromium/src/components/autofill/content/renderer/form_autofill_util.h
 *
 * @const {number}
 */
__gCrWeb.autofill.MAX_PARSEABLE_FIELDS = 100;

/**
 * A bit field mask to extract data from WebFormControlElement for
 * extracting none value.
 *
 * This variable is from enum ExtractMask in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.h
 *
 * @const {number}
 */
__gCrWeb.autofill.EXTRACT_MASK_NONE = 0;

/**
 * A bit field mask to extract data from WebFormControlElement for
 * extracting value from WebFormControlElement.
 *
 * This variable is from enum ExtractMask in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.h
 *
 * @const {number}
 */
__gCrWeb.autofill.EXTRACT_MASK_VALUE = 1 << 0;

/**
 * A bit field mask to extract data from WebFormControlElement for
 * extracting option text from WebFormSelectElement. Only valid when
 * EXTRACT_MASK_VALUE is set. This is used for form submission where human
 * readable value is captured.
 *
 * This variable is from enum ExtractMask in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.h
 *
 * @const {number}
 */
__gCrWeb.autofill.EXTRACT_MASK_OPTION_TEXT = 1 << 1;

/**
 * A bit field mask to extract data from WebFormControlElement for
 * extracting options from WebFormControlElement.
 *
 * This variable is from enum ExtractMask in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.h
 *
 * @const {number}
 */
__gCrWeb.autofill.EXTRACT_MASK_OPTIONS = 1 << 2;

/**
 * The last element that was autofilled.
 *
 * @type {Element}
 */
__gCrWeb.autofill.lastAutoFilledElement = null;

/**
 * The last element that was active (used to restore focus if necessary).
 *
 * @type {Element}
 */
__gCrWeb.autofill.lastActiveElement = null;

/**
 * Whether CSS for autofilled elements has been injected into the page.
 *
 * @type {boolean}
 */
__gCrWeb.autofill.styleInjected = false;

/**
 * Extracts fields from |controlElements| with |extractMask| to |formFields|.
 * The extracted fields are also placed in |elementArray|.
 *
 * It is based on the logic in
 *     bool ExtractFieldsFromControlElements(
 *         const WebVector<WebFormControlElement>& control_elements,
 *         ExtractMask extract_mask,
 *         ScopedVector<FormFieldData>* form_fields,
 *         std::vector<bool>* fields_extracted,
 *         std::map<WebFormControlElement, FormFieldData*>* element_map)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * TODO(thestig): Make |element_map| a Map when Chrome makes iOS 8 and Safari 8
 *                part of the minimal requirements.
 *
 * @param {Array<FormControlElement>} controlElements The control elements that
 *     will be processed.
 * @param {number} extractMask Mask controls what data is extracted from
 *     controlElements.
 * @param {Array<AutofillFormFieldData>} formFields The extracted form fields.
 * @param {Array<boolean>} fieldsExtracted Indicates whether the fields were
 *     extracted.
 * @param {Array<?AutofillFormFieldData>} elementArray The extracted form
 *     fields or null if a particular control has no corresponding field.
 * @return {boolean} Whether there are fields and not too many fields in the
 *     form.
 */
function extractFieldsFromControlElements_(controlElements, extractMask,
    formFields, fieldsExtracted, elementArray) {
  for (var i = 0; i < controlElements.length; ++i) {
    fieldsExtracted[i] = false;
    elementArray[i] = null;

    /** @type {FormControlElement} */
    var controlElement = controlElements[i];
    if (!__gCrWeb.autofill.isAutofillableElement(controlElement)) {
      continue;
    }

    // Create a new AutofillFormFieldData, fill it out and map it to the
    // field's name.
    var formField = new __gCrWeb['common'].JSONSafeObject;
    __gCrWeb.autofill.webFormControlElementToFormField(
        controlElement, extractMask, formField);
    formFields.push(formField);
    elementArray[i] = formField;
    fieldsExtracted[i] = true;

    // To avoid overly expensive computation, we impose a maximum number of
    // allowable fields.
    if (formFields.length > __gCrWeb.autofill.MAX_PARSEABLE_FIELDS) {
      return false;
    }
  }

  return formFields.length > 0;
}

/**
 * Check if the node is visible.
 *
 * @param {Node} node The node to be processed.
 * @return {boolean} Whether the node is visible or not.
 */
function isVisibleNode_(node) {
  if (!node)
    return false;

  if (node.nodeType === Node.ELEMENT_NODE) {
    var style = window.getComputedStyle(/** @type {Element} */(node));
    if (style.visibility == 'hidden' || style.display == 'none')
      return false;
  }

  // Verify all ancestors are focusable.
  return !node.parentNode || isVisibleNode_(node.parentNode);
}

/**
 * For each label element, get the corresponding form control element, use the
 * form control element along with |controlElements| and |elementArray| to find
 * the previously created AutofillFormFieldData and set the
 * AutofillFormFieldData's label to the label.firstChild().nodeValue() of the
 * label element.
 *
 * It is based on the logic in
 *     void MatchLabelsAndFields(
 *         const WebElementCollection& labels,
 *         std::map<WebFormControlElement, FormFieldData*>* element_map);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * This differs in that it takes a formElement field, instead of calling
 * field_element.isFormControlElement().
 *
 * This also uses (|controlElements|, |elementArray|) because there is no
 * guaranteeded Map support on iOS yet.
 *
 * @param {NodeList} labels The labels to match.
 * @param {HTMLFormElement} formElement The form element being processed.
 * @param {Array<FormControlElement>} controlElements The control elements that
 *     were processed.
 * @param {Array<?AutofillFormFieldData>} elementArray The extracted fields.
 */
function matchLabelsAndFields_(labels, formElement, controlElements,
    elementArray) {
  for (var index = 0; index < labels.length; ++index) {
    var label = labels[index];
    var fieldElement = label.control;
    var fieldData = null;
    if (!fieldElement) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      var elementName = label.htmlFor;
      if (!elementName)
        continue;
      // Look through the list for elements with this name. There can actually
      // be more than one. In this case, the label may not be particularly
      // useful, so just discard it.
      for (var elementIndex = 0; elementIndex < elementArray.length;
           ++elementIndex) {
        var currentFieldData = elementArray[elementIndex];
        if (currentFieldData && currentFieldData['name'] === elementName) {
          if (fieldData !== null) {
            fieldData = null;
            break;
          } else {
            fieldData = currentFieldData;
          }
        }
      }
    } else if (fieldElement.form != formElement ||
                   fieldElement.type === 'hidden') {
      continue;
    } else {
      // Typical case: look up |fieldData| in |elementArray|.
      for (var elementIndex = 0; elementIndex < elementArray.length;
           ++elementIndex) {
        if (controlElements[elementIndex] === fieldElement) {
          fieldData = elementArray[elementIndex];
          break;
        }
      }
    }

    if (!fieldData)
      continue;

    if (!('label' in fieldData)) {
      fieldData['label'] = '';
    }
    var labelText = __gCrWeb.autofill.findChildText(label);
    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (fieldData['label'].length > 0 && labelText.length > 0) {
      fieldData['label'] += ' ';
    }
    fieldData['label'] += labelText;
  }
}

/**
 * Common function shared by webFormElementToFormData() and
 * unownedFormElementsAndFieldSetsToFormData(). Either pass in:
 * 1) |formElement|, |formControlElement| and an empty |fieldsets|.
 * or
 * 2) a non-empty |fieldsets|.
 *
 * It is based on the logic in
 *     bool FormOrFieldsetsToFormData(
 *         const blink::WebFormElement* form_element,
 *         const blink::WebFormControlElement* form_control_element,
 *         const std::vector<blink::WebElement>& fieldsets,
 *         const WebVector<WebFormControlElement>& control_elements,
 *         ExtractMask extract_mask,
 *         FormData* form,
 *         FormFieldData* field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * @param {HTMLFormElement} formElement The form element that will be processed.
 * @param {FormControlElement} formControlElement A control element in
 *     formElment, the FormField of which will be returned in field.
 * @param {Array<Element>} fieldsets The fieldsets to look through if
 *     formElement and formControlElement are not specified.
 * @param {Array<FormControlElement>} controlElements The control elements that
 *     will be processed.
 * @param {number} extractMask Mask controls what data is extracted from
 *     formElement.
 * @param {AutofillFormData} form Form to fill in the AutofillFormData
 *     information of formElement.
 * @param {AutofillFormFieldData|null} field Field to fill in the form field
 *     information of formControlElement.
 * @return {boolean} Whether there are fields and not too many fields in the
 *     form.
 */
function formOrFieldsetsToFormData_(formElement, formControlElement,
    fieldsets, controlElements, extractMask, form, field) {
  // This should be a map from a control element to the AutofillFormFieldData.
  // However, without Map support, it's just an Array of AutofillFormFieldData.
  var elementArray = [];

  // The extracted FormFields.
  var formFields = [];

  // A vector of bools that indicate whether each element in |controlElements|
  // meets the requirements and thus will be in the resulting |form|.
  var fieldsExtracted = [];

  if (!extractFieldsFromControlElements_(controlElements, extractMask,
                                         formFields, fieldsExtracted,
                                         elementArray)) {
    return false;
  }

  if (formElement) {
    // Loop through the label elements inside the form element. For each label
    // element, get the corresponding form control element, use the form control
    // element along with |controlElements| and |elementArray| to find the
    // previously created AutofillFormFieldData and set the
    // AutofillFormFieldData's label.
    var labels = formElement.getElementsByTagName('label');
    matchLabelsAndFields_(labels, formElement, controlElements, elementArray);
  } else {
    // Same as the if block, but for all the labels in fieldset
    for (var i = 0; i < fieldsets.length; ++i) {
      var labels = fieldsets[i].getElementsByTagName('label');
      matchLabelsAndFields_(labels, formElement, controlElements, elementArray);
    }
  }

  // Loop through the form control elements, extracting the label text from
  // the DOM.  We use the |fieldsExtracted| vector to make sure we assign the
  // extracted label to the correct field, as it's possible |form_fields| will
  // not contain all of the elements in |control_elements|.
  for (var i = 0, fieldIdx = 0;
       i < controlElements.length && fieldIdx < formFields.length; ++i) {
    // This field didn't meet the requirements, so don't try to find a label
    // for it.
    if (!fieldsExtracted[i])
      continue;

    var controlElement = controlElements[i];
    var fieldLabel = formFields[fieldIdx]['label'];
    if (!fieldLabel) {
      formFields[fieldIdx]['label'] =
          __gCrWeb.autofill.inferLabelForElement(controlElement);
    }
    if (controlElement === formControlElement)
      field = formFields[fieldIdx];
    ++fieldIdx;
  }

  form['fields'] = formFields;
  // Protect against custom implementation of Array.toJSON in host pages.
  form['fields'].toJSON = null;
  return true;
}

/**
 * Scans DOM and returns a JSON string representation of forms and form
 * extraction results.
 *
 * TODO(thestig): Merge with extractNewForms()?
 *
 * @param {number} requiredFields The minimum number of fields forms must have
 *     to be extracted.
 * @return {string} A JSON encoded object with object['forms'] containing the
 *     forms data.
 */
__gCrWeb.autofill['extractForms'] = function(requiredFields) {
  var forms = [];
  // Protect against custom implementation of Array.toJSON in host pages.
  /** @suppress {checkTypes} */(function() { forms.toJSON = null; })();

  __gCrWeb.autofill.extractNewForms(
      window,
      requiredFields,
      forms);
  var results = new __gCrWeb.common.JSONSafeObject;
  results['forms'] = forms;
  return __gCrWeb.stringify(results);
};

/**
 * Stores the current active element. This is used to make the element active
 * again in case the web view loses focus when a dialog is presented over it.
 */
__gCrWeb.autofill['storeActiveElement'] = function() {
  __gCrWeb.autofill.lastActiveElement = document.activeElement;
}

/**
 * Clears the current active element by setting it to null.
 */
__gCrWeb.autofill['clearActiveElement'] = function() {
  __gCrWeb.autofill.lastActiveElement = null;
}

/**
 * Fills data into the active form field. The active form field is either
 * document.activeElement or the value of lastActiveElement if that value is
 * non-null.
 *
 * @param {AutofillFormFieldData} data The data to fill in.
 */
__gCrWeb.autofill['fillActiveFormField'] = function(data) {
  var activeElement = document.activeElement;
  if (__gCrWeb.autofill.lastActiveElement) {
    activeElement = __gCrWeb.autofill.lastActiveElement;
    activeElement.focus();
    __gCrWeb.autofill.lastActiveElement = null;
  }
  if (data['name'] !== __gCrWeb['common'].nameForAutofill(activeElement)) {
    return;
  }
  __gCrWeb.autofill.lastAutoFilledElement = activeElement;
  __gCrWeb.autofill.fillFormField(data, activeElement);
};

/**
 * Fills a number of fields in the same named form for full-form Autofill.
 * Applies Autofill CSS (i.e. yellow background) to filled elements.
 * Only empty fields will be filled, except that field named
 * |forceFillFieldName| will always be filled even if non-empty.
 *
 * @param {Object} data Dictionary of data to fill in.
 * @param {string} forceFillFieldName Named field will always be filled even if
 *     non-empty. May be null.
 */
__gCrWeb.autofill['fillForm'] = function(data, forceFillFieldName) {
  // Inject CSS to style the autofilled elements with a yellow background.
  if (!__gCrWeb.autofill.styleInjected) {
    var style = document.createElement('style');
    style.textContent = '[chrome-autofilled] {' +
      'background-color:#FAFFBD !important;' +
      'background-image:none !important;' +
      'color:#000000 !important;' +
      '}';
    document.head.appendChild(style);
    __gCrWeb.autofill.styleInjected = true;
  }

  // Remove Autofill styling when control element is edited.
  var controlElementInputListener = function(evt) {
    evt.target.removeAttribute('chrome-autofilled');
    evt.target.isAutofilled = false;
    evt.target.removeEventListener('input', controlElementInputListener);
  };

  var form = __gCrWeb.common.getFormElementFromIdentifier(data.formName);
  var controlElements = __gCrWeb.common.getFormControlElements(form);
  for (var i = 0; i < controlElements.length; ++i) {
    var element = controlElements[i];
    if (!__gCrWeb.autofill.isAutofillableElement(element)) {
      continue;
    }
    var fieldName = __gCrWeb['common'].nameForAutofill(element);

    // Skip non-empty fields unless this is the forceFillFieldName or it's a
    // 'select-one' element. 'select-one' elements are always autofilled even
    // if non-empty; see AutofillManager::FillOrPreviewDataModelForm().
    if (element.value && element.value.length > 0 &&
        !__gCrWeb.autofill.isSelectElement(element) &&
        fieldName !== forceFillFieldName) {
      continue;
    }

    // Don't fill field if source value is empty or missing.
    var value = data.fields[fieldName];
    if (!value)
      continue;

    if (__gCrWeb.autofill.isTextInput(element) ||
        __gCrWeb.autofill.isTextAreaElement(element)) {
      __gCrWeb.common.setInputElementValue(value, element, true);
    } else if (__gCrWeb.autofill.isSelectElement(element)) {
      if (element.value !== value) {
        element.value = value;
        __gCrWeb.common.createAndDispatchHTMLEvent(element, 'change', true,
            false);
      }
    }
    // TODO(bondd): Handle __gCrWeb.autofill.isCheckableElement(element) ==
    // true. |is_checked| is not currently passed in by the caller.

    element.setAttribute('chrome-autofilled');
    element.isAutofilled = true;
    element.addEventListener('input', controlElementInputListener);
  }

  // Remove Autofill styling when form receives 'reset' event.
  // Individual control elements may be left with 'input' event listeners but
  // they are harmless.
  var formResetListener = function(evt) {
    var controlElements = __gCrWeb.common.getFormControlElements(evt.target);
    for (var i = 0; i < controlElements.length; ++i) {
      controlElements[i].removeAttribute('chrome-autofilled');
      controlElements[i].isAutofilled = false;
    }
    evt.target.removeEventListener('reset', formResetListener);
  };
  form.addEventListener('reset', formResetListener);
};

/**
 * Clear autofilled fields of the specified form. Fields that are not currently
 * autofilled are not modified.
 * Field contents are cleared, and Autofill flag and styling are removed.
 * 'change' events are sent for fields whose contents changed.
 * Based on FormCache::ClearFormWithElement().
 *
 * @param {string} formName Identifier for form element (from
 *     getFormIdentifier).
 */
__gCrWeb.autofill['clearAutofilledFields'] = function(formName) {
  var form = __gCrWeb.common.getFormElementFromIdentifier(formName);
  var controlElements = __gCrWeb.common.getFormControlElements(form);
  for (var i = 0; i < controlElements.length; ++i) {
    var element = controlElements[i];
    if (!element.isAutofilled || element.disabled)
      continue;

    if (__gCrWeb.autofill.isTextInput(element) ||
        __gCrWeb.autofill.isTextAreaElement(element)) {
      __gCrWeb.common.setInputElementValue('', element, true);
    } else if (__gCrWeb.autofill.isSelectElement(element)) {
      // Reset to the first index.
      // TODO(bondd): Store initial values and reset to the correct one here.
      if (element.selectedIndex != 0) {
        element.selectedIndex = 0;
        __gCrWeb.common.createAndDispatchHTMLEvent(element, 'change', true,
            false);
      }
    } else if (__gCrWeb.autofill.isCheckableElement(element)) {
      // TODO(bondd): Handle checkable elements. They aren't properly supported
      // by iOS Autofill yet.
    }

    element.removeAttribute('chrome-autofilled');
    element.isAutofilled = false;
  }
};

/**
 * See extractFormsAndFormElements below.
 *
 * @param {HTMLFrameElement|Window} frame A window or a frame containing forms
 *     from which the data will be extracted.
 * @param {number} minimumRequiredFields The minimum number of fields a form
 *     should contain for autofill.
 * @param {Array<AutofillFormData>} forms Forms that will be filled in data of
 *     forms in frame.
 */
__gCrWeb.autofill.extractNewForms = function(
    frame, minimumRequiredFields, forms) {
  __gCrWeb.autofill.extractFormsAndFormElements(
      frame, minimumRequiredFields, forms);
}

/**
 * Scans the DOM in |frame| extracting and storing forms. Fills |forms| with
 * extracted forms.
 *
 * This method is based on the logic in method
 *
 *     bool FormCache::ExtractNewForms(
 *         const WebFrame& frame,
 *         std::vector<FormData>* forms)
 *
 * in chromium/src/components/autofill/content/renderer/form_cache.cc.
 *
 * The difference is in this implementation, the cache is not considered.
 * Initial values of select and checkable elements are not recorded at the
 * moment.
 *
 * This version still takes the minimumRequiredFields parameters. Whereas the
 * C++ version does not.
 *
 * TODO(thestig): Update iOS internal callers to use extractNewForms(). Once
 * that happens, this can be removed.
 *
 * @param {HTMLFrameElement|Window} frame A window or a frame containing forms
 *     from which the data will be extracted.
 * @param {number} minimumRequiredFields The minimum number of fields a form
 *     should contain for autofill.
 * @param {Array<AutofillFormData>} forms Forms that will be filled in data of
 *     forms in frame.
 * @return {boolean} Whether there are unextracted forms due to
 *     |minimumRequiredFields| limit.
 */
__gCrWeb.autofill.extractFormsAndFormElements = function(
    frame, minimumRequiredFields, forms) {
  if (!frame) {
    return false;
  }
  var doc = frame.document;
  if (!doc) {
    return false;
  }

  /** @type {HTMLCollection} */
  var webForms = doc.forms;

  var numFieldsSeen = 0;
  var hasSkippedForms = false;
  for (var formIndex = 0; formIndex < webForms.length; ++formIndex) {
    /** @type {HTMLFormElement} */
    var formElement = webForms[formIndex];
    var controlElements =
        __gCrWeb.autofill.extractAutofillableElementsInForm(formElement);
    var numEditableElements = 0;
    for (var elementIndex = 0; elementIndex < controlElements.length;
         ++elementIndex) {
      var element = controlElements[elementIndex];
      if (!__gCrWeb.autofill.isCheckableElement(element)) {
        ++numEditableElements;
      }
    }

    // To avoid overly expensive computation, we impose a minimum number of
    // allowable fields.  The corresponding maximum number of allowable
    // fields is imposed by webFormElementToFormData().
    if (numEditableElements < minimumRequiredFields &&
        controlElements.length > 0) {
      hasSkippedForms = true;
      continue;
    }

    var extractMask = __gCrWeb.autofill.EXTRACT_MASK_VALUE |
        __gCrWeb.autofill.EXTRACT_MASK_OPTIONS;
    var form = new __gCrWeb['common'].JSONSafeObject;
    if (!__gCrWeb.autofill.webFormElementToFormData(
        frame, formElement, null, extractMask, form, null /* field */)) {
      continue;
    }
    numFieldsSeen += form['fields'].length;
    if (numFieldsSeen > __gCrWeb.autofill.MAX_PARSEABLE_FIELDS) {
      break;
    }

    if (form.fields.length >= minimumRequiredFields) {
      forms.push(form);
    } else {
      hasSkippedForms = true;
    }
  }

  // Recursively invoke for all frames/iframes.
  var frames = frame.frames;
  for (var i = 0; i < frames.length; i++) {
    var hasSkippedInframe = __gCrWeb.autofill.extractFormsAndFormElements(
        frames[i], minimumRequiredFields, forms);
    hasSkippedForms = hasSkippedForms || hasSkippedInframe;
  }
  return hasSkippedForms;
};

/**
 * Fills |form| with the form data object corresponding to the |formElement|.
 * If |field| is non-NULL, also fills |field| with the FormField object
 * corresponding to the |formControlElement|.
 * |extract_mask| controls what data is extracted.
 * Returns true if |form| is filled out. Returns false if there are no fields or
 * too many fields in the |form|.
 *
 * It is based on the logic in
 *     bool WebFormElementToFormData(
 *         const blink::WebFormElement& form_element,
 *         const blink::WebFormControlElement& form_control_element,
 *         ExtractMask extract_mask,
 *         FormData* form,
 *         FormFieldData* field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * @param {HTMLFrameElement|Window} frame The window or frame where the
 *     formElement is in.
 * @param {HTMLFormElement} formElement The form element that will be processed.
 * @param {FormControlElement} formControlElement A control element in
 *     formElment, the FormField of which will be returned in field.
 * @param {number} extractMask Mask controls what data is extracted from
 *     formElement.
 * @param {AutofillFormData} form Form to fill in the AutofillFormData
 *     information of formElement.
 * @param {AutofillFormFieldData|null} field Field to fill in the form field
 *     information of formControlElement.
 * @return {boolean} Whether there are fields and not too many fields in the
 *     form.
 */
__gCrWeb.autofill.webFormElementToFormData = function(
    frame, formElement, formControlElement, extractMask, form, field) {
  if (!frame) {
    return false;
  }

  form['name'] = __gCrWeb.common.getFormIdentifier(formElement);
  var method = formElement.getAttribute('method');
  if (method) {
    form['method'] = method;
  }
  form['origin'] = __gCrWeb.common.removeQueryAndReferenceFromURL(
      frame.location.href);
  form['action'] = __gCrWeb.common.absoluteURL(
      frame.document,
      formElement.getAttribute('action'));

  // Note different from form_autofill_util.cc version of this method, which
  // computes |form.action| using document.completeURL(form_element.action())
  // and falls back to formElement.action() if the computed action is invalid,
  // here the action returned by |__gCrWeb.common.absoluteURL| is always
  // valid, which is computed by creating a <a> element, and we don't check if
  // the action is valid.

  var controlElements = __gCrWeb['common'].getFormControlElements(formElement);

  return formOrFieldsetsToFormData_(formElement, formControlElement,
      [] /* fieldsets */, controlElements, extractMask, form, field);
};

/**
 * Returns is the tag of an |element| is tag.
 *
 * It is based on the logic in
 *     bool HasTagName(const WebNode& node, const blink::WebString& tag)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Node} node Node to examine.
 * @param {string} tag Tag name.
 * @return {boolean} Whether the tag of node is tag.
 */
__gCrWeb.autofill.hasTagName = function(node, tag) {
  return node.nodeType === Node.ELEMENT_NODE &&
         /** @type {Element} */(node).tagName === tag.toUpperCase();
};

/**
 * Checks if an element is autofillable.
 *
 * It is based on the logic in
 *     bool IsAutofillableElement(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is one of the element types that can be
 *     autofilled.
 */
__gCrWeb.autofill.isAutofillableElement = function(element) {
  return __gCrWeb.autofill.isAutofillableInputElement(element) ||
         __gCrWeb.autofill.isSelectElement(element) ||
         __gCrWeb.autofill.isTextAreaElement(element);
};

/**
 * Trims whitespace from the start of the input string.
 * Simplified version of string_util::TrimWhitespace.
 * @param {string} input String to trim.
 * @return {string} The |input| string without leading whitespace.
 */
__gCrWeb.autofill.trimWhitespaceLeading = function(input) {
  return input.replace(/^\s+/gm, '');
};

/**
 * Trims whitespace from the end of the input string.
 * Simplified version of string_util::TrimWhitespace.
 * @param {string} input String to trim.
 * @return {string} The |input| string without trailing whitespace.
 */
__gCrWeb.autofill.trimWhitespaceTrailing = function(input) {
  return input.replace(/\s+$/gm, '');
};

/**
 * Appends |suffix| to |prefix| so that any intermediary whitespace is collapsed
 * to a single space.  If |force_whitespace| is true, then the resulting string
 * is guaranteed to have a space between |prefix| and |suffix|.  Otherwise, the
 * result includes a space only if |prefix| has trailing whitespace or |suffix|
 * has leading whitespace.
 *
 * A few examples:
 *     CombineAndCollapseWhitespace('foo', 'bar', false)       -> 'foobar'
 *     CombineAndCollapseWhitespace('foo', 'bar', true)        -> 'foo bar'
 *     CombineAndCollapseWhitespace('foo ', 'bar', false)      -> 'foo bar'
 *     CombineAndCollapseWhitespace('foo', ' bar', false)      -> 'foo bar'
 *     CombineAndCollapseWhitespace('foo', ' bar', true)       -> 'foo bar'
 *     CombineAndCollapseWhitespace('foo   ', '   bar', false) -> 'foo bar'
 *     CombineAndCollapseWhitespace(' foo', 'bar ', false)     -> ' foobar '
 *     CombineAndCollapseWhitespace(' foo', 'bar ', true)      -> ' foo bar '
 *
 * It is based on the logic in
 * const string16 CombineAndCollapseWhitespace(const string16& prefix,
 *                                             const string16& suffix,
 *                                             bool force_whitespace)
 * @param {string} prefix The prefix string in the string combination.
 * @param {string} suffix The suffix string in the string combination.
 * @param {boolean} forceWhitespace A boolean indicating if whitespace should
 *     be added as separator in the combination.
 * @return {string} The combined string.
 */
__gCrWeb.autofill.combineAndCollapseWhitespace = function(
    prefix, suffix, forceWhitespace) {
  var prefixTrimmed = __gCrWeb.autofill.trimWhitespaceTrailing(prefix);
  var prefixTrailingWhitespace = prefixTrimmed != prefix;
  var suffixTrimmed = __gCrWeb.autofill.trimWhitespaceLeading(suffix);
  var suffixLeadingWhitespace = suffixTrimmed != suffix;
  if (prefixTrailingWhitespace || suffixLeadingWhitespace || forceWhitespace) {
    return prefixTrimmed + ' ' + suffixTrimmed;
  } else {
    return prefixTrimmed + suffixTrimmed;
  }
};

/**
 * This is a helper function for the findChildText() function (see below).
 * Search depth is limited with the |depth| parameter.
 * Based on form_autofill_util::findChildTextInner().
 * @param {Node} node The node to fetch the text content from.
 * @param {number} depth The maximum depth to descend on the DOM.
 * @return {string} The discovered and adapted string.
 */
__gCrWeb.autofill.findChildTextInner = function(node, depth) {
  if (depth <= 0 || !node) {
    return '';
  }

  // Skip over comments.
  if (node.nodeType === Node.COMMENT_NODE) {
    return __gCrWeb.autofill.findChildTextInner(node.nextSibling, depth - 1);
  }

  if (node.nodeType !== Node.ELEMENT_NODE && node.nodeType !== Node.TEXT_NODE) {
    return '';
  }

  // Ignore elements known not to contain inferable labels.
  if (node.nodeType === Node.ELEMENT_NODE) {
    if (node.tagName === 'OPTION' ||
        node.tagName === 'SCRIPT' ||
        node.tagName === 'NOSCRIPT') {
      return '';
    }
    if (__gCrWeb.common.isFormControlElement(node)) {
      var input = /** @type {FormControlElement} */ (node);
      if (__gCrWeb.autofill.isAutofillableElement(input)) {
        return '';
      }
    }
  }

  // Extract the text exactly at this node.
  var nodeText = __gCrWeb.autofill.nodeValue(node);
  if (node.nodeType === Node.TEXT_NODE && !nodeText) {
    // In the C++ version, this text node would have been stripped completely.
    // Just pass the buck.
    return __gCrWeb.autofill.findChildTextInner(node.nextSibling, depth);
  }

  // Recursively compute the children's text.
  // Preserve inter-element whitespace separation.
  var childText =
      __gCrWeb.autofill.findChildTextInner(node.firstChild, depth - 1);
  var addSpace = node.nodeType === Node.TEXT_NODE && !nodeText;
  // Emulate apparently incorrect Chromium behavior tracked in crbug 239819.
  addSpace = false;
  nodeText = __gCrWeb.autofill.combineAndCollapseWhitespace(nodeText,
      childText, addSpace);

  // Recursively compute the siblings' text.
  // Again, preserve inter-element whitespace separation.
  var siblingText =
      __gCrWeb.autofill.findChildTextInner(node.nextSibling, depth - 1);
  addSpace = node.nodeType === Node.TEXT_NODE && !nodeText;
  // Emulate apparently incorrect Chromium behavior tracked in crbug 239819.
  addSpace = false;
  nodeText = __gCrWeb.autofill.combineAndCollapseWhitespace(nodeText,
      siblingText, addSpace);

  return nodeText;
};

/**
 * Returns the aggregated values of the descendants of |element| that are
 * non-empty text nodes.
 *
 * It is based on the logic in
 *    string16 FindChildText(const WebNode& node)
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc,
 * which is a faster alternative to |innerText()| for performance critical
 * operations.
 *
 * @param {Node} node A node of which the child text will be return.
 * @return {string} The child text.
 */
__gCrWeb.autofill.findChildText = function(node) {
  if (node.nodeType === Node.TEXT_NODE)
    return __gCrWeb.autofill.nodeValue(node);

  var child = node.firstChild;
  var kChildSearchDepth = 10;
  var nodeText = __gCrWeb.autofill.findChildTextInner(child, kChildSearchDepth);
  nodeText = nodeText.trim();
  return nodeText;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * a previous sibling of |element|,
 * e.g. Some Text <input ...>
 * or   Some <span>Text</span> <input ...>
 * or   <p>Some Text</p><input ...>
 * or   <label>Some Text</label> <input ...>
 * or   Some Text <img><input ...>
 * or   <b>Some Text</b><br/> <input ...>.
 *
 * It is based on the logic in
 *     string16 InferLabelFromPrevious(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.autofill.inferLabelFromPrevious = function(element) {
  var inferredLabel = '';
  var previous = element;
  if (!previous) {
    return '';
  }

  while (true) {
    previous = previous.previousSibling;
    if (!previous) {
      break;
    }

    // Skip over comments.
    var nodeType = previous.nodeType;
    if (nodeType === Node.COMMENT_NODE) {
      continue;
    }

    // Otherwise, only consider normal HTML elements and their contents.
    if (nodeType != Node.TEXT_NODE && nodeType != Node.ELEMENT_NODE) {
      break;
    }

    // A label might be split across multiple "lightweight" nodes.
    // Coalesce any text contained in multiple consecutive
    //  (a) plain text nodes or
    //  (b) inline HTML elements that are essentially equivalent to text nodes.
    if (nodeType === Node.TEXT_NODE ||
        __gCrWeb.autofill.hasTagName(previous, 'b') ||
        __gCrWeb.autofill.hasTagName(previous, 'strong') ||
        __gCrWeb.autofill.hasTagName(previous, 'span') ||
        __gCrWeb.autofill.hasTagName(previous, 'font')) {
      var value = __gCrWeb.autofill.findChildText(previous);
      // A text node's value will be empty if it is for a line break.
      var addSpace = nodeType === Node.TEXT_NODE && value.length === 0;
      inferredLabel =
          __gCrWeb.autofill.combineAndCollapseWhitespace(
              value, inferredLabel, addSpace);
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    var trimmedLabel = inferredLabel.trim();
    if (trimmedLabel.length > 0) {
      break;
    }

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    if (__gCrWeb.autofill.hasTagName(previous, 'img') ||
        __gCrWeb.autofill.hasTagName(previous, 'br')) {
      continue;
    }

    // We only expect <p> and <label> tags to contain the full label text.
    if (__gCrWeb.autofill.hasTagName(previous, 'p') ||
        __gCrWeb.autofill.hasTagName(previous, 'label')) {
      inferredLabel = __gCrWeb.autofill.findChildText(previous);
    }
    break;
  }
  return inferredLabel.trim();
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * the placeholder attribute.
 *
 * It is based on the logic in
 *     string16 InferLabelFromPlaceholder(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.autofill.inferLabelFromPlaceholder = function(element) {
  if (!element || !element.placeholder) {
    return '';
  }

  return element.placeholder;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * enclosing list item, e.g.
 *     <li>Some Text<input ...><input ...><input ...></li>
 *
 * It is based on the logic in
 *     string16 InferLabelFromListItem(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.autofill.inferLabelFromListItem = function(element) {
  if (!element) {
    return '';
  }

  var parentNode = element.parentNode;
  while (parentNode &&
         parentNode.nodeType === Node.ELEMENT_NODE &&
         !__gCrWeb.autofill.hasTagName(parentNode, 'li')) {
    parentNode = parentNode.parentNode;
  }

  if (parentNode && __gCrWeb.autofill.hasTagName(parentNode, 'li'))
    return __gCrWeb.autofill.findChildText(parentNode);

  return '';
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * surrounding table structure,
 * e.g. <tr><td>Some Text</td><td><input ...></td></tr>
 * or   <tr><th>Some Text</th><td><input ...></td></tr>
 * or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
 * or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
 *
 * It is based on the logic in
 *    string16 InferLabelFromTableColumn(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.autofill.inferLabelFromTableColumn = function(element) {
  if (!element) {
    return '';
  }

  var parentNode = element.parentNode;
  while (parentNode &&
         parentNode.nodeType === Node.ELEMENT_NODE &&
         !__gCrWeb.autofill.hasTagName(parentNode, 'td')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode) {
    return '';
  }

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  var inferredLabel = '';
  var previous = parentNode.previousSibling;
  while (inferredLabel.length === 0 && previous) {
    if (__gCrWeb.autofill.hasTagName(previous, 'td') ||
        __gCrWeb.autofill.hasTagName(previous, 'th')) {
      inferredLabel = __gCrWeb.autofill.findChildText(previous);
    }
    previous = previous.previousSibling;
  }

  return inferredLabel;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * surrounding table structure,
 * e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
 *
 * It is based on the logic in
 *     string16 InferLabelFromTableRow(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.autofill.inferLabelFromTableRow = function(element) {
  if (!element) {
    return '';
  }

  var parentNode = element.parentNode;
  while (parentNode &&
         parentNode.nodeType === Node.ELEMENT_NODE &&
         !__gCrWeb.autofill.hasTagName(parentNode, 'tr')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode) {
    return '';
  }

  var inferredLabel = '';
  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  var previous = parentNode.previousSibling;
  while (inferredLabel.length === 0 && previous) {
    if (__gCrWeb.autofill.hasTagName(previous, 'tr')) {
      inferredLabel = __gCrWeb.autofill.findChildText(previous);
    }
    previous = previous.previousSibling;
  }
  return inferredLabel;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * a surrounding div table,
 * e.g. <div>Some Text<span><input ...></span></div>
 * e.g. <div>Some Text</div><div><input ...></div>
 *
 * It is based on the logic in
 *    string16 InferLabelFromDivTable(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.autofill.inferLabelFromDivTable = function(element) {
  if (!element) {
    return '';
  }

  var node = element.parentNode;
  var lookingForParent = true;

  // Search the sibling and parent <div>s until we find a candidate label.
  var inferredLabel = '';
  while (inferredLabel.length === 0 && node) {
    if (__gCrWeb.autofill.hasTagName(node, 'div')) {
      lookingForParent = false;
      inferredLabel = __gCrWeb.autofill.findChildText(node);
    } else if (lookingForParent &&
        (__gCrWeb.autofill.hasTagName(node, 'table') ||
            __gCrWeb.autofill.hasTagName(node, 'fieldset'))) {
      // If the element is in a table or fieldset, its label most likely is too.
      break;
    }

    if (!node.previousSibling) {
      // If there are no more siblings, continue walking up the tree.
      lookingForParent = true;
    }

    if (lookingForParent) {
      node = node.parentNode;
    } else {
      node = node.previousSibling;
    }
  }

  return inferredLabel;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * a surrounding definition list,
 * e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
 * e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
 *
 * It is based on the logic in
 *    string16 InferLabelFromDefinitionList(
 *        const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.autofill.inferLabelFromDefinitionList = function(element) {
  if (!element) {
    return '';
  }

  var parentNode = element.parentNode;
  while (parentNode &&
         parentNode.nodeType === Node.ELEMENT_NODE &&
         !__gCrWeb.autofill.hasTagName(parentNode, 'dd')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode || !__gCrWeb.autofill.hasTagName(parentNode, 'dd')) {
    return '';
  }

  // Skip by any intervening text nodes.
  var previous = parentNode.previousSibling;
  while (previous && previous.nodeType === Node.TEXT_NODE) {
    previous = previous.previousSibling;
  }

  if (!previous || !__gCrWeb.autofill.hasTagName(previous, 'dt'))
    return '';

  return __gCrWeb.autofill.findChildText(previous);
};

/**
 * Checks if the element's closest ancestor is a TD or DIV.
 *
 * It is based on the logic in
 *    bool ClosestAncestorIsDivAndNotTD(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {boolean} true if the closest ancestor is a <div> and not a <td>.
 *     false if the closest ancestor is a <td> tag, or if there is no <div> or
 *     <td> ancestor.
 */
__gCrWeb.autofill.closestAncestorIsDivAndNotTD = function(element) {
  var parentNode = element.parentNode;
  while (parentNode) {
    if (parentNode.nodeType === Node.ELEMENT_NODE) {
      if (__gCrWeb.autofill.hasTagName(parentNode, 'div')) {
        return true;
      }
      if (__gCrWeb.autofill.hasTagName(parentNode, 'td')) {
        return false;
      }
    }
    parentNode = parentNode.parentNode;
  }
  return false;
}

/**
 * Infers corresponding label for |element| from surrounding context in the DOM,
 * e.g. the contents of the preceding <p> tag or text element.
 *
 * It is based on the logic in
 *    string16 InferLabelForElement(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.autofill.inferLabelForElement = function(element) {
  var inferredLabel = __gCrWeb.autofill.inferLabelFromPrevious(element);
  if (inferredLabel.length > 0) {
    return inferredLabel;
  }

  // If we didn't find a label, check for the placeholder case.
  inferredLabel = __gCrWeb.autofill.inferLabelFromPlaceholder(element);
  if (inferredLabel.length > 0) {
    return inferredLabel;
  }

  // If we didn't find a label, check for list item case.
  inferredLabel = __gCrWeb.autofill.inferLabelFromListItem(element);
  if (inferredLabel.length > 0) {
    return inferredLabel;
  }

  // If we didn't find a label, check for definition list case.
  inferredLabel = __gCrWeb.autofill.inferLabelFromDefinitionList(element);
  if (inferredLabel.length > 0) {
    return inferredLabel;
  }

  var checkDivFirst = __gCrWeb.autofill.closestAncestorIsDivAndNotTD(element);
  if (checkDivFirst) {
    // If we didn't find a label, check for div table case first since it's the
    // closest ancestor.
    inferredLabel = __gCrWeb.autofill.inferLabelFromDivTable(element);
    if (inferredLabel.length > 0) {
      return inferredLabel;
    }
  }

  // If we didn't find a label, check for table cell case.
  inferredLabel = __gCrWeb.autofill.inferLabelFromTableColumn(element);
  if (inferredLabel.length > 0) {
    return inferredLabel;
  }

  // If we didn't find a label, check for table row case.
  inferredLabel = __gCrWeb.autofill.inferLabelFromTableRow(element);
  if (inferredLabel.length > 0) {
    return inferredLabel;
  }

  if (!checkDivFirst) {
    // If we didn't find a label from the table, check for div table case if we
    // haven't already.
    inferredLabel = __gCrWeb.autofill.inferLabelFromDivTable(element);
  }

  return inferredLabel;
};

/**
 * Fills |field| data with the values of the <option> elements present in
 * |selectElement|.
 *
 * It is based on the logic in
 *     void GetOptionStringsFromElement(const WebSelectElement& select_element,
 *                                      std::vector<string16>* option_values,
 *                                      std::vector<string16>* option_contents)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} selectElement A select element from which option data are
 *     extracted.
 * @param {Object} field A field that will contain the extracted option
 *     information.
 */
__gCrWeb.autofill.getOptionStringsFromElement = function(
    selectElement, field) {
  field['option_values'] = [];
  // Protect against custom implementation of Array.toJSON in host pages.
  field['option_values'].toJSON = null;
  field['option_contents'] = [];
  field['option_contents'].toJSON = null;
  var options = selectElement.options;
  for (var i = 0; i < options.length; ++i) {
    var option = options[i];
    field['option_values'].push(option['value']);
    field['option_contents'].push(option['text']);
  }
};

/**
 * Sets the |field|'s value to the value in |data|.
 * Also sets the "autofilled" attribute.
 *
 * It is based on the logic in
 *     void FillFormField(const FormFieldData& data,
 *                        bool is_initiating_node,
 *                        blink::WebFormControlElement* field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * Different from FillFormField(), is_initiating_node is not considered in
 * this implementation.
 *
 * @param {AutofillFormFieldData} data Data that will be filled into field.
 * @param {FormControlElement} field The element to which data will be filled.
 */
__gCrWeb.autofill.fillFormField = function(data, field) {
  // Nothing to fill.
  if (!data['value'] || data['value'].length === 0) {
    return;
  }

  if (__gCrWeb.autofill.isTextInput(field) ||
      __gCrWeb.autofill.isTextAreaElement(field)) {
    var sanitizedValue = data['value'];

    if (__gCrWeb.autofill.isTextInput(field)) {
      // If the 'max_length' attribute contains a negative value, the default
      // maxlength value is used.
      var maxLength = data['max_length'];
      if (maxLength < 0) {
        maxLength = __gCrWeb.autofill.MAX_DATA_LENGTH;
      }
      sanitizedValue = data['value'].substr(0, maxLength);
    }

    __gCrWeb.common.setInputElementValue(sanitizedValue, field, true);
    field.isAutofilled = true;
  } else if (__gCrWeb.autofill.isSelectElement(field)) {
    if (field.value !== data['value']) {
      field.value = data['value'];
      __gCrWeb.common.createAndDispatchHTMLEvent(field, 'change', true, false);
    }
  } else {
    if (__gCrWeb.autofill.isCheckableElement(field)) {
      __gCrWeb.common.setInputElementChecked(data['is_checked'], field, true);
    }
  }
};

/**
 * Returns true if |element| is a text input element.
 *
 * It is based on the logic in
 *     bool IsTextInput(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is a text input field.
 */
__gCrWeb.autofill.isTextInput = function(element) {
  if (!element) {
    return false;
  }
  return __gCrWeb.common.isTextField(element);
};

/**
 * Returns true if |element| is a 'select' element.
 *
 * It is based on the logic in
 *     bool IsSelectElement(const blink::WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is a 'select' element.
 */
__gCrWeb.autofill.isSelectElement = function(element) {
  if (!element) {
    return false;
  }
  return element.type === 'select-one';
};

/**
 * Returns true if |element| is a 'textarea' element.
 *
 * It is based on the logic in
 *     bool IsTextAreaElement(const blink::WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is a 'textarea' element.
 */
__gCrWeb.autofill.isTextAreaElement = function(element) {
  if (!element) {
    return false;
  }
  return element.type === 'textarea';
};

/**
 * Returns true if |element| is a checkbox or a radio button element.
 *
 * It is based on the logic in
 *     bool IsCheckableElement(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is a checkbox or a radio button.
 */
__gCrWeb.autofill.isCheckableElement = function(element) {
  if (!element) {
    return false;
  }
  return element.type === 'checkbox' || element.type === 'radio';
};

/**
 * Returns true if |element| is one of the input element types that can be
 * autofilled. {Text, Radiobutton, Checkbox}.
 *
 * It is based on the logic in
 *    bool IsAutofillableInputElement(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is one of the input element types that
 *     can be autofilled.
 */
__gCrWeb.autofill.isAutofillableInputElement = function(element) {
  return __gCrWeb.autofill.isTextInput(element) ||
         __gCrWeb.autofill.isCheckableElement(element);
};

/**
 * Returns the nodeValue in a way similar to the C++ version of node.nodeValue,
 * used in src/components/autofill/content/renderer/form_autofill_util.h.
 * Newlines and tabs are stripped.
 *
 * @param {Node} node A node to examine.
 * @return {string} The text contained in |element|.
 */
__gCrWeb.autofill.nodeValue = function(node) {
  return (node.nodeValue || '').replace(/[\n\t]/gm, '');
};

/**
 * Returns the value in a way similar to the C++ version of node.value,
 * used in src/components/autofill/content/renderer/form_autofill_util.h.
 * Newlines and tabs are stripped.
 *
 * @param {Element} element An element to examine.
 * @return {string} The value for |element|.
 */
__gCrWeb.autofill.value = function(element) {
  return (element.value || '').replace(/[\n\t]/gm, '');
};

/**
 * Returns the auto-fillable form control elements in |formElement|.
 *
 * It is based on the logic in:
 *     std::vector<blink::WebFormControlElement>
 *     ExtractAutofillableElementsFromSet(
 *         const WebVector<WebFormControlElement>& control_elements);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {Array<FormControlElement>} controlElements Set of control elements.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.autofill.extractAutofillableElementsFromSet =
    function(controlElements) {
  var autofillableElements = [];
  for (var i = 0; i < controlElements.length; ++i) {
    var element = controlElements[i];
    if (!__gCrWeb.autofill.isAutofillableElement(element)) {
      continue;
    }
    autofillableElements.push(element);
  }
  return autofillableElements;
};

/**
 * Returns all the auto-fillable form control elements in |formElement|.
 *
 * It is based on the logic in
 *     void ExtractAutofillableElementsInForm(
 *         const blink::WebFormElement& form_element);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {HTMLFormElement} formElement A form element to be processed.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.autofill.extractAutofillableElementsInForm = function(formElement) {
  var controlElements = __gCrWeb.common.getFormControlElements(formElement);
  return __gCrWeb.autofill.extractAutofillableElementsFromSet(controlElements);
};

/**
 * Fills out a FormField object from a given form control element.
 *
 * It is based on the logic in
 *     void WebFormControlElementToFormField(
 *         const blink::WebFormControlElement& element,
 *         ExtractMask extract_mask,
 *         FormFieldData* field);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element The element to be processed.
 * @param {number} extractMask A bit field mask to extract data from |element|.
 *     See the document on variable __gCrWeb.autofill.EXTRACT_MASK_NONE,
 *     __gCrWeb.autofill.EXTRACT_MASK_VALUE,
 *     __gCrWeb.autofill.EXTRACT_MASK_OPTION_TEXT and
 *     __gCrWeb.autofill.EXTRACT_MASK_OPTIONS.
 * @param {AutofillFormFieldData} field Field to fill in the element
 *     information.
 */
__gCrWeb.autofill.webFormControlElementToFormField = function(
    element, extractMask, field) {
  if (!field || !element) {
    return;
  }
  // The label is not officially part of a form control element; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // form data.
  field['name'] = __gCrWeb['common'].nameForAutofill(element);
  field['form_control_type'] = element.type;
  var attribute = element.getAttribute('autocomplete');
  if (attribute) {
    field['autocomplete_attribute'] = attribute;
  }
  if (field['autocomplete_attribute'] != null &&
      field['autocomplete_attribute'].length >
          __gCrWeb.autofill.MAX_DATA_LENGTH) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process. However, send over a default string to indicate that the
    // attribute was present.
    field['autocomplete_attribute'] = 'x-max-data-length-exceeded';
  }

  if (!__gCrWeb.autofill.isAutofillableElement(element)) {
    return;
  }

  if (__gCrWeb.autofill.isAutofillableInputElement(element) ||
          __gCrWeb.autofill.isTextAreaElement(element) ||
          __gCrWeb.autofill.isSelectElement(element)) {
    field['is_autofilled'] = element.isAutofilled;
    field['should_autocomplete'] = __gCrWeb.common.autoComplete(element);
    field['is_focusable'] = !element.disabled && !element.readOnly &&
        element.tabIndex >= 0 && isVisibleNode_(element);
  }

  if (__gCrWeb.autofill.isAutofillableInputElement(element)) {
    if (__gCrWeb.autofill.isTextInput(element)) {
      field['max_length'] = element.maxLength;
    }
    field['is_checkable'] = __gCrWeb.autofill.isCheckableElement(element);
  } else if (__gCrWeb.autofill.isTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else if (extractMask & __gCrWeb.autofill.EXTRACT_MASK_OPTIONS) {
    __gCrWeb.autofill.getOptionStringsFromElement(element, field);
  }

  if (!extractMask & __gCrWeb.autofill.EXTRACT_MASK_VALUE) {
    return;
  }

  var value = __gCrWeb.autofill.value(element);

  if (!__gCrWeb.autofill.isAutofillableInputElement(element)) {
    // Convert the |select_element| value to text if requested.
    if (extractMask & __gCrWeb.autofill.EXTRACT_MASK_OPTION_TEXT) {
      var options = element.options;
      for (var index = 0; index < options.length; ++index) {
        var optionElement = options[index];
        if (__gCrWeb.autofill.value(optionElement) === value) {
          value = optionElement.text;
          break;
        }
      }
    }
  }

  // There is a constraint on the maximum data length in method
  // WebFormControlElementToFormField() in form_autofill_util.h in order to
  // prevent a malicious site from DOS'ing the browser: http://crbug.com/49332,
  // which isn't really meaningful here, but we need to follow the same logic to
  // get the same form signature wherever possible (to get the benefits of the
  // existing crowdsourced field detection corpus).
  if (value.length > __gCrWeb.autofill.MAX_DATA_LENGTH) {
    value = value.substr(0, __gCrWeb.autofill.MAX_DATA_LENGTH);
  }
  field['value'] = value;
};

/**
 * For debugging purposes, annotate forms on the page with prediction data using
 * the placeholder attribute.
 *
 * @param {Object<AutofillFormData>} data The form and field identifiers with
 *     their prediction data.
 */
__gCrWeb.autofill['fillPredictionData'] = function(data) {
  for (var formName in data) {
    var form = __gCrWeb.common.getFormElementFromIdentifier(formName);
    var formData = data[formName];
    var controlElements = __gCrWeb.common.getFormControlElements(form);
    for (var i = 0; i < controlElements.length; ++i) {
      var element = controlElements[i];
      if (!__gCrWeb.autofill.isAutofillableElement(element)) {
        continue;
      }
      var elementName = __gCrWeb['common'].nameForAutofill(element);
      var value = formData[elementName];
      if (value) {
        element.placeholder = value;
      }
    }
  }
};
