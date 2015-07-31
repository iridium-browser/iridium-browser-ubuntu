// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "url/gurl.h"

namespace autofill {

// The PasswordForm struct encapsulates information about a login form,
// which can be an HTML form or a dialog with username/password text fields.
//
// The Web Data database stores saved username/passwords and associated form
// metdata using a PasswordForm struct, typically one that was created from
// a parsed HTMLFormElement or LoginDialog, but the saved entries could have
// also been created by imported data from another browser.
//
// The PasswordManager implements a fuzzy-matching algorithm to compare saved
// PasswordForm entries against PasswordForms that were created from a parsed
// HTML or dialog form. As one might expect, the more data contained in one
// of the saved PasswordForms, the better the job the PasswordManager can do
// in matching it against the actual form it was saved on, and autofill
// accurately. But it is not always possible, especially when importing from
// other browsers with different data models, to copy over all the information
// about a particular "saved password entry" to our PasswordForm
// representation.
//
// The field descriptions in the struct specification below are intended to
// describe which fields are not strictly required when adding a saved password
// entry to the database and how they can affect the matching process.

struct PasswordForm {
  // Enum to keep track of what information has been sent to the server about
  // this form regarding password generation.
  enum GenerationUploadStatus {
    NO_SIGNAL_SENT,
    POSITIVE_SIGNAL_SENT,
    NEGATIVE_SIGNAL_SENT,
    // Reserve a few values for future use.
    UNKNOWN_STATUS = 10
  };

  // Enum to differentiate between HTML form based authentication, and dialogs
  // using basic or digest schemes. Default is SCHEME_HTML. Only PasswordForms
  // of the same Scheme will be matched/autofilled against each other.
  enum Scheme {
    SCHEME_HTML,
    SCHEME_BASIC,
    SCHEME_DIGEST,
    SCHEME_OTHER,
    SCHEME_LAST = SCHEME_OTHER
  } scheme;

  // During form parsing, Chrome tries to partly understand the type of the form
  // based on the layout of its fields. The result of this analysis helps to
  // treat the form correctly once the low-level information is lost by
  // converting the web form into a PasswordForm. It is only used for observed
  // HTML forms, not for stored credentials.
  enum class Layout {
    // Forms which either do not need to be classified, or cannot be classified
    // meaningfully.
    LAYOUT_OTHER,
    // Login and signup forms combined in one <form>, to distinguish them from,
    // e.g., change-password forms.
    LAYOUT_LOGIN_AND_SIGNUP,
    LAYOUT_LAST = LAYOUT_LOGIN_AND_SIGNUP
  };

  // The "Realm" for the sign-on. This is scheme, host, port for SCHEME_HTML.
  // Dialog based forms also contain the HTTP realm. Android based forms will
  // contain a string of the form "android://<hash of cert>@<package name>"
  //
  // The signon_realm is effectively the primary key used for retrieving
  // data from the database, so it must not be empty.
  std::string signon_realm;

  // The original "Realm" for the sign-on (scheme, host, port for SCHEME_HTML,
  // and contains the HTTP realm for dialog-based forms). This realm is only set
  // when two PasswordForms are matched when trying to find a login/pass pair
  // for a site. It is only set to a non-empty value during a match of the
  // original stored login/pass and the current observed form if all these
  // statements are true:
  // 1) The full signon_realm is not the same.
  // 2) The registry controlled domain is the same. For example; example.com,
  // m.example.com, foo.login.example.com and www.example.com would all resolve
  // to example.com since .com is the public suffix.
  // 3) The scheme is the same.
  // 4) The port is the same.
  // For example, if there exists a stored password for http://www.example.com
  // (where .com is the public suffix) and the observed form is
  // http://m.example.com, |original_signon_realm| must be set to
  // http://www.example.com.
  std::string original_signon_realm;

  // An origin URL consists of the scheme, host, port and path; the rest is
  // stripped. This is the primary data used by the PasswordManager to decide
  // (in longest matching prefix fashion) whether or not a given PasswordForm
  // result from the database is a good fit for a particular form on a page.
  // This should not be empty except for Android based credentials.
  // TODO(melandory): origin should be renamed in order to be consistent with
  // GURL definition of origin.
  GURL origin;

  // The action target of the form; like |origin| URL consists of the scheme,
  // host, port and path; the rest is stripped. This is the primary data used by
  // the PasswordManager for form autofill; that is, the action of the saved
  // credentials must match the action of the form on the page to be autofilled.
  // If this is empty / not available, it will result in a "restricted" IE-like
  // autofill policy, where we wait for the user to type in his username before
  // autofilling the password. In these cases, after successful login the action
  // URL will automatically be assigned by the PasswordManager.
  //
  // When parsing an HTML form, this must always be set.
  GURL action;

  // The name of the submit button used. Optional; only used in scoring
  // of PasswordForm results from the database to make matches as tight as
  // possible.
  //
  // When parsing an HTML form, this must always be set.
  base::string16 submit_element;

  // The name of the username input element. Optional (improves scoring).
  //
  // When parsing an HTML form, this must always be set.
  base::string16 username_element;

  // Whether the |username_element| has an autocomplete=username attribute. This
  // is only used in parsed HTML forms.
  bool username_marked_by_site;

