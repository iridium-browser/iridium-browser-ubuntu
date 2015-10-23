// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class ServiceImpl : public sample::Service {
 public:
  ServiceImpl() {}
  ~ServiceImpl() override {}

 private:
  // sample::Service implementation
  void Frobinate(sample::FooPtr foo,
                 BazOptions options,
                 sample::PortPtr port,
                 const FrobinateCallback& callback) override {
    callback.Run(1);
  }
  void GetPort(InterfaceRequest<sample::Port> port) override {}
};

class IntegerAccessorImpl : public sample::IntegerAccessor {
 public:
  IntegerAccessorImpl() {}
  ~IntegerAccessorImpl() override {}

 private:
  // sample::IntegerAccessor implementation.
  void GetInteger(const GetIntegerCallback& callback) override {
    callback.Run(1, sample::ENUM_VALUE);
  }
  void SetInteger(int64_t data, sample::Enum type) override {}
};

class BindingTest : public testing::Test {
 public:
  BindingTest() {}
  ~BindingTest() override {}

 protected:
  Environment env_;
  RunLoop loop_;
};

// Tests that destroying a mojo::Binding closes the bound message pipe handle.
TEST_F(BindingTest, DestroyClosesMessagePipe) {
  bool encountered_error = false;
  ServiceImpl impl;
  sample::ServicePtr ptr;
  auto request = GetProxy(&ptr);
  ptr.set_connection_error_handler(
      [&encountered_error]() { encountered_error = true; });
  bool called = false;
  auto called_cb = [&called](int32_t result) { called = true; };
  {
    Binding<sample::Service> binding(&impl, request.Pass());
    ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                   called_cb);
    loop_.RunUntilIdle();
    EXPECT_TRUE(called);
    EXPECT_FALSE(encountered_error);
  }
  // Now that the Binding is out of scope we should detect an error on the other
  // end of the pipe.
  loop_.RunUntilIdle();
  EXPECT_TRUE(encountered_error);

  // And calls should fail.
  called = false;
  ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                 called_cb);
  loop_.RunUntilIdle();
  EXPECT_FALSE(called);
}

// Tests that explicitly calling Unbind followed by rebinding works.
TEST_F(BindingTest, Unbind) {
  ServiceImpl impl;
  sample::ServicePtr ptr;
  Binding<sample::Service> binding(&impl, GetProxy(&ptr));

  bool called = false;
  auto called_cb = [&called](int32_t result) { called = true; };
  ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                 called_cb);
  loop_.RunUntilIdle();
  EXPECT_TRUE(called);

  called = false;
  auto request = binding.Unbind();
  EXPECT_FALSE(binding.is_bound());
  // All calls should fail when not bound...
  ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                 called_cb);
  loop_.RunUntilIdle();
  EXPECT_FALSE(called);

  called = false;
  binding.Bind(request.Pass());
  EXPECT_TRUE(binding.is_bound());
  // ...and should succeed again when the rebound.
  ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                 called_cb);
  loop_.RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(BindingTest, SetInterfacePtrVersion) {
  IntegerAccessorImpl impl;
  sample::IntegerAccessorPtr ptr;
  Binding<sample::IntegerAccessor> binding(&impl, &ptr);
  EXPECT_EQ(3u, ptr.version());
}

}  // namespace
}  // mojo
