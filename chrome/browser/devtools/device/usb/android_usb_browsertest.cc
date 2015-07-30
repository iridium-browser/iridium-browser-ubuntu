// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/device/adb/mock_adb_server.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using device::UsbConfigDescriptor;
using device::UsbDevice;
using device::UsbDeviceHandle;
using device::UsbEndpointDescriptor;
using device::UsbEndpointDirection;
using device::UsbInterfaceDescriptor;
using device::UsbService;
using device::UsbSynchronizationType;
using device::UsbTransferType;
using device::UsbUsageType;

namespace {

struct NoConfigTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
  static const bool kBreaks = false;
  static const bool kConfigured = false;
};

struct AndroidTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
  static const bool kBreaks = false;
  static const bool kConfigured = true;
};

struct NonAndroidTraits {
  static const int kClass = 0xf0;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x2;
  static const bool kBreaks = false;
  static const bool kConfigured = true;
};

struct BreakingAndroidTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
  static const bool kBreaks = true;
  static const bool kConfigured = true;
};

const uint32 kMaxPayload = 4096;
const uint32 kVersion = 0x01000000;

const char kDeviceManufacturer[] = "Test Manufacturer";
const char kDeviceModel[] = "Nexus 6";
const char kDeviceSerial[] = "01498B321301A00A";

template <class T>
class MockUsbDevice;

class MockLocalSocket : public MockAndroidConnection::Delegate {
 public:
  using Callback = base::Callback<void(int command,
                                       const std::string& message)>;

  MockLocalSocket(const Callback& callback,
                  const std::string& serial,
                  const std::string& command)
      : callback_(callback),
        connection_(new MockAndroidConnection(this, serial, command)) {
  }

  void Receive(const std::string& data) {
    connection_->Receive(data);
  }

 private:
  void SendSuccess(const std::string& message) override {
    if (!message.empty())
      callback_.Run(AdbMessage::kCommandWRTE, message);
  }

  void SendRaw(const std::string& message) override {
    callback_.Run(AdbMessage::kCommandWRTE, message);
  }

  void Close() override {
    callback_.Run(AdbMessage::kCommandCLSE, std::string());
  }

  Callback callback_;
  scoped_ptr<MockAndroidConnection> connection_;
};

template <class T>
class MockUsbDeviceHandle : public UsbDeviceHandle {
 public:
  explicit MockUsbDeviceHandle(MockUsbDevice<T>* device)
      : device_(device),
        remaining_body_length_(0),
        last_local_socket_(0),
        broken_(false) {
  }

  scoped_refptr<UsbDevice> GetDevice() const override {
    return device_;
  }

  void Close() override { device_ = nullptr; }

  void SetConfiguration(int configuration_value,
                        const ResultCallback& callback) override {
    NOTIMPLEMENTED();
  }

