// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_impl.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/url_constants.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND)
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::PlatformNotificationContext;
using message_center::NotifierId;

class ProfileAttributesEntry;

namespace {

// Invalid id for a renderer process. Used in cases where we need to check for
// permission without having an associated renderer process yet.
const int kInvalidRenderProcessId = -1;

void OnCloseNonPersistentNotificationProfileLoaded(
    const std::string& notification_id,
    Profile* profile) {
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationCommon::NON_PERSISTENT, notification_id);
}

// Callback used to close an non-persistent notification from blink.
void CancelNotification(const std::string& notification_id,
                        std::string profile_id,
                        bool incognito) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  profile_manager->LoadProfile(
      profile_id, incognito,
      base::Bind(&OnCloseNonPersistentNotificationProfileLoaded,
                 notification_id));
}

}  // namespace

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceImpl::GetInstance() {
  return base::Singleton<PlatformNotificationServiceImpl>::get();
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl()
    : test_display_service_(nullptr) {
#if BUILDFLAG(ENABLE_BACKGROUND)
  pending_click_dispatch_events_ = 0;
#endif
}

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() {}

void PlatformNotificationServiceImpl::OnPersistentNotificationClick(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    int action_index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::mojom::PermissionStatus permission_status =
      CheckPermissionOnUIThread(browser_context, origin,
                                kInvalidRenderProcessId);

  // TODO(peter): Change this to a CHECK() when Issue 555572 is resolved.
  // Also change this method to be const again.
  if (permission_status != blink::mojom::PermissionStatus::GRANTED) {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.ClickedWithoutPermission"));
    return;
  }

  if (action_index == -1) {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.Clicked"));
  } else {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.ClickedActionButton"));
  }

#if BUILDFLAG(ENABLE_BACKGROUND)
  // Ensure the browser stays alive while the event is processed.
  if (pending_click_dispatch_events_++ == 0) {
    click_dispatch_keep_alive_.reset(
        new ScopedKeepAlive(KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT,
                            KeepAliveRestartOption::DISABLED));
  }
#endif

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationClickEvent(
          browser_context, persistent_notification_id, origin, action_index,
          base::Bind(
              &PlatformNotificationServiceImpl::OnClickEventDispatchComplete,
              base::Unretained(this)));
}

void PlatformNotificationServiceImpl::OnPersistentNotificationClose(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If we programatically closed this notification, don't dispatch any event.
  if (closed_notifications_.erase(persistent_notification_id) != 0)
    return;

  if (by_user) {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.ClosedByUser"));
  } else {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.ClosedProgrammatically"));
  }
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationCloseEvent(
          browser_context, persistent_notification_id, origin, by_user,
          base::Bind(
              &PlatformNotificationServiceImpl::OnCloseEventDispatchComplete,
              base::Unretained(this)));
}

blink::mojom::PermissionStatus
PlatformNotificationServiceImpl::CheckPermissionOnUIThread(
    BrowserContext* browser_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

#if defined(ENABLE_EXTENSIONS)
  // Extensions support an API permission named "notification". This will grant
  // not only grant permission for using the Chrome App extension API, but also
  // for the Web Notification API.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser_context);
    extensions::ProcessMap* process_map =
        extensions::ProcessMap::Get(browser_context);

    const extensions::Extension* extension =
        registry->GetExtensionById(origin.host(),
                                   extensions::ExtensionRegistry::ENABLED);

    if (extension &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kNotifications) &&
        process_map->Contains(extension->id(), render_process_id)) {
      NotifierStateTracker* notifier_state_tracker =
          NotifierStateTrackerFactory::GetForProfile(profile);
      DCHECK(notifier_state_tracker);

      NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
      if (notifier_state_tracker->IsNotifierEnabled(notifier_id))
        return blink::mojom::PermissionStatus::GRANTED;
    }
  }
#endif

  return PermissionManager::Get(profile)->GetPermissionStatus(
      content::PermissionType::NOTIFICATIONS, origin, origin);
}

blink::mojom::PermissionStatus
PlatformNotificationServiceImpl::CheckPermissionOnIOThread(
    content::ResourceContext* resource_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
#if defined(ENABLE_EXTENSIONS)
  // Extensions support an API permission named "notification". This will grant
  // not only grant permission for using the Chrome App extension API, but also
  // for the Web Notification API.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    extensions::InfoMap* extension_info_map = io_data->GetExtensionInfoMap();
    const extensions::ProcessMap& process_map =
        extension_info_map->process_map();

    const extensions::Extension* extension =
        extension_info_map->extensions().GetByID(origin.host());

    if (extension &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kNotifications) &&
        process_map.Contains(extension->id(), render_process_id)) {
      if (!extension_info_map->AreNotificationsDisabled(extension->id()))
        return blink::mojom::PermissionStatus::GRANTED;
    }
  }
