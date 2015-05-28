// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"

#include <string>

#include "base/bind.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "components/gcm_driver/gcm_driver.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kWakeOnNone[] = "none";
const char kWakeOnPacket[] = "packet";
const char kWakeOnSsid[] = "ssid";
const char kWakeOnPacketAndSsid[] = "packet_and_ssid";

std::string WakeOnWifiFeatureToString(
    WakeOnWifiManager::WakeOnWifiFeature feature) {
  switch (feature) {
    case WakeOnWifiManager::WAKE_ON_NONE:
      return kWakeOnNone;
    case WakeOnWifiManager::WAKE_ON_PACKET:
      return kWakeOnPacket;
    case WakeOnWifiManager::WAKE_ON_SSID:
      return kWakeOnSsid;
    case WakeOnWifiManager::WAKE_ON_PACKET_AND_SSID:
      return kWakeOnPacketAndSsid;
    case WakeOnWifiManager::INVALID:
      return std::string();
    case WakeOnWifiManager::NOT_SUPPORTED:
      NOTREACHED();
      return std::string();
  }

  NOTREACHED() << "Unknown wake on wifi feature: " << feature;
  return std::string();
}

bool IsWakeOnPacketEnabled(WakeOnWifiManager::WakeOnWifiFeature feature) {
  return feature & WakeOnWifiManager::WAKE_ON_PACKET;
}

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
WakeOnWifiManager* g_wake_on_wifi_manager = NULL;

}  // namespace

// Simple class that listens for a connection to the GCM server and passes the
// connection information down to shill.  Each profile gets its own instance of
// this class.
class WakeOnWifiManager::WakeOnPacketConnectionObserver
    : public gcm::GCMConnectionObserver {
 public:
  WakeOnPacketConnectionObserver(Profile* profile,
                                 bool wifi_properties_received)
      : profile_(profile),
        ip_endpoint_(net::IPEndPoint()),
        wifi_properties_received_(wifi_properties_received) {
    gcm::GCMProfileServiceFactory::GetForProfile(profile_)
        ->driver()
        ->AddConnectionObserver(this);
  }

  ~WakeOnPacketConnectionObserver() override {
    if (!(ip_endpoint_ == net::IPEndPoint()))
      OnDisconnected();

    gcm::GCMProfileServiceFactory::GetForProfile(profile_)
        ->driver()
        ->RemoveConnectionObserver(this);
  }

  void HandleWifiDevicePropertiesReady() {
    wifi_properties_received_ = true;

    // IPEndPoint doesn't implement operator!=
    if (ip_endpoint_ == net::IPEndPoint())
      return;

    AddWakeOnPacketConnection();
  }

  // gcm::GCMConnectionObserver overrides.

  void OnConnected(const net::IPEndPoint& ip_endpoint) override {
    ip_endpoint_ = ip_endpoint;

    if (wifi_properties_received_)
      AddWakeOnPacketConnection();
  }

  void OnDisconnected() override {
    if (ip_endpoint_ == net::IPEndPoint()) {
      VLOG(1) << "Received GCMConnectionObserver::OnDisconnected without a "
              << "valid IPEndPoint.";
      return;
    }

    if (wifi_properties_received_)
      RemoveWakeOnPacketConnection();

    ip_endpoint_ = net::IPEndPoint();
  }

 private:
  void AddWakeOnPacketConnection() {
    NetworkHandler::Get()
        ->network_device_handler()
        ->AddWifiWakeOnPacketConnection(ip_endpoint_,
                                        base::Bind(&base::DoNothing),
                                        network_handler::ErrorCallback());
  }

  void RemoveWakeOnPacketConnection() {
    NetworkHandler::Get()
        ->network_device_handler()
        ->RemoveWifiWakeOnPacketConnection(ip_endpoint_,
                                           base::Bind(&base::DoNothing),
                                           network_handler::ErrorCallback());
  }

  Profile* profile_;
  net::IPEndPoint ip_endpoint_;
  bool wifi_properties_received_;

  DISALLOW_COPY_AND_ASSIGN(WakeOnPacketConnectionObserver);
};

// static
WakeOnWifiManager* WakeOnWifiManager::Get() {
  DCHECK(g_wake_on_wifi_manager);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_wake_on_wifi_manager;
}

WakeOnWifiManager::WakeOnWifiManager()
    : current_feature_(WakeOnWifiManager::INVALID),
      wifi_properties_received_(false),
      extension_event_observer_(new ExtensionEventObserver()),
      weak_ptr_factory_(this) {
  // This class must be constructed before any users are logged in, i.e., before
  // any profiles are created or added to the ProfileManager.  Additionally,
  // IsUserLoggedIn always returns true when we are not running on a Chrome OS
  // device so this check should only run on real devices.
  CHECK(!base::SysInfo::IsRunningOnChromeOS() ||
        !LoginState::Get()->IsUserLoggedIn());
  DCHECK(!g_wake_on_wifi_manager);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  g_wake_on_wifi_manager = this;

  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());

  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);

  GetWifiDeviceProperties();
}