  void ClaimInterface(int interface_number,
                      const ResultCallback& callback) override {
    bool success = false;
    if (device_->claimed_interfaces_.find(interface_number) ==
        device_->claimed_interfaces_.end()) {
      device_->claimed_interfaces_.insert(interface_number);
      success = true;
    }

    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, success));
  }

  bool ReleaseInterface(int interface_number) override {
    if (device_->claimed_interfaces_.find(interface_number) ==
        device_->claimed_interfaces_.end())
      return false;

    device_->claimed_interfaces_.erase(interface_number);
    return true;
  }

  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    const ResultCallback& callback) override {
    NOTIMPLEMENTED();
  }

  void ResetDevice(const ResultCallback& callback) override {
    NOTIMPLEMENTED();
  }

  // Async IO. Can be called on any thread.
  void ControlTransfer(UsbEndpointDirection direction,
                       TransferRequestType request_type,
                       TransferRecipient recipient,
                       uint8 request,
                       uint16 value,
                       uint16 index,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const TransferCallback& callback) override {}

  void BulkTransfer(UsbEndpointDirection direction,
                    uint8 endpoint,
                    scoped_refptr<net::IOBuffer> buffer,
                    size_t length,
                    unsigned int timeout,
                    const TransferCallback& callback) override {
    if (direction == device::USB_DIRECTION_OUTBOUND) {
      if (remaining_body_length_ == 0) {
        std::vector<uint32> header(6);
        memcpy(&header[0], buffer->data(), length);
        current_message_.reset(
            new AdbMessage(header[0], header[1], header[2], std::string()));
        remaining_body_length_ = header[3];
        uint32 magic = header[5];
        if ((current_message_->command ^ 0xffffffff) != magic) {
          DCHECK(false) << "Header checksum error";
          return;
        }
      } else {
        DCHECK(current_message_.get());
        current_message_->body += std::string(buffer->data(), length);
        remaining_body_length_ -= length;
      }

      if (remaining_body_length_ == 0) {
        ProcessIncoming();
      }

      device::UsbTransferStatus status =
          broken_ ? device::USB_TRANSFER_ERROR : device::USB_TRANSFER_COMPLETED;
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, status, nullptr, 0));
      ProcessQueries();
    } else if (direction == device::USB_DIRECTION_INBOUND) {
      queries_.push(Query(callback, buffer, length));
      ProcessQueries();
    }
  }

  template <class D>
  void append(D data) {
    std::copy(reinterpret_cast<char*>(&data),
              (reinterpret_cast<char*>(&data)) + sizeof(D),
              std::back_inserter(output_buffer_));
  }

  // Copied from AndroidUsbDevice::Checksum
  uint32 Checksum(const std::string& data) {
    unsigned char* x = (unsigned char*)data.data();
    int count = data.length();
    uint32 sum = 0;
    while (count-- > 0)
      sum += *x++;
    return sum;
  }

  void ProcessIncoming() {
    DCHECK(current_message_.get());
    switch (current_message_->command) {
      case AdbMessage::kCommandCNXN: {
        WriteResponse(kVersion,
                      kMaxPayload,
                      AdbMessage::kCommandCNXN,
                      "device::ro.product.name=SampleProduct;ro.product.model="
                      "SampleModel;ro.product.device=SampleDevice;");
        break;
      }
      case AdbMessage::kCommandCLSE: {
        WriteResponse(0,
                      current_message_->arg0,
                      AdbMessage::kCommandCLSE,
                      std::string());
        local_sockets_.erase(current_message_->arg0);
        break;
      }
      case AdbMessage::kCommandWRTE: {
        if (T::kBreaks) {
          broken_ = true;
          return;
        }
        auto it = local_sockets_.find(current_message_->arg0);
        if (it == local_sockets_.end())
          return;

        DCHECK(current_message_->arg1 != 0);
        WriteResponse(current_message_->arg1,
                      current_message_->arg0,
                      AdbMessage::kCommandOKAY,
                      std::string());
        it->second->Receive(current_message_->body);
        break;
      }
      case AdbMessage::kCommandOPEN: {
        DCHECK(current_message_->arg1 == 0);
        DCHECK(current_message_->arg0 != 0);
        std::string response;
        WriteResponse(++last_local_socket_,
                      current_message_->arg0,
                      AdbMessage::kCommandOKAY,
                      std::string());
        local_sockets_.set(
            current_message_->arg0,
            make_scoped_ptr(new MockLocalSocket(
                base::Bind(&MockUsbDeviceHandle::WriteResponse,
                           base::Unretained(this),
                           last_local_socket_,
                           current_message_->arg0),
                kDeviceSerial,
                current_message_->body.substr(
                    0, current_message_->body.size() - 1))));
        return;
      }
      default: {
        return;
      }
    }
    ProcessQueries();
  }

  void WriteResponse(int arg0, int arg1, int command, const std::string& body) {
    append(command);
    append(arg0);
    append(arg1);
    bool add_zero = !body.empty() && (command != AdbMessage::kCommandWRTE);
    append(static_cast<uint32>(body.size() + (add_zero ? 1 : 0)));
    append(Checksum(body));
    append(command ^ 0xffffffff);
    std::copy(body.begin(), body.end(), std::back_inserter(output_buffer_));
    if (add_zero) {
      output_buffer_.push_back(0);
    }
    ProcessQueries();
  }

  void ProcessQueries() {
    if (!queries_.size())
      return;
    Query query = queries_.front();
    if (broken_) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(query.callback,
                     device::USB_TRANSFER_ERROR,
                     nullptr,
                     0));
    }

    if (query.size > output_buffer_.size())
      return;

    queries_.pop();
    std::copy(output_buffer_.begin(),
              output_buffer_.begin() + query.size,
              query.buffer->data());
    output_buffer_.erase(output_buffer_.begin(),
                         output_buffer_.begin() + query.size);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(query.callback,
                   device::USB_TRANSFER_COMPLETED,
                   query.buffer,
                   query.size));

  }

  void InterruptTransfer(UsbEndpointDirection direction,
                         uint8 endpoint,
                         scoped_refptr<net::IOBuffer> buffer,
                         size_t length,
                         unsigned int timeout,
                         const TransferCallback& callback) override {}

  void IsochronousTransfer(UsbEndpointDirection direction,
                           uint8 endpoint,
                           scoped_refptr<net::IOBuffer> buffer,
                           size_t length,
                           unsigned int packets,
                           unsigned int packet_length,
                           unsigned int timeout,
                           const TransferCallback& callback) override {}

 protected:
  virtual ~MockUsbDeviceHandle() {}

  struct Query {
    TransferCallback callback;
    scoped_refptr<net::IOBuffer> buffer;
    size_t size;

    Query(TransferCallback callback,
          scoped_refptr<net::IOBuffer> buffer,
          int size)
        : callback(callback), buffer(buffer), size(size) {}
  };

  scoped_refptr<MockUsbDevice<T> > device_;
  uint32 remaining_body_length_;
  scoped_ptr<AdbMessage> current_message_;
  std::vector<char> output_buffer_;
  std::queue<Query> queries_;
  base::ScopedPtrHashMap<int, scoped_ptr<MockLocalSocket>> local_sockets_;
  int last_local_socket_;
  bool broken_;
};

