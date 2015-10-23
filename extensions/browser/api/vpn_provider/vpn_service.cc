// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/vpn_provider/vpn_service.h"

#include <stdint.h>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/shill_third_party_vpn_observer.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "crypto/sha2.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

namespace api_vpn = extensions::api::vpn_provider;

void DoNothingFailureCallback(const std::string& error_name,
                              const std::string& error_message) {
  LOG(ERROR) << error_name << ": " << error_message;
}

}  // namespace

class VpnService::VpnConfiguration : public ShillThirdPartyVpnObserver {
 public:
  VpnConfiguration(const std::string& extension_id,
                   const std::string& configuration_name,
                   const std::string& key,
                   base::WeakPtr<VpnService> vpn_service);
  ~VpnConfiguration() override;

  const std::string& extension_id() const { return extension_id_; }
  const std::string& configuration_name() const { return configuration_name_; }
  const std::string& key() const { return key_; }
  const std::string& service_path() const { return service_path_; }
  void set_service_path(const std::string& service_path) {
    service_path_ = service_path;
  }
  const std::string& object_path() const { return object_path_; }

  // ShillThirdPartyVpnObserver:
  void OnPacketReceived(const std::vector<char>& data) override;
  void OnPlatformMessage(uint32_t message) override;

 private:
  const std::string extension_id_;
  const std::string configuration_name_;
  const std::string key_;
  const std::string object_path_;

  std::string service_path_;

  base::WeakPtr<VpnService> vpn_service_;

  DISALLOW_COPY_AND_ASSIGN(VpnConfiguration);
};

VpnService::VpnConfiguration::VpnConfiguration(
    const std::string& extension_id,
    const std::string& configuration_name,
    const std::string& key,
    base::WeakPtr<VpnService> vpn_service)
    : extension_id_(extension_id),
      configuration_name_(configuration_name),
      key_(key),
      object_path_(shill::kObjectPathBase + key_),
      vpn_service_(vpn_service) {
}

VpnService::VpnConfiguration::~VpnConfiguration() {
}

void VpnService::VpnConfiguration::OnPacketReceived(
    const std::vector<char>& data) {
  if (!vpn_service_) {
    return;
  }
  scoped_ptr<base::ListValue> event_args =
      api_vpn::OnPacketReceived::Create(data);
  vpn_service_->SendSignalToExtension(
      extension_id_, extensions::events::VPN_PROVIDER_ON_PACKET_RECEIVED,
      api_vpn::OnPacketReceived::kEventName, event_args.Pass());
}

void VpnService::VpnConfiguration::OnPlatformMessage(uint32_t message) {
  if (!vpn_service_) {
    return;
  }
  DCHECK_GE(api_vpn::PLATFORM_MESSAGE_LAST, message);

  api_vpn::PlatformMessage platform_message =
      static_cast<api_vpn::PlatformMessage>(message);
  vpn_service_->SetActiveConfiguration(
      platform_message == api_vpn::PLATFORM_MESSAGE_CONNECTED ? this : nullptr);

  // TODO(kaliamoorthi): Update the lower layers to get the error message and
  // pass in the error instead of std::string().
  scoped_ptr<base::ListValue> event_args = api_vpn::OnPlatformMessage::Create(
      configuration_name_, platform_message, std::string());

  vpn_service_->SendSignalToExtension(
      extension_id_, extensions::events::VPN_PROVIDER_ON_PLATFORM_MESSAGE,
      api_vpn::OnPlatformMessage::kEventName, event_args.Pass());
}

VpnService::VpnService(
    content::BrowserContext* browser_context,
    const std::string& userid_hash,
    extensions::ExtensionRegistry* extension_registry,
    extensions::EventRouter* event_router,
    ShillThirdPartyVpnDriverClient* shill_client,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkStateHandler* network_state_handler)
    : browser_context_(browser_context),
      userid_hash_(userid_hash),
      extension_registry_(extension_registry),
      event_router_(event_router),
      shill_client_(shill_client),
      network_configuration_handler_(network_configuration_handler),
      network_profile_handler_(network_profile_handler),
      network_state_handler_(network_state_handler),
      active_configuration_(nullptr),
      weak_factory_(this) {
  extension_registry_->AddObserver(this);
  network_state_handler_->AddObserver(this, FROM_HERE);
  network_configuration_handler_->AddObserver(this);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&VpnService::NetworkListChanged, weak_factory_.GetWeakPtr()));
}

VpnService::~VpnService() {
  network_configuration_handler_->RemoveObserver(this);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  extension_registry_->RemoveObserver(this);
  STLDeleteContainerPairSecondPointers(key_to_configuration_map_.begin(),
                                       key_to_configuration_map_.end());
}

