// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store.h"

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_syncable_service.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

// http://crbug.com/404012. Let's see where the empty fields come from.
void CheckForEmptyUsernameAndPassword(const PasswordForm& form) {
  if (form.username_value.empty() &&
      form.password_value.empty() &&
      !form.blacklisted_by_user)
    base::debug::DumpWithoutCrashing();
}

}  // namespace

PasswordStore::GetLoginsRequest::GetLoginsRequest(
    PasswordStoreConsumer* consumer)
    : consumer_weak_(consumer->GetWeakPtr()) {
  origin_loop_ = base::MessageLoopProxy::current();
}

PasswordStore::GetLoginsRequest::~GetLoginsRequest() {
}

void PasswordStore::GetLoginsRequest::NotifyConsumerWithResults(
    ScopedVector<autofill::PasswordForm> results) {
  if (!ignore_logins_cutoff_.is_null()) {
    ScopedVector<autofill::PasswordForm> remaining_logins;
    remaining_logins.reserve(results.size());
    for (auto& login : results) {
      if (login->date_created >= ignore_logins_cutoff_) {
        remaining_logins.push_back(login);
        login = nullptr;
      }
    }
    results = remaining_logins.Pass();
  }

  origin_loop_->PostTask(
      FROM_HERE, base::Bind(&PasswordStoreConsumer::OnGetPasswordStoreResults,
                            consumer_weak_, base::Passed(&results)));
}

PasswordStore::PasswordStore(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner)
    : main_thread_runner_(main_thread_runner),
      db_thread_runner_(db_thread_runner),
      observers_(new ObserverListThreadSafe<Observer>()),
      shutdown_called_(false) {
}

bool PasswordStore::Init(const syncer::SyncableService::StartSyncFlare& flare) {
  ScheduleTask(base::Bind(&PasswordStore::InitSyncableService, this, flare));
  return true;
}

void PasswordStore::SetAffiliatedMatchHelper(
    scoped_ptr<AffiliatedMatchHelper> helper) {
  affiliated_match_helper_ = helper.Pass();
}

bool PasswordStore::HasAffiliatedMatchHelper() const {
  return affiliated_match_helper_;
}

void PasswordStore::AddLogin(const PasswordForm& form) {
  CheckForEmptyUsernameAndPassword(form);
  ScheduleTask(base::Bind(&PasswordStore::AddLoginInternal, this, form));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  CheckForEmptyUsernameAndPassword(form);
  ScheduleTask(base::Bind(&PasswordStore::UpdateLoginInternal, this, form));
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginInternal, this, form));
}

void PasswordStore::RemoveLoginsCreatedBetween(base::Time delete_begin,
                                               base::Time delete_end) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginsCreatedBetweenInternal,
                          this, delete_begin, delete_end));
}

void PasswordStore::RemoveLoginsSyncedBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginsSyncedBetweenInternal,
                          this, delete_begin, delete_end));
}

void PasswordStore::GetLogins(const PasswordForm& form,
                              AuthorizationPromptPolicy prompt_policy,
                              PasswordStoreConsumer* consumer) {
  // Per http://crbug.com/121738, we deliberately ignore saved logins for
  // http*://www.google.com/ that were stored prior to 2012. (Google now uses
  // https://accounts.google.com/ for all login forms, so these should be
  // unused.) We don't delete them just yet, and they'll still be visible in the
  // password manager, but we won't use them to autofill any forms. This is a
  // security feature to help minimize damage that can be done by XSS attacks.
  // TODO(mdm): actually delete them at some point, say M24 or so.
  base::Time ignore_logins_cutoff;  // the null time
  if (form.scheme == PasswordForm::SCHEME_HTML &&
      (form.signon_realm == "http://www.google.com" ||
       form.signon_realm == "http://www.google.com/" ||
       form.signon_realm == "https://www.google.com" ||
       form.signon_realm == "https://www.google.com/")) {
    static const base::Time::Exploded exploded_cutoff =
        { 2012, 1, 0, 1, 0, 0, 0, 0 };  // 00:00 Jan 1 2012
    ignore_logins_cutoff = base::Time::FromUTCExploded(exploded_cutoff);
  }
  scoped_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  request->set_ignore_logins_cutoff(ignore_logins_cutoff);

  if (affiliated_match_helper_) {
    affiliated_match_helper_->GetAffiliatedAndroidRealms(
        form, base::Bind(&PasswordStore::ScheduleGetLoginsWithAffiliations,
                         this, form, prompt_policy, base::Passed(&request)));
  } else {
    ScheduleTask(base::Bind(&PasswordStore::GetLoginsImpl, this, form,
                            prompt_policy, base::Passed(&request)));
  }
}

void PasswordStore::GetAutofillableLogins(PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetAutofillableLoginsImpl, consumer);
}

void PasswordStore::GetBlacklistLogins(PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetBlacklistLoginsImpl, consumer);
}

