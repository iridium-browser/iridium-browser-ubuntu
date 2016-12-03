// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_

#include <memory>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/service.h"
#include "services/video_capture/public/interfaces/video_capture_service.mojom.h"

namespace video_capture {

class VideoCaptureDeviceFactoryImpl;
class FakeVideoCaptureDeviceFactoryConfiguratorImpl;

// Implementation of mojom::VideoCaptureService as a Mojo Shell Service.
class VideoCaptureService
    : public shell::Service,
      public shell::InterfaceFactory<mojom::VideoCaptureService>,
      public mojom::VideoCaptureService {
 public:
  VideoCaptureService();
  ~VideoCaptureService() override;

  // shell::Service:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // shell::InterfaceFactory<mojom::VideoCaptureService>:
  void Create(const shell::Identity& remote_identity,
              mojom::VideoCaptureServiceRequest request) override;

  // mojom::VideoCaptureService
  void ConnectToDeviceFactory(
      mojom::VideoCaptureDeviceFactoryRequest request) override;
  void ConnectToFakeDeviceFactory(
      mojom::VideoCaptureDeviceFactoryRequest request) override;

 private:
  void LazyInitializeDeviceFactory();
  void LazyInitializeFakeDeviceFactory();

  mojo::BindingSet<mojom::VideoCaptureService> bindings_;
  mojo::BindingSet<mojom::VideoCaptureDeviceFactory> factory_bindings_;
  mojo::BindingSet<mojom::VideoCaptureDeviceFactory> fake_factory_bindings_;
  std::unique_ptr<VideoCaptureDeviceFactoryImpl> device_factory_;
  std::unique_ptr<VideoCaptureDeviceFactoryImpl> fake_device_factory_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_
