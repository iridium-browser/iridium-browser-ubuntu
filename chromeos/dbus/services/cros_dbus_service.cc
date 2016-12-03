// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/services/cros_dbus_service.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

CrosDBusService* g_cros_dbus_service = NULL;

}  // namespace

// The CrosDBusService implementation used in production, and unit tests.
class CrosDBusServiceImpl : public CrosDBusService {
 public:
  CrosDBusServiceImpl(dbus::Bus* bus,
                      ScopedVector<ServiceProviderInterface> service_providers)
      : service_started_(false),
        origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus),
        service_providers_(std::move(service_providers)) {}

  ~CrosDBusServiceImpl() override {
  }

  // Starts the D-Bus service.
  void Start() {
    // Make sure we're running on the origin thread (i.e. the UI thread in
    // production).
    DCHECK(OnOriginThread());

    // Return if the service has been already started.
    if (service_started_)
      return;

    // There are some situations, described in http://crbug.com/234382#c27,
    // where processes on Linux can wind up stuck in an uninterruptible state
    // for tens of seconds. If this happens when Chrome is trying to exit,
    // this unkillable process can wind up clinging to ownership of
    // kLibCrosServiceName while the system is trying to restart the browser.
    // This leads to a fatal situation if we don't allow the new browser
    // instance to replace the old as the owner of kLibCrosServiceName as seen
    // in http://crbug.com/234382. Hence, REQUIRE_PRIMARY_ALLOW_REPLACEMENT.
    bus_->RequestOwnership(kLibCrosServiceName,
                           dbus::Bus::REQUIRE_PRIMARY_ALLOW_REPLACEMENT,
                           base::Bind(&CrosDBusServiceImpl::OnOwnership,
                                      base::Unretained(this)));

    exported_object_ = bus_->GetExportedObject(
        dbus::ObjectPath(kLibCrosServicePath));

    for (size_t i = 0; i < service_providers_.size(); ++i)
      service_providers_[i]->Start(exported_object_);

    service_started_ = true;

    VLOG(1) << "CrosDBusServiceImpl started.";
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called when an ownership request is completed.
  void OnOwnership(const std::string& service_name,
                   bool success) {
    LOG_IF(FATAL, !success) << "Failed to own: " << service_name;
  }

  bool service_started_;
  base::PlatformThreadId origin_thread_id_;
  dbus::Bus* bus_;
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Service providers that form CrosDBusService.
  ScopedVector<ServiceProviderInterface> service_providers_;
};

// The stub CrosDBusService implementation used on Linux desktop,
// which does nothing as of now.
class CrosDBusServiceStubImpl : public CrosDBusService {
 public:
  CrosDBusServiceStubImpl() {
  }

  ~CrosDBusServiceStubImpl() override {}
};

// static
void CrosDBusService::Initialize(
    ScopedVector<ServiceProviderInterface> service_providers) {
  if (g_cros_dbus_service) {
    LOG(WARNING) << "CrosDBusService was already initialized";
    return;
  }
  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();
  if (base::SysInfo::IsRunningOnChromeOS() && bus) {
    auto* service = new CrosDBusServiceImpl(bus, std::move(service_providers));
    g_cros_dbus_service = service;
    service->Start();
  } else {
    g_cros_dbus_service = new CrosDBusServiceStubImpl;
  }
  VLOG(1) << "CrosDBusService initialized";
}

// static
void CrosDBusService::InitializeForTesting(
    dbus::Bus* bus,
    ScopedVector<ServiceProviderInterface> service_providers) {
  if (g_cros_dbus_service) {
    LOG(WARNING) << "CrosDBusService was already initialized";
    return;
  }
  auto* service = new CrosDBusServiceImpl(bus, std::move(service_providers));
  service->Start();
  g_cros_dbus_service = service;
  VLOG(1) << "CrosDBusService initialized";
}

// static
void CrosDBusService::Shutdown() {
  delete g_cros_dbus_service;
  g_cros_dbus_service = NULL;
  VLOG(1) << "CrosDBusService Shutdown completed";
}

CrosDBusService::~CrosDBusService() {
}

CrosDBusService::ServiceProviderInterface::~ServiceProviderInterface() {
}

}  // namespace chromeos
