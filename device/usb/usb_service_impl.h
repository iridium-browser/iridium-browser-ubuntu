// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include <map>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_device_impl.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_WIN)
#include "base/scoped_observer.h"
#include "device/core/device_monitor_win.h"
#endif  // OS_WIN

struct libusb_device;
struct libusb_context;

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace device {

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_context* PlatformUsbContext;

class UsbServiceImpl : public UsbService,
#if defined(OS_WIN)
                       public DeviceMonitorWin::Observer,
#endif  // OS_WIN
                       public base::MessageLoop::DestructionObserver {
 public:
  static UsbService* Create(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

 private:
  explicit UsbServiceImpl(
      PlatformUsbContext context,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  ~UsbServiceImpl() override;

  // device::UsbService implementation
  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) override;
  void GetDevices(const GetDevicesCallback& callback) override;

#if defined(OS_WIN)
  // device::DeviceMonitorWin::Observer implementation
  void OnDeviceAdded(const GUID& class_guid,
                     const std::string& device_path) override;
  void OnDeviceRemoved(const GUID& class_guid,
                       const std::string& device_path) override;
#endif  // OS_WIN

  // base::MessageLoop::DestructionObserver implementation
  void WillDestroyCurrentMessageLoop() override;

  // Enumerate USB devices from OS and update devices_ map. |new_device_path| is
  // an optional hint used on Windows to prevent enumerations before drivers for
  // a new device have been completely loaded.
  void RefreshDevices(const std::string& new_device_path);

  static void RefreshDevicesOnBlockingThread(
      base::WeakPtr<UsbServiceImpl> usb_service,
      const std::string& new_device_path,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<UsbContext> usb_context,
      const std::set<PlatformUsbDevice>& previous_devices);

  static void AddDeviceOnBlockingThread(
      base::WeakPtr<UsbServiceImpl> usb_service,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      PlatformUsbDevice platform_device);

  void RefreshDevicesComplete(libusb_device** platform_devices,
                              ssize_t device_count);

  // Adds a new UsbDevice to the devices_ map based on the given libusb device.
  void AddDevice(PlatformUsbDevice platform_device,
                 uint16 vendor_id,
                 uint16 product_id,
                 base::string16 manufacturer_string,
                 base::string16 product_string,
                 base::string16 serial_number,
                 std::string device_node);

  void RemoveDevice(scoped_refptr<UsbDeviceImpl> device);

  // Handle hotplug events from libusb.
  static int LIBUSB_CALL HotplugCallback(libusb_context* context,
                                         PlatformUsbDevice device,
                                         libusb_hotplug_event event,
                                         void* user_data);
  // These functions release a reference to the provided platform device.
  void OnPlatformDeviceAdded(PlatformUsbDevice platform_device);
  void OnPlatformDeviceRemoved(PlatformUsbDevice platform_device);

  scoped_refptr<UsbContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // TODO(reillyg): Figure out a better solution for device IDs.
  uint32 next_unique_id_ = 0;

  // When available the device list will be updated when new devices are
  // connected instead of only when a full enumeration is requested.
  // TODO(reillyg): Support this on all platforms. crbug.com/411715
  bool hotplug_enabled_ = false;
  libusb_hotplug_callback_handle hotplug_handle_;

  // Enumeration callbacks are queued until an enumeration completes.
  bool enumeration_ready_ = false;
  std::vector<GetDevicesCallback> pending_enumerations_;

  // The map from unique IDs to UsbDevices.
  typedef std::map<uint32, scoped_refptr<UsbDeviceImpl>> DeviceMap;
  DeviceMap devices_;

  // The map from PlatformUsbDevices to UsbDevices.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDeviceImpl>>
      PlatformDeviceMap;
  PlatformDeviceMap platform_devices_;

#if defined(OS_WIN)
  ScopedObserver<DeviceMonitorWin, DeviceMonitorWin::Observer> device_observer_;
#endif  // OS_WIN

  base::WeakPtrFactory<UsbServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbServiceImpl);
};

}  // namespace device
