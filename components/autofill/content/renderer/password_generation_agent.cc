// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_generation_agent.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

namespace {

// Returns true if we think that this form is for account creation. |passwords|
// is filled with the password field(s) in the form.
bool GetAccountCreationPasswordFields(
    const blink::WebFormElement& form,
    std::vector<blink::WebInputElement>* passwords) {
  // Grab all of the passwords for the form.
  blink::WebVector<blink::WebFormControlElement> control_elements;
  form.getFormControlElements(control_elements);

  size_t num_input_elements = 0;
  for (size_t i = 0; i < control_elements.size(); i++) {
    blink::WebInputElement* input_element =
        toWebInputElement(&control_elements[i]);
    if (input_element && input_element->isTextField()) {
      num_input_elements++;
      if (input_element->isPasswordField())
        passwords->push_back(*input_element);
    }
  }

  // This may be too lenient, but we assume that any form with at least three
  // input elements where at least one of them is a password is an account
  // creation form.
  if (!passwords->empty() && num_input_elements >= 3) {
    // We trim |passwords| because occasionally there are forms where the
    // security question answers are put in password fields and we don't want
    // to fill those.
    if (passwords->size() > 2)
      passwords->resize(2);

    return true;
  }

  return false;
}

bool ContainsURL(const std::vector<GURL>& urls, const GURL& url) {
  return std::find(urls.begin(), urls.end(), url) != urls.end();
}

bool ContainsForm(const std::vector<autofill::FormData>& forms,
                  const PasswordForm& form) {
  for (std::vector<autofill::FormData>::const_iterator it =
           forms.begin(); it != forms.end(); ++it) {
    if (it->SameFormAs(form.form_data))
      return true;
  }
  return false;
}

void CopyElementValueToOtherInputElements(
    const blink::WebInputElement* element,
    std::vector<blink::WebInputElement>* elements) {
  for (std::vector<blink::WebInputElement>::iterator it = elements->begin();
       it != elements->end(); ++it) {
    if (*element != *it) {
      it->setValue(element->value(), true /* sendEvents */);
    }
  }
}

bool AutocompleteAttributesSetForGeneration(const PasswordForm& form) {
  return form.username_marked_by_site && form.new_password_marked_by_site;
}

}  // namespace

PasswordGenerationAgent::AccountCreationFormData::AccountCreationFormData(
    linked_ptr<PasswordForm> password_form,
    std::vector<blink::WebInputElement> passwords)
    : form(password_form),
      password_elements(passwords) {}

PasswordGenerationAgent::AccountCreationFormData::~AccountCreationFormData() {}

PasswordGenerationAgent::PasswordGenerationAgent(
    content::RenderFrame* render_frame,
    PasswordAutofillAgent* password_agent)
    : content::RenderFrameObserver(render_frame),
      password_is_generated_(false),
      password_edited_(false),
      generation_popup_shown_(false),
      editing_popup_shown_(false),
      enabled_(password_generation::IsPasswordGenerationEnabled()),
      password_agent_(password_agent) {
  VLOG(2) << "Password Generation is " << (enabled_ ? "Enabled" : "Disabled");
}
PasswordGenerationAgent::~PasswordGenerationAgent() {}

void PasswordGenerationAgent::DidFinishDocumentLoad() {
  // Update stats for main frame navigation.
  if (!render_frame()->GetWebFrame()->parent()) {
    // In every navigation, the IPC message sent by the password autofill
    // manager to query whether the current form is blacklisted or not happens
    // when the document load finishes, so we need to clear previous states
    // here before we hear back from the browser. We only clear this state on
    // main frame load as we don't want subframe loads to clear state that we
    // have received from the main frame. Note that we assume there is only one
    // account creation form, but there could be multiple password forms in
    // each frame.
    not_blacklisted_password_form_origins_.clear();
    generation_enabled_forms_.clear();
    generation_element_.reset();
    possible_account_creation_forms_.clear();

    // Log statistics after navigation so that we only log once per page.
    if (generation_form_data_ &&
        generation_form_data_->password_elements.empty()) {
      password_generation::LogPasswordGenerationEvent(
          password_generation::NO_SIGN_UP_DETECTED);
    } else {
      password_generation::LogPasswordGenerationEvent(
          password_generation::SIGN_UP_DETECTED);
    }
    generation_form_data_.reset();
    password_is_generated_ = false;
    if (password_edited_) {
      password_generation::LogPasswordGenerationEvent(
          password_generation::PASSWORD_EDITED);
    }
    password_edited_ = false;

    if (generation_popup_shown_) {
      password_generation::LogPasswordGenerationEvent(
          password_generation::GENERATION_POPUP_SHOWN);
    }
    generation_popup_shown_ = false;

    if (editing_popup_shown_) {
      password_generation::LogPasswordGenerationEvent(
          password_generation::EDITING_POPUP_SHOWN);
    }
    editing_popup_shown_ = false;
  }

  FindPossibleGenerationForm();
}

