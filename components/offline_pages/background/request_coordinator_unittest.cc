// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/background/scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// put test constants here
const GURL kUrl1("http://universe.com/everything");
const GURL kUrl2("http://universe.com/toinfinityandbeyond");
const std::string kClientNamespace("bookmark");
const std::string kId1("42");
const std::string kId2("life*universe+everything");
const ClientId kClientId1(kClientNamespace, kId1);
const ClientId kClientId2(kClientNamespace, kId2);
const int kRequestId1(1);
const int kRequestId2(2);
const long kTestTimeoutSeconds = 1;
const long kTestTimeBudgetSeconds = 200;
const int kBatteryPercentageHigh = 75;
const bool kPowerRequired = true;
const bool kUserRequested = true;
const int kAttemptCount = 1;
}  // namespace

class SchedulerStub : public Scheduler {
 public:
  SchedulerStub()
      : schedule_called_(false),
        unschedule_called_(false),
        conditions_(false, 0, false) {}

  void Schedule(const TriggerConditions& trigger_conditions) override {
    schedule_called_ = true;
    conditions_ = trigger_conditions;
  }

  // Unschedules the currently scheduled task, if any.
  void Unschedule() override {
    unschedule_called_ = true;
  }

  bool schedule_called() const { return schedule_called_; }

  bool unschedule_called() const { return unschedule_called_; }

  TriggerConditions const* conditions() const { return &conditions_; }

 private:
  bool schedule_called_;
  bool unschedule_called_;
  TriggerConditions conditions_;
};

class OfflinerStub : public Offliner {
 public:
  OfflinerStub()
      : request_(kRequestId1, kUrl1, kClientId1, base::Time::Now(),
                 kUserRequested),
        enable_callback_(false),
        cancel_called_(false) {}

  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& callback) override {
    callback_ = callback;
    request_ = request;
    // Post the callback on the run loop.
    if (enable_callback_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(callback, request, Offliner::RequestStatus::SAVED));
    }
    return true;
  }

  void Cancel() override { cancel_called_ = true; }

  void enable_callback(bool enable) {
    enable_callback_ = enable;
  }

  bool cancel_called() { return cancel_called_; }

 private:
  CompletionCallback callback_;
  SavePageRequest request_;
  bool enable_callback_;
  bool cancel_called_;
};

class OfflinerFactoryStub : public OfflinerFactory {
 public:
  OfflinerFactoryStub() : offliner_(nullptr) {}

  Offliner* GetOffliner(const OfflinerPolicy* policy) override {
    if (offliner_.get() == nullptr) {
      offliner_.reset(new OfflinerStub());
    }
    return offliner_.get();
  }

 private:
  std::unique_ptr<OfflinerStub> offliner_;
};

class ObserverStub : public RequestCoordinator::Observer {
 public:
  ObserverStub()
      : added_called_(false),
        completed_called_(false),
        changed_called_(false),
        last_status_(RequestCoordinator::SavePageStatus::SUCCESS),
        state_(SavePageRequest::RequestState::PRERENDERING) {}

  void Clear() {
    added_called_ = false;
    completed_called_ = false;
    changed_called_ = false;
    state_ = SavePageRequest::RequestState::PRERENDERING;
    last_status_ = RequestCoordinator::SavePageStatus::SUCCESS;
  }

  void OnAdded(const SavePageRequest& request) override {
    added_called_ = true;
  }

  void OnCompleted(const SavePageRequest& request,
                   RequestCoordinator::SavePageStatus status) override {
    completed_called_ = true;
    last_status_ = status;
  }

  void OnChanged(const SavePageRequest& request) override {
    changed_called_ = true;
    state_ = request.request_state();
  }

  bool added_called() { return added_called_; }
  bool completed_called() { return completed_called_; }
  bool changed_called() { return changed_called_; }
  RequestCoordinator::SavePageStatus last_status() { return last_status_; }
  SavePageRequest::RequestState state() { return state_; }

 private:
  bool added_called_;
  bool completed_called_;
  bool changed_called_;
  RequestCoordinator::SavePageStatus last_status_;
  SavePageRequest::RequestState state_;
};

