// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_api.h"
#include "extensions/common/test_util.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothSocket;
using device::BluetoothUUID;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothSocket;
using extensions::Extension;
using extensions::ResultCatcher;

namespace api = extensions::api;

namespace {

class BluetoothSocketApiTest : public extensions::ShellApiTest {
 public:
  BluetoothSocketApiTest() {}

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    empty_extension_ = extensions::test_util::CreateEmptyExtension();
    SetUpMockAdapter();
  }

  void SetUpMockAdapter() {
    // The browser will clean this up when it is torn down.
    mock_adapter_ = new testing::StrictMock<MockBluetoothAdapter>();
    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    mock_device1_.reset(
        new testing::NiceMock<MockBluetoothDevice>(mock_adapter_.get(),
                                                   0,
                                                   "d1",
                                                   "11:12:13:14:15:16",
                                                   true /* paired */,
                                                   false /* connected */));
    mock_device2_.reset(
        new testing::NiceMock<MockBluetoothDevice>(mock_adapter_.get(),
                                                   0,
                                                   "d2",
                                                   "21:22:23:24:25:26",
                                                   true /* paired */,
                                                   false /* connected */));
  }

 protected:
  scoped_refptr<testing::StrictMock<MockBluetoothAdapter> > mock_adapter_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > mock_device1_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > mock_device2_;

 private:
  scoped_refptr<Extension> empty_extension_;
};

// testing::InvokeArgument<N> does not work with base::Callback, fortunately
// gmock makes it simple to create action templates that do for the various
// possible numbers of arguments.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  ::std::tr1::get<k>(args).Run();
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  ::std::tr1::get<k>(args).Run(p0);
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  ::std::tr1::get<k>(args).Run(p0, p1);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BluetoothSocketApiTest, Connect) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  // Return the right mock device object for the address used by the test,
  // return NULL for the "Device not found" test.
  EXPECT_CALL(*mock_adapter_.get(), GetDevice(mock_device1_->GetAddress()))
      .WillRepeatedly(testing::Return(mock_device1_.get()));
  EXPECT_CALL(*mock_adapter_.get(), GetDevice(std::string("aa:aa:aa:aa:aa:aa")))
      .WillOnce(testing::Return(static_cast<BluetoothDevice*>(NULL)));

  // Return a mock socket object as a successful result to the connect() call.
  BluetoothUUID service_uuid("8e3ad063-db38-4289-aa8f-b30e4223cf40");
  scoped_refptr<testing::StrictMock<MockBluetoothSocket> > mock_socket
      = new testing::StrictMock<MockBluetoothSocket>();
  EXPECT_CALL(*mock_device1_,
              ConnectToService(service_uuid, testing::_, testing::_))
      .WillOnce(InvokeCallbackArgument<1>(mock_socket));

  // Since the socket is unpaused, expect a call to Receive() from the socket
  // dispatcher. Since there is no data, this will not call its callback.
  EXPECT_CALL(*mock_socket.get(), Receive(testing::_, testing::_, testing::_));

  // The test also cleans up by calling Disconnect and Close.
  EXPECT_CALL(*mock_socket.get(), Disconnect(testing::_))
      .WillOnce(InvokeCallbackArgument<0>());
  EXPECT_CALL(*mock_socket.get(), Close());

  // Run the test.
  ExtensionTestMessageListener listener("ready", true);
  scoped_refptr<const Extension> extension(
      LoadApp("api_test/bluetooth_socket/connect"));
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#if defined(_LIBCPP_VERSION)
// This test fails in libc++ builds, see http://crbug.com/392205.
#define MAYBE_Listen DISABLED_Listen
#else
#define MAYBE_Listen Listen
#endif
IN_PROC_BROWSER_TEST_F(BluetoothSocketApiTest, MAYBE_Listen) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  // Return a mock socket object as a successful result to the create service
  // call.
  BluetoothUUID service_uuid("2de497f9-ab28-49db-b6d2-066ea69f1737");
  scoped_refptr<testing::StrictMock<MockBluetoothSocket> > mock_server_socket
      = new testing::StrictMock<MockBluetoothSocket>();
  BluetoothAdapter::ServiceOptions service_options;
  service_options.name.reset(new std::string("MyServiceName"));
  EXPECT_CALL(
      *mock_adapter_.get(),
      CreateRfcommService(
          service_uuid,
          testing::Field(&BluetoothAdapter::ServiceOptions::name,
                         testing::Pointee(testing::Eq("MyServiceName"))),
          testing::_,
          testing::_)).WillOnce(InvokeCallbackArgument<2>(mock_server_socket));

  // Since the socket is unpaused, expect a call to Accept() from the socket
  // dispatcher. We'll immediately send back another mock socket to represent
  // the client API. Further calls will return no data and behave as if
  // pending.
  scoped_refptr<testing::StrictMock<MockBluetoothSocket> > mock_client_socket
      = new testing::StrictMock<MockBluetoothSocket>();
  EXPECT_CALL(*mock_server_socket.get(), Accept(testing::_, testing::_))
      .Times(2)
      .WillOnce(
           InvokeCallbackArgument<0>(mock_device1_.get(), mock_client_socket))
      .WillOnce(testing::Return());

  // Run the test, it sends a ready signal once it's ready for us to dispatch
  // a client connection to it.
  ExtensionTestMessageListener socket_listening("ready", true);
  scoped_refptr<const Extension> extension(
      LoadApp("api_test/bluetooth_socket/listen"));
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(socket_listening.WaitUntilSatisfied());

  // Connection events are dispatched using a couple of PostTask to the UI
  // thread. Waiting until idle ensures the event is dispatched to the
  // receiver(s).
  base::RunLoop().RunUntilIdle();
  ExtensionTestMessageListener listener("ready", true);
  socket_listening.Reply("go");

  // Second stage of tests checks for error conditions, and will clean up
  // the existing server and client sockets.
  EXPECT_CALL(*mock_server_socket.get(), Disconnect(testing::_))
      .WillOnce(InvokeCallbackArgument<0>());
  EXPECT_CALL(*mock_server_socket.get(), Close());

  EXPECT_CALL(*mock_client_socket.get(), Disconnect(testing::_))
      .WillOnce(InvokeCallbackArgument<0>());
  EXPECT_CALL(*mock_client_socket.get(), Close());

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothSocketApiTest, PermissionDenied) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  // Run the test.
  scoped_refptr<const Extension> extension(
      LoadApp("api_test/bluetooth_socket/permission_denied"));
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