void VpnService::SendShowAddDialogToExtension(const std::string& extension_id) {
  SendSignalToExtension(extension_id,
                        extensions::events::VPN_PROVIDER_ON_UI_EVENT,
                        api_vpn::OnUIEvent::kEventName,
                        api_vpn::OnUIEvent::Create(
                            api_vpn::UI_EVENT_SHOWADDDIALOG, std::string()));
}

void VpnService::SendShowConfigureDialogToExtension(
    const std::string& extension_id,
    const std::string& configuration_id) {
  SendSignalToExtension(
      extension_id, extensions::events::VPN_PROVIDER_ON_UI_EVENT,
      api_vpn::OnUIEvent::kEventName,
      api_vpn::OnUIEvent::Create(api_vpn::UI_EVENT_SHOWCONFIGUREDIALOG,
                                 configuration_id));
}

void VpnService::SendPlatformError(const std::string& extension_id,
                                   const std::string& configuration_id,
                                   const std::string& error_message) {
  SendSignalToExtension(
      extension_id, extensions::events::VPN_PROVIDER_ON_PLATFORM_MESSAGE,
      api_vpn::OnPlatformMessage::kEventName,
      api_vpn::OnPlatformMessage::Create(
          configuration_id, api_vpn::PLATFORM_MESSAGE_ERROR, error_message));
}

std::string VpnService::GetKey(const std::string& extension_id,
                               const std::string& name) {
  const std::string key = crypto::SHA256HashString(extension_id + name);
  return base::HexEncode(key.data(), key.size());
}

void VpnService::OnConfigurationCreated(const std::string& service_path,
                                        const std::string& profile_path,
                                        const base::DictionaryValue& properties,
                                        Source source) {
}

void VpnService::OnConfigurationRemoved(const std::string& service_path,
                                        const std::string& guid,
                                        Source source) {
  if (source == SOURCE_EXTENSION_INSTALL) {
    // No need to process if the configuration was removed using an extension
    // API since the API would have already done the cleanup.
    return;
  }

  if (service_path_to_configuration_map_.find(service_path) ==
      service_path_to_configuration_map_.end()) {
    // Ignore removal of a configuration unknown to VPN service, which means the
    // configuration was created internally by the platform.
    return;
  }

  VpnConfiguration* configuration =
      service_path_to_configuration_map_[service_path];
  scoped_ptr<base::ListValue> event_args =
      api_vpn::OnConfigRemoved::Create(configuration->configuration_name());
  SendSignalToExtension(configuration->extension_id(),
                        extensions::events::VPN_PROVIDER_ON_CONFIG_REMOVED,
                        api_vpn::OnConfigRemoved::kEventName,
                        event_args.Pass());

  DestroyConfigurationInternal(configuration);
}

void VpnService::OnPropertiesSet(const std::string& service_path,
                                 const std::string& guid,
                                 const base::DictionaryValue& set_properties,
                                 Source source) {
}

void VpnService::OnConfigurationProfileChanged(const std::string& service_path,
                                               const std::string& profile_path,
                                               Source source) {
}

void VpnService::OnGetPropertiesSuccess(
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  if (service_path_to_configuration_map_.find(service_path) !=
      service_path_to_configuration_map_.end()) {
    return;
  }
  std::string vpn_type;
  std::string extension_id;
  std::string type;
  std::string configuration_name;
  if (!dictionary.GetString(shill::kProviderTypeProperty, &vpn_type) ||
      !dictionary.GetString(shill::kProviderHostProperty, &extension_id) ||
      !dictionary.GetString(shill::kTypeProperty, &type) ||
      !dictionary.GetString(shill::kNameProperty, &configuration_name) ||
      vpn_type != shill::kProviderThirdPartyVpn || type != shill::kTypeVPN) {
    return;
  }

  if (!extension_registry_->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED)) {
    // Does not belong to this instance of VpnService.
    return;
  }

  const std::string key = GetKey(extension_id, configuration_name);
  VpnConfiguration* configuration =
      CreateConfigurationInternal(extension_id, configuration_name, key);
  configuration->set_service_path(service_path);
  service_path_to_configuration_map_[service_path] = configuration;
  shill_client_->AddShillThirdPartyVpnObserver(configuration->object_path(),
                                               configuration);
}

void VpnService::OnGetPropertiesFailure(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
}

void VpnService::NetworkListChanged() {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(NetworkTypePattern::VPN(),
                                                      &network_list);
  for (auto& iter : network_list) {
    if (service_path_to_configuration_map_.find(iter->path()) !=
        service_path_to_configuration_map_.end()) {
      continue;
    }

    network_configuration_handler_->GetShillProperties(
        iter->path(), base::Bind(&VpnService::OnGetPropertiesSuccess,
                                 weak_factory_.GetWeakPtr()),
        base::Bind(&VpnService::OnGetPropertiesFailure,
                   weak_factory_.GetWeakPtr()));
  }
}