class RequestCoordinatorTest
    : public testing::Test {
 public:
  RequestCoordinatorTest();
  ~RequestCoordinatorTest() override;

  void SetUp() override;

  void PumpLoop();

  RequestCoordinator* coordinator() {
    return coordinator_.get();
  }

  bool is_busy() {
    return coordinator_->is_busy();
  }

  bool is_starting() { return coordinator_->is_starting(); }

  // Empty callback function.
  void EmptyCallbackFunction(bool result) {
  }

  // Callback function which releases a wait for it.
  void WaitingCallbackFunction(bool result) {
    waiter_.Signal();
  }

  // Callback for Add requests.
  void AddRequestDone(RequestQueue::AddRequestResult result,
                      const SavePageRequest& request);

  // Callback for getting requests.
  void GetRequestsDone(RequestQueue::GetRequestsResult result,
                       const std::vector<SavePageRequest>& requests);

  // Callback for removing requests.
  void RemoveRequestsDone(
      const RequestQueue::UpdateMultipleRequestResults& results);

  // Callback for getting request statuses.
  void GetQueuedRequestsDone(const std::vector<SavePageRequest>& requests);

  void SendOfflinerDoneCallback(const SavePageRequest& request,
                                Offliner::RequestStatus status);

  RequestQueue::GetRequestsResult last_get_requests_result() const {
    return last_get_requests_result_;
  }

  const std::vector<SavePageRequest>& last_requests() const {
    return last_requests_;
  }

  const RequestQueue::UpdateMultipleRequestResults& last_remove_results()
      const {
    return last_remove_results_;
  }

  void EnableOfflinerCallback(bool enable) {
    offliner_->enable_callback(enable);
  }

  void SetNetworkConditionsForTest(
      net::NetworkChangeNotifier::ConnectionType connection) {
    coordinator()->SetNetworkConditionsForTest(connection);
  }

  void SetOfflinerTimeoutForTest(const base::TimeDelta& timeout) {
    coordinator_->SetOfflinerTimeoutForTest(timeout);
  }

  void SetDeviceConditionsForTest(DeviceConditions device_conditions) {
    coordinator_->SetDeviceConditionsForTest(device_conditions);
  }

  void WaitForCallback() {
    waiter_.Wait();
  }

  void AdvanceClockBy(base::TimeDelta delta) {
    task_runner_->FastForwardBy(delta);
  }

  Offliner::RequestStatus last_offlining_status() const {
    return coordinator_->last_offlining_status_;
  }

  bool OfflinerWasCanceled() const { return offliner_->cancel_called(); }

  ObserverStub observer() { return observer_; }

 private:
  RequestQueue::GetRequestsResult last_get_requests_result_;
  std::vector<SavePageRequest> last_requests_;
  RequestQueue::UpdateMultipleRequestResults last_remove_results_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<RequestCoordinator> coordinator_;
  OfflinerStub* offliner_;
  base::WaitableEvent waiter_;
  ObserverStub observer_;
};

RequestCoordinatorTest::RequestCoordinatorTest()
    : last_get_requests_result_(RequestQueue::GetRequestsResult::STORE_FAILURE),
      task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_),
      offliner_(nullptr),
      waiter_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

RequestCoordinatorTest::~RequestCoordinatorTest() {}

void RequestCoordinatorTest::SetUp() {
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());
  std::unique_ptr<OfflinerFactory> factory(new OfflinerFactoryStub());
  // Save the offliner for use by the tests.
  offliner_ =
      reinterpret_cast<OfflinerStub*>(factory->GetOffliner(policy.get()));
  std::unique_ptr<RequestQueueInMemoryStore>
      store(new RequestQueueInMemoryStore());
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(store)));
  std::unique_ptr<Scheduler> scheduler_stub(new SchedulerStub());
  coordinator_.reset(new RequestCoordinator(
      std::move(policy), std::move(factory), std::move(queue),
      std::move(scheduler_stub)));
  coordinator_->AddObserver(&observer_);
}

void RequestCoordinatorTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestCoordinatorTest::GetRequestsDone(
    RequestQueue::GetRequestsResult result,
    const std::vector<SavePageRequest>& requests) {
  last_get_requests_result_ = result;
  last_requests_ = requests;
}

void RequestCoordinatorTest::RemoveRequestsDone(
    const RequestQueue::UpdateMultipleRequestResults& results) {
  last_remove_results_ = results;
  waiter_.Signal();
}

void RequestCoordinatorTest::GetQueuedRequestsDone(
    const std::vector<SavePageRequest>& requests) {
  last_requests_ = requests;
  waiter_.Signal();
}

void RequestCoordinatorTest::AddRequestDone(
    RequestQueue::AddRequestResult result,
    const SavePageRequest& request) {}

