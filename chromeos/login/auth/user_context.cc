// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/user_context.h"
#include "chromeos/login/user_names.h"

namespace chromeos {

UserContext::UserContext()
    : is_using_oauth_(true),
      auth_flow_(AUTH_FLOW_OFFLINE),
      user_type_(user_manager::USER_TYPE_REGULAR) {
}

UserContext::UserContext(const UserContext& other)
    : user_id_(other.user_id_),
      gaia_id_(other.gaia_id_),
      key_(other.key_),
      auth_code_(other.auth_code_),
      refresh_token_(other.refresh_token_),
      access_token_(other.access_token_),
      user_id_hash_(other.user_id_hash_),
      is_using_oauth_(other.is_using_oauth_),
      auth_flow_(other.auth_flow_),
      user_type_(other.user_type_),
      public_session_locale_(other.public_session_locale_),
      public_session_input_method_(other.public_session_input_method_),
      device_id_(other.device_id_),
      gaps_cookie_(other.gaps_cookie_) {
}

UserContext::UserContext(const std::string& user_id)
    : user_id_(login::CanonicalizeUserID(user_id)),
      is_using_oauth_(true),
      auth_flow_(AUTH_FLOW_OFFLINE),
      user_type_(user_manager::USER_TYPE_REGULAR) {
}

UserContext::UserContext(user_manager::UserType user_type,
                         const std::string& user_id)
    : is_using_oauth_(true),
      auth_flow_(AUTH_FLOW_OFFLINE),
      user_type_(user_type) {
  if (user_type_ == user_manager::USER_TYPE_REGULAR)
    user_id_ = login::CanonicalizeUserID(user_id);
  else
    user_id_ = user_id;
}

UserContext::~UserContext() {
}

bool UserContext::operator==(const UserContext& context) const {
  return context.user_id_ == user_id_ && context.gaia_id_ == gaia_id_ &&
         context.key_ == key_ && context.auth_code_ == auth_code_ &&
         context.refresh_token_ == refresh_token_ &&
         context.access_token_ == access_token_ &&
         context.user_id_hash_ == user_id_hash_ &&
         context.is_using_oauth_ == is_using_oauth_ &&
         context.auth_flow_ == auth_flow_ && context.user_type_ == user_type_ &&
         context.public_session_locale_ == public_session_locale_ &&
         context.public_session_input_method_ == public_session_input_method_;
}

bool UserContext::operator!=(const UserContext& context) const {
  return !(*this == context);
}

const std::string& UserContext::GetUserID() const {
  return user_id_;
}

const std::string& UserContext::GetGaiaID() const {
  return gaia_id_;
}

const Key* UserContext::GetKey() const {
  return &key_;
}

Key* UserContext::GetKey() {
  return &key_;
}

const std::string& UserContext::GetAuthCode() const {
  return auth_code_;
}

const std::string& UserContext::GetRefreshToken() const {
  return refresh_token_;
}

const std::string& UserContext::GetAccessToken() const {
  return access_token_;
}

const std::string& UserContext::GetUserIDHash() const {
  return user_id_hash_;
}

bool UserContext::IsUsingOAuth() const {
  return is_using_oauth_;
}

UserContext::AuthFlow UserContext::GetAuthFlow() const {
  return auth_flow_;
}

user_manager::UserType UserContext::GetUserType() const {
  return user_type_;
}

const std::string& UserContext::GetPublicSessionLocale() const {
  return public_session_locale_;
}

const std::string& UserContext::GetPublicSessionInputMethod() const {
  return public_session_input_method_;
}

const std::string& UserContext::GetDeviceId() const {
  return device_id_;
}

const std::string& UserContext::GetGAPSCookie() const {
  return gaps_cookie_;
}

bool UserContext::HasCredentials() const {
  return (!user_id_.empty() && !key_.GetSecret().empty()) ||
         !auth_code_.empty();
}

void UserContext::SetUserID(const std::string& user_id) {
  user_id_ = login::CanonicalizeUserID(user_id);
}

void UserContext::SetGaiaID(const std::string& gaia_id) {
  gaia_id_ = gaia_id;
}

void UserContext::SetKey(const Key& key) {
  key_ = key;
}

void UserContext::SetAuthCode(const std::string& auth_code) {
  auth_code_ = auth_code;
}

void UserContext::SetRefreshToken(const std::string& refresh_token) {
  refresh_token_ = refresh_token;
}

void UserContext::SetAccessToken(const std::string& access_token) {
  access_token_ = access_token;
}

void UserContext::SetUserIDHash(const std::string& user_id_hash) {
  user_id_hash_ = user_id_hash;
}

void UserContext::SetIsUsingOAuth(bool is_using_oauth) {
  is_using_oauth_ = is_using_oauth;
}

void UserContext::SetAuthFlow(AuthFlow auth_flow) {
  auth_flow_ = auth_flow;
}

void UserContext::SetUserType(user_manager::UserType user_type) {
  user_type_ = user_type;
}

void UserContext::SetPublicSessionLocale(const std::string& locale) {
  public_session_locale_ = locale;
}

void UserContext::SetPublicSessionInputMethod(const std::string& input_method) {
  public_session_input_method_ = input_method;
}

void UserContext::SetDeviceId(const std::string& device_id) {
  device_id_ = device_id;
}

void UserContext::SetGAPSCookie(const std::string& gaps_cookie) {
  gaps_cookie_ = gaps_cookie;
}

void UserContext::ClearSecrets() {
  key_.ClearSecret();
  auth_code_.clear();
  refresh_token_.clear();
}

}  // namespace chromeos