  // The username. Optional.
  //
  // When parsing an HTML form, this is typically empty unless the site
  // has implemented some form of autofill.
  base::string16 username_value;

  // This member is populated in cases where we there are multiple input
  // elements that could possibly be the username. Used when our heuristics for
  // determining the username are incorrect. Optional.
  //
  // When parsing an HTML form, this is typically empty.
  std::vector<base::string16> other_possible_usernames;

  // The name of the input element corresponding to the current password.
  // Optional (improves scoring).
  //
  // When parsing an HTML form, this will always be set, unless it is a sign-up
  // form or a change password form that does not ask for the current password.
  // In these two cases the |new_password_element| will always be set.
  base::string16 password_element;

  // The current password. Must be non-empty for PasswordForm instances that are
  // meant to be persisted to the password store.
  //
  // When parsing an HTML form, this is typically empty.
  base::string16 password_value;

  // False if autocomplete is set to "off" for the password input element;
  // True otherwise.
  bool password_autocomplete_set;

  // If the form was a sign-up or a change password form, the name of the input
  // element corresponding to the new password. Optional, and not persisted.
  base::string16 new_password_element;

  // The new password. Optional, and not persisted.
  base::string16 new_password_value;

  // Whether the |new_password_element| has an autocomplete=new-password
  // attribute. This is only used in parsed HTML forms.
  bool new_password_marked_by_site;

  // Whether or not this login was saved under an HTTPS session with a valid
  // SSL cert. We will never match or autofill a PasswordForm where
  // ssl_valid == true with a PasswordForm where ssl_valid == false. This means
  // passwords saved under HTTPS will never get autofilled onto an HTTP page.
  // When importing, this should be set to true if the page URL is HTTPS, thus
  // giving it "the benefit of the doubt" that the SSL cert was valid when it
  // was saved. Default to false.
  bool ssl_valid;

  // True if this PasswordForm represents the last username/password login the
  // user selected to log in to the site. If there is only one saved entry for
  // the site, this will always be true, but when there are multiple entries
  // the PasswordManager ensures that only one of them has a preferred bit set
  // to true. Default to false.
  //
  // When parsing an HTML form, this is not used.
  bool preferred;

  // When the login was saved (by chrome).
  //
  // When parsing an HTML form, this is not used.
  base::Time date_created;

  // When the login was downloaded from the sync server. For local passwords is
  // not used.
  //
  // When parsing an HTML form, this is not used.
  base::Time date_synced;

  // Tracks if the user opted to never remember passwords for this form. Default
  // to false.
  //
  // When parsing an HTML form, this is not used.
  bool blacklisted_by_user;

  // Enum to differentiate between manually filled forms and forms with auto
  // generated passwords.
  enum Type {
    TYPE_MANUAL,
    TYPE_GENERATED,
    TYPE_LAST = TYPE_GENERATED
  };

  // The form type.
  Type type;

  // The number of times that this username/password has been used to
  // authenticate the user.
  //
  // When parsing an HTML form, this is not used.
  int times_used;

  // Autofill representation of this form. Used to communicate with the
  // Autofill servers if necessary. Currently this is only used to help
  // determine forms where we can trigger password generation.
  //
  // When parsing an HTML form, this is normally set.
  FormData form_data;

  // What information has been sent to the Autofill server about this form.
  GenerationUploadStatus generation_upload_status;

  // These following fields are set by a website using the Credential Manager
  // API. They will be empty and remain unused for sites which do not use that
  // API.
  //
  // User friendly name to show in the UI.
  base::string16 display_name;

  // The URL of the user's avatar to display in the UI.
  GURL avatar_url;

  // The URL of identity provider used for federated login.
  GURL federation_url;

  // If true, Chrome will not return this credential to a site in response to
  // 'navigator.credentials.request()' without user interaction.
  // Once user selects this credential the flag is reseted.
  bool skip_zero_click;

  // The layout as determined during parsing. Default value is LAYOUT_OTHER.
  Layout layout;

  // If true, this form was parsed using Autofill predictions.
  bool was_parsed_using_autofill_predictions;

  // TODO(vabr): Remove |is_alive| once http://crbug.com/486931 is fixed.
  bool is_alive;  // Set on construction, reset on destruction.

  // Returns true if this match was found using public suffix matching.
  bool IsPublicSuffixMatch() const;

  // Return true if we consider this form to be a change password form.
  // We use only client heuristics, so it could include signup forms.
  bool IsPossibleChangePasswordForm() const;

  // Equality operators for testing.
  bool operator==(const PasswordForm& form) const;
  bool operator!=(const PasswordForm& form) const;

  PasswordForm();
  ~PasswordForm();
};

// Map username to PasswordForm* for convenience. See password_form_manager.h.
typedef std::map<base::string16, PasswordForm*> PasswordFormMap;

typedef std::map<base::string16, const PasswordForm*> ConstPasswordFormMap;

// For testing.
std::ostream& operator<<(std::ostream& os, PasswordForm::Layout layout);
std::ostream& operator<<(std::ostream& os, const autofill::PasswordForm& form);
std::ostream& operator<<(std::ostream& os, autofill::PasswordForm* form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__