void VpnService::CreateConfiguration(const std::string& extension_id,
                                     const std::string& extension_name,
                                     const std::string& configuration_name,
                                     const SuccessCallback& success,
                                     const FailureCallback& failure) {
  if (configuration_name.empty()) {
    failure.Run(std::string(), std::string("Empty name not supported."));
    return;
  }

  const std::string key = GetKey(extension_id, configuration_name);
  if (ContainsKey(key_to_configuration_map_, key)) {
    failure.Run(std::string(), std::string("Name not unique."));
    return;
  }

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userid_hash_);
  if (!profile) {
    failure.Run(
        std::string(),
        std::string("No user profile for unshared network configuration."));
    return;
  }

  VpnConfiguration* configuration =
      CreateConfigurationInternal(extension_id, configuration_name, key);

  base::DictionaryValue properties;
  properties.SetStringWithoutPathExpansion(shill::kTypeProperty,
                                           shill::kTypeVPN);
  properties.SetStringWithoutPathExpansion(shill::kNameProperty,
                                           configuration_name);
  properties.SetStringWithoutPathExpansion(shill::kProviderHostProperty,
                                           extension_id);
  properties.SetStringWithoutPathExpansion(shill::kObjectPathSuffixProperty,
                                           configuration->key());
  properties.SetStringWithoutPathExpansion(shill::kProviderTypeProperty,
                                           shill::kProviderThirdPartyVpn);
  properties.SetStringWithoutPathExpansion(shill::kProfileProperty,
                                           profile->path);

  // Note: This will not create an entry in |policy_util|. TODO(pneubeck):
  // Determine the correct thing to do here, crbug.com/459278.
  std::string guid = base::GenerateGUID();
  properties.SetStringWithoutPathExpansion(shill::kGuidProperty, guid);

  network_configuration_handler_->CreateShillConfiguration(
      properties, NetworkConfigurationObserver::SOURCE_EXTENSION_INSTALL,
      base::Bind(&VpnService::OnCreateConfigurationSuccess,
                 weak_factory_.GetWeakPtr(), success, configuration),
      base::Bind(&VpnService::OnCreateConfigurationFailure,
                 weak_factory_.GetWeakPtr(), failure, configuration));
}

void VpnService::DestroyConfiguration(const std::string& extension_id,
                                      const std::string& configuration_id,
                                      const SuccessCallback& success,
                                      const FailureCallback& failure) {
  // The ID is the configuration name for now. This may change in the future.
  const std::string key = GetKey(extension_id, configuration_id);
  if (!ContainsKey(key_to_configuration_map_, key)) {
    failure.Run(std::string(), std::string("Unauthorized access."));
    return;
  }

  VpnConfiguration* configuration = key_to_configuration_map_[key];
  const std::string service_path = configuration->service_path();
  if (service_path.empty()) {
    failure.Run(std::string(), std::string("Pending create."));
    return;
  }
  if (active_configuration_ == configuration) {
    configuration->OnPlatformMessage(api_vpn::PLATFORM_MESSAGE_DISCONNECTED);
  }
  DestroyConfigurationInternal(configuration);

  network_configuration_handler_->RemoveConfiguration(
      service_path, NetworkConfigurationObserver::SOURCE_EXTENSION_INSTALL,
      base::Bind(&VpnService::OnRemoveConfigurationSuccess,
                 weak_factory_.GetWeakPtr(), success),
      base::Bind(&VpnService::OnRemoveConfigurationFailure,
                 weak_factory_.GetWeakPtr(), failure));
}

void VpnService::SetParameters(const std::string& extension_id,
                               const base::DictionaryValue& parameters,
                               const StringCallback& success,
                               const FailureCallback& failure) {
  if (!DoesActiveConfigurationExistAndIsAccessAuthorized(extension_id)) {
    failure.Run(std::string(), std::string("Unauthorized access."));
    return;
  }

  shill_client_->SetParameters(active_configuration_->object_path(), parameters,
                               success, failure);
}

void VpnService::SendPacket(const std::string& extension_id,
                            const std::vector<char>& data,
                            const SuccessCallback& success,
                            const FailureCallback& failure) {
  if (!DoesActiveConfigurationExistAndIsAccessAuthorized(extension_id)) {
    failure.Run(std::string(), std::string("Unauthorized access."));
    return;
  }

  if (data.empty()) {
    failure.Run(std::string(), std::string("Can't send an empty packet."));
    return;
  }

  shill_client_->SendPacket(active_configuration_->object_path(), data, success,
                            failure);
}

