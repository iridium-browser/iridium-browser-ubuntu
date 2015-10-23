// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_DRIVER_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_DRIVER_H_

#include <string>

#include "base/containers/scoped_ptr_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace instance_id {

class InstanceID;

// Bridge between Instance ID users in Chrome and the platform-specific
// implementation.
class InstanceIDDriver {
 public:
  // Returns whether InstanceID is enabled.
  static bool IsInstanceIDEnabled();

  explicit InstanceIDDriver(gcm::GCMDriver* gcm_driver);
  virtual ~InstanceIDDriver();

  // Returns the InstanceID that provides the Instance ID service for the given
  // application. The lifetime of InstanceID will be managed by this class.
  InstanceID* GetInstanceID(const std::string& app_id);

  // Removes the InstanceID when it is not longer needed, i.e. the app is being
  // uninstalled.
  void RemoveInstanceID(const std::string& app_id);

  // Returns true if the InstanceID for the given application has been created.
  // This is currently only used for testing purpose.
  bool ExistsInstanceID(const std::string& app_id) const;

 private:
  gcm::GCMDriver* gcm_driver_;  // Not owned.
  base::ScopedPtrMap<std::string, scoped_ptr<InstanceID>> instance_id_map_;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDDriver);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_DRIVER_H_
