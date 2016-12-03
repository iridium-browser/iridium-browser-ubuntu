// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "device/core/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/public/interfaces/device_manager.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

}  //  namespace

class UsbChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  UsbChooserControllerTest() {}
  ~UsbChooserControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mojo::Array<device::usb::DeviceFilterPtr> device_filters;
    device::usb::ChooserService::GetPermissionCallback callback;
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
    usb_chooser_controller_.reset(new UsbChooserController(
        main_rfh(), std::move(device_filters), main_rfh(), callback));
  }

 protected:
  scoped_refptr<device::MockUsbDevice> CreateMockUsbDevice(
      const std::string& product_string,
      const std::string& serial_number) {
    scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
        0, 1, "Google", product_string, serial_number));
    std::unique_ptr<device::WebUsbAllowedOrigins> webusb_allowed_origins(
        new device::WebUsbAllowedOrigins());
    webusb_allowed_origins->origins.push_back(GURL(kDefaultTestUrl));
    device->set_webusb_allowed_origins(std::move(webusb_allowed_origins));
    return device;
  }

  device::MockDeviceClient device_client_;
  std::unique_ptr<UsbChooserController> usb_chooser_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbChooserControllerTest);
};

TEST_F(UsbChooserControllerTest, AddDevice) {
  scoped_refptr<device::MockUsbDevice> device_a =
      CreateMockUsbDevice("a", "001");
  device_client_.usb_service()->AddDevice(device_a);
  EXPECT_EQ(1u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(0));

  scoped_refptr<device::MockUsbDevice> device_b =
      CreateMockUsbDevice("b", "002");
  device_client_.usb_service()->AddDevice(device_b);
  EXPECT_EQ(2u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("b"), usb_chooser_controller_->GetOption(1));

  scoped_refptr<device::MockUsbDevice> device_c =
      CreateMockUsbDevice("c", "003");
  device_client_.usb_service()->AddDevice(device_c);
  EXPECT_EQ(3u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("c"), usb_chooser_controller_->GetOption(2));
}

TEST_F(UsbChooserControllerTest, RemoveDevice) {
  scoped_refptr<device::MockUsbDevice> device_a =
      CreateMockUsbDevice("a", "001");
  device_client_.usb_service()->AddDevice(device_a);
  scoped_refptr<device::MockUsbDevice> device_b =
      CreateMockUsbDevice("b", "002");
  device_client_.usb_service()->AddDevice(device_b);
  scoped_refptr<device::MockUsbDevice> device_c =
      CreateMockUsbDevice("c", "003");
  device_client_.usb_service()->AddDevice(device_c);

  device_client_.usb_service()->RemoveDevice(device_b);
  EXPECT_EQ(2u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), usb_chooser_controller_->GetOption(1));

  // Remove a non-existent device, the number of devices should not change.
  scoped_refptr<device::MockUsbDevice> device_non_existent =
      CreateMockUsbDevice("d", "001");
  device_client_.usb_service()->RemoveDevice(device_non_existent);
  EXPECT_EQ(2u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), usb_chooser_controller_->GetOption(1));

  device_client_.usb_service()->RemoveDevice(device_a);
  EXPECT_EQ(1u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("c"), usb_chooser_controller_->GetOption(0));

  device_client_.usb_service()->RemoveDevice(device_c);
  EXPECT_EQ(0u, usb_chooser_controller_->NumOptions());
}

TEST_F(UsbChooserControllerTest, AddAndRemoveDeviceWithSameName) {
  scoped_refptr<device::MockUsbDevice> device_a_1 =
      CreateMockUsbDevice("a", "001");
  device_client_.usb_service()->AddDevice(device_a_1);
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(0));
  scoped_refptr<device::MockUsbDevice> device_b =
      CreateMockUsbDevice("b", "002");
  device_client_.usb_service()->AddDevice(device_b);
  scoped_refptr<device::MockUsbDevice> device_a_2 =
      CreateMockUsbDevice("a", "002");
  device_client_.usb_service()->AddDevice(device_a_2);
  EXPECT_EQ(base::ASCIIToUTF16("a (001)"),
            usb_chooser_controller_->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("b"), usb_chooser_controller_->GetOption(1));
  EXPECT_EQ(base::ASCIIToUTF16("a (002)"),
            usb_chooser_controller_->GetOption(2));

  device_client_.usb_service()->RemoveDevice(device_a_1);
  EXPECT_EQ(base::ASCIIToUTF16("b"), usb_chooser_controller_->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(1));
}
