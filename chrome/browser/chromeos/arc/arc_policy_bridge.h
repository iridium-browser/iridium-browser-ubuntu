// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_POLICY_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_POLICY_BRIDGE_H_

#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/instance_holder.h"
#include "components/policy/core/common/policy_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace policy {
class PolicyMap;
}  // namespace policy

namespace arc {

// Constants for the ARC certs sync mode are defined in the policy, please keep
// its in sync.
enum ArcCertsSyncMode : int32_t {
  // Certificates sync is disabled.
  SYNC_DISABLED = 0,
  // Copy of CA certificates is enabled.
  COPY_CA_CERTS = 1
};

class ArcPolicyBridge : public ArcService,
                        public InstanceHolder<mojom::PolicyInstance>::Observer,
                        public mojom::PolicyHost,
                        public policy::PolicyService::Observer {
 public:
  explicit ArcPolicyBridge(ArcBridgeService* bridge_service);
  ArcPolicyBridge(ArcBridgeService* bridge_service,
                  policy::PolicyService* policy_service);
  ~ArcPolicyBridge() override;

  void OverrideIsManagedForTesting(bool is_managed);

  // InstanceHolder<mojom::PolicyInstance>::Observer overrides.
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // PolicyHost overrides.
  void GetPolicies(const GetPoliciesCallback& callback) override;

  // PolicyService::Observer overrides.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

 private:
  void InitializePolicyService();

  mojo::Binding<PolicyHost> binding_;
  policy::PolicyService* policy_service_ = nullptr;
  bool is_managed_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcPolicyBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_POLICY_BRIDGE_H_