void VpnService::NotifyConnectionStateChanged(const std::string& extension_id,
                                              api_vpn::VpnConnectionState state,
                                              const SuccessCallback& success,
                                              const FailureCallback& failure) {
  if (!DoesActiveConfigurationExistAndIsAccessAuthorized(extension_id)) {
    failure.Run(std::string(), std::string("Unauthorized access."));
    return;
  }

  shill_client_->UpdateConnectionState(active_configuration_->object_path(),
                                       static_cast<uint32_t>(state), success,
                                       failure);
}

bool VpnService::VerifyConfigExistsForTesting(
    const std::string& extension_id,
    const std::string& configuration_name) {
  const std::string key = GetKey(extension_id, configuration_name);
  return ContainsKey(key_to_configuration_map_, key);
}

bool VpnService::VerifyConfigIsConnectedForTesting(
    const std::string& extension_id) {
  return DoesActiveConfigurationExistAndIsAccessAuthorized(extension_id);
}

void VpnService::DestroyConfigurationsForExtension(
    const extensions::Extension* extension) {
  std::vector<VpnConfiguration*> to_be_destroyed;
  for (const auto& iter : key_to_configuration_map_) {
    if (iter.second->extension_id() == extension->id()) {
      to_be_destroyed.push_back(iter.second);
    }
  }

  for (auto& iter : to_be_destroyed) {
    DestroyConfiguration(extension->id(),             // Extension ID
                         iter->configuration_name(),  // Configuration name
                         base::Bind(base::DoNothing),
                         base::Bind(DoNothingFailureCallback));
  }
}

void VpnService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (browser_context != browser_context_) {
    NOTREACHED();
    return;
  }

  DestroyConfigurationsForExtension(extension);
}

void VpnService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (browser_context != browser_context_) {
    NOTREACHED();
    return;
  }

  if (active_configuration_ &&
      active_configuration_->extension_id() == extension->id()) {
    shill_client_->UpdateConnectionState(
        active_configuration_->object_path(),
        static_cast<uint32_t>(api_vpn::VPN_CONNECTION_STATE_FAILURE),
        base::Bind(base::DoNothing), base::Bind(DoNothingFailureCallback));
  }
  if (reason == extensions::UnloadedExtensionInfo::REASON_DISABLE ||
      reason == extensions::UnloadedExtensionInfo::REASON_BLACKLIST) {
    DestroyConfigurationsForExtension(extension);
  }
}

void VpnService::OnCreateConfigurationSuccess(
    const VpnService::SuccessCallback& callback,
    VpnConfiguration* configuration,
    const std::string& service_path) {
  configuration->set_service_path(service_path);
  service_path_to_configuration_map_[service_path] = configuration;
  shill_client_->AddShillThirdPartyVpnObserver(configuration->object_path(),
                                               configuration);
  callback.Run();
}

void VpnService::OnCreateConfigurationFailure(
    const VpnService::FailureCallback& callback,
    VpnConfiguration* configuration,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  DestroyConfigurationInternal(configuration);
  callback.Run(error_name, std::string());
}

void VpnService::OnRemoveConfigurationSuccess(
    const VpnService::SuccessCallback& callback) {
  callback.Run();
}

void VpnService::OnRemoveConfigurationFailure(
    const VpnService::FailureCallback& callback,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  callback.Run(error_name, std::string());
}

void VpnService::SendSignalToExtension(
    const std::string& extension_id,
    extensions::events::HistogramValue histogram_value,
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args) {
  scoped_ptr<extensions::Event> event(new extensions::Event(
      histogram_value, event_name, event_args.Pass(), browser_context_));

  event_router_->DispatchEventToExtension(extension_id, event.Pass());
}

void VpnService::SetActiveConfiguration(
    VpnService::VpnConfiguration* configuration) {
  active_configuration_ = configuration;
}

VpnService::VpnConfiguration* VpnService::CreateConfigurationInternal(
    const std::string& extension_id,
    const std::string& configuration_name,
    const std::string& key) {
  VpnConfiguration* configuration = new VpnConfiguration(
      extension_id, configuration_name, key, weak_factory_.GetWeakPtr());
  // The object is owned by key_to_configuration_map_ henceforth.
  key_to_configuration_map_[key] = configuration;
  return configuration;
}

void VpnService::DestroyConfigurationInternal(VpnConfiguration* configuration) {
  key_to_configuration_map_.erase(configuration->key());
  if (active_configuration_ == configuration) {
    active_configuration_ = nullptr;
  }
  if (!configuration->service_path().empty()) {
    shill_client_->RemoveShillThirdPartyVpnObserver(
        configuration->object_path());
    service_path_to_configuration_map_.erase(configuration->service_path());
  }
  delete configuration;
}

bool VpnService::DoesActiveConfigurationExistAndIsAccessAuthorized(
    const std::string& extension_id) {
  return active_configuration_ &&
         active_configuration_->extension_id() == extension_id;
}

}  // namespace chromeos