void RequestCoordinatorTest::SendOfflinerDoneCallback(
    const SavePageRequest& request, Offliner::RequestStatus status) {
  // Using the fact that the test class is a friend, call to the callback
  coordinator_->OfflinerDoneCallback(request, status);
}

TEST_F(RequestCoordinatorTest, StartProcessingWithNoRequests) {
  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));
}

TEST_F(RequestCoordinatorTest, StartProcessingWithRequestInProgress) {
  SetNetworkConditionsForTest(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);
  // Put the request on the queue.
  EXPECT_TRUE(coordinator()->SavePageLater(kUrl1, kClientId1, kUserRequested));

  // Set up for the call to StartProcessing by building arguments.
  DeviceConditions device_conditions(
      false, 75, net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(&RequestCoordinatorTest::EmptyCallbackFunction,
                 base::Unretained(this));

  // Ensure that the forthcoming request does not finish - we simulate it being
  // in progress by asking it to skip making the completion callback.
  EnableOfflinerCallback(false);

  // Sending the request to the offliner should make it busy.
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));
  PumpLoop();
  EXPECT_TRUE(is_busy());

  // Now trying to start processing on another request should return false.
  EXPECT_FALSE(coordinator()->StartProcessing(device_conditions, callback));
}

TEST_F(RequestCoordinatorTest, SavePageLater) {
  EXPECT_TRUE(coordinator()->SavePageLater(kUrl1, kClientId1, kUserRequested));

  // Expect that a request got placed on the queue.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));

  // Wait for callbacks to finish, both request queue and offliner.
  PumpLoop();

  // Check the request queue is as expected.
  EXPECT_EQ(1UL, last_requests().size());
  EXPECT_EQ(kUrl1, last_requests()[0].url());
  EXPECT_EQ(kClientId1, last_requests()[0].client_id());

  // Expect that the scheduler got notified.
  SchedulerStub* scheduler_stub = reinterpret_cast<SchedulerStub*>(
      coordinator()->scheduler());
  EXPECT_TRUE(scheduler_stub->schedule_called());
  EXPECT_EQ(coordinator()
                ->GetTriggerConditionsForUserRequest()
                .minimum_battery_percentage,
            scheduler_stub->conditions()->minimum_battery_percentage);

  // Check that the observer got the notification that a page is available
  EXPECT_TRUE(observer().added_called());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneRequestSucceeded) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  request.MarkAttemptStarted(base::Time::Now());
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // We need to give a callback to the request.
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  coordinator()->SetProcessingCallbackForTest(callback);

  // Set up device conditions for the test.
  DeviceConditions device_conditions(
      false, 75, net::NetworkChangeNotifier::CONNECTION_3G);
  SetDeviceConditionsForTest(device_conditions);

  // Call the OfflinerDoneCallback to simulate the page being completed, wait
  // for callbacks.
  EnableOfflinerCallback(true);
  SendOfflinerDoneCallback(request, Offliner::RequestStatus::SAVED);
  PumpLoop();

  // Verify the request gets removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));
  PumpLoop();

  // We should not find any requests in the queue anymore.
  // RequestPicker should *not* have tried to start an additional job,
  // because the request queue is empty now.
  EXPECT_EQ(0UL, last_requests().size());
  // Check that the observer got the notification that we succeeded, and that
  // the request got removed from the queue.
  EXPECT_TRUE(observer().completed_called());
  EXPECT_EQ(RequestCoordinator::SavePageStatus::SUCCESS,
            observer().last_status());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneRequestFailed) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  request.MarkAttemptStarted(base::Time::Now());
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // Add second request to the queue to check handling when first fails.
  offline_pages::SavePageRequest request2(
      kRequestId2, kUrl2, kClientId2, base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request2,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // We need to give a callback to the request.
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  coordinator()->SetProcessingCallbackForTest(callback);

  // Set up device conditions for the test.
  DeviceConditions device_conditions(
      false, 75, net::NetworkChangeNotifier::CONNECTION_3G);
  SetDeviceConditionsForTest(device_conditions);

  // Call the OfflinerDoneCallback to simulate the request failed, wait
  // for callbacks.
  EnableOfflinerCallback(true);
  SendOfflinerDoneCallback(request,
                           Offliner::RequestStatus::PRERENDERING_FAILED);
  PumpLoop();

  // TODO(dougarnett): Consider injecting mock RequestPicker for this test
  // and verifying that there is no attempt to pick another request following
  // this failure code.

  // Verify neither request is removed from the queue; wait for callbacks.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));
  PumpLoop();

  // Now just one request in the queue since failed request removed
  // (for single attempt policy).
  EXPECT_EQ(1UL, last_requests().size());
  // Check that the observer got the notification that we failed (and the
  // subsequent notification that the request was removed).
  EXPECT_TRUE(observer().completed_called());
  EXPECT_EQ(RequestCoordinator::SavePageStatus::RETRY_COUNT_EXCEEDED,
            observer().last_status());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneForegroundCancel) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  request.MarkAttemptStarted(base::Time::Now());
  coordinator()->queue()->AddRequest(
      request, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                          base::Unretained(this)));
  PumpLoop();

  // We need to give a callback to the request.
  base::Callback<void(bool)> callback = base::Bind(
      &RequestCoordinatorTest::EmptyCallbackFunction, base::Unretained(this));
  coordinator()->SetProcessingCallbackForTest(callback);

  // Set up device conditions for the test.
  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  SetDeviceConditionsForTest(device_conditions);

  // Call the OfflinerDoneCallback to simulate the request failed, wait
  // for callbacks.
  EnableOfflinerCallback(true);
  SendOfflinerDoneCallback(request,
                           Offliner::RequestStatus::FOREGROUND_CANCELED);
  PumpLoop();

  // Verify the request is not removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(base::Bind(
      &RequestCoordinatorTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Request no longer in the queue (for single attempt policy).
  EXPECT_EQ(1UL, last_requests().size());
  // Verify foreground cancel not counted as an attempt after all.
  const SavePageRequest& found_request = last_requests().front();
  EXPECT_EQ(0L, found_request.completed_attempt_count());
}

