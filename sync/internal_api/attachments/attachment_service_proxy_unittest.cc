// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_service_proxy.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "sync/api/attachments/attachment.h"
#include "sync/internal_api/public/attachments/attachment_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// A stub implementation of AttachmentService that counts the number of times
// its methods are invoked.
class StubAttachmentService : public AttachmentService,
                              public base::NonThreadSafe {
 public:
  StubAttachmentService() : call_count_(0), weak_ptr_factory_(this) {
    // DetachFromThread because we will be constructed in one thread and
    // used/destroyed in another.
    DetachFromThread();
  }

  ~StubAttachmentService() override {}

  void GetOrDownloadAttachments(
      const AttachmentIdList& attachment_ids,
      const GetOrDownloadCallback& callback) override {
    CalledOnValidThread();
    Increment();
    scoped_ptr<AttachmentMap> attachments(new AttachmentMap());
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   AttachmentService::GET_UNSPECIFIED_ERROR,
                   base::Passed(&attachments)));
  }

  void UploadAttachments(const AttachmentIdList& attachments_ids) override {
    CalledOnValidThread();
    Increment();
  }

  virtual base::WeakPtr<AttachmentService> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Return the number of method invocations.
  int GetCallCount() const {
    base::AutoLock lock(mutex_);
    return call_count_;
  }

 private:
  // Protects call_count_.
  mutable base::Lock mutex_;
  int call_count_;

  // Must be last data member.
  base::WeakPtrFactory<AttachmentService> weak_ptr_factory_;

  void Increment() {
    base::AutoLock lock(mutex_);
    ++call_count_;
  }
};

class AttachmentServiceProxyTest : public testing::Test,
                                   public base::NonThreadSafe {
 protected:
  AttachmentServiceProxyTest() {}

  void SetUp() override {
    CalledOnValidThread();
    stub_thread.reset(new base::Thread("attachment service stub thread"));
    stub_thread->Start();
    stub.reset(new StubAttachmentService);
    proxy.reset(new AttachmentServiceProxy(stub_thread->task_runner(),
                                           stub->AsWeakPtr()));

    callback_get_or_download =
        base::Bind(&AttachmentServiceProxyTest::IncrementGetOrDownload,
                   base::Unretained(this));
    count_callback_get_or_download = 0;
  }

  void TearDown() override {
    // We must take care to call the stub's destructor on the stub_thread
    // because that's the thread to which its WeakPtrs are bound.
    if (stub) {
      stub_thread->message_loop()->DeleteSoon(FROM_HERE, stub.release());
      WaitForStubThread();
    }
    stub_thread->Stop();
  }

  // a GetOrDownloadCallback
  void IncrementGetOrDownload(const AttachmentService::GetOrDownloadResult&,
                              scoped_ptr<AttachmentMap>) {
    CalledOnValidThread();
    ++count_callback_get_or_download;
  }

  void WaitForStubThread() {
    base::WaitableEvent done(false, false);
    stub_thread->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
  }

  base::MessageLoop loop;
  scoped_ptr<base::Thread> stub_thread;
  scoped_ptr<StubAttachmentService> stub;
  scoped_ptr<AttachmentServiceProxy> proxy;

  AttachmentService::GetOrDownloadCallback callback_get_or_download;

  // number of times callback_get_or_download was invoked
  int count_callback_get_or_download;
};

// Verify that each of AttachmentServiceProxy's methods are invoked on the stub.
// Verify that the methods that take callbacks invoke passed callbacks on this
// thread.
TEST_F(AttachmentServiceProxyTest, MethodsAreProxied) {
  proxy->GetOrDownloadAttachments(AttachmentIdList(), callback_get_or_download);
  proxy->UploadAttachments(AttachmentIdList());
  // Wait for the posted calls to execute in the stub thread.
  WaitForStubThread();
  EXPECT_EQ(2, stub->GetCallCount());
  // At this point the stub thread has finished executed the calls. However, the
  // result callbacks it has posted may not have executed yet. Wait a second
  // time to ensure the stub thread has executed the posted result callbacks.
  WaitForStubThread();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, count_callback_get_or_download);
}

// Verify that it's safe to use an AttachmentServiceProxy even after its wrapped
// AttachmentService has been destroyed.
TEST_F(AttachmentServiceProxyTest, WrappedIsDestroyed) {
  proxy->GetOrDownloadAttachments(AttachmentIdList(), callback_get_or_download);
  // Wait for the posted calls to execute in the stub thread.
  WaitForStubThread();
  EXPECT_EQ(1, stub->GetCallCount());
  // Wait a second time ensure the stub thread has executed the posted result
  // callbacks.
  WaitForStubThread();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, count_callback_get_or_download);

  // Destroy the stub and call GetOrDownloadAttachments again.
  stub_thread->message_loop()->DeleteSoon(FROM_HERE, stub.release());
  WaitForStubThread();

  // Now that the wrapped object has been destroyed, call again and see that we
  // don't crash and the count remains the same.
  proxy->GetOrDownloadAttachments(AttachmentIdList(), callback_get_or_download);
  WaitForStubThread();
  WaitForStubThread();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, count_callback_get_or_download);
}

}  // namespace syncer
