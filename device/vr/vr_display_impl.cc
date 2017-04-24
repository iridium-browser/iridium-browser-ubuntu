// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_service_impl.h"

namespace device {

VRDisplayImpl::VRDisplayImpl(device::VRDevice* device, VRServiceImpl* service)
    : binding_(this),
      device_(device),
      service_(service),
      weak_ptr_factory_(this) {
  base::Callback<void(mojom::VRDisplayInfoPtr)> callback = base::Bind(
      &VRDisplayImpl::OnVRDisplayInfoCreated, weak_ptr_factory_.GetWeakPtr());
  device->GetVRDevice(callback);
}

void VRDisplayImpl::OnVRDisplayInfoCreated(
    mojom::VRDisplayInfoPtr display_info) {
  if (service_->client() && display_info) {
    service_->client()->OnDisplayConnected(binding_.CreateInterfacePtrAndBind(),
                                           mojo::MakeRequest(&client_),
                                           std::move(display_info));
  }
}

VRDisplayImpl::~VRDisplayImpl() {
  device_->RemoveDisplay(this);
}

void VRDisplayImpl::ResetPose() {
  if (!device_->IsAccessAllowed(this))
    return;

  device_->ResetPose();
}

void VRDisplayImpl::RequestPresent(bool secure_origin,
                                   const RequestPresentCallback& callback) {
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(false);
    return;
  }

  device_->RequestPresent(base::Bind(&VRDisplayImpl::RequestPresentResult,
                                     weak_ptr_factory_.GetWeakPtr(), callback,
                                     secure_origin));
}

void VRDisplayImpl::RequestPresentResult(const RequestPresentCallback& callback,
                                         bool secure_origin,
                                         bool success) {
  if (success) {
    device_->SetPresentingDisplay(this);
    device_->SetSecureOrigin(secure_origin);
  }
  callback.Run(success);
}

void VRDisplayImpl::ExitPresent() {
  if (device_->CheckPresentingDisplay(this))
    device_->ExitPresent();
}

void VRDisplayImpl::SubmitFrame(mojom::VRPosePtr pose) {
  if (!device_->CheckPresentingDisplay(this))
    return;
  device_->SubmitFrame(std::move(pose));
}

void VRDisplayImpl::UpdateLayerBounds(int16_t frame_index,
                                      mojom::VRLayerBoundsPtr left_bounds,
                                      mojom::VRLayerBoundsPtr right_bounds) {
  if (!device_->IsAccessAllowed(this))
    return;

  device_->UpdateLayerBounds(frame_index, std::move(left_bounds),
                             std::move(right_bounds));
}

void VRDisplayImpl::GetVRVSyncProvider(mojom::VRVSyncProviderRequest request) {
  if (!device_->IsAccessAllowed(this)) {
    return;
  }
  device_->GetVRVSyncProvider(std::move(request));
}
}
