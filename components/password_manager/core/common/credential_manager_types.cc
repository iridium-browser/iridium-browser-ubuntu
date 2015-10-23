// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/credential_manager_types.h"

#include "base/logging.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

CredentialInfo::CredentialInfo() : type(CredentialType::CREDENTIAL_TYPE_EMPTY) {
}

CredentialInfo::CredentialInfo(const autofill::PasswordForm& form,
                               CredentialType form_type)
    : type(form_type),
      id(form.username_value),
      name(form.display_name),
      icon(form.icon_url),
      password(form.password_value),
      federation(form.federation_url) {
  switch (form_type) {
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      password = base::string16();
      federation = GURL();
      break;
    case CredentialType::CREDENTIAL_TYPE_PASSWORD:
      federation = GURL();
      break;
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      password = base::string16();
      break;
  }
}

CredentialInfo::~CredentialInfo() {
}

scoped_ptr<autofill::PasswordForm> CreatePasswordFormFromCredentialInfo(
    const CredentialInfo& info,
    const GURL& origin) {
  scoped_ptr<autofill::PasswordForm> form;
  if (info.type == CredentialType::CREDENTIAL_TYPE_EMPTY)
    return form.Pass();

  form.reset(new autofill::PasswordForm);
  form->icon_url = info.icon;
  form->display_name = info.name;
  form->federation_url = info.federation;
  form->origin = origin;
  form->password_value = info.password;
  form->username_value = info.id;
  form->scheme = autofill::PasswordForm::SCHEME_HTML;

  form->signon_realm =
      info.type == CredentialType::CREDENTIAL_TYPE_PASSWORD
          ? origin.spec()
          : "federation://" + origin.host() + "/" + info.federation.host();
  form->username_value = info.id;
  return form.Pass();
}

}  // namespace password_manager