TEST_F(RequestCoordinatorTest, OfflinerDonePrerenderingCancel) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(kRequestId1, kUrl1, kClientId1,
                                         base::Time::Now(), kUserRequested);
  request.MarkAttemptStarted(base::Time::Now());
  coordinator()->queue()->AddRequest(
      request, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                          base::Unretained(this)));
  PumpLoop();

  // We need to give a callback to the request.
  base::Callback<void(bool)> callback = base::Bind(
      &RequestCoordinatorTest::EmptyCallbackFunction, base::Unretained(this));
  coordinator()->SetProcessingCallbackForTest(callback);

  // Set up device conditions for the test.
  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  SetDeviceConditionsForTest(device_conditions);

  // Call the OfflinerDoneCallback to simulate the request failed, wait
  // for callbacks.
  EnableOfflinerCallback(true);
  SendOfflinerDoneCallback(request,
                           Offliner::RequestStatus::PRERENDERING_CANCELED);
  PumpLoop();

  // Verify the request is not removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(base::Bind(
      &RequestCoordinatorTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Request still in the queue.
  EXPECT_EQ(1UL, last_requests().size());
  // Verify prerendering cancel not counted as an attempt after all.
  const SavePageRequest& found_request = last_requests().front();
  EXPECT_EQ(0L, found_request.completed_attempt_count());
}

// This tests a StopProcessing call before we have actually started the
// prerenderer.
TEST_F(RequestCoordinatorTest, StartProcessingThenStopProcessingImmediately) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));
  EXPECT_TRUE(is_starting());

  // Now, quick, before it can do much (we haven't called PumpLoop), cancel it.
  coordinator()->StopProcessing();

  // Let the async callbacks in the request coordinator run.
  PumpLoop();

  EXPECT_FALSE(is_starting());

  // OfflinerDoneCallback will not end up getting called with status SAVED,
  // since we cancelled the event before it called offliner_->LoadAndSave().
  EXPECT_EQ(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED,
            last_offlining_status());

  // Since offliner was not started, it will not have seen cancel call.
  EXPECT_FALSE(OfflinerWasCanceled());
}

// This tests a StopProcessing call after the prerenderer has been started.
TEST_F(RequestCoordinatorTest, StartProcessingThenStopProcessingLater) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // Ensure the start processing request stops before the completion callback.
  EnableOfflinerCallback(false);

  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));
  EXPECT_TRUE(is_starting());

  // Let all the async parts of the start processing pipeline run to completion.
  PumpLoop();

  // Coordinator should now be busy.
  EXPECT_TRUE(is_busy());
  EXPECT_FALSE(is_starting());

  // Now we cancel it while the prerenderer is busy.
  coordinator()->StopProcessing();

  // Let the async callbacks in the cancel run.
  PumpLoop();

  EXPECT_FALSE(is_busy());

  // OfflinerDoneCallback will not end up getting called with status SAVED,
  // since we cancelled the event before the LoadAndSave completed.
  EXPECT_EQ(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED,
            last_offlining_status());

  // Since offliner was started, it will have seen cancel call.
  EXPECT_TRUE(OfflinerWasCanceled());
}