#endif

  // No enabled extensions exist, so check the normal host content settings.
  HostContentSettingsMap* host_content_settings_map =
      io_data->GetHostContentSettingsMap();
  ContentSetting setting = host_content_settings_map->GetContentSetting(
      origin,
      origin,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      content_settings::ResourceIdentifier());

  if (setting == CONTENT_SETTING_ALLOW)
    return blink::mojom::PermissionStatus::GRANTED;
  if (setting == CONTENT_SETTING_BLOCK)
    return blink::mojom::PermissionStatus::DENIED;

  return blink::mojom::PermissionStatus::ASK;
}

void PlatformNotificationServiceImpl::DisplayNotification(
    BrowserContext* browser_context,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources,
    std::unique_ptr<content::DesktopNotificationDelegate> delegate,
    base::Closure* cancel_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posted tasks can request notifications to be added, which would cause a
  // crash (see |ScopedKeepAlive|). We just do nothing here, the user would not
  // see the notification anyway, since we are shutting down.
  if (g_browser_process->IsShuttingDown())
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  DCHECK_EQ(0u, notification_data.actions.size());
  DCHECK_EQ(0u, notification_resources.action_icons.size());

  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(browser_context, std::move(delegate));
  Notification notification = CreateNotificationFromData(
      profile, GURL() /* service_worker_scope */, origin, notification_data,
      notification_resources, proxy);

  GetNotificationDisplayService(profile)->Display(
      NotificationCommon::NON_PERSISTENT, notification.delegate_id(),
      notification);
  if (cancel_callback) {
#if defined(OS_WIN)
    std::string profile_id =
        base::WideToUTF8(profile->GetPath().BaseName().value());
#elif defined(OS_POSIX)
    std::string profile_id = profile->GetPath().BaseName().value();
#endif
    *cancel_callback =
        base::Bind(&CancelNotification, notification.delegate_id(), profile_id,
                   profile->IsOffTheRecord());
  }

  HostContentSettingsMapFactory::GetForProfile(profile)->UpdateLastUsage(
      origin, origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posted tasks can request notifications to be added, which would cause a
  // crash (see |ScopedKeepAlive|). We just do nothing here, the user would not
  // see the notification anyway, since we are shutting down.
  if (g_browser_process->IsShuttingDown())
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  // The notification settings button will be appended after the developer-
  // supplied buttons, available in |notification_data.actions|.
  int settings_button_index = notification_data.actions.size();
  PersistentNotificationDelegate* delegate = new PersistentNotificationDelegate(
      browser_context, persistent_notification_id, origin,
      settings_button_index);

  Notification notification = CreateNotificationFromData(
      profile, service_worker_scope, origin, notification_data,
      notification_resources, delegate);

  // TODO(peter): Remove this mapping when we have reliable id generation for
  // the message_center::Notification objects.
  persistent_notifications_[persistent_notification_id] = notification.id();

  GetNotificationDisplayService(profile)->Display(
      NotificationCommon::PERSISTENT,
      base::Int64ToString(delegate->persistent_notification_id()),
      notification);
  content::RecordAction(
      base::UserMetricsAction("Notifications.Persistent.Shown"));

  HostContentSettingsMapFactory::GetForProfile(profile)->UpdateLastUsage(
      origin, origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    BrowserContext* browser_context,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  closed_notifications_.insert(persistent_notification_id);

#if defined(OS_ANDROID)
  bool cancel_by_persistent_id = true;
#else
  bool cancel_by_persistent_id =
      GetNotificationDisplayService(profile)->SupportsNotificationCenter();
#endif

  if (cancel_by_persistent_id) {
    // TODO(peter): Remove this conversion when the notification ids are being
    // generated by the caller of this method.
    GetNotificationDisplayService(profile)->Close(
        NotificationCommon::PERSISTENT,
        base::Int64ToString(persistent_notification_id));
  } else {
    auto iter = persistent_notifications_.find(persistent_notification_id);
    if (iter == persistent_notifications_.end())
      return;
    GetNotificationDisplayService(profile)->Close(
        NotificationCommon::PERSISTENT, iter->second);
  }

  persistent_notifications_.erase(persistent_notification_id);
}

bool PlatformNotificationServiceImpl::GetDisplayedPersistentNotifications(
    BrowserContext* browser_context,
    std::set<std::string>* displayed_notifications) {
  DCHECK(displayed_notifications);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || profile->AsTestingProfile())
    return false;  // Tests will not have a message center.

  // TODO(peter): Filter for persistent notifications only.
  return GetNotificationDisplayService(profile)->GetDisplayed(
      displayed_notifications);
}

void PlatformNotificationServiceImpl::OnClickEventDispatchComplete(
    content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationClickResult", status,
      content::PersistentNotificationStatus::
          PERSISTENT_NOTIFICATION_STATUS_MAX);
#if BUILDFLAG(ENABLE_BACKGROUND)
  DCHECK_GT(pending_click_dispatch_events_, 0);
  if (--pending_click_dispatch_events_ == 0) {
    click_dispatch_keep_alive_.reset();
  }
#endif
}

