// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_content_utils.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebFederatedCredential.h"
#include "third_party/WebKit/public/platform/WebLocalCredential.h"

namespace password_manager {

CredentialInfo WebCredentialToCredentialInfo(
    const blink::WebCredential& credential) {
  CredentialInfo credential_info;
  credential_info.id = credential.id();
  credential_info.name = credential.name();
  credential_info.avatar = credential.avatarURL();
  credential_info.type = credential.isLocalCredential()
                             ? CredentialType::CREDENTIAL_TYPE_LOCAL
                             : CredentialType::CREDENTIAL_TYPE_FEDERATED;
  if (credential_info.type == CredentialType::CREDENTIAL_TYPE_LOCAL) {
    DCHECK(credential.isLocalCredential());
    credential_info.password =
        static_cast<const blink::WebLocalCredential&>(credential).password();
  } else {
    DCHECK(credential.isFederatedCredential());
    credential_info.federation =
        static_cast<const blink::WebFederatedCredential&>(credential)
            .federation();
  }
  return credential_info;
}

}  // namespace password_manager