// This tests that canceling a request will result in TryNextRequest() getting
// called.
TEST_F(RequestCoordinatorTest, RemoveInflightRequest) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request1(kRequestId1, kUrl1, kClientId1,
                                          base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request1, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  PumpLoop();

  // Ensure the start processing request stops before the completion callback.
  EnableOfflinerCallback(false);

  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback = base::Bind(
      &RequestCoordinatorTest::EmptyCallbackFunction, base::Unretained(this));
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));

  // Let all the async parts of the start processing pipeline run to completion.
  PumpLoop();

  // Remove the request while it is processing.
  std::vector<int64_t> request_ids{kRequestId1};
  coordinator()->RemoveRequests(
      request_ids, base::Bind(&RequestCoordinatorTest::RemoveRequestsDone,
                              base::Unretained(this)));

  // Let the async callbacks in the cancel run.
  PumpLoop();

  // Since offliner was started, it will have seen cancel call.
  EXPECT_TRUE(OfflinerWasCanceled());
}

TEST_F(RequestCoordinatorTest, WatchdogTimeout) {
  // Build a request to use with the pre-renderer, and put it on the queue.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // Set up for the call to StartProcessing.
  DeviceConditions device_conditions(
      !kPowerRequired, kBatteryPercentageHigh,
      net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(&RequestCoordinatorTest::WaitingCallbackFunction,
                 base::Unretained(this));

  // Ensure that the new request does not finish - we simulate it being
  // in progress by asking it to skip making the completion callback.
  EnableOfflinerCallback(false);

  // Ask RequestCoordinator to stop waiting for the offliner after this many
  // seconds.
  SetOfflinerTimeoutForTest(base::TimeDelta::FromSeconds(kTestTimeoutSeconds));

  // Sending the request to the offliner.
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));
  PumpLoop();

  // Advance the mock clock far enough to cause a watchdog timeout
  AdvanceClockBy(base::TimeDelta::FromSeconds(kTestTimeoutSeconds + 1));
  PumpLoop();

  // Wait for timeout to expire.  Use a TaskRunner with a DelayedTaskRunner
  // which won't time out immediately, so the watchdog thread doesn't kill valid
  // tasks too soon.
  WaitForCallback();
  PumpLoop();

  EXPECT_FALSE(is_starting());
  EXPECT_TRUE(OfflinerWasCanceled());
  EXPECT_EQ(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED,
            last_offlining_status());
}

