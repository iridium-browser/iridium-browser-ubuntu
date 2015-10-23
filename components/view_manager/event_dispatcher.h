// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_EVENT_DISPATCHER_H_
#define COMPONENTS_VIEW_MANAGER_EVENT_DISPATCHER_H_

#include <set>

#include "base/basictypes.h"
#include "ui/mojo/events/input_event_constants.mojom.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "ui/mojo/events/input_key_codes.mojom.h"

namespace view_manager {

class ConnectionManager;
class ServerView;

// Handles dispatching events to the right location as well as updating focus.
class EventDispatcher {
 public:
  explicit EventDispatcher(ConnectionManager* connection_manager);
  ~EventDispatcher();

  void AddAccelerator(mojo::KeyboardCode keyboard_code, mojo::EventFlags flags);
  void RemoveAccelerator(mojo::KeyboardCode keyboard_code,
                         mojo::EventFlags flags);

  void OnEvent(ServerView* root, mojo::EventPtr event);

 private:
  struct Accelerator {
    Accelerator(mojo::KeyboardCode keyboard_code, mojo::EventFlags flags)
        : keyboard_code(keyboard_code), flags(flags) {}

    // So we can use this in a set.
    bool operator<(const Accelerator& other) const {
      if (keyboard_code == other.keyboard_code)
        return flags < other.flags;
      return keyboard_code < other.keyboard_code;
    }

    mojo::KeyboardCode keyboard_code;
    mojo::EventFlags flags;
  };

  ConnectionManager* connection_manager_;

  std::set<Accelerator> accelerators_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_EVENT_DISPATCHER_H_
