// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/declarative_user_script_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/value_store/testing_value_store.h"

using content::BrowserThread;

namespace extensions {

TestExtensionSystem::TestExtensionSystem(Profile* profile)
    : profile_(profile),
      value_store_(NULL),
      info_map_(new InfoMap()),
      error_console_(new ErrorConsole(profile)),
      quota_service_(new QuotaService()) {}

TestExtensionSystem::~TestExtensionSystem() {
}

void TestExtensionSystem::Shutdown() {
  if (extension_service_)
    extension_service_->Shutdown();
}

void TestExtensionSystem::CreateLazyBackgroundTaskQueue() {
  lazy_background_task_queue_.reset(new LazyBackgroundTaskQueue(profile_));
}

ExtensionPrefs* TestExtensionSystem::CreateExtensionPrefs(
    const base::CommandLine* command_line,
    const base::FilePath& install_directory) {
  bool extensions_disabled =
      command_line && command_line->HasSwitch(switches::kDisableExtensions);

  // Note that the GetPrefs() creates a TestingPrefService, therefore
  // the extension controlled pref values set in ExtensionPrefs
  // are not reflected in the pref service. One would need to
  // inject a new ExtensionPrefStore(extension_pref_value_map, false).

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Create(
      profile_->GetPrefs(),
      install_directory,
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile_),
      ExtensionsBrowserClient::Get()->CreateAppSorting().Pass(),
      extensions_disabled,
      std::vector<ExtensionPrefsObserver*>());
    ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(
        profile_,
        extension_prefs);
    return extension_prefs;
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    bool autoupdate_enabled) {
  if (!ExtensionPrefs::Get(profile_))
    CreateExtensionPrefs(command_line, install_directory);
  install_verifier_.reset(
      new InstallVerifier(ExtensionPrefs::Get(profile_), profile_));
  // The ownership of |value_store_| is immediately transferred to state_store_,
  // but we keep a naked pointer to the TestingValueStore.
  scoped_ptr<TestingValueStore> value_store(new TestingValueStore());
  value_store_ = value_store.get();
  state_store_.reset(new StateStore(profile_, value_store.Pass()));
  declarative_user_script_manager_.reset(
      new DeclarativeUserScriptManager(profile_));
  management_policy_.reset(new ManagementPolicy());
  management_policy_->RegisterProviders(
      ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->GetProviders());
  runtime_data_.reset(new RuntimeData(ExtensionRegistry::Get(profile_)));
  extension_service_.reset(new ExtensionService(profile_,
                                                command_line,
                                                install_directory,
                                                ExtensionPrefs::Get(profile_),
                                                Blacklist::Get(profile_),
                                                autoupdate_enabled,
                                                true,
                                                &ready_));
  extension_service_->ClearProvidersForTesting();
  return extension_service_.get();
}

ExtensionService* TestExtensionSystem::extension_service() {
  return extension_service_.get();
}

RuntimeData* TestExtensionSystem::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* TestExtensionSystem::management_policy() {
  return management_policy_.get();
}

void TestExtensionSystem::SetExtensionService(ExtensionService* service) {
  extension_service_.reset(service);
}

SharedUserScriptMaster* TestExtensionSystem::shared_user_script_master() {
  return NULL;
}

DeclarativeUserScriptManager*
TestExtensionSystem::declarative_user_script_manager() {
  return declarative_user_script_manager_.get();
}

StateStore* TestExtensionSystem::state_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::rules_store() {
  return state_store_.get();
}

InfoMap* TestExtensionSystem::info_map() { return info_map_.get(); }

LazyBackgroundTaskQueue*
TestExtensionSystem::lazy_background_task_queue() {
  return lazy_background_task_queue_.get();
}

void TestExtensionSystem::SetEventRouter(scoped_ptr<EventRouter> event_router) {
  event_router_.reset(event_router.release());
}

EventRouter* TestExtensionSystem::event_router() { return event_router_.get(); }

ErrorConsole* TestExtensionSystem::error_console() {
  return error_console_.get();
}

InstallVerifier* TestExtensionSystem::install_verifier() {
  return install_verifier_.get();
}

QuotaService* TestExtensionSystem::quota_service() {
  return quota_service_.get();
}

const OneShotEvent& TestExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* TestExtensionSystem::content_verifier() {
  return NULL;
}

scoped_ptr<ExtensionSet> TestExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return extension_service()->shared_module_service()->GetDependentExtensions(
      extension);
}

// static
KeyedService* TestExtensionSystem::Build(content::BrowserContext* profile) {
  return new TestExtensionSystem(static_cast<Profile*>(profile));
}

}  // namespace extensions
