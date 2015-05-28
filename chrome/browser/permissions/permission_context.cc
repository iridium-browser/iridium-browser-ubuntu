// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_context.h"

#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"
#include "chrome/browser/media/midi_permission_context.h"
#include "chrome/browser/media/midi_permission_context_factory.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context_factory.h"
#include "content/public/browser/permission_type.h"

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#endif

// static
PermissionContextBase* PermissionContext::Get(
    Profile* profile,
    content::PermissionType permission_type) {
  // NOTE: the factories used in this method have to stay in sync with
  // ::GetFactories() below.
  switch (permission_type) {
    case content::PermissionType::GEOLOCATION:
      return GeolocationPermissionContextFactory::GetForProfile(profile);
    case content::PermissionType::NOTIFICATIONS:
      return DesktopNotificationServiceFactory::GetForProfile(profile);
    case content::PermissionType::MIDI_SYSEX:
      return MidiPermissionContextFactory::GetForProfile(profile);
    case content::PermissionType::PUSH_MESSAGING:
      return PushMessagingPermissionContextFactory::GetForProfile(profile);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return ProtectedMediaIdentifierPermissionContextFactory::GetForProfile(
          profile);
#endif
    default:
      NOTREACHED() << "No PermissionContext associated with "
                   << static_cast<int>(permission_type);
      break;
  }

  return nullptr;
}

// static
const std::list<KeyedServiceBaseFactory*>& PermissionContext::GetFactories() {
  // NOTE: this list has to stay in sync with the factories used by ::Get().
  CR_DEFINE_STATIC_LOCAL(std::list<KeyedServiceBaseFactory*>, factories, ());

  if (factories.empty()) {
    factories.push_back(GeolocationPermissionContextFactory::GetInstance());
    factories.push_back(DesktopNotificationServiceFactory::GetInstance());
    factories.push_back(MidiPermissionContextFactory::GetInstance());
    factories.push_back(PushMessagingPermissionContextFactory::GetInstance());
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    factories.push_back(
        ProtectedMediaIdentifierPermissionContextFactory::GetInstance());
#endif
  }

  return factories;
}
