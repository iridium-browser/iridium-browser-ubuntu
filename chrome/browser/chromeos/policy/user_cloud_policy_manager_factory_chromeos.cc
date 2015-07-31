// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_external_data_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {

// Subdirectory in the user's profile for storing legacy user policies.
const base::FilePath::CharType kDeviceManagementDir[] =
    FILE_PATH_LITERAL("Device Management");

// File in the above directory for storing legacy user policy dmtokens.
const base::FilePath::CharType kToken[] = FILE_PATH_LITERAL("Token");

// This constant is used to build two different paths. It can be a file inside
// kDeviceManagementDir where legacy user policy data is stored, and it can be
// a directory inside the profile directory where other resources are stored.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");

// Directory under kPolicy, in the user's profile dir, where policy for
// components is cached.
const base::FilePath::CharType kComponentsDir[] =
    FILE_PATH_LITERAL("Components");

// Directory in which to store external policy data. This is specified relative
// to kPolicy.
const base::FilePath::CharType kPolicyExternalDataDir[] =
    FILE_PATH_LITERAL("External Data");

// Timeout in seconds after which to abandon the initial policy fetch and start
// the session regardless.
const int kInitialPolicyFetchTimeoutSeconds = 10;

}  // namespace

// static
UserCloudPolicyManagerFactoryChromeOS*
    UserCloudPolicyManagerFactoryChromeOS::GetInstance() {
  return Singleton<UserCloudPolicyManagerFactoryChromeOS>::get();
}

// static
UserCloudPolicyManagerChromeOS*
    UserCloudPolicyManagerFactoryChromeOS::GetForProfile(
        Profile* profile) {
  return GetInstance()->GetManagerForProfile(profile);
}

// static
scoped_ptr<UserCloudPolicyManagerChromeOS>
    UserCloudPolicyManagerFactoryChromeOS::CreateForProfile(
        Profile* profile,
        bool force_immediate_load,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  return GetInstance()->CreateManagerForProfile(
      profile, force_immediate_load, background_task_runner);
}

UserCloudPolicyManagerFactoryChromeOS::UserCloudPolicyManagerFactoryChromeOS()
    : BrowserContextKeyedBaseFactory(
        "UserCloudPolicyManagerChromeOS",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SchemaRegistryServiceFactory::GetInstance());
}

UserCloudPolicyManagerFactoryChromeOS::
    ~UserCloudPolicyManagerFactoryChromeOS() {}

UserCloudPolicyManagerChromeOS*
    UserCloudPolicyManagerFactoryChromeOS::GetManagerForProfile(
        Profile* profile) {
  // Get the manager for the original profile, since the PolicyService is
  // also shared between the incognito Profile and the original Profile.
  ManagerMap::const_iterator it = managers_.find(profile->GetOriginalProfile());
  return it != managers_.end() ? it->second : NULL;
}

