// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_info_watcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/signin_manager.h"

ProfileInfoWatcher::ProfileInfoWatcher(
    Profile* profile, const base::Closure& callback)
    : profile_(profile), callback_(callback) {
  DCHECK(profile_);
  DCHECK(!callback_.is_null());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The profile_manager might be NULL in testing environments.
  if (profile_manager)
    profile_manager->GetProfileInfoCache().AddObserver(this);

  signin_allowed_pref_.Init(prefs::kSigninAllowed, profile_->GetPrefs(),
      base::Bind(&ProfileInfoWatcher::RunCallback, base::Unretained(this)));
}

ProfileInfoWatcher::~ProfileInfoWatcher() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The profile_manager might be NULL in testing environments.
  if (profile_manager)
    profile_manager->GetProfileInfoCache().RemoveObserver(this);
}

void ProfileInfoWatcher::OnProfileAuthInfoChanged(
    const base::FilePath& profile_path) {
  RunCallback();
}

std::string ProfileInfoWatcher::GetAuthenticatedUsername() const {
  std::string username;
  SigninManagerBase* signin_manager = GetSigninManager();
  if (signin_manager)
    username = signin_manager->GetAuthenticatedUsername();
  return username;
}

SigninManagerBase* ProfileInfoWatcher::GetSigninManager() const {
  return SigninManagerFactory::GetForProfile(profile_);
}

void ProfileInfoWatcher::RunCallback() {
  if (GetSigninManager())
    callback_.Run();
}
