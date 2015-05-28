// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_

#include <string>

namespace extensions {
namespace core_api {
namespace cast_channel {

class AuthResponse;
class CastMessage;
class DeviceAuthMessage;

struct AuthResult {
 public:
  enum ErrorType {
    ERROR_NONE,
    ERROR_PEER_CERT_EMPTY,
    ERROR_WRONG_PAYLOAD_TYPE,
    ERROR_NO_PAYLOAD,
    ERROR_PAYLOAD_PARSING_FAILED,
    ERROR_MESSAGE_ERROR,
    ERROR_NO_RESPONSE,
    ERROR_FINGERPRINT_NOT_FOUND,
    ERROR_CERT_PARSING_FAILED,
    ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA,
    ERROR_CANNOT_EXTRACT_PUBLIC_KEY,
    ERROR_SIGNED_BLOBS_MISMATCH,
    ERROR_UNEXPECTED_AUTH_LIBRARY_RESULT
  };

  enum PolicyType { POLICY_NONE = 0, POLICY_AUDIO_ONLY = 1 << 0 };

  // Constructs a AuthResult that corresponds to success.
  AuthResult();
  ~AuthResult();

  static AuthResult CreateWithParseError(const std::string& error_message,
                                         ErrorType error_type);
  static AuthResult CreateWithNSSError(const std::string& error_message,
                                       ErrorType error_type,
                                       int nss_error_code);

  bool success() const { return error_type == ERROR_NONE; }

  std::string error_message;
  ErrorType error_type;
  int nss_error_code;
  unsigned int channel_policies;

 private:
  AuthResult(const std::string& error_message,
             ErrorType error_type,
             int nss_error_code);
};

// Authenticates the given |challenge_reply|:
// 1. Signature contained in the reply is valid.
// 2. Certficate used to sign is rooted to a trusted CA.
AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const std::string& peer_cert);

// Auth-library specific implementation of cryptographic signature
// verification routines. Verifies that |response| contains a
// valid signed form of |peer_cert|.
AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& peer_cert);

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_