scoped_ptr<UserCloudPolicyManagerChromeOS>
    UserCloudPolicyManagerFactoryChromeOS::CreateManagerForProfile(
        Profile* profile,
        bool force_immediate_load,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // Don't initialize cloud policy for the signin profile.
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return scoped_ptr<UserCloudPolicyManagerChromeOS>();

  // |user| should never be NULL except for the signin profile. This object is
  // created as part of the Profile creation, which happens right after
  // sign-in. The just-signed-in User is the active user during that time.
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  CHECK(user);

  // User policy exists for enterprise accounts only:
  // - For regular enterprise users (those who have a GAIA account), a
  //   |UserCloudPolicyManagerChromeOS| is created here.
  // - For device-local accounts, policy is provided by
  //   |DeviceLocalAccountPolicyService|.
  // All other user types do not have user policy.
  const std::string& username = user->email();
  if (!user->HasGaiaAccount() ||
      user->IsSupervised() ||
      BrowserPolicyConnector::IsNonEnterpriseUser(username)) {
    return scoped_ptr<UserCloudPolicyManagerChromeOS>();
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  UserAffiliation affiliation = connector->GetUserAffiliation(username);
  const bool is_affiliated_user = affiliation == USER_AFFILIATION_MANAGED;
  const bool is_browser_restart =
      command_line->HasSwitch(chromeos::switches::kLoginUser);
  const bool wait_for_initial_policy =
      !is_browser_restart &&
      (user_manager::UserManager::Get()->IsCurrentUserNew() ||
       is_affiliated_user);

  const base::TimeDelta initial_policy_fetch_timeout =
      user_manager::UserManager::Get()->IsCurrentUserNew()
          ? base::TimeDelta::Max()
          : base::TimeDelta::FromSeconds(kInitialPolicyFetchTimeoutSeconds);

  DeviceManagementService* device_management_service =
      connector->device_management_service();
  if (wait_for_initial_policy)
    device_management_service->ScheduleInitialization(0);

  base::FilePath profile_dir = profile->GetPath();
  const base::FilePath legacy_dir = profile_dir.Append(kDeviceManagementDir);
  const base::FilePath policy_cache_file = legacy_dir.Append(kPolicy);
  const base::FilePath token_cache_file = legacy_dir.Append(kToken);
  const base::FilePath component_policy_cache_dir =
      profile_dir.Append(kPolicy).Append(kComponentsDir);
  const base::FilePath external_data_dir =
        profile_dir.Append(kPolicy).Append(kPolicyExternalDataDir);
  base::FilePath policy_key_dir;
  CHECK(PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &policy_key_dir));

  scoped_ptr<UserCloudPolicyStoreChromeOS> store(
      new UserCloudPolicyStoreChromeOS(
          chromeos::DBusThreadManager::Get()->GetCryptohomeClient(),
          chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
          background_task_runner,
          username, policy_key_dir, token_cache_file, policy_cache_file));

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner =
      content::BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken());
  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO);
  scoped_ptr<CloudExternalDataManager> external_data_manager(
      new UserCloudExternalDataManager(base::Bind(&GetChromePolicyDetails),
                                       backend_task_runner,
                                       io_task_runner,
                                       external_data_dir,
                                       store.get()));
  if (force_immediate_load)
    store->LoadImmediately();

  scoped_refptr<base::SequencedTaskRunner> file_task_runner =
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE);

  scoped_ptr<UserCloudPolicyManagerChromeOS> manager(
      new UserCloudPolicyManagerChromeOS(
          store.Pass(), external_data_manager.Pass(),
          component_policy_cache_dir, wait_for_initial_policy,
          initial_policy_fetch_timeout, base::ThreadTaskRunnerHandle::Get(),
          file_task_runner, io_task_runner));

  bool wildcard_match = false;
  if (connector->IsEnterpriseManaged() &&
      chromeos::CrosSettings::IsWhitelisted(username, &wildcard_match) &&
      wildcard_match && !connector->IsNonEnterpriseUser(username)) {
    manager->EnableWildcardLoginCheck(username);
  }

  manager->Init(
      SchemaRegistryServiceFactory::GetForContext(profile)->registry());
  manager->Connect(g_browser_process->local_state(),
                   device_management_service,
                   g_browser_process->system_request_context(),
                   affiliation);

  DCHECK(managers_.find(profile) == managers_.end());
  managers_[profile] = manager.get();
  return manager.Pass();
}

void UserCloudPolicyManagerFactoryChromeOS::BrowserContextShutdown(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  if (profile->IsOffTheRecord())
    return;
  UserCloudPolicyManagerChromeOS* manager = GetManagerForProfile(profile);
  if (manager)
    manager->Shutdown();
}

void UserCloudPolicyManagerFactoryChromeOS::BrowserContextDestroyed(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  managers_.erase(profile);
  BrowserContextKeyedBaseFactory::BrowserContextDestroyed(context);
}

void UserCloudPolicyManagerFactoryChromeOS::SetEmptyTestingFactory(
    content::BrowserContext* context) {}

bool UserCloudPolicyManagerFactoryChromeOS::HasTestingFactory(
    content::BrowserContext* context) {
  return false;
}

void UserCloudPolicyManagerFactoryChromeOS::CreateServiceNow(
    content::BrowserContext* context) {}

}  // namespace policy
