// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_FAKE_SECURE_CONTEXT_H_
#define COMPONENTS_PROXIMITY_AUTH_FAKE_SECURE_CONTEXT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/proximity_auth/secure_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace proximity_auth {

class FakeSecureContext : public SecureContext {
 public:
  FakeSecureContext();
  ~FakeSecureContext() override;

  // SecureContext:
  ProtocolVersion GetProtocolVersion() const override;
  std::string GetChannelBindingData() const override;
  void Encode(const std::string& message,
              const MessageCallback& callback) override;
  void Decode(const std::string& encoded_message,
              const MessageCallback& callback) override;

  void set_protocol_version(ProtocolVersion protocol_version) {
    protocol_version_ = protocol_version;
  }

 private:
  ProtocolVersion protocol_version_;

  DISALLOW_COPY_AND_ASSIGN(FakeSecureContext);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_FAKE_SECURE_CONTEXT_H_
