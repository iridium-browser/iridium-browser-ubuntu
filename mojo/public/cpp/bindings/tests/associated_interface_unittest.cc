// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_associated_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using mojo::internal::MultiplexRouter;

class IntegerSenderImpl : public IntegerSender {
 public:
  explicit IntegerSenderImpl(AssociatedInterfaceRequest<IntegerSender> request)
      : binding_(this, std::move(request)) {}

  ~IntegerSenderImpl() override {}

  void set_notify_send_method_called(
      const base::Callback<void(int32_t)>& callback) {
    notify_send_method_called_ = callback;
  }

  void Echo(int32_t value, const EchoCallback& callback) override {
    callback.Run(value);
  }
  void Send(int32_t value) override { notify_send_method_called_.Run(value); }

  AssociatedBinding<IntegerSender>* binding() { return &binding_; }

  void set_connection_error_handler(const base::Closure& handler) {
    binding_.set_connection_error_handler(handler);
  }

 private:
  AssociatedBinding<IntegerSender> binding_;
  base::Callback<void(int32_t)> notify_send_method_called_;
};

class IntegerSenderConnectionImpl : public IntegerSenderConnection {
 public:
  explicit IntegerSenderConnectionImpl(
      InterfaceRequest<IntegerSenderConnection> request)
      : binding_(this, std::move(request)) {}

  ~IntegerSenderConnectionImpl() override {}

  void GetSender(AssociatedInterfaceRequest<IntegerSender> sender) override {
    IntegerSenderImpl* sender_impl = new IntegerSenderImpl(std::move(sender));
    sender_impl->set_connection_error_handler(
        base::Bind(&DeleteSender, sender_impl));
  }

  void AsyncGetSender(const AsyncGetSenderCallback& callback) override {
    AssociatedInterfaceRequest<IntegerSender> request;
    IntegerSenderAssociatedPtrInfo ptr_info;
    binding_.associated_group()->CreateAssociatedInterface(
        AssociatedGroup::WILL_PASS_PTR, &ptr_info, &request);
    GetSender(std::move(request));
    callback.Run(std::move(ptr_info));
  }

  Binding<IntegerSenderConnection>* binding() { return &binding_; }

 private:
  static void DeleteSender(IntegerSenderImpl* sender) { delete sender; }

  Binding<IntegerSenderConnection> binding_;
};

class AssociatedInterfaceTest : public testing::Test {
 public:
  AssociatedInterfaceTest() {}
  ~AssociatedInterfaceTest() override { base::RunLoop().RunUntilIdle(); }

  void PumpMessages() { base::RunLoop().RunUntilIdle(); }

  template <typename T>
  AssociatedInterfacePtrInfo<T> EmulatePassingAssociatedPtrInfo(
      AssociatedInterfacePtrInfo<T> ptr_info,
      scoped_refptr<MultiplexRouter> target) {
    ScopedInterfaceEndpointHandle handle = ptr_info.PassHandle();
    CHECK(!handle.is_local());
    return AssociatedInterfacePtrInfo<T>(
        target->CreateLocalEndpointHandle(handle.release()),
        ptr_info.version());
  }

  template <typename T>
  AssociatedInterfaceRequest<T> EmulatePassingAssociatedRequest(
      AssociatedInterfaceRequest<T> request,
      scoped_refptr<MultiplexRouter> target) {
    ScopedInterfaceEndpointHandle handle = request.PassHandle();
    CHECK(!handle.is_local());
    return MakeAssociatedRequest<T>(
        target->CreateLocalEndpointHandle(handle.release()));
  }

  // Okay to call from any thread.
  void QuitRunLoop(base::RunLoop* run_loop) {
    if (loop_.task_runner()->BelongsToCurrentThread()) {
      run_loop->Quit();
    } else {
      loop_.task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&AssociatedInterfaceTest::QuitRunLoop,
                     base::Unretained(this), base::Unretained(run_loop)));
    }
  }

 private:
  base::MessageLoop loop_;
};

void DoSetFlagAndRunClosure(bool* flag, const base::Closure& closure) {
  *flag = true;
  closure.Run();
}

void DoExpectValueSetFlagAndRunClosure(int32_t expected_value,
                                       bool* flag,
                                       const base::Closure& closure,
                                       int32_t value) {
  EXPECT_EQ(expected_value, value);
  DoSetFlagAndRunClosure(flag, closure);
}

