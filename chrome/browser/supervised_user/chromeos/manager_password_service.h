// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

class UserContext;

// Handles supervised user password change that is detected while manager is
// signed in.
// It uses manager's master key to authorize update of supervised user's key.
// Edge case: Pre-M35 supervised users don't have correct labels for keys.
// After new supervised user key is added, migration is done in following way:
// 1) Master key is added with correct label
// 2) Old supervised user's key is deleted.
// 3) Old master key is deleted.
class ManagerPasswordService
    : public KeyedService,
      public chromeos::ExtendedAuthenticator::NewAuthStatusConsumer {
 public:
  ManagerPasswordService();
  ~ManagerPasswordService() override;

  void Shutdown() override;

  void Init(const std::string& user_id,
            SupervisedUserSyncService* user_service,
            SupervisedUserSharedSettingsService* service);

  // chromeos::ExtendedAuthenticator::AuthStatusConsumer overrides:
  void OnAuthenticationFailure(ExtendedAuthenticator::AuthState state) override;

 private:
  void OnSharedSettingsChange(const std::string& su_id, const std::string& key);
  void GetSupervisedUsersCallback(
      const std::string& sync_su_id,
      const std::string& user_id,
      std::unique_ptr<base::DictionaryValue> password_data,
      const base::DictionaryValue* supervised_users);
  void OnAddKeySuccess(const UserContext& master_key_context,
                       const std::string& user_id,
                       std::unique_ptr<base::DictionaryValue> password_data);
  void OnKeyTransformedIfNeeded(const UserContext& master_key_context);
  void OnNewManagerKeySuccess(const UserContext& master_key_context);
  void OnOldSupervisedUserKeyDeleted(const UserContext& master_key_context);
  void OnOldManagerKeyDeleted(const UserContext& master_key_context);

  // Cached value from Init().
  // User id of currently logged in user, that have supervised users on device.
  std::string user_id_;
  SupervisedUserSyncService* user_service_;
  SupervisedUserSharedSettingsService* settings_service_;

  std::unique_ptr<
      SupervisedUserSharedSettingsService::ChangeCallbackList::Subscription>
      settings_service_subscription_;

  scoped_refptr<ExtendedAuthenticator> authenticator_;

  base::WeakPtrFactory<ManagerPasswordService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagerPasswordService);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_