void PasswordStore::ReportMetrics(const std::string& sync_username,
                                  bool custom_passphrase_sync_enabled) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      GetBackgroundTaskRunner());
  if (task_runner) {
    base::Closure task =
        base::Bind(&PasswordStore::ReportMetricsImpl, this, sync_username,
                   custom_passphrase_sync_enabled);
    task_runner->PostDelayedTask(FROM_HERE, task,
                                 base::TimeDelta::FromSeconds(30));
  }
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

bool PasswordStore::ScheduleTask(const base::Closure& task) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      GetBackgroundTaskRunner());
  if (task_runner.get())
    return task_runner->PostTask(FROM_HERE, task);
  return false;
}

void PasswordStore::Shutdown() {
  ScheduleTask(base::Bind(&PasswordStore::DestroySyncableService, this));
  // The AffiliationService must be destroyed from the main thread.
  affiliated_match_helper_.reset();
  shutdown_called_ = true;
}

base::WeakPtr<syncer::SyncableService>
PasswordStore::GetPasswordSyncableService() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  DCHECK(syncable_service_);
  return syncable_service_->AsWeakPtr();
}

PasswordStore::~PasswordStore() {
  DCHECK(shutdown_called_);
}

scoped_refptr<base::SingleThreadTaskRunner>
PasswordStore::GetBackgroundTaskRunner() {
  return db_thread_runner_;
}

void PasswordStore::GetLoginsImpl(const autofill::PasswordForm& form,
                                  AuthorizationPromptPolicy prompt_policy,
                                  scoped_ptr<GetLoginsRequest> request) {
  request->NotifyConsumerWithResults(FillMatchingLogins(form, prompt_policy));
}


void PasswordStore::LogStatsForBulkDeletion(int num_deletions) {
  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsDeletedByBulkDelete",
                       num_deletions);
}

void PasswordStore::LogStatsForBulkDeletionDuringRollback(int num_deletions) {
  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsDeletedDuringRollback",
                       num_deletions);
}

void PasswordStore::NotifyLoginsChanged(
    const PasswordStoreChangeList& changes) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!changes.empty()) {
    observers_->Notify(FROM_HERE, &Observer::OnLoginsChanged, changes);
    if (syncable_service_)
      syncable_service_->ActOnPasswordStoreChanges(changes);
  }
}

void PasswordStore::Schedule(
    void (PasswordStore::*func)(scoped_ptr<GetLoginsRequest>),
    PasswordStoreConsumer* consumer) {
  scoped_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  consumer->cancelable_task_tracker()->PostTask(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(func, this, base::Passed(&request)));
}

void PasswordStore::WrapModificationTask(ModificationTask task) {
  PasswordStoreChangeList changes = task.Run();
  NotifyLoginsChanged(changes);
}

void PasswordStore::AddLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = AddLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::UpdateLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = UpdateLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::RemoveLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = RemoveLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::RemoveLoginsCreatedBetweenInternal(base::Time delete_begin,
                                                       base::Time delete_end) {
  PasswordStoreChangeList changes =
      RemoveLoginsCreatedBetweenImpl(delete_begin, delete_end);
  NotifyLoginsChanged(changes);
}

void PasswordStore::RemoveLoginsSyncedBetweenInternal(base::Time delete_begin,
                                                      base::Time delete_end) {
  PasswordStoreChangeList changes =
      RemoveLoginsSyncedBetweenImpl(delete_begin, delete_end);
  NotifyLoginsChanged(changes);
}

void PasswordStore::GetLoginsWithAffiliationsImpl(
    const PasswordForm& form,
    AuthorizationPromptPolicy prompt_policy,
    scoped_ptr<GetLoginsRequest> request,
    const std::vector<std::string>& additional_android_realms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  ScopedVector<PasswordForm> results(FillMatchingLogins(form, prompt_policy));
  for (const std::string& realm : additional_android_realms) {
    PasswordForm android_form;
    android_form.scheme = PasswordForm::SCHEME_HTML;
    android_form.signon_realm = realm;
    ScopedVector<PasswordForm> more_results(
        AffiliatedMatchHelper::TransformAffiliatedAndroidCredentials(
            form, FillMatchingLogins(android_form, DISALLOW_PROMPT)));
    results.insert(results.end(), more_results.begin(), more_results.end());
    more_results.weak_clear();
  }
  request->NotifyConsumerWithResults(results.Pass());
}

void PasswordStore::ScheduleGetLoginsWithAffiliations(
    const PasswordForm& form,
    AuthorizationPromptPolicy prompt_policy,
    scoped_ptr<GetLoginsRequest> request,
    const std::vector<std::string>& additional_android_realms) {
  ScheduleTask(base::Bind(&PasswordStore::GetLoginsWithAffiliationsImpl, this,
                          form, prompt_policy, base::Passed(&request),
                          additional_android_realms));
}

void PasswordStore::InitSyncableService(
    const syncer::SyncableService::StartSyncFlare& flare) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  DCHECK(!syncable_service_);
  syncable_service_.reset(new PasswordSyncableService(this));
  syncable_service_->InjectStartSyncFlare(flare);
}

void PasswordStore::DestroySyncableService() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  syncable_service_.reset();
}

}  // namespace password_manager