base::Closure SetFlagAndRunClosure(bool* flag, const base::Closure& closure) {
  return base::Bind(&DoSetFlagAndRunClosure, flag, closure);
}

base::Callback<void(int32_t)> ExpectValueSetFlagAndRunClosure(
    int32_t expected_value,
    bool* flag,
    const base::Closure& closure) {
  return base::Bind(
      &DoExpectValueSetFlagAndRunClosure, expected_value, flag, closure);
}

TEST_F(AssociatedInterfaceTest, InterfacesAtBothEnds) {
  // Bind to the same pipe two associated interfaces, whose implementation lives
  // at different ends. Test that the two don't interfere with each other.

  MessagePipe pipe;
  scoped_refptr<MultiplexRouter> router0(new MultiplexRouter(
      true, std::move(pipe.handle0), base::ThreadTaskRunnerHandle::Get()));
  scoped_refptr<MultiplexRouter> router1(new MultiplexRouter(
      false, std::move(pipe.handle1), base::ThreadTaskRunnerHandle::Get()));

  AssociatedInterfaceRequest<IntegerSender> request;
  IntegerSenderAssociatedPtrInfo ptr_info;

  router0->CreateAssociatedGroup()->CreateAssociatedInterface(
      AssociatedGroup::WILL_PASS_PTR, &ptr_info, &request);
  ptr_info = EmulatePassingAssociatedPtrInfo(std::move(ptr_info), router1);

  IntegerSenderImpl impl0(std::move(request));
  AssociatedInterfacePtr<IntegerSender> ptr0;
  ptr0.Bind(std::move(ptr_info));

  router0->CreateAssociatedGroup()->CreateAssociatedInterface(
      AssociatedGroup::WILL_PASS_REQUEST, &ptr_info, &request);
  request = EmulatePassingAssociatedRequest(std::move(request), router1);

  IntegerSenderImpl impl1(std::move(request));
  AssociatedInterfacePtr<IntegerSender> ptr1;
  ptr1.Bind(std::move(ptr_info));

  base::RunLoop run_loop, run_loop2;
  bool ptr0_callback_run = false;
  ptr0->Echo(123, ExpectValueSetFlagAndRunClosure(123, &ptr0_callback_run,
                                                  run_loop.QuitClosure()));

  bool ptr1_callback_run = false;
  ptr1->Echo(456, ExpectValueSetFlagAndRunClosure(456, &ptr1_callback_run,
                                                  run_loop2.QuitClosure()));

  run_loop.Run();
  run_loop2.Run();
  EXPECT_TRUE(ptr0_callback_run);
  EXPECT_TRUE(ptr1_callback_run);

  bool ptr0_error_callback_run = false;
  base::RunLoop run_loop3;
  ptr0.set_connection_error_handler(
      SetFlagAndRunClosure(&ptr0_error_callback_run, run_loop3.QuitClosure()));

  impl0.binding()->Close();
  run_loop3.Run();
  EXPECT_TRUE(ptr0_error_callback_run);

  bool impl1_error_callback_run = false;
  base::RunLoop run_loop4;
  impl1.binding()->set_connection_error_handler(
      SetFlagAndRunClosure(&impl1_error_callback_run, run_loop4.QuitClosure()));

  ptr1.reset();
  run_loop4.Run();
  EXPECT_TRUE(impl1_error_callback_run);
}

class TestSender {
 public:
  TestSender()
      : sender_thread_("TestSender"),
        next_sender_(nullptr),
        max_value_to_send_(-1) {
    sender_thread_.Start();
  }

  // The following three methods are called on the corresponding sender thread.
  void SetUp(IntegerSenderAssociatedPtrInfo ptr_info,
             TestSender* next_sender,
             int32_t max_value_to_send) {
    CHECK(sender_thread_.task_runner()->BelongsToCurrentThread());

    ptr_.Bind(std::move(ptr_info));
    next_sender_ = next_sender ? next_sender : this;
    max_value_to_send_ = max_value_to_send;
  }

  void Send(int32_t value) {
    CHECK(sender_thread_.task_runner()->BelongsToCurrentThread());

    if (value > max_value_to_send_)
      return;

    ptr_->Send(value);

    next_sender_->sender_thread()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&TestSender::Send, base::Unretained(next_sender_), ++value));
  }

  void TearDown() {
    CHECK(sender_thread_.task_runner()->BelongsToCurrentThread());

    ptr_.reset();
  }

  base::Thread* sender_thread() { return &sender_thread_; }

 private:
  base::Thread sender_thread_;
  TestSender* next_sender_;
  int32_t max_value_to_send_;

  AssociatedInterfacePtr<IntegerSender> ptr_;
};