template <class T>
class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice()
      : UsbDevice(0,
                  0,
                  0,
                  base::UTF8ToUTF16(kDeviceManufacturer),
                  base::UTF8ToUTF16(kDeviceModel),
                  base::UTF8ToUTF16(kDeviceSerial)) {
    UsbEndpointDescriptor bulk_in;
    bulk_in.address = 0x81;
    bulk_in.direction = device::USB_DIRECTION_INBOUND;
    bulk_in.maximum_packet_size = 512;
    bulk_in.transfer_type = device::USB_TRANSFER_BULK;

    UsbEndpointDescriptor bulk_out;
    bulk_out.address = 0x01;
    bulk_out.direction = device::USB_DIRECTION_OUTBOUND;
    bulk_out.maximum_packet_size = 512;
    bulk_out.transfer_type = device::USB_TRANSFER_BULK;

    UsbInterfaceDescriptor interface_desc;
    interface_desc.interface_number = 0;
    interface_desc.alternate_setting = 0;
    interface_desc.interface_class = T::kClass;
    interface_desc.interface_subclass = T::kSubclass;
    interface_desc.interface_protocol = T::kProtocol;
    interface_desc.endpoints.push_back(bulk_in);
    interface_desc.endpoints.push_back(bulk_out);

    config_desc_.interfaces.push_back(interface_desc);
  }

  void Open(const OpenCallback& callback) override {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, make_scoped_refptr(
                                            new MockUsbDeviceHandle<T>(this))));
  }

  const UsbConfigDescriptor* GetConfiguration() override {
    return T::kConfigured ? &config_desc_ : nullptr;
  }

  bool Close(scoped_refptr<UsbDeviceHandle> handle) override {
    return true;
  }

  std::set<int> claimed_interfaces_;

 protected:
  virtual ~MockUsbDevice() {}

 private:
  UsbConfigDescriptor config_desc_;
};

class MockUsbService : public UsbService {
 public:
  MockUsbService() {
    devices_.push_back(new MockUsbDevice<AndroidTraits>());
  }

  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void GetDevices(const GetDevicesCallback& callback) override {
    callback.Run(devices_);
  }

  std::vector<scoped_refptr<UsbDevice> > devices_;
};

class MockBreakingUsbService : public MockUsbService {
 public:
  MockBreakingUsbService() {
    devices_.clear();
    devices_.push_back(new MockUsbDevice<BreakingAndroidTraits>());
  }
};