TEST_F(RequestCoordinatorTest, TimeBudgetExceeded) {
  // Build two requests to use with the pre-renderer, and put it on the queue.
  offline_pages::SavePageRequest request1(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  offline_pages::SavePageRequest request2(
      kRequestId1 + 1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);
  coordinator()->queue()->AddRequest(
      request1,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  coordinator()->queue()->AddRequest(
      request1,  // TODO(petewil): This is a bug, should be request2.
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // Set up for the call to StartProcessing.
  DeviceConditions device_conditions(
      !kPowerRequired, kBatteryPercentageHigh,
      net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(&RequestCoordinatorTest::WaitingCallbackFunction,
                 base::Unretained(this));

  // Sending the request to the offliner.
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));
  PumpLoop();

  // Advance the mock clock far enough to exceed our time budget.
  AdvanceClockBy(base::TimeDelta::FromSeconds(kTestTimeBudgetSeconds));
  PumpLoop();

  // TryNextRequest should decide that there is no more work to be done,
  // and call back to the scheduler, even though there is another request in the
  // queue.  There should be one request left in the queue.
  // Verify the request gets removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));
  PumpLoop();

  // We should find one request in the queue.
  EXPECT_EQ(1UL, last_requests().size());
}

TEST_F(RequestCoordinatorTest, GetAllRequests) {
  // Add two requests to the queue.
  offline_pages::SavePageRequest request1(kRequestId1, kUrl1, kClientId1,
                                          base::Time::Now(), kUserRequested);
  offline_pages::SavePageRequest request2(kRequestId1 + 1, kUrl2, kClientId2,
                                          base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request1, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  coordinator()->queue()->AddRequest(
      request2, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  PumpLoop();

  // Start the async status fetching.
  coordinator()->GetAllRequests(base::Bind(
      &RequestCoordinatorTest::GetQueuedRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Wait for async get to finish.
  WaitForCallback();
  PumpLoop();

  // Check that the statuses found in the callback match what we expect.
  EXPECT_EQ(2UL, last_requests().size());
  EXPECT_EQ(kRequestId1, last_requests().at(0).request_id());
  EXPECT_EQ(kRequestId2, last_requests().at(1).request_id());
}

TEST_F(RequestCoordinatorTest, PauseAndResumeObserver) {
  // Add a request to the queue.
  offline_pages::SavePageRequest request1(kRequestId1, kUrl1, kClientId1,
                                          base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request1, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  PumpLoop();

  // Pause the request.
  std::vector<int64_t> request_ids;
  request_ids.push_back(kRequestId1);
  coordinator()->PauseRequests(request_ids);
  PumpLoop();

  EXPECT_TRUE(observer().changed_called());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED, observer().state());

  // Clear out the observer before the next call.
  observer().Clear();

  // Resume the request.
  coordinator()->ResumeRequests(request_ids);
  PumpLoop();

  EXPECT_TRUE(observer().changed_called());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE, observer().state());
}

TEST_F(RequestCoordinatorTest, RemoveRequest) {
  // Add a request to the queue.
  offline_pages::SavePageRequest request1(kRequestId1, kUrl1, kClientId1,
                                          base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request1, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  PumpLoop();

  // Remove the request.
  std::vector<int64_t> request_ids;
  request_ids.push_back(kRequestId1);
  coordinator()->RemoveRequests(
      request_ids, base::Bind(&RequestCoordinatorTest::RemoveRequestsDone,
                              base::Unretained(this)));

  PumpLoop();
  WaitForCallback();
  PumpLoop();

  EXPECT_TRUE(observer().completed_called());
  EXPECT_EQ(RequestCoordinator::SavePageStatus::REMOVED,
            observer().last_status());
  EXPECT_EQ(1UL, last_remove_results().size());
  EXPECT_EQ(kRequestId1, std::get<0>(last_remove_results().at(0)));
}

TEST_F(RequestCoordinatorTest,
       SavePageStartsProcessingWhenConnectedAndNotLowEndDevice) {
  SetNetworkConditionsForTest(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(coordinator()->SavePageLater(kUrl1, kClientId1, kUserRequested));
  PumpLoop();

  // Now whether processing triggered immediately depends on whether test
  // is run on svelte device or not.
  if (base::SysInfo::IsLowEndDevice()) {
    EXPECT_FALSE(is_busy());
  } else {
    EXPECT_TRUE(is_busy());
  }
}

TEST_F(RequestCoordinatorTest, SavePageDoesntStartProcessingWhenDisconnected) {
  SetNetworkConditionsForTest(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);
  EXPECT_TRUE(coordinator()->SavePageLater(kUrl1, kClientId1, kUserRequested));
  PumpLoop();
  EXPECT_FALSE(is_busy());
}

TEST_F(RequestCoordinatorTest,
       ResumeStartsProcessingWhenConnectedAndNotLowEndDevice) {
  SetNetworkConditionsForTest(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

  // Add a request to the queue.
  offline_pages::SavePageRequest request1(kRequestId1, kUrl1, kClientId1,
                                          base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request1, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  PumpLoop();
  EXPECT_FALSE(is_busy());

  // Pause the request.
  std::vector<int64_t> request_ids;
  request_ids.push_back(kRequestId1);
  coordinator()->PauseRequests(request_ids);
  PumpLoop();

  // Resume the request while disconnected.
  coordinator()->ResumeRequests(request_ids);
  PumpLoop();
  EXPECT_FALSE(is_busy());

  // Pause the request again.
  coordinator()->PauseRequests(request_ids);
  PumpLoop();

  // Now simulate being connected.
  SetNetworkConditionsForTest(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);

  // Resume the request while connected.
  coordinator()->ResumeRequests(request_ids);
  EXPECT_FALSE(is_busy());
  PumpLoop();

  // Now whether processing triggered immediately depends on whether test
  // is run on svelte device or not.
  if (base::SysInfo::IsLowEndDevice()) {
    EXPECT_FALSE(is_busy());
  } else {
    EXPECT_TRUE(is_busy());
  }
}

}  // namespace offline_pages
