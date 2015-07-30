// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list_threadsafe.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "sync/api/syncable_service.h"

namespace autofill {
struct PasswordForm;
}

namespace syncer {
class SyncableService;
}

namespace password_manager {

class AffiliatedMatchHelper;
class PasswordStoreConsumer;
class PasswordSyncableService;

// Interface for storing form passwords in a platform-specific secure way.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
// Implementations, however, should carry out most tasks asynchronously on a
// background thread: the base class provides functionality to facilitate this.
// I/O heavy initialization should also be performed asynchronously in this
// manner. If this deferred initialization fails, all subsequent method calls
// should fail without side effects, return no data, and send no notifications.
// PasswordStoreSync is a hidden base class because only PasswordSyncableService
// needs to access these methods.
class PasswordStore : protected PasswordStoreSync,
                      public base::RefCountedThreadSafe<PasswordStore> {
 public:
  // Whether or not it's acceptable for Chrome to request access to locked
  // passwords, which requires prompting the user for permission.
  enum AuthorizationPromptPolicy { ALLOW_PROMPT, DISALLOW_PROMPT };

  // An interface used to notify clients (observers) of this object that data in
  // the password store has changed. Register the observer via
  // PasswordStore::AddObserver.
  class Observer {
   public:
    // Notifies the observer that password data changed. Will be called from
    // the UI thread.
    virtual void OnLoginsChanged(const PasswordStoreChangeList& changes) = 0;

   protected:
    virtual ~Observer() {}
  };

  PasswordStore(scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
                scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner);

  // Reimplement this to add custom initialization. Always call this too.
  virtual bool Init(const syncer::SyncableService::StartSyncFlare& flare);

  // Sets the affiliation-based match |helper| that will be used by subsequent
  // GetLogins() calls to return credentials stored not only for the requested
  // sign-on realm, but also for affiliated Android applications. If |helper| is
  // null, clears the the currently set helper if any. Unless a helper is set,
  // affiliation-based matching is disabled. The passed |helper| must already be
  // initialized if it is non-null.
  void SetAffiliatedMatchHelper(scoped_ptr<AffiliatedMatchHelper> helper);

  // Toggles whether or not to propagate password changes in Android credentials
  // to the affiliated Web credentials.
  void enable_propagating_password_changes_to_web_credentials(bool enabled) {
    is_propagating_password_changes_to_web_credentials_enabled_ = enabled;
  }

  // Returns whether or not an affiliation-based match helper is set.
  bool HasAffiliatedMatchHelper() const;

  // Adds the given PasswordForm to the secure password store asynchronously.
  virtual void AddLogin(const autofill::PasswordForm& form);

  // Updates the matching PasswordForm in the secure password store (async).
  virtual void UpdateLogin(const autofill::PasswordForm& form);

  // Removes the matching PasswordForm from the secure password store (async).
  virtual void RemoveLogin(const autofill::PasswordForm& form);

  // Removes all logins created in the given date range.
  virtual void RemoveLoginsCreatedBetween(base::Time delete_begin,
                                          base::Time delete_end);

  // Removes all logins synced in the given date range.
  virtual void RemoveLoginsSyncedBetween(base::Time delete_begin,
                                         base::Time delete_end);

  // Removes cached affiliation data that is no longer needed; provided that
  // affiliation-based matching is enabled.
  void TrimAffiliationCache();

  // Searches for a matching PasswordForm, and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  // |prompt_policy| indicates whether it's permissible to prompt the user to
  // authorize access to locked passwords. This argument is only used on
  // platforms that support prompting the user for access (such as Mac OS).
  // NOTE: This means that this method can return different results depending
  // on the value of |prompt_policy|.
  virtual void GetLogins(const autofill::PasswordForm& form,
                         AuthorizationPromptPolicy prompt_policy,
                         PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are not blacklist entries--and
  // are thus auto-fillable. |consumer| will be notified on completion.
  // The request will be cancelled if the consumer is destroyed.
  virtual void GetAutofillableLogins(PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are blacklist entries,
  // and notify |consumer| on completion. The request will be cancelled if the
  // consumer is destroyed.
  virtual void GetBlacklistLogins(PasswordStoreConsumer* consumer);

  // Reports usage metrics for the database. |sync_username| and
  // |custom_passphrase_sync_enabled| determine some of the UMA stats that
  // may be reported.
  virtual void ReportMetrics(const std::string& sync_username,
                             bool custom_passphrase_sync_enabled);

  // Adds or replaces the statistics for the domain |stats.origin_domain|.
  void AddSiteStats(const InteractionsStats& stats);

  // Removes the statistics for |origin_domain|.
  void RemoveSiteStats(const GURL& origin_domain);

  // Retrieves the statistics for |origin_domain| and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  void GetSiteStats(const GURL& origin_domain, PasswordStoreConsumer* consumer);

  // Adds an observer to be notified when the password store data changes.
  void AddObserver(Observer* observer);

  // Removes |observer| from the observer list.
  void RemoveObserver(Observer* observer);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  bool ScheduleTask(const base::Closure& task);

  // Before you destruct the store, call Shutdown to indicate that the store
  // needs to shut itself down.
  virtual void Shutdown();

  base::WeakPtr<syncer::SyncableService> GetPasswordSyncableService();

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;

  typedef base::Callback<PasswordStoreChangeList(void)> ModificationTask;

  // Represents a single GetLogins() request. Implements functionality to filter
  // results and send them to the consumer on the consumer's message loop.
  class GetLoginsRequest {
   public:
    explicit GetLoginsRequest(PasswordStoreConsumer* consumer);
    ~GetLoginsRequest();

    // Removes any credentials in |results| that were saved before the cutoff,
    // then notifies the consumer with the remaining results.
    // Note that if this method is not called before destruction, the consumer
    // will not be notified.
    void NotifyConsumerWithResults(
        ScopedVector<autofill::PasswordForm> results);

    void NotifyWithSiteStatistics(scoped_ptr<InteractionsStats> stats);

    void set_ignore_logins_cutoff(base::Time cutoff) {
      ignore_logins_cutoff_ = cutoff;
    }

   private:
    // See GetLogins(). Logins older than this will be removed from the reply.
    base::Time ignore_logins_cutoff_;

    scoped_refptr<base::MessageLoopProxy> origin_loop_;
    base::WeakPtr<PasswordStoreConsumer> consumer_weak_;

    DISALLOW_COPY_AND_ASSIGN(GetLoginsRequest);
  };

  ~PasswordStore() override;

  // Get the TaskRunner to use for PasswordStore background tasks.
  // By default, a SingleThreadTaskRunner on the DB thread is used, but
  // subclasses can override.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetBackgroundTaskRunner();

  // Methods below will be run in PasswordStore's own thread.
  // Synchronous implementation that reports usage metrics.
  virtual void ReportMetricsImpl(const std::string& sync_username,
                                 bool custom_passphrase_sync_enabled) = 0;

  // Synchronous implementation to remove the given logins.
  virtual PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to remove the given logins.
  virtual PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Finds all PasswordForms with a signon_realm that is equal to, or is a
  // PSL-match to that of |form|, and takes care of notifying the consumer with
  // the results when done.
  // Note: subclasses should implement FillMatchingLogins() instead. This needs
  // to be virtual only because asynchronous behavior in PasswordStoreWin.
  // TODO(engedy): Make this non-virtual once https://crbug.com/78830 is fixed.
  virtual void GetLoginsImpl(const autofill::PasswordForm& form,
                             AuthorizationPromptPolicy prompt_policy,
                             scoped_ptr<GetLoginsRequest> request);

  // Synchronous implementation provided by subclasses to add the given login.
  virtual PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) = 0;

  // Synchronous implementation provided by subclasses to update the given
  // login.
  virtual PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) = 0;

  // Synchronous implementation provided by subclasses to remove the given
  // login.
  virtual PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) = 0;