WakeOnWifiManager::~WakeOnWifiManager() {
  DCHECK(g_wake_on_wifi_manager);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (current_feature_ != NOT_SUPPORTED) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
  g_wake_on_wifi_manager = NULL;
}

void WakeOnWifiManager::OnPreferenceChanged(
    WakeOnWifiManager::WakeOnWifiFeature feature) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (current_feature_ == NOT_SUPPORTED)
    return;
  if (!switches::WakeOnWifiEnabled())
    feature = WAKE_ON_NONE;
  if (feature == current_feature_)
    return;

  current_feature_ = feature;

  if (wifi_properties_received_)
    HandleWakeOnWifiFeatureUpdated();
}

bool WakeOnWifiManager::WakeOnWifiSupported() {
  return current_feature_ != NOT_SUPPORTED && current_feature_ != INVALID;
}

void WakeOnWifiManager::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      OnProfileAdded(content::Source<Profile>(source).ptr());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      OnProfileDestroyed(content::Source<Profile>(source).ptr());
      break;
    }
    default:
      NOTREACHED();
  }
}

void WakeOnWifiManager::DeviceListChanged() {
  if (current_feature_ != NOT_SUPPORTED)
    GetWifiDeviceProperties();
}

void WakeOnWifiManager::DevicePropertiesUpdated(const DeviceState* device) {
  if (device->Matches(NetworkTypePattern::WiFi()) &&
      current_feature_ != NOT_SUPPORTED) {
    GetWifiDeviceProperties();
  }
}

void WakeOnWifiManager::HandleWakeOnWifiFeatureUpdated() {
  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
          NetworkTypePattern::WiFi());
  if (!device)
    return;

  std::string feature_string(WakeOnWifiFeatureToString(current_feature_));
  DCHECK(!feature_string.empty());

  NetworkHandler::Get()->network_device_handler()->SetDeviceProperty(
      device->path(),
      shill::kWakeOnWiFiFeaturesEnabledProperty,
      base::StringValue(feature_string),
      base::Bind(&base::DoNothing),
      network_handler::ErrorCallback());

  bool wake_on_packet_enabled = IsWakeOnPacketEnabled(current_feature_);
  for (const auto& kv_pair : connection_observers_) {
    Profile* profile = kv_pair.first;
    gcm::GCMProfileServiceFactory::GetForProfile(profile)
        ->driver()
        ->WakeFromSuspendForHeartbeat(wake_on_packet_enabled);
  }

  extension_event_observer_->SetShouldDelaySuspend(wake_on_packet_enabled);
}

void WakeOnWifiManager::GetWifiDeviceProperties() {
  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
          NetworkTypePattern::WiFi());
  if (!device)
    return;

  NetworkHandler::Get()->network_device_handler()->GetDeviceProperties(
      device->path(),
      base::Bind(&WakeOnWifiManager::GetDevicePropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()),
      network_handler::ErrorCallback());
}

void WakeOnWifiManager::GetDevicePropertiesCallback(
    const std::string& device_path,
    const base::DictionaryValue& properties) {
  std::string enabled;
  if (!properties.HasKey(shill::kWakeOnWiFiFeaturesEnabledProperty) ||
      !properties.GetString(shill::kWakeOnWiFiFeaturesEnabledProperty,
                            &enabled) ||
      enabled == shill::kWakeOnWiFiFeaturesEnabledNotSupported) {
    current_feature_ = NOT_SUPPORTED;
    connection_observers_.clear();
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
    registrar_.RemoveAll();
    extension_event_observer_.reset();

    return;
  }

  // We always resend the wake on wifi setting unless it hasn't been set yet.
  // This covers situations where shill restarts or ends up recreating the wifi
  // device (crbug.com/475199).
  if (current_feature_ != INVALID)
    HandleWakeOnWifiFeatureUpdated();

  if (wifi_properties_received_)
    return;

  wifi_properties_received_ = true;

  NetworkHandler::Get()
      ->network_device_handler()
      ->RemoveAllWifiWakeOnPacketConnections(base::Bind(&base::DoNothing),
                                             network_handler::ErrorCallback());

  for (const auto& kv_pair : connection_observers_) {
    WakeOnPacketConnectionObserver* observer = kv_pair.second;
    observer->HandleWifiDevicePropertiesReady();
  }
}

void WakeOnWifiManager::OnProfileAdded(Profile* profile) {
  // add will do nothing if |profile| already exists in |connection_observers_|.
  auto result = connection_observers_.add(
      profile,
      make_scoped_ptr(new WakeOnWifiManager::WakeOnPacketConnectionObserver(
          profile, wifi_properties_received_)));

  if (result.second) {
    // This is a profile we haven't seen before.
    gcm::GCMProfileServiceFactory::GetForProfile(profile)
        ->driver()
        ->WakeFromSuspendForHeartbeat(
            IsWakeOnPacketEnabled(current_feature_));
  }
}

void WakeOnWifiManager::OnProfileDestroyed(Profile* profile) {
  connection_observers_.erase(profile);
}

}  // namespace chromeos
