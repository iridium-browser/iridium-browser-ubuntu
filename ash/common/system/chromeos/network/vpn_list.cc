// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/vpn_list.h"

#include <utility>

#include "base/logging.h"

namespace ash {

VPNProvider::VPNProvider() : third_party(false) {}

VPNProvider::VPNProvider(const std::string& extension_id,
                         const std::string& third_party_provider_name)
    : third_party(true),
      extension_id(extension_id),
      third_party_provider_name(third_party_provider_name) {
  DCHECK(!extension_id.empty());
  DCHECK(!third_party_provider_name.empty());
}

bool VPNProvider::operator==(const VPNProvider& other) const {
  return third_party == other.third_party &&
         extension_id == other.extension_id &&
         third_party_provider_name == other.third_party_provider_name;
}

VpnList::Observer::~Observer() {}

VpnList::VpnList() {
  AddBuiltInProvider();
}

VpnList::~VpnList() {}

bool VpnList::HaveThirdPartyVPNProviders() const {
  for (const VPNProvider& provider : vpn_providers_) {
    if (provider.third_party)
      return true;
  }
  return false;
}

void VpnList::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VpnList::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VpnList::BindRequest(mojom::VpnListRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void VpnList::SetThirdPartyVpnProviders(
    std::vector<mojom::ThirdPartyVpnProviderPtr> providers) {
  vpn_providers_.clear();
  vpn_providers_.reserve(providers.size() + 1);
  // Add the OpenVPN provider.
  AddBuiltInProvider();
  // Append the extension-backed providers.
  for (const auto& provider : providers) {
    vpn_providers_.push_back(
        VPNProvider(provider->extension_id, provider->name));
  }
  NotifyObservers();
}

void VpnList::NotifyObservers() {
  for (auto& observer : observer_list_)
    observer.OnVPNProvidersChanged();
}

void VpnList::AddBuiltInProvider() {
  // The VPNProvider() constructor generates the built-in provider and has no
  // extension ID.
  vpn_providers_.push_back(VPNProvider());
}

}  // namespace ash
