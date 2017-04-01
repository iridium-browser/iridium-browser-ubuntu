// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SESSION_H_
#define COMPONENTS_ARC_ARC_SESSION_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

class ArcSessionObserver;

// Starts the ARC instance and bootstraps the bridge connection.
// Clients should implement the Delegate to be notified upon communications
// being available.
// The instance can be safely removed 1) before Start() is called, or 2) after
// OnStopped() is called.
// The number of instances must be at most one. Otherwise, ARC instances will
// conflict.
class ArcSession {
 public:
  // Creates a default instance of ArcSession.
  static std::unique_ptr<ArcSession> Create(
      ArcBridgeService* arc_bridge_service,
      const scoped_refptr<base::TaskRunner>& blocking_task_runner);
  virtual ~ArcSession();

  // Starts and bootstraps a connection with the instance. The Observer's
  // OnReady() will be called if the bootstrapping is successful, or
  // OnStopped() if it is not. Start() should not be called twice or more.
  virtual void Start() = 0;

  // Requests to stop the currently-running instance.
  // The completion is notified via OnStopped() of the Delegate.
  virtual void Stop() = 0;

  // Called when Chrome is in shutdown state. This is called when the message
  // loop is already stopped, and the instance will soon be deleted.
  virtual void OnShutdown() = 0;

  void AddObserver(ArcSessionObserver* observer);
  void RemoveObserver(ArcSessionObserver* observer);

 protected:
  ArcSession();

  base::ObserverList<ArcSessionObserver> observer_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SESSION_H_
