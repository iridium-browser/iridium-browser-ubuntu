// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync_driver/data_type_encryption_handler.h"
#include "components/sync_driver/sync_service_observer.h"
#include "sync/internal_api/public/base/model_type.h"

class GoogleServiceAuthError;

namespace sync_driver {

class SyncService : public sync_driver::DataTypeEncryptionHandler {
 public:
  // Used to specify the kind of passphrase with which sync data is encrypted.
  enum PassphraseType {
    IMPLICIT,  // The user did not provide a custom passphrase for encryption.
               // We implicitly use the GAIA password in such cases.
    EXPLICIT,  // The user selected the "use custom passphrase" radio button
               // during sync setup and provided a passphrase.
  };

  ~SyncService() override {}

  // Whether sync is enabled by user or not. This does not necessarily mean
  // that sync is currently running (due to delayed startup, unrecoverable
  // errors, or shutdown). See SyncActive below for checking whether sync
  // is actually running.
  virtual bool HasSyncSetupCompleted() const = 0;

  // Returns true if sync is fully initialized and active. This implies that
  // an initial configuration has successfully completed, although there may
  // be datatype specific, auth, or other transient errors. To see which
  // datetypes are actually syncing, see GetActiveTypes() below.
  // Note that if sync is in backup or rollback mode, SyncActive() will be
  // false.
  virtual bool SyncActive() const = 0;

  // Get the set of current active data types (those chosen or configured by
  // the user which have not also encountered a runtime error).
  // Note that if the Sync engine is in the middle of a configuration, this
  // will the the empty set. Once the configuration completes the set will
  // be updated.
  virtual syncer::ModelTypeSet GetActiveDataTypes() const = 0;

  // Adds/removes an observer. SyncService does not take ownership of the
  // observer.
  virtual void AddObserver(SyncServiceObserver* observer) = 0;
  virtual void RemoveObserver(SyncServiceObserver* observer) = 0;

  // Returns true if |observer| has already been added as an observer.
  virtual bool HasObserver(const SyncServiceObserver* observer) const = 0;

  // ---------------------------------------------------------------------------
  // TODO(sync): The methods below were pulled from ProfileSyncService, and
  // should be evaluated to see if they should stay.

  // Returns true if sync is enabled/not suppressed and the user is logged in.
  // (being logged in does not mean that tokens are available - tokens may
  // be missing because they have not loaded yet, or because they were deleted
  // due to http://crbug.com/121755).
  // Virtual to enable mocking in tests.
  // TODO(tim): Remove this? Nothing in ProfileSyncService uses it, and outside
  // callers use a seemingly arbitrary / redundant / bug prone combination of
  // this method, IsSyncAccessible, and others.
  virtual bool IsSyncEnabledAndLoggedIn() = 0;

  // Disables sync for user. Use ShowLoginDialog to enable.
  virtual void DisableForUser() = 0;

  // Stops the sync backend and sets the flag for suppressing sync startup.
  virtual void StopAndSuppress() = 0;

  // Resets the flag for suppressing sync startup and starts the sync backend.
  virtual void UnsuppressAndStart() = 0;

  // Returns the set of types which are preferred for enabling. This is a
  // superset of the active types (see GetActiveDataTypes()).
  virtual syncer::ModelTypeSet GetPreferredDataTypes() const = 0;

  // Called when a user chooses which data types to sync as part of the sync
  // setup wizard.  |sync_everything| represents whether they chose the
  // "keep everything synced" option; if true, |chosen_types| will be ignored
  // and all data types will be synced.  |sync_everything| means "sync all
  // current and future data types."
  virtual void OnUserChoseDatatypes(bool sync_everything,
                                    syncer::ModelTypeSet chosen_types) = 0;

  // Called whe Sync has been setup by the user and can be started.
  virtual void SetSyncSetupCompleted() = 0;

  // Returns true if initial sync setup is in progress (does not return true
  // if the user is customizing sync after already completing setup once).
  // SyncService uses this to determine if it's OK to start syncing, or if the
  // user is still setting up the initial sync configuration.
  virtual bool FirstSetupInProgress() const = 0;

  // Called by the UI to notify the SyncService that UI is visible so it will
  // not start syncing. This tells sync whether it's safe to start downloading
  // data types yet (we don't start syncing until after sync setup is complete).
  // The UI calls this as soon as any part of the signin wizard is displayed
  // (even just the login UI).
  // If |setup_in_progress| is false, this also kicks the sync engine to ensure
  // that data download starts. In this case, |ReconfigureDatatypeManager| will
  // get triggered.
  virtual void SetSetupInProgress(bool setup_in_progress) = 0;

  // Used by tests.
  virtual bool setup_in_progress() const = 0;

  // Whether the data types active for the current mode have finished
  // configuration.
  virtual bool ConfigurationDone() const = 0;

  virtual const GoogleServiceAuthError& GetAuthError() const = 0;
  virtual bool HasUnrecoverableError() const = 0;

  // Returns true if the SyncBackendHost has told us it's ready to accept
  // changes. This should only be used for sync's internal configuration logic
  // (such as deciding when to prompt for an encryption passphrase).
  virtual bool backend_initialized() const = 0;

  // Returns true if OnPassphraseRequired has been called for decryption and
  // we have an encrypted data type enabled.
  virtual bool IsPassphraseRequiredForDecryption() const = 0;

  // Returns the time the current explicit passphrase (if any), was set.
  // If no secondary passphrase is in use, or no time is available, returns an
  // unset base::Time.
  virtual base::Time GetExplicitPassphraseTime() const = 0;

  // Returns true if a secondary (explicit) passphrase is being used. It is not
  // legal to call this method before the backend is initialized.
  virtual bool IsUsingSecondaryPassphrase() const = 0;

  // Turns on encryption for all data. Callers must call OnUserChoseDatatypes()
  // after calling this to force the encryption to occur.
  virtual void EnableEncryptEverything() = 0;

  // Asynchronously sets the passphrase to |passphrase| for encryption. |type|
  // specifies whether the passphrase is a custom passphrase or the GAIA
  // password being reused as a passphrase.
  // TODO(atwilson): Change this so external callers can only set an EXPLICIT
  // passphrase with this API.
  virtual void SetEncryptionPassphrase(const std::string& passphrase,
                                       PassphraseType type) = 0;

  // Asynchronously decrypts pending keys using |passphrase|. Returns false
  // immediately if the passphrase could not be used to decrypt a locally cached
  // copy of encrypted keys; returns true otherwise.
  virtual bool SetDecryptionPassphrase(const std::string& passphrase)
      WARN_UNUSED_RESULT = 0;

 protected:
  SyncService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncService);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_
