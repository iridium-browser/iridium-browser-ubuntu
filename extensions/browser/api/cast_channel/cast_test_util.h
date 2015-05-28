// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_TEST_UTIL_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_TEST_UTIL_H_

#include <string>

#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/api/cast_channel/cast_transport.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

extern const char kTestExtensionId[];

class MockCastTransport
    : public extensions::core_api::cast_channel::CastTransport {
 public:
  MockCastTransport();
  ~MockCastTransport() override;

  void SetReadDelegate(scoped_ptr<CastTransport::Delegate> delegate) override;

  MOCK_METHOD2(
      SendMessage,
      void(const extensions::core_api::cast_channel::CastMessage& message,
           const net::CompletionCallback& callback));

  MOCK_METHOD0(Start, void(void));

  // Gets the read delegate that is currently active for this transport.
  CastTransport::Delegate* current_delegate() const;

 private:
  scoped_ptr<CastTransport::Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockCastTransport);
};

class MockCastTransportDelegate : public CastTransport::Delegate {
 public:
  MockCastTransportDelegate();
  ~MockCastTransportDelegate() override;

  MOCK_METHOD1(OnError, void(ChannelError error));
  MOCK_METHOD1(OnMessage, void(const CastMessage& message));
  MOCK_METHOD0(Start, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastTransportDelegate);
};

class MockCastSocket : public CastSocket {
 public:
  MockCastSocket();
  ~MockCastSocket() override;

  // Mockable version of Connect. Accepts a bare pointer to a mock object.
  // (GMock won't compile with scoped_ptr method parameters.)
  MOCK_METHOD2(ConnectRawPtr,
               void(CastTransport::Delegate* delegate,
                    base::Callback<void(ChannelError)> callback));

  // Proxy for ConnectRawPtr. Unpacks scoped_ptr into a GMock-friendly bare
  // ptr.
  void Connect(scoped_ptr<CastTransport::Delegate> delegate,
               base::Callback<void(ChannelError)> callback) override {
    delegate_ = delegate.Pass();
    ConnectRawPtr(delegate_.get(), callback);
  }

  MOCK_METHOD1(Close, void(const net::CompletionCallback& callback));
  MOCK_CONST_METHOD0(ip_endpoint, const net::IPEndPoint&());
  MOCK_CONST_METHOD0(id, int());
  MOCK_METHOD1(set_id, void(int id));
  MOCK_CONST_METHOD0(channel_auth, ChannelAuthType());
  MOCK_CONST_METHOD0(cast_url, std::string());
  MOCK_CONST_METHOD0(ready_state, ReadyState());
  MOCK_CONST_METHOD0(error_state, ChannelError());
  MOCK_CONST_METHOD0(keep_alive, bool(void));
  MOCK_METHOD1(SetErrorState, void(ChannelError error_state));

  CastTransport* transport() const override { return mock_transport_.get(); }

  MockCastTransport* mock_transport() const { return mock_transport_.get(); }

 private:
  scoped_ptr<MockCastTransport> mock_transport_;
  scoped_ptr<CastTransport::Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockCastSocket);
};

// Creates the IPEndpoint 192.168.1.1.
net::IPEndPoint CreateIPEndPointForTest();

// Checks if two proto messages are the same.
// From
// third_party/cacheinvalidation/overrides/google/cacheinvalidation/deps/gmock.h
// TODO(kmarshall): promote to a shared testing library.
MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

ACTION_TEMPLATE(RunCompletionCallback,
                HAS_1_TEMPLATE_PARAMS(int, cb_idx),
                AND_1_VALUE_PARAMS(rv)) {
  testing::get<cb_idx>(args).Run(rv);
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_TEST_UTIL_H_