class TestReceiver {
 public:
  TestReceiver() : receiver_thread_("TestReceiver"), expected_calls_(0) {
    receiver_thread_.Start();
  }

  void SetUp(AssociatedInterfaceRequest<IntegerSender> request0,
             AssociatedInterfaceRequest<IntegerSender> request1,
             size_t expected_calls,
             const base::Closure& notify_finish) {
    CHECK(receiver_thread_.task_runner()->BelongsToCurrentThread());

    impl0_.reset(new IntegerSenderImpl(std::move(request0)));
    impl0_->set_notify_send_method_called(
        base::Bind(&TestReceiver::SendMethodCalled, base::Unretained(this)));
    impl1_.reset(new IntegerSenderImpl(std::move(request1)));
    impl1_->set_notify_send_method_called(
        base::Bind(&TestReceiver::SendMethodCalled, base::Unretained(this)));

    expected_calls_ = expected_calls;
    notify_finish_ = notify_finish;
  }

  void TearDown() {
    CHECK(receiver_thread_.task_runner()->BelongsToCurrentThread());

    impl0_.reset();
    impl1_.reset();
  }

  base::Thread* receiver_thread() { return &receiver_thread_; }
  const std::vector<int32_t>& values() const { return values_; }

 private:
  void SendMethodCalled(int32_t value) {
    values_.push_back(value);

    if (values_.size() >= expected_calls_)
      notify_finish_.Run();
  }

  base::Thread receiver_thread_;
  size_t expected_calls_;

  std::unique_ptr<IntegerSenderImpl> impl0_;
  std::unique_ptr<IntegerSenderImpl> impl1_;

  std::vector<int32_t> values_;

  base::Closure notify_finish_;
};

class NotificationCounter {
 public:
  NotificationCounter(size_t total_count, const base::Closure& notify_finish)
      : total_count_(total_count),
        current_count_(0),
        notify_finish_(notify_finish) {}

  ~NotificationCounter() {}

  // Okay to call from any thread.
  void OnGotNotification() {
    bool finshed = false;
    {
      base::AutoLock locker(lock_);
      CHECK_LT(current_count_, total_count_);
      current_count_++;
      finshed = current_count_ == total_count_;
    }

    if (finshed)
      notify_finish_.Run();
  }

 private:
  base::Lock lock_;
  const size_t total_count_;
  size_t current_count_;
  base::Closure notify_finish_;
};

