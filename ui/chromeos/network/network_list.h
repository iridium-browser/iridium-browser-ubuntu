// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_LIST_H_
#define UI_CHROMEOS_NETWORK_NETWORK_LIST_H_

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "chromeos/network/network_state_handler.h"
#include "ui/chromeos/network/network_icon_animation_observer.h"
#include "ui/chromeos/network/network_list_view_base.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/gfx/image/image_skia.h"

namespace views {
class Label;
class View;
}

namespace ui {

struct NetworkInfo;
class NetworkListDelegate;

// A list of available networks of a given type. This class is used for all
// network types except VPNs. For VPNs, see the |VPNList| class.
class UI_CHROMEOS_EXPORT NetworkListView
    : public NetworkListViewBase,
      public network_icon::AnimationObserver {
 public:
  explicit NetworkListView(NetworkListDelegate* delegate);
  ~NetworkListView() override;

  // NetworkListViewBase:
  void Update() override;
  bool IsNetworkEntry(views::View* view,
                      std::string* service_path) const override;

 private:
  void UpdateNetworks(
      const chromeos::NetworkStateHandler::NetworkStateList& networks);
  void UpdateNetworkIcons();
  void UpdateNetworkListInternal();
  void HandleRelayout();
  bool UpdateNetworkListEntries(std::set<std::string>* new_service_paths);
  bool UpdateNetworkChildren(std::set<std::string>* new_service_paths,
                             int* child_index,
                             bool highlighted);
  bool UpdateNetworkChild(int index, const NetworkInfo* info);
  bool PlaceViewAtIndex(views::View* view, int index);
  bool UpdateInfoLabel(int message_id, int index, views::Label** label);

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  NetworkListDelegate* delegate_;

  views::Label* no_wifi_networks_view_;
  views::Label* no_cellular_networks_view_;

  // An owned list of network info.
  ScopedVector<NetworkInfo> network_list_;

  typedef std::map<views::View*, std::string> NetworkMap;
  NetworkMap network_map_;

  // A map of network service paths to their view.
  typedef std::map<std::string, views::View*> ServicePathMap;
  ServicePathMap service_path_map_;

  DISALLOW_COPY_AND_ASSIGN(NetworkListView);
};

}  // namespace ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_LIST_H_
