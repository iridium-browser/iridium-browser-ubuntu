// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

namespace password_manager_util {

bool AuthenticateUser(gfx::NativeWindow window) {
  return true;
}

void GetOsPasswordStatus(const base::Callback<void(OsPasswordStatus)>& reply) {
  reply.Run(PASSWORD_STATUS_UNSUPPORTED);
}

}  // namespace password_manager_util
