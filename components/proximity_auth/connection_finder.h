// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CONNECTION_FINDER_H
#define COMPONENTS_PROXIMITY_AUTH_CONNECTION_FINDER_H

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace proximity_auth {

class Connection;

// Interface for finding a connection to a remote device.
class ConnectionFinder {
 public:
  virtual ~ConnectionFinder() {}

  // Attempts to find a connection to a remote device. The finder will try to
  // find the connection indefinitely until the finder is destroyed. Calls
  // |connection_callback| with the open connection once the remote device is
  // connected.
  // TODO(isherman): Can this just be done as part of the constructor?
  typedef base::Callback<void(scoped_ptr<Connection> connection)>
      ConnectionCallback;
  virtual void Find(const ConnectionCallback& connection_callback) = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CONNECTION_FINDER_H
