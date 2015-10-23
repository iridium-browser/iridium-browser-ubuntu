// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNCED_SESSION_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNCED_SESSION_H_

#include <map>
#include <string>

#include "base/time/time.h"
#include "components/sessions/session_id.h"
#include "components/sessions/session_types.h"
#include "sync/protocol/session_specifics.pb.h"

namespace sessions {
struct SessionWindow;
}

namespace sync_driver {

// Defines a synced session for use by session sync. A synced session is a
// list of windows along with a unique session identifer (tag) and meta-data
// about the device being synced.
struct SyncedSession {
  typedef std::map<SessionID::id_type, sessions::SessionWindow*>
      SyncedWindowMap;

  // The type of device.
  // Please keep in sync with ForeignSessionHelper.java
  enum DeviceType {
    TYPE_UNSET = 0,
    TYPE_WIN = 1,
    TYPE_MACOSX = 2,
    TYPE_LINUX = 3,
    TYPE_CHROMEOS = 4,
    TYPE_OTHER = 5,
    TYPE_PHONE = 6,
    TYPE_TABLET = 7
  };

  SyncedSession();
  ~SyncedSession();

  // Unique tag for each session.
  std::string session_tag;
  // User-visible name
  std::string session_name;

  // Type of device this session is from.
  DeviceType device_type;

  // Last time this session was modified remotely.
  base::Time modified_time;

  // Map of windows that make up this session. Windowws are owned by the session
  // itself and free'd on destruction.
  SyncedWindowMap windows;

  // Converts the DeviceType enum value to a string. This is used
  // in the NTP handler for foreign sessions for matching session
  // types to an icon style.
  std::string DeviceTypeAsString() const {
    switch (device_type) {
      case SyncedSession::TYPE_WIN:
        return "win";
      case SyncedSession::TYPE_MACOSX:
        return "macosx";
      case SyncedSession::TYPE_LINUX:
        return "linux";
      case SyncedSession::TYPE_CHROMEOS:
        return "chromeos";
      case SyncedSession::TYPE_OTHER:
        return "other";
      case SyncedSession::TYPE_PHONE:
        return "phone";
      case SyncedSession::TYPE_TABLET:
        return "tablet";
      default:
        return std::string();
    }
  }

  // Convert this object to its protocol buffer equivalent. Shallow conversion,
  // does not create SessionTab protobufs.
  sync_pb::SessionHeader ToSessionHeader() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedSession);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNCED_SESSION_H_