void PasswordGenerationAgent::OnDynamicFormsSeen() {
  FindPossibleGenerationForm();
}

void PasswordGenerationAgent::FindPossibleGenerationForm() {
  if (!enabled_)
    return;

  // We don't want to generate passwords if the browser won't store or sync
  // them.
  if (!ShouldAnalyzeDocument())
    return;

  // If we have already found a signup form for this page, no need to continue.
  if (generation_form_data_)
    return;

  blink::WebVector<blink::WebFormElement> forms;
  render_frame()->GetWebFrame()->document().forms(forms);
  for (size_t i = 0; i < forms.size(); ++i) {
    if (forms[i].isNull())
      continue;

    // If we can't get a valid PasswordForm, we skip this form because the
    // the password won't get saved even if we generate it.
    scoped_ptr<PasswordForm> password_form(
        CreatePasswordForm(forms[i], nullptr, nullptr));
    if (!password_form.get()) {
      VLOG(2) << "Skipping form as it would not be saved";
      continue;
    }

    // Do not generate password for GAIA since it is used to retrieve the
    // generated paswords.
    GURL realm(password_form->signon_realm);
    if (realm == GaiaUrls::GetInstance()->gaia_login_form_realm())
      continue;

    std::vector<blink::WebInputElement> passwords;
    if (GetAccountCreationPasswordFields(forms[i], &passwords)) {
      AccountCreationFormData ac_form_data(
          make_linked_ptr(password_form.release()), passwords);
      possible_account_creation_forms_.push_back(ac_form_data);
    }
  }

  if (!possible_account_creation_forms_.empty()) {
    VLOG(2) << possible_account_creation_forms_.size()
            << " possible account creation forms deteceted";
    DetermineGenerationElement();
  }
}

bool PasswordGenerationAgent::ShouldAnalyzeDocument() const {
  // Make sure that this security origin is allowed to use password manager.
  // Generating a password that can't be saved is a bad idea.
  blink::WebSecurityOrigin origin =
      render_frame()->GetWebFrame()->document().securityOrigin();
  if (!origin.canAccessPasswordManager()) {
    VLOG(1) << "No PasswordManager access";
    return false;
  }

  return true;
}

bool PasswordGenerationAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordGenerationAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_FormNotBlacklisted,
                        OnFormNotBlacklisted)
    IPC_MESSAGE_HANDLER(AutofillMsg_GeneratedPasswordAccepted,
                        OnPasswordAccepted)
    IPC_MESSAGE_HANDLER(AutofillMsg_AccountCreationFormsDetected,
                        OnAccountCreationFormsDetected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordGenerationAgent::OnFormNotBlacklisted(const PasswordForm& form) {
  not_blacklisted_password_form_origins_.push_back(form.origin);
  DetermineGenerationElement();
}

void PasswordGenerationAgent::OnPasswordAccepted(
    const base::string16& password) {
  password_is_generated_ = true;
  password_generation::LogPasswordGenerationEvent(
      password_generation::PASSWORD_ACCEPTED);
  for (auto& password_element : generation_form_data_->password_elements) {
    password_element.setValue(password, true /* sendEvents */);
    password_element.setAutofilled(true);
    // Needed to notify password_autofill_agent that the content of the field
    // has changed. Without this we will overwrite the generated
    // password with an Autofilled password when saving.
    // https://crbug.com/493455
    password_agent_->UpdateStateForTextChange(password_element);
    // Advance focus to the next input field. We assume password fields in
    // an account creation form are always adjacent.
    render_frame()->GetRenderView()->GetWebView()->advanceFocus(false);
  }
}

void PasswordGenerationAgent::OnAccountCreationFormsDetected(
    const std::vector<autofill::FormData>& forms) {
  generation_enabled_forms_.insert(
      generation_enabled_forms_.end(), forms.begin(), forms.end());
  DetermineGenerationElement();
}

void PasswordGenerationAgent::DetermineGenerationElement() {
  if (generation_form_data_) {
    VLOG(2) << "Account creation form already found";
    return;
  }

  // Make sure local heuristics have identified a possible account creation
  // form.
  if (possible_account_creation_forms_.empty()) {
    VLOG(2) << "Local hueristics have not detected a possible account "
             << "creation form";
    return;
  }

  // Note that no messages will be sent if this feature is disabled
  // (e.g. password saving is disabled).
  for (auto& possible_form_data : possible_account_creation_forms_) {
    PasswordForm* possible_password_form = possible_form_data.form.get();
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kLocalHeuristicsOnlyForPasswordGeneration)) {
      VLOG(2) << "Bypassing additional checks.";
    } else if (!ContainsURL(not_blacklisted_password_form_origins_,
                            possible_password_form->origin)) {
      VLOG(2) << "Have not received confirmation that password form isn't "
               << "blacklisted";
      continue;
    } else if (!ContainsForm(generation_enabled_forms_,
                             *possible_password_form)) {
      if (AutocompleteAttributesSetForGeneration(*possible_password_form)) {
        VLOG(2) << "Ignoring lack of Autofill signal due to Autocomplete "
                << "attributes";
        password_generation::LogPasswordGenerationEvent(
            password_generation::AUTOCOMPLETE_ATTRIBUTES_ENABLED_GENERATION);
      } else {
        VLOG(2) << "Have not received confirmation from Autofill that form is "
                << "used for account creation";
        continue;
      }
    }

    VLOG(2) << "Password generation eligible form found";
    generation_form_data_.reset(
        new AccountCreationFormData(possible_form_data.form,
                                    possible_form_data.password_elements));
    generation_element_ = generation_form_data_->password_elements[0];
    generation_element_.setAttribute("aria-autocomplete", "list");
    password_generation::LogPasswordGenerationEvent(
        password_generation::GENERATION_AVAILABLE);
    possible_account_creation_forms_.clear();
    return;
  }
}