class MockNoConfigUsbService : public MockUsbService {
 public:
  MockNoConfigUsbService() {
    devices_.push_back(new MockUsbDevice<NoConfigTraits>());
  }
};

class MockUsbServiceForCheckingTraits : public MockUsbService {
 public:
  MockUsbServiceForCheckingTraits() : step_(0) {}

  void GetDevices(const GetDevicesCallback& callback) override {
    std::vector<scoped_refptr<UsbDevice>> devices;
    // This switch should be kept in sync with
    // AndroidUsbBrowserTest::DeviceCountChanged.
    switch (step_) {
      case 0:
        // No devices.
        break;
      case 1:
        // Android device.
        devices.push_back(new MockUsbDevice<AndroidTraits>());
        break;
      case 2:
        // Android and non-android device.
        devices.push_back(new MockUsbDevice<AndroidTraits>());
        devices.push_back(new MockUsbDevice<NonAndroidTraits>());
        break;
      case 3:
        // Non-android device.
        devices.push_back(new MockUsbDevice<NonAndroidTraits>());
        break;
    }
    step_++;
    callback.Run(devices);
  }

 private:
  int step_;
};

class DevToolsAndroidBridgeWarmUp
    : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  DevToolsAndroidBridgeWarmUp(base::Closure closure,
                              DevToolsAndroidBridge* adb_bridge)
      : closure_(closure), adb_bridge_(adb_bridge) {}

  void DeviceCountChanged(int count) override {
    adb_bridge_->RemoveDeviceCountListener(this);
    closure_.Run();
  }

  base::Closure closure_;
  DevToolsAndroidBridge* adb_bridge_;
};

class AndroidUsbDiscoveryTest : public InProcessBrowserTest {
 protected:
  AndroidUsbDiscoveryTest()
      : scheduler_invoked_(0) {
  }
  void SetUpOnMainThread() override {
    mock_usb_service_.reset(CreateMockService());

    adb_bridge_ =
        DevToolsAndroidBridge::Factory::GetForProfile(browser()->profile());
    DCHECK(adb_bridge_);
    adb_bridge_->set_task_scheduler_for_test(base::Bind(
        &AndroidUsbDiscoveryTest::ScheduleDeviceCountRequest, this));

    scoped_refptr<UsbDeviceProvider> provider =
        new UsbDeviceProvider(browser()->profile());

    AndroidDeviceManager::DeviceProviders providers;
    providers.push_back(provider);
    adb_bridge_->set_device_providers_for_test(providers);
    runner_ = new content::MessageLoopRunner;
  }

  void ScheduleDeviceCountRequest(const base::Closure& request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    scheduler_invoked_++;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, request);
  }

  virtual MockUsbService* CreateMockService() { return new MockUsbService(); }

  scoped_refptr<content::MessageLoopRunner> runner_;
  scoped_ptr<MockUsbService> mock_usb_service_;
  DevToolsAndroidBridge* adb_bridge_;
  int scheduler_invoked_;
};

class AndroidUsbCountTest : public AndroidUsbDiscoveryTest {
 protected:
  void SetUpOnMainThread() override {
    AndroidUsbDiscoveryTest::SetUpOnMainThread();
    DevToolsAndroidBridgeWarmUp warmup(runner_->QuitClosure(), adb_bridge_);
    adb_bridge_->AddDeviceCountListener(&warmup);
    runner_->Run();
    runner_ = new content::MessageLoopRunner;
  }
};

class AndroidUsbTraitsTest : public AndroidUsbDiscoveryTest {
 protected:
  MockUsbService* CreateMockService() override {
    return new MockUsbServiceForCheckingTraits();
  }
};

class AndroidBreakingUsbTest : public AndroidUsbDiscoveryTest {
 protected:
  MockUsbService* CreateMockService() override {
    return new MockBreakingUsbService();
  }
};

class AndroidNoConfigUsbTest : public AndroidUsbDiscoveryTest {
 protected:
  MockUsbService* CreateMockService() override {
    return new MockNoConfigUsbService();
  }
};

