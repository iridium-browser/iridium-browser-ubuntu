// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The AllStatus object watches various sync engine components and aggregates
// the status of all of them into one place.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_ALL_STATUS_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_ALL_STATUS_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/engine_impl/nudge_source.h"
#include "components/sync/engine_impl/sync_engine_event_listener.h"
#include "components/sync/engine_impl/syncer_types.h"

namespace syncer {

class ScopedStatusLock;
struct ServerConnectionEvent;
struct SyncCycleEvent;

// This class collects data and uses it to update its internal state.  It can
// return a snapshot of this state as a SyncerStatus object.
//
// Most of this data ends up on the about:sync page.  But the page is only
// 'pinged' to update itself at the end of a sync cycle.  A user could refresh
// manually, but unless their timing is excellent it's unlikely that a user will
// see any state in mid-sync cycle.  We have no plans to change this.  However,
// we will continue to collect data and update state mid-sync-cycle in case we
// need to debug slow or stuck sync cycles.
class AllStatus : public SyncEngineEventListener {
 public:
  AllStatus();
  ~AllStatus() override;

  // SyncEngineEventListener implementation.
  void OnSyncCycleEvent(const SyncCycleEvent& event) override;
  void OnActionableError(const SyncProtocolError& error) override;
  void OnRetryTimeChanged(base::Time retry_time) override;
  void OnThrottledTypesChanged(ModelTypeSet throttled_types) override;
  void OnMigrationRequested(ModelTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;

  SyncStatus status() const;

  void SetNotificationsEnabled(bool notifications_enabled);

  void IncrementNotifiableCommits();

  void IncrementNotificationsReceived();

  void SetEncryptedTypes(ModelTypeSet types);
  void SetCryptographerReady(bool ready);
  void SetCryptoHasPendingKeys(bool has_pending_keys);
  void SetPassphraseType(PassphraseType type);
  void SetHasKeystoreKey(bool has_keystore_key);
  void SetKeystoreMigrationTime(const base::Time& migration_time);

  void SetSyncId(const std::string& sync_id);
  void SetInvalidatorClientId(const std::string& invalidator_client_id);

  void IncrementNudgeCounter(NudgeSource source);

 protected:
  // Examines syncer to calculate syncing and the unsynced count,
  // and returns a Status with new values.
  SyncStatus CalcSyncing(const SyncCycleEvent& event) const;
  SyncStatus CreateBlankStatus() const;

  SyncStatus status_;

  mutable base::Lock mutex_;  // Protects all data members.

 private:
  friend class ScopedStatusLock;

  DISALLOW_COPY_AND_ASSIGN(AllStatus);
};

class ScopedStatusLock {
 public:
  explicit ScopedStatusLock(AllStatus* allstatus);
  ~ScopedStatusLock();

 protected:
  AllStatus* allstatus_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_ALL_STATUS_H_