bool PasswordGenerationAgent::FocusedNodeHasChanged(
    const blink::WebNode& node) {
  if (!generation_element_.isNull())
    generation_element_.setShouldRevealPassword(false);

  if (node.isNull() || !node.isElementNode())
    return false;

  const blink::WebElement web_element = node.toConst<blink::WebElement>();
  if (!web_element.document().frame())
    return false;

  const blink::WebInputElement* element = toWebInputElement(&web_element);
  if (!element || *element != generation_element_)
    return false;

  if (password_is_generated_) {
    generation_element_.setShouldRevealPassword(true);
    ShowEditingPopup();
    return true;
  }

  // Assume that if the password field has less than kMaximumOfferSize
  // characters then the user is not finished typing their password and display
  // the password suggestion.
  if (!element->isReadOnly() &&
      element->isEnabled() &&
      element->value().length() <= kMaximumOfferSize) {
    ShowGenerationPopup();
    return true;
  }

  return false;
}

bool PasswordGenerationAgent::TextDidChangeInTextField(
    const blink::WebInputElement& element) {
  if (element != generation_element_)
    return false;

  if (element.value().isEmpty()) {
    if (password_is_generated_) {
      // User generated a password and then deleted it.
      password_generation::LogPasswordGenerationEvent(
          password_generation::PASSWORD_DELETED);
      CopyElementValueToOtherInputElements(&element,
          &generation_form_data_->password_elements);
      Send(new AutofillHostMsg_PasswordNoLongerGenerated(
          routing_id(),
          *generation_form_data_->form));
    }

    // Do not treat the password as generated, either here or in the browser.
    password_is_generated_ = false;
    generation_element_.setShouldRevealPassword(false);

    // Offer generation again.
    ShowGenerationPopup();
  } else if (password_is_generated_) {
    password_edited_ = true;
    // Mirror edits to any confirmation password fields.
    CopyElementValueToOtherInputElements(&element,
        &generation_form_data_->password_elements);
  } else if (element.value().length() > kMaximumOfferSize) {
    // User has rejected the feature and has started typing a password.
    HidePopup();
  } else {
    // Password isn't generated and there are fewer than kMaximumOfferSize
    // characters typed, so keep offering the password. Note this function
    // will just keep the previous popup if one is already showing.
    ShowGenerationPopup();
  }

  return true;
}

void PasswordGenerationAgent::ShowGenerationPopup() {
  gfx::RectF bounding_box_scaled = GetScaledBoundingBox(
      render_frame()->GetRenderView()->GetWebView()->pageScaleFactor(),
      &generation_element_);

  Send(new AutofillHostMsg_ShowPasswordGenerationPopup(
      routing_id(),
      bounding_box_scaled,
      generation_element_.maxLength(),
      *generation_form_data_->form));

  generation_popup_shown_ = true;
}

void PasswordGenerationAgent::ShowEditingPopup() {
  gfx::RectF bounding_box_scaled = GetScaledBoundingBox(
      render_frame()->GetRenderView()->GetWebView()->pageScaleFactor(),
      &generation_element_);

  Send(new AutofillHostMsg_ShowPasswordEditingPopup(
      routing_id(),
      bounding_box_scaled,
      *generation_form_data_->form));

  editing_popup_shown_ = true;
}

void PasswordGenerationAgent::HidePopup() {
  Send(new AutofillHostMsg_HidePasswordGenerationPopup(routing_id()));
}

}  // namespace autofill