  // Finds and returns all PasswordForms with the same signon_realm as |form|,
  // or with a signon_realm that is a PSL-match to that of |form|.
  virtual ScopedVector<autofill::PasswordForm> FillMatchingLogins(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy) = 0;

  // Finds all non-blacklist PasswordForms, and notifies the consumer.
  virtual void GetAutofillableLoginsImpl(
      scoped_ptr<GetLoginsRequest> request) = 0;

  // Finds all blacklist PasswordForms, and notifies the consumer.
  virtual void GetBlacklistLoginsImpl(scoped_ptr<GetLoginsRequest> request) = 0;

  // Synchronous implementation for manipulating with statistics.
  virtual void AddSiteStatsImpl(const InteractionsStats& stats) = 0;
  virtual void RemoveSiteStatsImpl(const GURL& origin_domain) = 0;
  // Returns a raw pointer so that InteractionsStats can be forward declared.
  virtual scoped_ptr<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) WARN_UNUSED_RESULT = 0;

  // Log UMA stats for number of bulk deletions.
  void LogStatsForBulkDeletion(int num_deletions);

  // Log UMA stats for password deletions happening on clear browsing data
  // since first sync during rollback.
  void LogStatsForBulkDeletionDuringRollback(int num_deletions);

  // PasswordStoreSync:
  PasswordStoreChangeList AddLoginSync(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList UpdateLoginSync(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList RemoveLoginSync(
      const autofill::PasswordForm& form) override;

  // Called by WrapModificationTask() once the underlying data-modifying
  // operation has been performed. Notifies observers that password store data
  // may have been changed.
  void NotifyLoginsChanged(const PasswordStoreChangeList& changes) override;

  // TaskRunner for tasks that run on the main thread (usually the UI thread).
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  // TaskRunner for the DB thread. By default, this is the task runner used for
  // background tasks -- see |GetBackgroundTaskRunner|.
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner_;

 private:
  FRIEND_TEST_ALL_PREFIXES(PasswordStoreTest, GetLoginImpl);
  FRIEND_TEST_ALL_PREFIXES(PasswordStoreTest,
                           UpdatePasswordsStoredForAffiliatedWebsites);

  // Schedule the given |func| to be run in the PasswordStore's own thread with
  // responses delivered to |consumer| on the current thread.
  void Schedule(void (PasswordStore::*func)(scoped_ptr<GetLoginsRequest>),
                PasswordStoreConsumer* consumer);

  // Wrapper method called on the destination thread (DB for non-mac) that
  // invokes |task| and then calls back into the source thread to notify
  // observers that the password store may have been modified via
  // NotifyLoginsChanged(). Note that there is no guarantee that the called
  // method will actually modify the password store data.
  void WrapModificationTask(ModificationTask task);

  // Temporary specializations of WrapModificationTask for a better stack trace.
  void AddLoginInternal(const autofill::PasswordForm& form);
  void UpdateLoginInternal(const autofill::PasswordForm& form);
  void RemoveLoginInternal(const autofill::PasswordForm& form);
  void RemoveLoginsCreatedBetweenInternal(base::Time delete_begin,
                                          base::Time delete_end);
  void RemoveLoginsSyncedBetweenInternal(base::Time delete_begin,
                                         base::Time delete_end);

  // Notifies |request| about the stats for |origin_domain|.
  void NotifySiteStats(const GURL& origin_domain,
                       scoped_ptr<GetLoginsRequest> request);

  // Extended version of GetLoginsImpl that also returns credentials stored for
  // the specified affiliated Android applications. That is, it finds all
  // PasswordForms with a signon_realm that is either:
  //  * equal to that of |form|,
  //  * is a PSL-match to the realm of |form|,
  //  * is one of those in |additional_android_realms|,
  // and takes care of notifying the consumer with the results when done.
  void GetLoginsWithAffiliationsImpl(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy,
      scoped_ptr<GetLoginsRequest> request,
      const std::vector<std::string>& additional_android_realms);

  // Schedules GetLoginsWithAffiliationsImpl() to be run on the DB thread.
  void ScheduleGetLoginsWithAffiliations(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy,
      scoped_ptr<GetLoginsRequest> request,
      const std::vector<std::string>& additional_android_realms);

  // Retrieves the currently stored form, if any, with the same primary key as
  // |form|, that is, with the same signon_realm, origin, username_element,
  // username_value and password_element attributes. To be called on the
  // background thread.
  scoped_ptr<autofill::PasswordForm> GetLoginImpl(
      const autofill::PasswordForm& primary_key);

  // Called when a password is added or updated for an Android application, and
  // triggers finding web sites affiliated with the Android application and
  // propagating the new password to credentials for those web sites, if any.
  // Called on the main thread.
  void FindAndUpdateAffiliatedWebLogins(
      const autofill::PasswordForm& added_or_updated_android_form);

  // Posts FindAndUpdateAffiliatedWebLogins() to the main thread. Should be
  // called from the background thread.
  void ScheduleFindAndUpdateAffiliatedWebLogins(
      const autofill::PasswordForm& added_or_updated_android_form);

  // Called when a password is added or updated for an Android application, and
  // propagates these changes to credentials stored for |affiliated_web_realms|
  // under the same username, if there are any. Called on the background thread.
  void UpdateAffiliatedWebLoginsImpl(
      const autofill::PasswordForm& updated_android_form,
      const std::vector<std::string>& affiliated_web_realms);

  // Schedules UpdateAffiliatedWebLoginsImpl() to run on the background thread.
  // Should be called from the main thread.
  void ScheduleUpdateAffiliatedWebLoginsImpl(
      const autofill::PasswordForm& updated_android_form,
      const std::vector<std::string>& affiliated_web_realms);

  // Creates PasswordSyncableService instance on the background thread.
  void InitSyncableService(
      const syncer::SyncableService::StartSyncFlare& flare);

  // Deletes PasswordSyncableService instance on the background thread.
  void DestroySyncableService();

  // The observers.
  scoped_refptr<ObserverListThreadSafe<Observer>> observers_;

  scoped_ptr<PasswordSyncableService> syncable_service_;
  scoped_ptr<AffiliatedMatchHelper> affiliated_match_helper_;
  bool is_propagating_password_changes_to_web_credentials_enabled_;

  bool shutdown_called_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStore);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
