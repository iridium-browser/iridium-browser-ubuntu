// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_KIOSK_ARC_KIOSK_BRIDGE_H_
#define COMPONENTS_ARC_KIOSK_ARC_KIOSK_BRIDGE_H_

#include <map>

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/kiosk.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

class ArcKioskBridge : public ArcService,
                       public InstanceHolder<mojom::KioskInstance>::Observer,
                       public mojom::KioskHost {
 public:
  // Received IPCs are passed to this delegate.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnMaintenanceSessionCreated() = 0;
    virtual void OnMaintenanceSessionFinished() = 0;
  };

  // |delegate| should be alive while the ArcKioskBridge instance is alive.
  ArcKioskBridge(ArcBridgeService* bridge_service, Delegate* delegate);
  ~ArcKioskBridge() override;

  // InstanceHolder<mojom::KioskInstance>::Observer overrides.
  void OnInstanceReady() override;

  // mojom::KioskHost overrides.
  void OnMaintenanceSessionCreated(int32_t session_id) override;
  void OnMaintenanceSessionFinished(int32_t session_id, bool success) override;

 private:
  mojo::Binding<mojom::KioskHost> binding_;
  Delegate* const delegate_;

  // Tracks current maintenance session id.
  int32_t session_id_ = -1;

  DISALLOW_COPY_AND_ASSIGN(ArcKioskBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_KIOSK_ARC_KIOSK_BRIDGE_H_
