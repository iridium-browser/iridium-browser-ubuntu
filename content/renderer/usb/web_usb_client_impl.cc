// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/usb/web_usb_client_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/move.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/scoped_web_callbacks.h"
#include "content/renderer/usb/type_converters.h"
#include "content/renderer/usb/web_usb_device_impl.h"
#include "device/devices_app/public/cpp/constants.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"
#include "third_party/WebKit/public/platform/WebPassOwnPtr.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceFilter.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceInfo.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceRequestOptions.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBError.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace content {

namespace {

const char kNoServiceError[] = "USB service unavailable.";

// Generic default rejection handler for any WebUSB callbacks type. Assumes
// |CallbacksType| is a blink::WebCallbacks<T, const blink::WebUSBError&>
// for any type |T|.
template <typename CallbacksType>
void RejectCallbacksWithError(const blink::WebUSBError& error,
                              scoped_ptr<CallbacksType> callbacks) {
  callbacks->onError(error);
}

// Create a new ScopedWebCallbacks for WebUSB client callbacks, defaulting to
// a "no service" rejection.
template <typename CallbacksType>
ScopedWebCallbacks<CallbacksType> MakeScopedUSBCallbacks(
    CallbacksType* callbacks) {
  return make_scoped_web_callbacks(
      callbacks,
      base::Bind(&RejectCallbacksWithError<CallbacksType>,
                 blink::WebUSBError(blink::WebUSBError::Error::Service,
                                    base::UTF8ToUTF16(kNoServiceError))));
}

void OnGetDevicesComplete(
    ScopedWebCallbacks<blink::WebUSBClientGetDevicesCallbacks> scoped_callbacks,
    mojo::ServiceProvider* device_services,
    mojo::Array<device::usb::DeviceInfoPtr> results) {
  blink::WebVector<blink::WebUSBDevice*>* devices =
      new blink::WebVector<blink::WebUSBDevice*>(results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    device::usb::DeviceManagerPtr device_manager;
    mojo::ConnectToService(device_services, &device_manager);
    (*devices)[i] = new WebUSBDeviceImpl(
        device_manager.Pass(),
        mojo::ConvertTo<blink::WebUSBDeviceInfo>(results[i]));
  }
  scoped_callbacks.PassCallbacks()->onSuccess(blink::adoptWebPtr(devices));
}

}  // namespace

WebUSBClientImpl::WebUSBClientImpl(mojo::ServiceProviderPtr device_services)
    : device_services_(device_services.Pass()) {
  mojo::ConnectToService(device_services_.get(), &device_manager_);
}

WebUSBClientImpl::~WebUSBClientImpl() {}

void WebUSBClientImpl::getDevices(
    blink::WebUSBClientGetDevicesCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  // TODO(rockot): Remove this once DeviceManager is updated. It should no
  // longer take enumeration options.
  device::usb::EnumerationOptionsPtr options =
      device::usb::EnumerationOptions::New();
  options->filters = mojo::Array<device::usb::DeviceFilterPtr>::New(0);
  device_manager_->GetDevices(
      options.Pass(),
      base::Bind(&OnGetDevicesComplete, base::Passed(&scoped_callbacks),
                 base::Unretained(device_services_.get())));
}

void WebUSBClientImpl::requestDevice(
    const blink::WebUSBDeviceRequestOptions& options,
    blink::WebUSBClientRequestDeviceCallbacks* callbacks) {
  callbacks->onError(blink::WebUSBError(blink::WebUSBError::Error::Service,
                                        base::UTF8ToUTF16("Not implemented.")));
  delete callbacks;
}

}  // namespace content
