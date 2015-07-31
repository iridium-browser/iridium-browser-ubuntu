// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_auth_attempt.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/signin/easy_unlock_app_manager.h"
#include "chrome/browser/signin/proximity_auth_facade.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#endif

namespace {

// Decrypts the secret that should be used to login from |wrapped_secret| using
// raw AES key |raw_key|.
// In a case of error, an empty string is returned.
std::string UnwrapSecret(const std::string& wrapped_secret,
                         const std::string& raw_key) {
  if (raw_key.empty())
    return std::string();

  // Import the key structure.
  scoped_ptr<crypto::SymmetricKey> key(
     crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key));

  if (!key)
    return std::string();

  std::string iv(raw_key.size(), ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key.get(), crypto::Encryptor::CBC, iv))
    return std::string();

  std::string secret;
  if (!encryptor.Decrypt(wrapped_secret, &secret))
    return std::string();

  return secret;
}

void DefaultAuthAttemptFinalizedHandler(
    EasyUnlockAuthAttempt::Type auth_attempt_type,
    bool success,
    const std::string& user_id,
    const std::string& key_secret,
    const std::string& key_label) {
  if (!GetScreenlockBridgeInstance()->IsLocked())
    return;

  switch (auth_attempt_type) {
    case EasyUnlockAuthAttempt::TYPE_UNLOCK:
      if (success) {
        GetScreenlockBridgeInstance()->lock_handler()->Unlock(user_id);
      } else {
        GetScreenlockBridgeInstance()->lock_handler()->EnableInput();
      }
      return;
    case EasyUnlockAuthAttempt::TYPE_SIGNIN:
      if (success) {
        GetScreenlockBridgeInstance()->lock_handler()->AttemptEasySignin(
            user_id, key_secret, key_label);
      } else {
        // Attempting signin with an empty secret is equivalent to canceling the
        // attempt.
        GetScreenlockBridgeInstance()->lock_handler()->AttemptEasySignin(
            user_id, std::string(), std::string());
      }
      return;
  }
}

}  // namespace

EasyUnlockAuthAttempt::EasyUnlockAuthAttempt(
    EasyUnlockAppManager* app_manager,
    const std::string& user_id,
    Type type,
    const FinalizedCallback& finalized_callback)
    : app_manager_(app_manager),
      state_(STATE_IDLE),
      user_id_(user_id),
      type_(type),
      finalized_callback_(finalized_callback) {
  if (finalized_callback_.is_null())
    finalized_callback_ = base::Bind(&DefaultAuthAttemptFinalizedHandler);
}

EasyUnlockAuthAttempt::~EasyUnlockAuthAttempt() {
  if (state_ == STATE_RUNNING)
    Cancel(user_id_);
}

bool EasyUnlockAuthAttempt::Start() {
  DCHECK_EQ(STATE_IDLE, state_);

  if (!GetScreenlockBridgeInstance()->IsLocked())
    return false;

  proximity_auth::ScreenlockBridge::LockHandler::AuthType auth_type =
      GetScreenlockBridgeInstance()->lock_handler()->GetAuthType(user_id_);

  if (auth_type != proximity_auth::ScreenlockBridge::LockHandler::USER_CLICK) {
    Cancel(user_id_);
    return false;
  }

  state_ = STATE_RUNNING;

  if (!app_manager_->SendAuthAttemptEvent()) {
    Cancel(user_id_);
    return false;
  }

  return true;
}

void EasyUnlockAuthAttempt::FinalizeUnlock(const std::string& user_id,
                                           bool success) {
  if (state_ != STATE_RUNNING || user_id != user_id_)
    return;

  if (!GetScreenlockBridgeInstance()->IsLocked())
    return;

  if (type_ != TYPE_UNLOCK) {
    Cancel(user_id_);
    return;
  }

  finalized_callback_.Run(type_, success, user_id, std::string(),
                          std::string());
  state_ = STATE_DONE;
}

void EasyUnlockAuthAttempt::FinalizeSignin(const std::string& user_id,
                                           const std::string& wrapped_secret,
                                           const std::string& raw_session_key) {
  if (state_ != STATE_RUNNING || user_id != user_id_)
    return;

  if (!GetScreenlockBridgeInstance()->IsLocked())
    return;

  if (type_ != TYPE_SIGNIN) {
    Cancel(user_id_);
    return;
  }

  if (wrapped_secret.empty()) {
    Cancel(user_id_);
    return;
  }

  std::string unwrapped_secret = UnwrapSecret(wrapped_secret, raw_session_key);

  std::string key_label;
#if defined(OS_CHROMEOS)
  key_label = chromeos::EasyUnlockKeyManager::GetKeyLabel(0u);
#endif  // defined(OS_CHROMEOS)

  const bool kSuccess = true;
  finalized_callback_.Run(type_, kSuccess, user_id, unwrapped_secret,
                          key_label);
  state_ = STATE_DONE;
}

void EasyUnlockAuthAttempt::Cancel(const std::string& user_id) {
  state_ = STATE_DONE;

  const bool kFailure = false;
  finalized_callback_.Run(type_, kFailure, user_id, std::string(),
                          std::string());
}