TEST_F(AssociatedInterfaceTest, MultiThreadAccess) {
  // Set up four associated interfaces on a message pipe. Use the inteface
  // pointers on four threads in parallel; run the interface implementations on
  // two threads. Test that multi-threaded access works.

  const int32_t kMaxValue = 1000;
  MessagePipe pipe;
  scoped_refptr<MultiplexRouter> router0(new MultiplexRouter(
      true, std::move(pipe.handle0), base::ThreadTaskRunnerHandle::Get()));
  scoped_refptr<MultiplexRouter> router1(new MultiplexRouter(
      false, std::move(pipe.handle1), base::ThreadTaskRunnerHandle::Get()));

  AssociatedInterfaceRequest<IntegerSender> requests[4];
  IntegerSenderAssociatedPtrInfo ptr_infos[4];

  for (size_t i = 0; i < 4; ++i) {
    router0->CreateAssociatedGroup()->CreateAssociatedInterface(
        AssociatedGroup::WILL_PASS_PTR, &ptr_infos[i], &requests[i]);
    ptr_infos[i] =
        EmulatePassingAssociatedPtrInfo(std::move(ptr_infos[i]), router1);
  }

  TestSender senders[4];
  for (size_t i = 0; i < 4; ++i) {
    senders[i].sender_thread()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&TestSender::SetUp, base::Unretained(&senders[i]),
                              base::Passed(&ptr_infos[i]), nullptr,
                              kMaxValue * (i + 1) / 4));
  }

  base::RunLoop run_loop;
  TestReceiver receivers[2];
  NotificationCounter counter(
      2, base::Bind(&AssociatedInterfaceTest::QuitRunLoop,
                    base::Unretained(this), base::Unretained(&run_loop)));
  for (size_t i = 0; i < 2; ++i) {
    receivers[i].receiver_thread()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&TestReceiver::SetUp, base::Unretained(&receivers[i]),
                   base::Passed(&requests[2 * i]),
                   base::Passed(&requests[2 * i + 1]),
                   static_cast<size_t>(kMaxValue / 2),
                   base::Bind(&NotificationCounter::OnGotNotification,
                              base::Unretained(&counter))));
  }

  for (size_t i = 0; i < 4; ++i) {
    senders[i].sender_thread()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&TestSender::Send, base::Unretained(&senders[i]),
                              kMaxValue * i / 4 + 1));
  }

  run_loop.Run();

  for (size_t i = 0; i < 4; ++i) {
    base::RunLoop run_loop;
    senders[i].sender_thread()->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&TestSender::TearDown, base::Unretained(&senders[i])),
        base::Bind(&AssociatedInterfaceTest::QuitRunLoop,
                   base::Unretained(this), base::Unretained(&run_loop)));
    run_loop.Run();
  }

  for (size_t i = 0; i < 2; ++i) {
    base::RunLoop run_loop;
    receivers[i].receiver_thread()->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&TestReceiver::TearDown, base::Unretained(&receivers[i])),
        base::Bind(&AssociatedInterfaceTest::QuitRunLoop,
                   base::Unretained(this), base::Unretained(&run_loop)));
    run_loop.Run();
  }

  EXPECT_EQ(static_cast<size_t>(kMaxValue / 2), receivers[0].values().size());
  EXPECT_EQ(static_cast<size_t>(kMaxValue / 2), receivers[1].values().size());

  std::vector<int32_t> all_values;
  all_values.insert(all_values.end(), receivers[0].values().begin(),
                    receivers[0].values().end());
  all_values.insert(all_values.end(), receivers[1].values().begin(),
                    receivers[1].values().end());

  std::sort(all_values.begin(), all_values.end());
  for (size_t i = 0; i < all_values.size(); ++i)
    ASSERT_EQ(static_cast<int32_t>(i + 1), all_values[i]);
}

TEST_F(AssociatedInterfaceTest, FIFO) {
  // Set up four associated interfaces on a message pipe. Use the inteface
  // pointers on four threads; run the interface implementations on two threads.
  // Take turns to make calls using the four pointers. Test that FIFO-ness is
  // preserved.

  const int32_t kMaxValue = 100;
  MessagePipe pipe;
  scoped_refptr<MultiplexRouter> router0(new MultiplexRouter(
      true, std::move(pipe.handle0), base::ThreadTaskRunnerHandle::Get()));
  scoped_refptr<MultiplexRouter> router1(new MultiplexRouter(
      false, std::move(pipe.handle1), base::ThreadTaskRunnerHandle::Get()));

  AssociatedInterfaceRequest<IntegerSender> requests[4];
  IntegerSenderAssociatedPtrInfo ptr_infos[4];

  for (size_t i = 0; i < 4; ++i) {
    router0->CreateAssociatedGroup()->CreateAssociatedInterface(
        AssociatedGroup::WILL_PASS_PTR, &ptr_infos[i], &requests[i]);
    ptr_infos[i] =
        EmulatePassingAssociatedPtrInfo(std::move(ptr_infos[i]), router1);
  }

  TestSender senders[4];
  for (size_t i = 0; i < 4; ++i) {
    senders[i].sender_thread()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&TestSender::SetUp, base::Unretained(&senders[i]),
                   base::Passed(&ptr_infos[i]),
                   base::Unretained(&senders[(i + 1) % 4]), kMaxValue));
  }

  base::RunLoop run_loop;
  TestReceiver receivers[2];
  NotificationCounter counter(
      2, base::Bind(&AssociatedInterfaceTest::QuitRunLoop,
                    base::Unretained(this), base::Unretained(&run_loop)));
  for (size_t i = 0; i < 2; ++i) {
    receivers[i].receiver_thread()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&TestReceiver::SetUp, base::Unretained(&receivers[i]),
                   base::Passed(&requests[2 * i]),
                   base::Passed(&requests[2 * i + 1]),
                   static_cast<size_t>(kMaxValue / 2),
                   base::Bind(&NotificationCounter::OnGotNotification,
                              base::Unretained(&counter))));
  }

  senders[0].sender_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&TestSender::Send, base::Unretained(&senders[0]), 1));

  run_loop.Run();

  for (size_t i = 0; i < 4; ++i) {
    base::RunLoop run_loop;
    senders[i].sender_thread()->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&TestSender::TearDown, base::Unretained(&senders[i])),
        base::Bind(&AssociatedInterfaceTest::QuitRunLoop,
                   base::Unretained(this), base::Unretained(&run_loop)));
    run_loop.Run();
  }

  for (size_t i = 0; i < 2; ++i) {
    base::RunLoop run_loop;
    receivers[i].receiver_thread()->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&TestReceiver::TearDown, base::Unretained(&receivers[i])),
        base::Bind(&AssociatedInterfaceTest::QuitRunLoop,
                   base::Unretained(this), base::Unretained(&run_loop)));
    run_loop.Run();
  }

  EXPECT_EQ(static_cast<size_t>(kMaxValue / 2), receivers[0].values().size());
  EXPECT_EQ(static_cast<size_t>(kMaxValue / 2), receivers[1].values().size());

  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 1; j < receivers[i].values().size(); ++j)
      EXPECT_LT(receivers[i].values()[j - 1], receivers[i].values()[j]);
  }
}

