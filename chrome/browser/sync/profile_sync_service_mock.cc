// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_mock.h"

#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_store.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"

ProfileSyncServiceMock::ProfileSyncServiceMock(Profile* profile)
    : ProfileSyncServiceMock(
          scoped_ptr<ProfileSyncComponentsFactory>(
              new ProfileSyncComponentsFactoryMock()),
          profile) {
}

ProfileSyncServiceMock::ProfileSyncServiceMock(
    scoped_ptr<ProfileSyncComponentsFactory> factory, Profile* profile)
    : ProfileSyncService(
          factory.Pass(),
          profile,
          make_scoped_ptr(new SupervisedUserSigninManagerWrapper(
              profile,
              SigninManagerFactory::GetForProfile(profile))),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          browser_sync::MANUAL_START) {
    ON_CALL(*this, IsSyncRequested()).WillByDefault(testing::Return(true));
}

ProfileSyncServiceMock::~ProfileSyncServiceMock() {
}

// static
TestingProfile* ProfileSyncServiceMock::MakeSignedInTestingProfile() {
  TestingProfile* profile = new TestingProfile();
  SigninManagerFactory::GetForProfile(profile)->
      SetAuthenticatedAccountInfo("12345", "foo");
  return profile;
}

// static
scoped_ptr<KeyedService> ProfileSyncServiceMock::BuildMockProfileSyncService(
    content::BrowserContext* profile) {
  return make_scoped_ptr(
      new ProfileSyncServiceMock(static_cast<Profile*>(profile)));
}
