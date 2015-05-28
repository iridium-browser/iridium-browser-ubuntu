// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_

#include <libsecret/secret.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"

namespace autofill {
struct PasswordForm;
}

class LibsecretLoader {
 public:
  static decltype(&::secret_password_store_sync) secret_password_store_sync;
  static decltype(&::secret_service_search_sync) secret_service_search_sync;
  static decltype(&::secret_password_clear_sync) secret_password_clear_sync;
  static decltype(&::secret_item_get_secret) secret_item_get_secret;
  static decltype(&::secret_value_get_text) secret_value_get_text;
  static decltype(&::secret_item_get_attributes) secret_item_get_attributes;
  static decltype(&::secret_item_load_secret_sync) secret_item_load_secret_sync;
  static decltype(&::secret_value_unref) secret_value_unref;

 protected:
  static bool LoadLibsecret();
  static bool LibsecretIsAvailable();

  static bool libsecret_loaded;

 private:
  struct FunctionInfo {
    const char* name;
    void** pointer;
  };

  static const FunctionInfo functions[];
};

class NativeBackendLibsecret : public PasswordStoreX::NativeBackend,
                               public LibsecretLoader {
 public:
  explicit NativeBackendLibsecret(LocalProfileId id);

  ~NativeBackendLibsecret() override;

  bool Init() override;

  // Implements NativeBackend interface.
  password_manager::PasswordStoreChangeList AddLogin(
      const autofill::PasswordForm& form) override;
  bool UpdateLogin(const autofill::PasswordForm& form,
                   password_manager::PasswordStoreChangeList* changes) override;
  bool RemoveLogin(const autofill::PasswordForm& form) override;
  bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override;
  bool RemoveLoginsSyncedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override;
  bool GetLogins(const autofill::PasswordForm& form,
                 ScopedVector<autofill::PasswordForm>* forms) override;
  bool GetAutofillableLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;
  bool GetBlacklistLogins(ScopedVector<autofill::PasswordForm>* forms) override;

 private:
  enum TimestampToCompare {
    CREATION_TIMESTAMP,
    SYNC_TIMESTAMP,
  };

  enum AddUpdateLoginSearchOptions {
    SEARCH_USE_SUBMIT,
    SEARCH_IGNORE_SUBMIT,
  };

  // Returns credentials matching |lookup_form| and |options|.
  ScopedVector<autofill::PasswordForm> AddUpdateLoginSearch(
      const autofill::PasswordForm& lookup_form,
      AddUpdateLoginSearchOptions options);

  // Adds a login form without checking for one to replace first.
  bool RawAddLogin(const autofill::PasswordForm& form);

  enum GetLoginsListOptions {
    ALL_LOGINS,
    AUTOFILLABLE_LOGINS,
    BLACKLISTED_LOGINS,
  };

  // Retrieves credentials matching |options| from the keyring into |forms|,
  // overwriting the original contents of |forms|. If |lookup_form| is not NULL,
  // only retrieves credentials PSL-matching it. Returns true on success.
  bool GetLoginsList(const autofill::PasswordForm* lookup_form,
                     GetLoginsListOptions options,
                     ScopedVector<autofill::PasswordForm>* forms)
      WARN_UNUSED_RESULT;

  // Retrieves password created/synced in the time interval into |forms|,
  // overwriting the original contents of |forms|. Returns true on success.
  bool GetLoginsBetween(base::Time get_begin,
                        base::Time get_end,
                        TimestampToCompare date_to_compare,
                        ScopedVector<autofill::PasswordForm>* forms)
      WARN_UNUSED_RESULT;

  // Removes password created/synced in the time interval. Returns |true| if the
  // operation succeeded. |changes| will contain the changes applied.
  bool RemoveLoginsBetween(base::Time get_begin,
                           base::Time get_end,
                           TimestampToCompare date_to_compare,
                           password_manager::PasswordStoreChangeList* changes);

  // convert data get from Libsecret to Passwordform
  ScopedVector<autofill::PasswordForm> ConvertFormList(
      GList* found,
      const autofill::PasswordForm* lookup_form);

  // The app string, possibly based on the local profile id.
  std::string app_string_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendLibsecret);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_
