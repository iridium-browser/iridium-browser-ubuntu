// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_proximity_auth_client.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/common/extensions/api/screenlock_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace screenlock = api::screenlock_private;

namespace {

screenlock::AuthType FromLockHandlerAuthType(
    proximity_auth::ScreenlockBridge::LockHandler::AuthType auth_type) {
  switch (auth_type) {
    case proximity_auth::ScreenlockBridge::LockHandler::OFFLINE_PASSWORD:
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
    case proximity_auth::ScreenlockBridge::LockHandler::NUMERIC_PIN:
      return screenlock::AUTH_TYPE_NUMERICPIN;
    case proximity_auth::ScreenlockBridge::LockHandler::USER_CLICK:
      return screenlock::AUTH_TYPE_USERCLICK;
    case proximity_auth::ScreenlockBridge::LockHandler::ONLINE_SIGN_IN:
      // Apps should treat forced online sign in same as system password.
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
    case proximity_auth::ScreenlockBridge::LockHandler::EXPAND_THEN_USER_CLICK:
      // This type is used for public sessions, which do not support screen
      // locking.
      NOTREACHED();
      return screenlock::AUTH_TYPE_NONE;
    case proximity_auth::ScreenlockBridge::LockHandler::FORCE_OFFLINE_PASSWORD:
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
  }
  NOTREACHED();
  return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
}

}  // namespace

ScreenlockPrivateGetLockedFunction::ScreenlockPrivateGetLockedFunction() {}

ScreenlockPrivateGetLockedFunction::~ScreenlockPrivateGetLockedFunction() {}

bool ScreenlockPrivateGetLockedFunction::RunAsync() {
  SetResult(new base::FundamentalValue(
      proximity_auth::ScreenlockBridge::Get()->IsLocked()));
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateSetLockedFunction::ScreenlockPrivateSetLockedFunction() {}

ScreenlockPrivateSetLockedFunction::~ScreenlockPrivateSetLockedFunction() {}

bool ScreenlockPrivateSetLockedFunction::RunAsync() {
  scoped_ptr<screenlock::SetLocked::Params> params(
      screenlock::SetLocked::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  EasyUnlockService* service = EasyUnlockService::Get(GetProfile());
  if (params->locked) {
    if (extension()->id() == extension_misc::kEasyUnlockAppId &&
        AppWindowRegistry::Get(browser_context())
            ->GetAppWindowForAppAndKey(extension()->id(),
                                       "easy_unlock_pairing")) {
      // Mark the Easy Unlock behaviour on the lock screen as the one initiated
      // by the Easy Unlock setup app as a trial one.
      // TODO(tbarzic): Move this logic to a new easyUnlockPrivate function.
      service->SetTrialRun();
    }
    proximity_auth::ScreenlockBridge::Get()->Lock();
  } else {
    proximity_auth::ScreenlockBridge::Get()->Unlock(
        service->proximity_auth_client()->GetAuthenticatedUsername());
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateAcceptAuthAttemptFunction::
    ScreenlockPrivateAcceptAuthAttemptFunction() {}

ScreenlockPrivateAcceptAuthAttemptFunction::
    ~ScreenlockPrivateAcceptAuthAttemptFunction() {}

bool ScreenlockPrivateAcceptAuthAttemptFunction::RunSync() {
  scoped_ptr<screenlock::AcceptAuthAttempt::Params> params(
      screenlock::AcceptAuthAttempt::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService* service = EasyUnlockService::Get(profile);
  if (service)
    service->FinalizeUnlock(params->accept);
  return true;
}

ScreenlockPrivateEventRouter::ScreenlockPrivateEventRouter(
    content::BrowserContext* context)
    : browser_context_(context) {
  proximity_auth::ScreenlockBridge::Get()->AddObserver(this);
}

ScreenlockPrivateEventRouter::~ScreenlockPrivateEventRouter() {}

void ScreenlockPrivateEventRouter::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  DispatchEvent(events::SCREENLOCK_PRIVATE_ON_CHANGED,
                screenlock::OnChanged::kEventName,
                new base::FundamentalValue(true));
}

void ScreenlockPrivateEventRouter::OnScreenDidUnlock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  DispatchEvent(events::SCREENLOCK_PRIVATE_ON_CHANGED,
                screenlock::OnChanged::kEventName,
                new base::FundamentalValue(false));
}

void ScreenlockPrivateEventRouter::OnFocusedUserChanged(
    const std::string& user_id) {
}

void ScreenlockPrivateEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value* arg) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  if (arg)
    args->Append(arg);
  scoped_ptr<Event> event(new Event(histogram_value, event_name, args.Pass()));
  EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ScreenlockPrivateEventRouter>> g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ScreenlockPrivateEventRouter>*
ScreenlockPrivateEventRouter::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ScreenlockPrivateEventRouter::Shutdown() {
  proximity_auth::ScreenlockBridge::Get()->RemoveObserver(this);
}

bool ScreenlockPrivateEventRouter::OnAuthAttempted(
    proximity_auth::ScreenlockBridge::LockHandler::AuthType auth_type,
    const std::string& value) {
  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router->HasEventListener(screenlock::OnAuthAttempted::kEventName))
    return false;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(screenlock::ToString(FromLockHandlerAuthType(auth_type)));
  args->AppendString(value);

  scoped_ptr<Event> event(
      new Event(events::SCREENLOCK_PRIVATE_ON_AUTH_ATTEMPTED,
                screenlock::OnAuthAttempted::kEventName, args.Pass()));
  router->BroadcastEvent(event.Pass());
  return true;
}

}  // namespace extensions
