// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/mock_credential_manager_client.h"

#include "third_party/WebKit/public/platform/WebCredential.h"

namespace test_runner {

MockCredentialManagerClient::MockCredentialManagerClient() {
}

MockCredentialManagerClient::~MockCredentialManagerClient() {
}

void MockCredentialManagerClient::SetResponse(
    blink::WebCredential* credential) {
  credential_.reset(credential);
}

void MockCredentialManagerClient::dispatchStore(
    const blink::WebCredential&,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  callbacks->onSuccess();
  delete callbacks;
}

void MockCredentialManagerClient::dispatchRequireUserMediation(
    NotificationCallbacks* callbacks) {
  callbacks->onSuccess();
  delete callbacks;
}

void MockCredentialManagerClient::dispatchGet(
    bool zeroClickOnly,
    const blink::WebVector<blink::WebURL>& federations,
    RequestCallbacks* callbacks) {
  callbacks->onSuccess(credential_.get());
  delete callbacks;
}

}  // namespace test_runner
