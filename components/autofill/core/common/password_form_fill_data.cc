// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_fill_data.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

UsernamesCollectionKey::UsernamesCollectionKey() {}

UsernamesCollectionKey::~UsernamesCollectionKey() {}

bool UsernamesCollectionKey::operator<(
    const UsernamesCollectionKey& other) const {
  if (username != other.username)
    return username < other.username;
  if (password != other.password)
    return password < other.password;
  return realm < other.realm;
}

PasswordFormFillData::PasswordFormFillData()
    : user_submitted(false),
      wait_for_username(false),
      is_possible_change_password_form(false) {
}

PasswordFormFillData::~PasswordFormFillData() {
}

void InitPasswordFormFillData(
    const PasswordForm& form_on_page,
    const PasswordFormMap& matches,
    const PasswordForm* const preferred_match,
    bool wait_for_username_before_autofill,
    bool enable_other_possible_usernames,
    PasswordFormFillData* result) {
  // Note that many of the |FormFieldData| members are not initialized for
  // |username_field| and |password_field| because they are currently not used
  // by the password autocomplete code.
  FormFieldData username_field;
  username_field.name = form_on_page.username_element;
  username_field.value = preferred_match->username_value;
  FormFieldData password_field;
  password_field.name = form_on_page.password_element;
  password_field.value = preferred_match->password_value;
  password_field.form_control_type = "password";

  // Fill basic form data.
  result->name = form_on_page.form_data.name;
  result->origin = form_on_page.origin;
  result->action = form_on_page.action;
  result->user_submitted = form_on_page.form_data.user_submitted;
  result->username_field = username_field;
  result->password_field = password_field;
  result->wait_for_username = wait_for_username_before_autofill;
  result->is_possible_change_password_form =
      form_on_page.IsPossibleChangePasswordForm();

  result->preferred_realm = preferred_match->original_signon_realm;

  // Copy additional username/value pairs.
  PasswordFormMap::const_iterator iter;
  for (iter = matches.begin(); iter != matches.end(); iter++) {
    if (iter->second != preferred_match) {
      PasswordAndRealm value;
      value.password = iter->second->password_value;
      value.realm = iter->second->original_signon_realm;
      result->additional_logins[iter->first] = value;
    }
    if (enable_other_possible_usernames &&
        !iter->second->other_possible_usernames.empty()) {
      // Note that there may be overlap between other_possible_usernames and
      // other saved usernames or with other other_possible_usernames. For now
      // we will ignore this overlap as it should be a rare occurence. We may
      // want to revisit this in the future.
      UsernamesCollectionKey key;
      key.username = iter->first;
      key.password = iter->second->password_value;
      key.realm = iter->second->original_signon_realm;
      result->other_possible_usernames[key] =
          iter->second->other_possible_usernames;
    }
  }
}

}  // namespace autofill