void PlatformNotificationServiceImpl::OnCloseEventDispatchComplete(
    content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationCloseResult", status,
      content::PersistentNotificationStatus::
          PERSISTENT_NOTIFICATION_STATUS_MAX);
}

Notification PlatformNotificationServiceImpl::CreateNotificationFromData(
    Profile* profile,
    const GURL& service_worker_scope,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources,
    NotificationDelegate* delegate) const {
  DCHECK_EQ(notification_data.actions.size(),
            notification_resources.action_icons.size());

  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_data.title,
      notification_data.body,
      gfx::Image::CreateFrom1xBitmap(notification_resources.notification_icon),
      message_center::NotifierId(origin), base::UTF8ToUTF16(origin.host()),
      origin, notification_data.tag, message_center::RichNotificationData(),
      delegate);

  notification.set_service_worker_scope(service_worker_scope);
  notification.set_context_message(
      DisplayNameForContextMessage(profile, origin));
  notification.set_vibration_pattern(notification_data.vibration_pattern);
  notification.set_timestamp(notification_data.timestamp);
  notification.set_renotify(notification_data.renotify);
  notification.set_silent(notification_data.silent);

  if (!notification_resources.image.drawsNothing()) {
    notification.set_type(message_center::NOTIFICATION_TYPE_IMAGE);
    notification.set_image(
        gfx::Image::CreateFrom1xBitmap(notification_resources.image));
  }

  // Badges are only supported on Android, primarily because it's the only
  // platform that makes good use of them in the status bar.
#if defined(OS_ANDROID)
  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  notification.set_small_image(
      gfx::Image::CreateFrom1xBitmap(notification_resources.badge));
#endif  // defined(OS_ANDROID)

  // Developer supplied action buttons.
  std::vector<message_center::ButtonInfo> buttons;
  for (size_t i = 0; i < notification_data.actions.size(); i++) {
    message_center::ButtonInfo button(notification_data.actions[i].title);
    // TODO(peter): Handle different screen densities instead of always using
    // the 1x bitmap - crbug.com/585815.
    button.icon =
        gfx::Image::CreateFrom1xBitmap(notification_resources.action_icons[i]);
    buttons.push_back(button);
  }
  notification.set_buttons(buttons);

  // On desktop, notifications with require_interaction==true stay on-screen
  // rather than minimizing to the notification center after a timeout.
  // On mobile, this is ignored (notifications are minimized at all times).
  if (notification_data.require_interaction)
    notification.set_never_timeout(true);

  return notification;
}

NotificationDisplayService*
PlatformNotificationServiceImpl::GetNotificationDisplayService(
    Profile* profile) {
  if (test_display_service_ != nullptr)
    return test_display_service_;
  return NotificationDisplayServiceFactory::GetForProfile(profile);
}

base::string16 PlatformNotificationServiceImpl::DisplayNameForContextMessage(
    Profile* profile,
    const GURL& origin) const {
#if defined(ENABLE_EXTENSIONS)
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            origin.host(), extensions::ExtensionRegistry::EVERYTHING);
    DCHECK(extension);

    return base::UTF8ToUTF16(extension->name());
  }
#endif

  return base::string16();
}

void PlatformNotificationServiceImpl::SetNotificationDisplayServiceForTesting(
    NotificationDisplayService* display_service) {
  test_display_service_ = display_service;
}
