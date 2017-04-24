// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_USER_ID_TRACKER_H_
#define SERVICES_UI_WS_USER_ID_TRACKER_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/public/interfaces/user_access_manager.mojom.h"
#include "services/ui/ws/user_id.h"

namespace ui {
namespace ws {

class UserIdTrackerObserver;

// Tracks the set of known/valid user ids.
class UserIdTracker : public mojom::UserAccessManager {
 public:
  UserIdTracker();
  ~UserIdTracker() override;

  bool IsValidUserId(const UserId& id) const;

  const UserId& active_id() const { return active_id_; }

  // Adds a new known user. Does nothing if |id| is not known.
  void SetActiveUserId(const UserId& id);
  void AddUserId(const UserId& id);
  void RemoveUserId(const UserId& id);

  void AddObserver(UserIdTrackerObserver* observer);
  void RemoveObserver(UserIdTrackerObserver* observer);

  void Bind(mojom::UserAccessManagerRequest request);

 private:
  using UserIdSet = std::set<UserId>;

  // mojom::UserAccessManager:
  void SetActiveUser(const std::string& user_id) override;

  base::ObserverList<UserIdTrackerObserver> observers_;

  UserIdSet ids_;
  UserId active_id_;

  mojo::BindingSet<mojom::UserAccessManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(UserIdTracker);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_USER_ID_TRACKER_H_
