// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

using blink::WebDocument;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {
namespace {

// Layout classification of password forms
// A layout sequence of a form is the sequence of it's non-password and password
// input fields, represented by "N" and "P", respectively. A form like this
// <form>
//   <input type='text' ...>
//   <input type='hidden' ...>
//   <input type='password' ...>
//   <input type='submit' ...>
// </form>
// has the layout sequence "NP" -- "N" for the first field, and "P" for the
// third. The second and fourth fields are ignored, because they are not text
// fields.
//
// The code below classifies the layout (see PasswordForm::Layout) of a form
// based on its layout sequence. This is done by assigning layouts regular
// expressions over the alphabet {N, P}. LAYOUT_OTHER is implicitly the type
// corresponding to all layout sequences not matching any other layout.
//
// LAYOUT_LOGIN_AND_SIGNUP is classified by NPN+P.*. This corresponds to a form
// which starts with a login section (NP) and continues with a sign-up section
// (N+P.*). The aim is to distinguish such forms from change password-forms
// (N*PPP?.*) and forms which use password fields to store private but
// non-password data (could look like, e.g., PN+P.*).
const char kLoginAndSignupRegex[] =
    "NP"   // Login section.
    "N+P"  // Sign-up section.
    ".*";  // Anything beyond that.

const char kAutocompleteUsername[] = "username";
const char kAutocompleteCurrentPassword[] = "current-password";
const char kAutocompleteNewPassword[] = "new-password";

icu::RegexMatcher* CreateMatcher(
    void* instance, const char* pattern) {
  const icu::UnicodeString icu_pattern(pattern);

  UErrorCode status = U_ZERO_ERROR;
  // Use placement new to initialize the instance in the preallocated space.
  // The "(instance)" is very important to force POD type initialization.
  icu::RegexMatcher* matcher = new (instance) icu::RegexMatcher(
      icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  DCHECK(U_SUCCESS(status));
  return matcher;
}

struct LoginAndSignupLazyInstanceTraits
    : public base::DefaultLazyInstanceTraits<icu::RegexMatcher> {
  static icu::RegexMatcher* New(void* instance) {
    return CreateMatcher(instance, kLoginAndSignupRegex);
  }
};

bool Matches(icu::RegexMatcher* matcher, base::StringPiece expression) {
  icu::UnicodeString icu_input(icu::UnicodeString::fromUTF8(
      icu::StringPiece(expression.data(), expression.length())));
  matcher->reset(icu_input);

  UErrorCode status = U_ZERO_ERROR;
  UBool match = matcher->find(0, status);
  DCHECK(U_SUCCESS(status));
  return match == TRUE;
}

base::LazyInstance<icu::RegexMatcher, LoginAndSignupLazyInstanceTraits>
    login_and_signup_matcher = LAZY_INSTANCE_INITIALIZER;

// Given the sequence of non-password and password text input fields of a form,
// represented as a string of Ns (non-password) and Ps (password), computes the
// layout type of that form.
PasswordForm::Layout SequenceToLayout(base::StringPiece layout_sequence) {
  if (Matches(login_and_signup_matcher.Pointer(), layout_sequence))
    return PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP;
  return PasswordForm::Layout::LAYOUT_OTHER;
}

// Checks in a case-insensitive way if the autocomplete attribute for the given
// |element| is present and has the specified |value_in_lowercase|.
bool HasAutocompleteAttributeValue(const WebInputElement& element,
                                   const char* value_in_lowercase) {
  return base::LowerCaseEqualsASCII(
      base::StringPiece16(element.getAttribute("autocomplete")),
      value_in_lowercase);
}

// Helper to determine which password is the main (current) one, and which is
// the new password (e.g., on a sign-up or change password form), if any.
bool LocateSpecificPasswords(std::vector<WebInputElement> passwords,
                             WebInputElement* current_password,
                             WebInputElement* new_password) {
  DCHECK(current_password && current_password->isNull());
  DCHECK(new_password && new_password->isNull());

  // First, look for elements marked with either autocomplete='current-password'
  // or 'new-password' -- if we find any, take the hint, and treat the first of
  // each kind as the element we are looking for.
  for (std::vector<WebInputElement>::const_iterator it = passwords.begin();
       it != passwords.end(); it++) {
    if (HasAutocompleteAttributeValue(*it, kAutocompleteCurrentPassword) &&
        current_password->isNull()) {
      *current_password = *it;
    } else if (HasAutocompleteAttributeValue(*it, kAutocompleteNewPassword) &&
        new_password->isNull()) {
      *new_password = *it;
    }
  }

  // If we have seen an element with either of autocomplete attributes above,
  // take that as a signal that the page author must have intentionally left the
  // rest of the password fields unmarked. Perhaps they are used for other
  // purposes, e.g., PINs, OTPs, and the like. So we skip all the heuristics we
  // normally do, and ignore the rest of the password fields.
  if (!current_password->isNull() || !new_password->isNull())
    return true;

  if (passwords.empty())
    return false;

  switch (passwords.size()) {
    case 1:
      // Single password, easy.
      *current_password = passwords[0];
      break;
    case 2:
      if (!passwords[0].value().isEmpty() &&
          passwords[0].value() == passwords[1].value()) {
        // Two identical non-empty passwords: assume we are seeing a new
        // password with a confirmation. This can be either a sign-up form or a
        // password change form that does not ask for the old password.
        *new_password = passwords[0];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        // This case also includes empty passwords in order to allow filling of
        // password change forms (that also could autofill for sign up form, but
        // we can't do anything with this using only client side information).
        *current_password = passwords[0];
        *new_password = passwords[1];
      }
      break;
    default:
      if (!passwords[0].value().isEmpty() &&
          passwords[0].value() == passwords[1].value() &&
          passwords[0].value() == passwords[2].value()) {
        // All three passwords are the same and non-empty? This does not make
        // any sense, give up.
        return false;
      } else if (passwords[1].value() == passwords[2].value()) {
        // New password is the duplicated one, and comes second; or empty form
        // with 3 password fields, in which case we will assume this layout.
        *current_password = passwords[0];
        *new_password = passwords[1];
      } else if (passwords[0].value() == passwords[1].value()) {
        // It is strange that the new password comes first, but trust more which
        // fields are duplicated than the ordering of fields. Assume that
        // any password fields after the new password contain sensitive
        // information that isn't actually a password (security hint, SSN, etc.)
        *new_password = passwords[0];
      } else {
        // Three different passwords, or first and last match with middle
        // different. No idea which is which, so no luck.
        return false;
      }
  }
  return true;
}

// Checks the |form_predictions| map to see if there is a key associated with
// the |prediction_type| value. Assigns the key to |prediction_field| and
// returns true if it is found.
bool MapContainsPrediction(
    const std::map<WebInputElement, PasswordFormFieldPredictionType>&
        form_predictions,
    PasswordFormFieldPredictionType prediction_type,
    WebInputElement* prediction_field) {
  for (auto it = form_predictions.begin(); it != form_predictions.end(); ++it) {
    if (it->second == prediction_type) {
      (*prediction_field) = it->first;
      return true;
    }
  }
  return false;
}

void FindPredictedElements(
    const WebFormElement& form,
    const std::map<FormData, PasswordFormFieldPredictionMap>& form_predictions,
    WebVector<WebFormControlElement>* control_elements,
    std::map<WebInputElement, PasswordFormFieldPredictionType>*
        predicted_elements) {
  FormData form_data;
  if (!WebFormElementToFormData(form, WebFormControlElement(), EXTRACT_NONE,
                                &form_data, nullptr)) {
    return;
  }

  // Matching only requires that action and name of the form match to allow
  // the username to be updated even if the form is changed after page load.
  // See https://crbug.com/476092 for more details.
  auto predictions_iterator = form_predictions.begin();
  for (;predictions_iterator != form_predictions.end();
       ++predictions_iterator) {
    if (predictions_iterator->first.action == form_data.action &&
        predictions_iterator->first.name == form_data.name) {
      break;
    }
  }

  if (predictions_iterator == form_predictions.end())
    return;

  std::vector<blink::WebFormControlElement> autofillable_elements =
      ExtractAutofillableElementsFromSet(*control_elements);

  const PasswordFormFieldPredictionMap& field_predictions =
      predictions_iterator->second;
  for (PasswordFormFieldPredictionMap::const_iterator prediction =
           field_predictions.begin();
       prediction != field_predictions.end(); ++prediction) {
    const FormFieldData& target_field = prediction->first;
    const PasswordFormFieldPredictionType& type = prediction->second;

    for (size_t i = 0; i < autofillable_elements.size(); ++i) {
      if (autofillable_elements[i].nameForAutofill() == target_field.name) {
        WebInputElement* input_element =
            toWebInputElement(&autofillable_elements[i]);
        // TODO(sebsg): Investigate why this guard is necessary, see
        // https://crbug.com/517490 for more details.
        if (input_element) {
          (*predicted_elements)[*input_element] = type;
        }
        break;
      }
    }
  }
}

// TODO(msramek): Move the reauthentication recognition code to the browser.
const char kPasswordSiteUrlRegex[] =
    "passwords(?:-[a-z-]+\\.corp)?\\.google\\.com";

struct PasswordSiteUrlLazyInstanceTraits
    : public base::DefaultLazyInstanceTraits<icu::RegexMatcher> {
  static icu::RegexMatcher* New(void* instance) {
    return CreateMatcher(instance, kPasswordSiteUrlRegex);
  }
};

base::LazyInstance<icu::RegexMatcher, PasswordSiteUrlLazyInstanceTraits>
    password_site_matcher = LAZY_INSTANCE_INITIALIZER;

// Get information about a login form encapsulated in a PasswordForm struct.
// If an element of |form| has an entry in |nonscript_modified_values|, the
// associated string is used instead of the element's value to create
// the PasswordForm.
void GetPasswordForm(
    const WebFormElement& form,
    PasswordForm* password_form,
    const std::map<const blink::WebInputElement, blink::WebString>*
        nonscript_modified_values,
    const std::map<FormData, PasswordFormFieldPredictionMap>*
        form_predictions) {
  WebInputElement latest_input_element;
  WebInputElement username_element;
  password_form->username_marked_by_site = false;
  std::vector<WebInputElement> passwords;
  std::vector<base::string16> other_possible_usernames;

  WebVector<WebFormControlElement> control_elements;
  form.getFormControlElements(control_elements);

  // Bail if this is a GAIA passwords site reauthentication form, so that
  // the form will be ignored.
  // TODO(msramek): Move this logic to the browser, and disable filling only
  // for the sync credential and if passwords are being synced.
  if (IsGaiaReauthenticationForm(
          GURL(form.document().url()).GetOrigin(), control_elements)) {
    return;
  }

  std::map<WebInputElement, PasswordFormFieldPredictionType> predicted_elements;
  if (form_predictions) {
    FindPredictedElements(form, *form_predictions, &control_elements,
                          &predicted_elements);
  }

  std::string layout_sequence;
  layout_sequence.reserve(control_elements.size());
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement control_element = control_elements[i];
    if (control_element.isActivatedSubmit())
      password_form->submit_element = control_element.formControlName();

    WebInputElement* input_element = toWebInputElement(&control_element);
    if (!input_element || !input_element->isEnabled())
      continue;

    if (input_element->isTextField()) {
      if (input_element->isPasswordField())
        layout_sequence.push_back('P');
      else
        layout_sequence.push_back('N');
    }

    bool password_marked_by_autocomplete_attribute =
        HasAutocompleteAttributeValue(*input_element,
                                      kAutocompleteCurrentPassword) ||
        HasAutocompleteAttributeValue(*input_element, kAutocompleteNewPassword);

    // If the password field is readonly, the page is likely using a virtual
    // keyboard and bypassing the password field value (see
    // http://crbug.com/475488). There is nothing Chrome can do to fill
    // passwords for now. Continue processing in case when the password field
    // was made readonly by JavaScript before submission. We can do this by
    // checking whether password element was updated not from JavaScript.
    if (input_element->isPasswordField() &&
        (!input_element->isReadOnly() ||
         (nonscript_modified_values &&
          nonscript_modified_values->find(*input_element) !=
              nonscript_modified_values->end()) ||
         password_marked_by_autocomplete_attribute)) {
      // We add the field to the list of password fields if it was not flagged
      // as a special NOT_PASSWORD prediction by Autofill. The NOT_PASSWORD
      // mechanism exists because some webpages use the type "password" for
      // fields which Autofill knows shouldn't be treated as passwords by the
      // Password Manager. This is ultimately bypassed if the field has
      // autocomplete attributes.
      auto possible_password_element_iterator =
          predicted_elements.find(*input_element);
      if (password_marked_by_autocomplete_attribute ||
          possible_password_element_iterator == predicted_elements.end() ||
          possible_password_element_iterator->second !=
              PREDICTION_NOT_PASSWORD) {
        passwords.push_back(*input_element);
      }
      // If we have not yet considered any element to be the username so far,
      // provisionally select the input element just before the first password
      // element to be the username. This choice will be overruled if we later
      // find an element with autocomplete='username'.
      if (username_element.isNull() && !latest_input_element.isNull()) {
        username_element = latest_input_element;
        // Remove the selected username from other_possible_usernames.
        if (!latest_input_element.value().isEmpty()) {
          DCHECK(!other_possible_usernames.empty());
          DCHECK_EQ(base::string16(latest_input_element.value()),
                    other_possible_usernames.back());
          other_possible_usernames.pop_back();
        }
      }
    }

    // Various input types such as text, url, email can be a username field.
    if (input_element->isTextField() && !input_element->isPasswordField()) {
      if (HasAutocompleteAttributeValue(*input_element,
                                        kAutocompleteUsername)) {
        if (password_form->username_marked_by_site) {
          // A second or subsequent element marked with autocomplete='username'.
          // This makes us less confident that we have understood the form. We
          // will stick to our choice that the first such element was the real
          // username, but will start collecting other_possible_usernames from
          // the extra elements marked with autocomplete='username'. Note that
          // unlike username_element, other_possible_usernames is used only for
          // autofill, not for form identification, and blank autofill entries
          // are not useful, so we do not collect empty strings.
          if (!input_element->value().isEmpty())
            other_possible_usernames.push_back(input_element->value());
        } else {
          // The first element marked with autocomplete='username'. Take the
          // hint and treat it as the username (overruling the tentative choice
          // we might have made before). Furthermore, drop all other possible
          // usernames we have accrued so far: they come from fields not marked
          // with the autocomplete attribute, making them unlikely alternatives.
          username_element = *input_element;
          password_form->username_marked_by_site = true;
          other_possible_usernames.clear();
        }
      } else {
        if (password_form->username_marked_by_site) {
          // Having seen elements with autocomplete='username', elements without
          // this attribute are no longer interesting. No-op.
        } else {
          // No elements marked with autocomplete='username' so far whatsoever.
          // If we have not yet selected a username element even provisionally,
          // then remember this element for the case when the next field turns
          // out to be a password. Save a non-empty username as a possible
          // alternative, at least for now.
          if (username_element.isNull())
            latest_input_element = *input_element;
          if (!input_element->value().isEmpty())
            other_possible_usernames.push_back(input_element->value());
        }
      }
    }
  }
  password_form->layout = SequenceToLayout(layout_sequence);

  WebInputElement predicted_username_element;
  bool map_has_username_prediction = MapContainsPrediction(
      predicted_elements, PREDICTION_USERNAME, &predicted_username_element);

  // Let server predictions override the selection of the username field. This
  // allows instant adjusting without changing Chromium code.
  auto username_element_iterator = predicted_elements.find(username_element);
  if (map_has_username_prediction &&
      (username_element_iterator == predicted_elements.end() ||
       username_element_iterator->second != PREDICTION_USERNAME)) {
    auto it =
        find(other_possible_usernames.begin(), other_possible_usernames.end(),
             predicted_username_element.value());
    if (it != other_possible_usernames.end())
      other_possible_usernames.erase(it);
    if (!username_element.isNull()) {
      other_possible_usernames.push_back(username_element.value());
    }
    username_element = predicted_username_element;
    password_form->was_parsed_using_autofill_predictions = true;
  }

  if (!username_element.isNull()) {
    password_form->username_element = username_element.nameForAutofill();
    base::string16 username_value = username_element.value();
    if (nonscript_modified_values != nullptr) {
      auto username_iterator =
        nonscript_modified_values->find(username_element);
      if (username_iterator != nonscript_modified_values->end()) {
        base::string16 typed_username_value = username_iterator->second;
        if (!base::StartsWith(
                base::i18n::ToLower(username_value),
                base::i18n::ToLower(typed_username_value),
                base::CompareCase::SENSITIVE)) {
          // We check that |username_value| was not obtained by autofilling
          // |typed_username_value|. In case when it was, |typed_username_value|
          // is incomplete, so we should leave autofilled value.
          username_value = typed_username_value;
        }
      }
    }
    password_form->username_value = username_value;
  }

  WebInputElement password;
  WebInputElement new_password;
  if (!LocateSpecificPasswords(passwords, &password, &new_password))
    return;

  password_form->action = GetCanonicalActionForForm(form);
  if (!password_form->action.is_valid())
    return;

  password_form->origin = GetCanonicalOriginForDocument(form.document());
  GURL::Replacements rep;
  rep.SetPathStr("");
  password_form->signon_realm =
      password_form->origin.ReplaceComponents(rep).spec();
  password_form->other_possible_usernames.swap(other_possible_usernames);

  if (!password.isNull()) {
    password_form->password_element = password.nameForAutofill();
    blink::WebString password_value = password.value();
    if (nonscript_modified_values != nullptr) {
      auto password_iterator = nonscript_modified_values->find(password);
      if (password_iterator != nonscript_modified_values->end())
        password_value = password_iterator->second;
    }
    password_form->password_value = password_value;
  }
  if (!new_password.isNull()) {
    password_form->new_password_element = new_password.nameForAutofill();
    password_form->new_password_value = new_password.value();
    if (HasAutocompleteAttributeValue(new_password, kAutocompleteNewPassword))
      password_form->new_password_marked_by_site = true;
  }

  if (username_element.isNull()) {
    // To get a better idea on how password forms without a username field
    // look like, report the total number of text and password fields.
    UMA_HISTOGRAM_COUNTS_100(
        "PasswordManager.EmptyUsernames.TextAndPasswordFieldCount",
        layout_sequence.size());
    // For comparison, also report the number of password fields.
    UMA_HISTOGRAM_COUNTS_100(
        "PasswordManager.EmptyUsernames.PasswordFieldCount",
        std::count(layout_sequence.begin(), layout_sequence.end(), 'P'));
  }

  password_form->scheme = PasswordForm::SCHEME_HTML;
  password_form->ssl_valid = false;
  password_form->preferred = false;
  password_form->blacklisted_by_user = false;
  password_form->type = PasswordForm::TYPE_MANUAL;
}

GURL StripAuthAndParams(const GURL& gurl) {
  // We want to keep the path but strip any authentication data, as well as
  // query and ref portions of URL, for the form action and form origin.
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

}  // namespace

GURL GetCanonicalActionForForm(const WebFormElement& form) {
  WebString action = form.action();
  if (action.isNull())
    action = WebString(""); // missing 'action' attribute implies current URL
  GURL full_action(form.document().completeURL(action));
  return StripAuthAndParams(full_action);
}

GURL GetCanonicalOriginForDocument(const WebDocument& document) {
  GURL full_origin(document.url());
  return StripAuthAndParams(full_origin);
}

bool IsGaiaReauthenticationForm(
    const GURL& origin,
    const WebVector<blink::WebFormControlElement>& control_elements) {
  if (origin != GaiaUrls::GetInstance()->gaia_url().GetOrigin())
    return false;

  bool has_rart_field = false;
  bool has_continue_field = false;

  for (const blink::WebFormControlElement& element : control_elements) {
    // We're only interested in the presence
    // of <input type="hidden" /> elements.
    CR_DEFINE_STATIC_LOCAL(WebString, kHidden, ("hidden"));
    const blink::WebInputElement* input = blink::toWebInputElement(&element);
    if (!input || input->formControlType() != kHidden)
      continue;

    // There must be a hidden input named "rart".
    if (input->formControlName() == "rart")
      has_rart_field = true;

    // There must be a hidden input named "continue", whose value points
    // to a password (or password testing) site.
    if (input->formControlName() == "continue" &&
        Matches(password_site_matcher.Pointer(), input->value().utf8())) {
      has_continue_field = true;
    }
  }

  return has_rart_field && has_continue_field;
}

scoped_ptr<PasswordForm> CreatePasswordForm(
    const WebFormElement& web_form,
    const std::map<const blink::WebInputElement, blink::WebString>*
        nonscript_modified_values,
    const std::map<FormData, PasswordFormFieldPredictionMap>*
        form_predictions) {
  if (web_form.isNull())
    return scoped_ptr<PasswordForm>();

  scoped_ptr<PasswordForm> password_form(new PasswordForm());
  GetPasswordForm(web_form, password_form.get(), nonscript_modified_values,
                  form_predictions);

  if (!password_form->action.is_valid())
    return scoped_ptr<PasswordForm>();

  WebFormElementToFormData(web_form,
                           blink::WebFormControlElement(),
                           EXTRACT_NONE,
                           &password_form->form_data,
                           NULL /* FormFieldData */);

  return password_form.Pass();
}

}  // namespace autofill
