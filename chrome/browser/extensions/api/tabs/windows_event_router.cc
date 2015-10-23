// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_event_router.h"

#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/tabs/app_base_window.h"
#include "chrome/browser/extensions/api/tabs/app_window_controller.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/constants.h"

using content::BrowserContext;

namespace extensions {

namespace keys = extensions::tabs_constants;
namespace windows = extensions::api::windows;

WindowsEventRouter::WindowsEventRouter(Profile* profile)
    : profile_(profile),
      focused_profile_(nullptr),
      focused_window_id_(extension_misc::kUnknownWindowId),
      observed_app_registry_(this),
      observed_controller_list_(this) {
  DCHECK(!profile->IsOffTheRecord());

  observed_app_registry_.Add(AppWindowRegistry::Get(profile_));
  observed_controller_list_.Add(WindowControllerList::GetInstance());
  // Needed for when no suitable window can be passed to an extension as the
  // currently focused window. On Mac (even in a toolkit-views build) always
  // rely on the notification sent by AppControllerMac after AppKit sends
  // NSWindowDidBecomeKeyNotification and there is no [NSApp keyWindo7w]. This
  // allows windows not created by toolkit-views to be tracked.
  // TODO(tapted): Remove the ifdefs (and NOTIFICATION_NO_KEY_WINDOW) when
  // Chrome on Mac only makes windows with toolkit-views.
#if defined(OS_MACOSX)
  registrar_.Add(this, chrome::NOTIFICATION_NO_KEY_WINDOW,
                 content::NotificationService::AllSources());
#elif defined(TOOLKIT_VIEWS)
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
#else
#error Unsupported
#endif

  AppWindowRegistry* registry = AppWindowRegistry::Get(profile_);
  for (AppWindow* app_window : registry->app_windows())
    AddAppWindow(app_window);
}

WindowsEventRouter::~WindowsEventRouter() {
#if !defined(OS_MACOSX)
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
#endif
}

void WindowsEventRouter::OnAppWindowAdded(extensions::AppWindow* app_window) {
  if (!profile_->IsSameProfile(
          Profile::FromBrowserContext(app_window->browser_context())))
    return;
  AddAppWindow(app_window);
}

void WindowsEventRouter::OnAppWindowRemoved(extensions::AppWindow* app_window) {
  if (!profile_->IsSameProfile(
          Profile::FromBrowserContext(app_window->browser_context())))
    return;

  scoped_ptr<WindowController> controller =
      app_windows_.take_and_erase(app_window->session_id().id());
}

void WindowsEventRouter::OnAppWindowActivated(
    extensions::AppWindow* app_window) {
  AppWindowMap::const_iterator iter =
      app_windows_.find(app_window->session_id().id());
  OnActiveWindowChanged(iter != app_windows_.end() ? iter->second : nullptr);
}

void WindowsEventRouter::OnWindowControllerAdded(
    WindowController* window_controller) {
  if (!HasEventListener(windows::OnCreated::kEventName))
    return;
  if (!profile_->IsSameProfile(window_controller->profile()))
    return;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  base::DictionaryValue* window_dictionary =
      window_controller->CreateWindowValue();
  args->Append(window_dictionary);
  DispatchEvent(events::WINDOWS_ON_CREATED, windows::OnCreated::kEventName,
                window_controller, args.Pass());
}

void WindowsEventRouter::OnWindowControllerRemoved(
    WindowController* window_controller) {
  if (!HasEventListener(windows::OnRemoved::kEventName))
    return;
  if (!profile_->IsSameProfile(window_controller->profile()))
    return;

  int window_id = window_controller->GetWindowId();
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(new base::FundamentalValue(window_id));
  DispatchEvent(events::WINDOWS_ON_REMOVED, windows::OnRemoved::kEventName,
                window_controller, args.Pass());
}

#if !defined(OS_MACOSX)
void WindowsEventRouter::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (!focused_now)
    OnActiveWindowChanged(nullptr);
}
#endif

void WindowsEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_MACOSX)
  if (chrome::NOTIFICATION_NO_KEY_WINDOW == type) {
    OnActiveWindowChanged(nullptr);
    return;
  }
#endif
}

static bool WillDispatchWindowFocusedEvent(
    WindowController* window_controller,
    BrowserContext* context,
    const Extension* extension,
    Event* event,
    const base::DictionaryValue* listener_filter) {
  int window_id = window_controller ? window_controller->GetWindowId()
                                    : extension_misc::kUnknownWindowId;
  Profile* new_active_context =
      window_controller ? window_controller->profile() : nullptr;

  // When switching between windows in the default and incognito profiles,
  // dispatch WINDOW_ID_NONE to extensions whose profile lost focus that
  // can't see the new focused window across the incognito boundary.
  // See crbug.com/46610.
  if (new_active_context && new_active_context != context &&
      !util::CanCrossIncognito(extension, context)) {
    event->event_args->Clear();
    event->event_args->Append(
        new base::FundamentalValue(extension_misc::kUnknownWindowId));
  } else {
    event->event_args->Clear();
    event->event_args->Append(new base::FundamentalValue(window_id));
  }
  return true;
}

void WindowsEventRouter::OnActiveWindowChanged(
    WindowController* window_controller) {
  Profile* window_profile = nullptr;
  int window_id = extension_misc::kUnknownWindowId;
  if (window_controller &&
      profile_->IsSameProfile(window_controller->profile())) {
    window_profile = window_controller->profile();
    window_id = window_controller->GetWindowId();
  }

  if (focused_window_id_ == window_id)
    return;

  // window_profile is either the default profile for the active window, its
  // incognito profile, or nullptr if the previous profile is losing focus.
  focused_profile_ = window_profile;
  focused_window_id_ = window_id;

  if (!HasEventListener(windows::OnFocusChanged::kEventName))
    return;

  scoped_ptr<Event> event(new Event(events::WINDOWS_ON_FOCUS_CHANGED,
                                    windows::OnFocusChanged::kEventName,
                                    make_scoped_ptr(new base::ListValue())));
  event->will_dispatch_callback =
      base::Bind(&WillDispatchWindowFocusedEvent, window_controller);
  // Set the window type to 'normal' if we don't have a window
  // controller, so the event is not filtered.
  event->filter_info.SetWindowType(window_controller
                                       ? window_controller->GetWindowTypeText()
                                       : keys::kWindowTypeValueNormal);
  EventRouter::Get(profile_)->BroadcastEvent(event.Pass());
}

void WindowsEventRouter::DispatchEvent(events::HistogramValue histogram_value,
                                       const std::string& event_name,
                                       WindowController* window_controller,
                                       scoped_ptr<base::ListValue> args) {
  scoped_ptr<Event> event(new Event(histogram_value, event_name, args.Pass()));
  event->restrict_to_browser_context = window_controller->profile();
  event->filter_info.SetWindowType(window_controller->GetWindowTypeText());
  EventRouter::Get(profile_)->BroadcastEvent(event.Pass());
}

bool WindowsEventRouter::HasEventListener(const std::string& event_name) {
  return EventRouter::Get(profile_)->HasEventListener(event_name);
}

void WindowsEventRouter::AddAppWindow(extensions::AppWindow* app_window) {
  scoped_ptr<AppWindowController> controller(new AppWindowController(
      app_window, make_scoped_ptr(new AppBaseWindow(app_window)), profile_));
  app_windows_.set(app_window->session_id().id(), controller.Pass());
}

}  // namespace extensions
