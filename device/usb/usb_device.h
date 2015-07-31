// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_H_
#define DEVICE_USB_USB_DEVICE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"

namespace device {

class UsbDeviceHandle;
struct UsbConfigDescriptor;

// A UsbDevice object represents a detected USB device, providing basic
// information about it. Methods other than simple property accessors must be
// called from the thread on which this object was created. For further
// manipulation of the device, a UsbDeviceHandle must be created from Open()
// method.
class UsbDevice : public base::RefCountedThreadSafe<UsbDevice> {
 public:
  using OpenCallback = base::Callback<void(scoped_refptr<UsbDeviceHandle>)>;
  using ResultCallback = base::Callback<void(bool success)>;

  // Accessors to basic information.
  uint16 vendor_id() const { return vendor_id_; }
  uint16 product_id() const { return product_id_; }
  uint32 unique_id() const { return unique_id_; }
  const base::string16& manufacturer_string() const {
    return manufacturer_string_;
  }
  const base::string16& product_string() const { return product_string_; }
  const base::string16& serial_number() const { return serial_number_; }

  // On ChromeOS the permission_broker service is used to change the ownership
  // of USB device nodes so that Chrome can open them. On other platforms these
  // functions are no-ops and always return true.
  virtual void CheckUsbAccess(const ResultCallback& callback);

  // Like CheckUsbAccess but actually changes the ownership of the device node.
  virtual void RequestUsbAccess(int interface_id,
                                const ResultCallback& callback);

  // Creates a UsbDeviceHandle for further manipulation.
  virtual void Open(const OpenCallback& callback) = 0;

  // Explicitly closes a device handle. This method will be automatically called
  // by the destructor of a UsbDeviceHandle as well.
  virtual bool Close(scoped_refptr<UsbDeviceHandle> handle) = 0;

  // Gets the UsbConfigDescriptor for the active device configuration or nullptr
  // if the device is unconfigured.
  virtual const UsbConfigDescriptor* GetConfiguration() = 0;

 protected:
  UsbDevice(uint16 vendor_id,
            uint16 product_id,
            uint32 unique_id,
            const base::string16& manufacturer_string,
            const base::string16& product_string,
            const base::string16& serial_number);
  virtual ~UsbDevice();

 private:
  friend class base::RefCountedThreadSafe<UsbDevice>;

  const uint16 vendor_id_;
  const uint16 product_id_;
  const uint32 unique_id_;
  const base::string16 manufacturer_string_;
  const base::string16 product_string_;
  const base::string16 serial_number_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_H_