class MockListListener : public DevToolsAndroidBridge::DeviceListListener {
 public:
  MockListListener(DevToolsAndroidBridge* adb_bridge,
                   const base::Closure& callback)
      : adb_bridge_(adb_bridge),
        callback_(callback) {
  }

  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override {
    if (devices.size() > 0) {
      for (const auto& device : devices) {
        if (device->is_connected()) {
          ASSERT_EQ(kDeviceModel, device->model());
          ASSERT_EQ(kDeviceSerial, device->serial());
          adb_bridge_->RemoveDeviceListListener(this);
          callback_.Run();
          break;
        }
      }
    }
  }

  DevToolsAndroidBridge* adb_bridge_;
  base::Closure callback_;
};

class MockCountListener : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  explicit MockCountListener(DevToolsAndroidBridge* adb_bridge)
      : adb_bridge_(adb_bridge), invoked_(0) {}

  void DeviceCountChanged(int count) override {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    Shutdown();
  }

  void Shutdown() { base::MessageLoop::current()->Quit(); }

  DevToolsAndroidBridge* adb_bridge_;
  int invoked_;
};

class MockCountListenerWithReAdd : public MockCountListener {
 public:
  explicit MockCountListenerWithReAdd(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        readd_count_(2) {
  }

  void DeviceCountChanged(int count) override {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    if (readd_count_ > 0) {
      readd_count_--;
      adb_bridge_->AddDeviceCountListener(this);
      adb_bridge_->RemoveDeviceCountListener(this);
      adb_bridge_->AddDeviceCountListener(this);
    } else {
      Shutdown();
    }
  }

  int readd_count_;
};

class MockCountListenerWithReAddWhileQueued : public MockCountListener {
 public:
  MockCountListenerWithReAddWhileQueued(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        readded_(false) {
  }

  void DeviceCountChanged(int count) override {
    ++invoked_;
    if (!readded_) {
      readded_ = true;
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&MockCountListenerWithReAddWhileQueued::ReAdd,
                     base::Unretained(this)));
    } else {
      adb_bridge_->RemoveDeviceCountListener(this);
      Shutdown();
    }
  }

  void ReAdd() {
    adb_bridge_->RemoveDeviceCountListener(this);
    adb_bridge_->AddDeviceCountListener(this);
  }

  bool readded_;
};

class MockCountListenerForCheckingTraits : public MockCountListener {
 public:
  MockCountListenerForCheckingTraits(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        step_(0) {
  }
  void DeviceCountChanged(int count) override {
    switch (step_) {
      case 0:
        // Check for 0 devices when no devices present.
        EXPECT_EQ(0, count);
        break;
      case 1:
        // Check for 1 device when only android device present.
        EXPECT_EQ(1, count);
        break;
      case 2:
        // Check for 1 device when android and non-android devices present.
        EXPECT_EQ(1, count);
        break;
      case 3:
        // Check for 0 devices when only non-android devices present.
        EXPECT_EQ(0, count);
        adb_bridge_->RemoveDeviceCountListener(this);
        Shutdown();
        break;
      default:
        EXPECT_TRUE(false) << "Unknown step " << step_;
    }
    step_++;
  }

  int step_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AndroidUsbDiscoveryTest, TestDeviceDiscovery) {
  MockListListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceListListener(&listener);
  runner_->Run();
}

IN_PROC_BROWSER_TEST_F(AndroidBreakingUsbTest, TestDeviceBreaking) {
  MockListListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceListListener(&listener);
  runner_->Run();
}

IN_PROC_BROWSER_TEST_F(AndroidNoConfigUsbTest, TestDeviceNoConfig) {
  MockListListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceListListener(&listener);
  runner_->Run();
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveInCallback) {
  MockCountListener listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(1, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
  EXPECT_TRUE(base::MessageLoop::current()->IsIdleForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddInCallback) {
  MockCountListenerWithReAdd listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(3, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
  EXPECT_TRUE(base::MessageLoop::current()->IsIdleForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddOnStart) {
  MockCountListener listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  adb_bridge_->RemoveDeviceCountListener(&listener);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(1, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
  EXPECT_TRUE(base::MessageLoop::current()->IsIdleForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddWhileQueued) {
  MockCountListenerWithReAddWhileQueued listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(2, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
  EXPECT_TRUE(base::MessageLoop::current()->IsIdleForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidUsbTraitsTest, TestDeviceCounting) {
  MockCountListenerForCheckingTraits listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
}