void CaptureInt32(int32_t* storage,
                  const base::Closure& closure,
                  int32_t value) {
  *storage = value;
  closure.Run();
}

void CaptureSenderPtrInfo(IntegerSenderAssociatedPtr* storage,
                          const base::Closure& closure,
                          IntegerSenderAssociatedPtrInfo info) {
  storage->Bind(std::move(info));
  closure.Run();
}

TEST_F(AssociatedInterfaceTest, PassAssociatedInterfaces) {
  IntegerSenderConnectionPtr connection_ptr;
  IntegerSenderConnectionImpl connection(GetProxy(&connection_ptr));

  IntegerSenderAssociatedPtr sender0;
  connection_ptr->GetSender(
      GetProxy(&sender0, connection_ptr.associated_group()));

  int32_t echoed_value = 0;
  base::RunLoop run_loop;
  sender0->Echo(123, base::Bind(&CaptureInt32, &echoed_value,
                                run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(123, echoed_value);

  IntegerSenderAssociatedPtr sender1;
  base::RunLoop run_loop2;
  connection_ptr->AsyncGetSender(
      base::Bind(&CaptureSenderPtrInfo, &sender1, run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_TRUE(sender1);

  base::RunLoop run_loop3;
  sender1->Echo(456, base::Bind(&CaptureInt32, &echoed_value,
                                run_loop3.QuitClosure()));
  run_loop3.Run();
  EXPECT_EQ(456, echoed_value);
}

TEST_F(AssociatedInterfaceTest, BindingWaitAndPauseWhenNoAssociatedInterfaces) {
  IntegerSenderConnectionPtr connection_ptr;
  IntegerSenderConnectionImpl connection(GetProxy(&connection_ptr));

  IntegerSenderAssociatedPtr sender0;
  connection_ptr->GetSender(
      GetProxy(&sender0, connection_ptr.associated_group()));

  EXPECT_FALSE(connection.binding()->HasAssociatedInterfaces());
  // There are no associated interfaces running on the pipe yet. It is okay to
  // pause.
  connection.binding()->PauseIncomingMethodCallProcessing();
  connection.binding()->ResumeIncomingMethodCallProcessing();

  // There are no associated interfaces running on the pipe yet. It is okay to
  // wait.
  EXPECT_TRUE(connection.binding()->WaitForIncomingMethodCall());

  // The previous wait has dispatched the GetSender request message, therefore
  // an associated interface has been set up on the pipe. It is not allowed to
  // wait or pause.
  EXPECT_TRUE(connection.binding()->HasAssociatedInterfaces());
}

class PingServiceImpl : public PingService {
 public:
  explicit PingServiceImpl(PingServiceAssociatedRequest request)
      : binding_(this, std::move(request)) {}
  ~PingServiceImpl() override {}

  AssociatedBinding<PingService>& binding() { return binding_; }

  void set_ping_handler(const base::Closure& handler) {
    ping_handler_ = handler;
  }

  // PingService:
  void Ping(const PingCallback& callback) override {
    if (!ping_handler_.is_null())
      ping_handler_.Run();
    callback.Run();
  }

 private:
  AssociatedBinding<PingService> binding_;
  base::Closure ping_handler_;
};

class PingProviderImpl : public AssociatedPingProvider {
 public:
  explicit PingProviderImpl(AssociatedPingProviderRequest request)
      : binding_(this, std::move(request)) {}
  ~PingProviderImpl() override {}

  // AssociatedPingProvider:
  void GetPing(PingServiceAssociatedRequest request) override {
    ping_services_.emplace_back(new PingServiceImpl(std::move(request)));

    if (expected_bindings_count_ > 0 &&
        ping_services_.size() == expected_bindings_count_ &&
        !quit_waiting_.is_null()) {
      expected_bindings_count_ = 0;
      base::ResetAndReturn(&quit_waiting_).Run();
    }
  }

  std::vector<std::unique_ptr<PingServiceImpl>>& ping_services() {
    return ping_services_;
  }

  void WaitForBindings(size_t count) {
    DCHECK(quit_waiting_.is_null());

    expected_bindings_count_ = count;
    base::RunLoop loop;
    quit_waiting_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  Binding<AssociatedPingProvider> binding_;
  std::vector<std::unique_ptr<PingServiceImpl>> ping_services_;
  size_t expected_bindings_count_ = 0;
  base::Closure quit_waiting_;
};

class CallbackFilter : public MessageReceiver {
 public:
  explicit CallbackFilter(const base::Closure& callback)
      : callback_(callback) {}
  ~CallbackFilter() override {}

  static std::unique_ptr<CallbackFilter> Wrap(const base::Closure& callback) {
    return base::MakeUnique<CallbackFilter>(callback);
  }

  // MessageReceiver:
  bool Accept(Message* message) override {
    callback_.Run();
    return true;
  }

 private:
  const base::Closure callback_;
};

// Verifies that filters work as expected on associated bindings, i.e. that
// they're notified in order, before dispatch; and that each associated
// binding in a group operates with its own set of filters.
TEST_F(AssociatedInterfaceTest, BindingWithFilters) {
  AssociatedPingProviderPtr provider;
  PingProviderImpl provider_impl(GetProxy(&provider));

  PingServiceAssociatedPtr ping_a, ping_b;
  provider->GetPing(GetProxy(&ping_a, provider.associated_group()));
  provider->GetPing(GetProxy(&ping_b, provider.associated_group()));
  provider_impl.WaitForBindings(2);

  ASSERT_EQ(2u, provider_impl.ping_services().size());
  PingServiceImpl& ping_a_impl = *provider_impl.ping_services()[0];
  PingServiceImpl& ping_b_impl = *provider_impl.ping_services()[1];

  int a_status, b_status;
  auto handler_helper = [] (int* a_status, int* b_status, int expected_a_status,
                            int new_a_status, int expected_b_status,
                            int new_b_status) {
    EXPECT_EQ(expected_a_status, *a_status);
    EXPECT_EQ(expected_b_status, *b_status);
    *a_status = new_a_status;
    *b_status = new_b_status;
  };
  auto create_handler = [&] (int expected_a_status, int new_a_status,
                             int expected_b_status, int new_b_status) {
    return base::Bind(handler_helper, &a_status, &b_status, expected_a_status,
                      new_a_status, expected_b_status, new_b_status);
  };

  ping_a_impl.binding().AddFilter(
      CallbackFilter::Wrap(create_handler(0, 1, 0, 0)));
  ping_a_impl.binding().AddFilter(
      CallbackFilter::Wrap(create_handler(1, 2, 0, 0)));
  ping_a_impl.set_ping_handler(create_handler(2, 3, 0, 0));

  ping_b_impl.binding().AddFilter(
      CallbackFilter::Wrap(create_handler(3, 3, 0, 1)));
  ping_b_impl.binding().AddFilter(
      CallbackFilter::Wrap(create_handler(3, 3, 1, 2)));
  ping_b_impl.set_ping_handler(create_handler(3, 3, 2, 3));

  for (int i = 0; i < 10; ++i) {
    a_status = 0;
    b_status = 0;

    {
      base::RunLoop loop;
      ping_a->Ping(loop.QuitClosure());
      loop.Run();
    }

    EXPECT_EQ(3, a_status);
    EXPECT_EQ(0, b_status);

    {
      base::RunLoop loop;
      ping_b->Ping(loop.QuitClosure());
      loop.Run();
    }

    EXPECT_EQ(3, a_status);
    EXPECT_EQ(3, b_status);
  }
}

}  // namespace
}  // namespace test
}  // namespace mojo
