// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "device/serial/data_source_sender.h"
#include "device/serial/data_stream.mojom.h"
#include "extensions/renderer/api_test_base.h"
#include "gin/dictionary.h"
#include "gin/wrappable.h"
#include "grit/extensions_renderer_resources.h"

namespace extensions {

class DataReceiverFactory : public gin::Wrappable<DataReceiverFactory> {
 public:
  using Callback = base::Callback<void(
      mojo::InterfaceRequest<device::serial::DataSource>,
      mojo::InterfacePtr<device::serial::DataSourceClient>)>;
  static gin::Handle<DataReceiverFactory> Create(v8::Isolate* isolate,
                                                 const Callback& callback) {
    return gin::CreateHandle(isolate,
                             new DataReceiverFactory(callback, isolate));
  }

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return Wrappable<DataReceiverFactory>::GetObjectTemplateBuilder(isolate)
        .SetMethod("create", &DataReceiverFactory::CreateReceiver);
  }

  gin::Dictionary CreateReceiver() {
    mojo::InterfacePtr<device::serial::DataSource> sink;
    mojo::InterfacePtr<device::serial::DataSourceClient> client;
    mojo::InterfaceRequest<device::serial::DataSourceClient> client_request =
        mojo::GetProxy(&client);
    callback_.Run(mojo::GetProxy(&sink), client.Pass());

    gin::Dictionary result = gin::Dictionary::CreateEmpty(isolate_);
    result.Set("source", sink.PassInterface().PassHandle().release());
    result.Set("client", client_request.PassMessagePipe().release());
    return result;
  }

  static gin::WrapperInfo kWrapperInfo;

 private:
  DataReceiverFactory(const Callback& callback, v8::Isolate* isolate)
      : callback_(callback), isolate_(isolate) {}

  base::Callback<void(mojo::InterfaceRequest<device::serial::DataSource>,
                      mojo::InterfacePtr<device::serial::DataSourceClient>)>
      callback_;
  v8::Isolate* isolate_;
};

gin::WrapperInfo DataReceiverFactory::kWrapperInfo = {gin::kEmbedderNativeGin};

// Runs tests defined in extensions/test/data/data_receiver_unittest.js
class DataReceiverTest : public ApiTestBase {
 public:
  DataReceiverTest() {}

  void SetUp() override {
    ApiTestBase::SetUp();
    gin::ModuleRegistry::From(env()->context()->v8_context())
        ->AddBuiltinModule(env()->isolate(),
                           "device/serial/data_receiver_test_factory",
                           DataReceiverFactory::Create(
                               env()->isolate(),
                               base::Bind(&DataReceiverTest::CreateDataSource,
                                          base::Unretained(this))).ToV8());
  }

  void TearDown() override {
    if (sender_.get()) {
      sender_->ShutDown();
      sender_ = NULL;
    }
    ApiTestBase::TearDown();
  }

  std::queue<int32_t> error_to_send_;
  std::queue<std::string> data_to_send_;

 private:
  void CreateDataSource(
      mojo::InterfaceRequest<device::serial::DataSource> request,
      mojo::InterfacePtr<device::serial::DataSourceClient> client) {
    sender_ = new device::DataSourceSender(
        request.Pass(), client.Pass(),
        base::Bind(&DataReceiverTest::ReadyToSend, base::Unretained(this)),
        base::Bind(base::DoNothing));
  }

  void ReadyToSend(scoped_ptr<device::WritableBuffer> buffer) {
    if (data_to_send_.empty() && error_to_send_.empty())
      return;

    std::string data;
    int32_t error = 0;
    if (!data_to_send_.empty()) {
      data = data_to_send_.front();
      data_to_send_.pop();
    }
    if (!error_to_send_.empty()) {
      error = error_to_send_.front();
      error_to_send_.pop();
    }
    DCHECK(buffer->GetSize() >= static_cast<uint32_t>(data.size()));
    memcpy(buffer->GetData(), data.c_str(), data.size());
    if (error)
      buffer->DoneWithError(data.size(), error);
    else
      buffer->Done(data.size());
  }

  scoped_refptr<device::DataSourceSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(DataReceiverTest);
};

TEST_F(DataReceiverTest, Receive) {
  data_to_send_.push("a");
  RunTest("data_receiver_unittest.js", "testReceive");
}

TEST_F(DataReceiverTest, ReceiveError) {
  error_to_send_.push(1);
  RunTest("data_receiver_unittest.js", "testReceiveError");
}

TEST_F(DataReceiverTest, ReceiveDataAndError) {
  data_to_send_.push("a");
  data_to_send_.push("b");
  error_to_send_.push(1);
  RunTest("data_receiver_unittest.js", "testReceiveDataAndError");
}

TEST_F(DataReceiverTest, ReceiveErrorThenData) {
  data_to_send_.push("");
  data_to_send_.push("a");
  error_to_send_.push(1);
  RunTest("data_receiver_unittest.js", "testReceiveErrorThenData");
}

TEST_F(DataReceiverTest, ReceiveBeforeAndAfterSerialization) {
  data_to_send_.push("a");
  data_to_send_.push("b");
  RunTest("data_receiver_unittest.js",
          "testReceiveBeforeAndAfterSerialization");
}

TEST_F(DataReceiverTest, ReceiveErrorSerialization) {
  error_to_send_.push(1);
  error_to_send_.push(3);
  RunTest("data_receiver_unittest.js", "testReceiveErrorSerialization");
}

TEST_F(DataReceiverTest, ReceiveDataAndErrorSerialization) {
  data_to_send_.push("a");
  data_to_send_.push("b");
  error_to_send_.push(1);
  error_to_send_.push(3);
  RunTest("data_receiver_unittest.js", "testReceiveDataAndErrorSerialization");
}

TEST_F(DataReceiverTest, SerializeDuringReceive) {
  data_to_send_.push("a");
  RunTest("data_receiver_unittest.js", "testSerializeDuringReceive");
}

TEST_F(DataReceiverTest, SerializeAfterClose) {
  data_to_send_.push("a");
  RunTest("data_receiver_unittest.js", "testSerializeAfterClose");
}

}  // namespace extensions
