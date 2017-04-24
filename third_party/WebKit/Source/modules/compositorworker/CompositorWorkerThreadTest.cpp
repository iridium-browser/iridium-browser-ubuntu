// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/CompositorWorkerProxyClient.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/Handle.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAddressSpace.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {
namespace {

// A null InProcessWorkerObjectProxy, supplied when creating
// CompositorWorkerThreads.
class TestCompositorWorkerObjectProxy : public InProcessWorkerObjectProxy {
 public:
  static std::unique_ptr<TestCompositorWorkerObjectProxy> create(
      ParentFrameTaskRunners* parentFrameTaskRunners) {
    return WTF::wrapUnique(
        new TestCompositorWorkerObjectProxy(parentFrameTaskRunners));
  }

  // (Empty) WorkerReportingProxy implementation:
  virtual void dispatchErrorEvent(const String& errorMessage,
                                  std::unique_ptr<SourceLocation>,
                                  int exceptionId) {}
  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override {}
  void postMessageToPageInspector(const String&) override {}
  void didCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override {}
  void didEvaluateWorkerScript(bool success) override {}
  void didCloseWorkerGlobalScope() override {}
  void willDestroyWorkerGlobalScope() override {}
  void didTerminateWorkerThread() override {}

 private:
  explicit TestCompositorWorkerObjectProxy(
      ParentFrameTaskRunners* parentFrameTaskRunners)
      : InProcessWorkerObjectProxy(nullptr, parentFrameTaskRunners) {}
};

class TestCompositorWorkerProxyClient : public CompositorWorkerProxyClient {
 public:
  TestCompositorWorkerProxyClient() {}

  void dispose() override {}
  void setGlobalScope(WorkerGlobalScope*) override {}
  void requestAnimationFrame() override {}
  void registerCompositorProxy(CompositorProxy*) override {}
  void unregisterCompositorProxy(CompositorProxy*) override {}
};

class CompositorWorkerTestPlatform : public TestingPlatformSupport {
 public:
  CompositorWorkerTestPlatform()
      : m_thread(WTF::wrapUnique(m_oldPlatform->createThread("Compositor"))) {}

  WebThread* compositorThread() const override { return m_thread.get(); }

  WebCompositorSupport* compositorSupport() override {
    return &m_compositorSupport;
  }

 private:
  std::unique_ptr<WebThread> m_thread;
  TestingCompositorSupport m_compositorSupport;
};

}  // namespace

class CompositorWorkerThreadTest : public ::testing::Test {
 public:
  void SetUp() override {
    CompositorWorkerThread::createSharedBackingThreadForTest();
    m_parentFrameTaskRunners = ParentFrameTaskRunners::create(nullptr);
    m_objectProxy =
        TestCompositorWorkerObjectProxy::create(m_parentFrameTaskRunners.get());
    m_securityOrigin =
        SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
  }

  void TearDown() override {
    CompositorWorkerThread::clearSharedBackingThread();
  }

  std::unique_ptr<CompositorWorkerThread> createCompositorWorker() {
    std::unique_ptr<CompositorWorkerThread> workerThread =
        CompositorWorkerThread::create(nullptr, *m_objectProxy, 0);
    WorkerClients* clients = WorkerClients::create();
    provideCompositorWorkerProxyClientTo(clients,
                                         new TestCompositorWorkerProxyClient);
    workerThread->start(
        WorkerThreadStartupData::create(
            KURL(ParsedURLString, "http://fake.url/"), "fake user agent",
            "//fake source code", nullptr, DontPauseWorkerGlobalScopeOnStart,
            nullptr, "", m_securityOrigin.get(), clients, WebAddressSpaceLocal,
            nullptr, nullptr, WorkerV8Settings::Default()),
        m_parentFrameTaskRunners.get());
    return workerThread;
  }

  // Attempts to run some simple script for |worker|.
  void checkWorkerCanExecuteScript(WorkerThread* worker) {
    std::unique_ptr<WaitableEvent> waitEvent = WTF::makeUnique<WaitableEvent>();
    worker->workerBackingThread().backingThread().postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&CompositorWorkerThreadTest::executeScriptInWorker,
                        crossThreadUnretained(this),
                        crossThreadUnretained(worker),
                        crossThreadUnretained(waitEvent.get())));
    waitEvent->wait();
  }

 private:
  void executeScriptInWorker(WorkerThread* worker, WaitableEvent* waitEvent) {
    WorkerOrWorkletScriptController* scriptController =
        worker->globalScope()->scriptController();
    bool evaluateResult = scriptController->evaluate(
        ScriptSourceCode("var counter = 0; ++counter;"));
    DCHECK(evaluateResult);
    waitEvent->signal();
  }

  RefPtr<SecurityOrigin> m_securityOrigin;
  std::unique_ptr<InProcessWorkerObjectProxy> m_objectProxy;
  Persistent<ParentFrameTaskRunners> m_parentFrameTaskRunners;
  ScopedTestingPlatformSupport<CompositorWorkerTestPlatform> m_platform;
};

TEST_F(CompositorWorkerThreadTest, Basic) {
  std::unique_ptr<CompositorWorkerThread> compositorWorker =
      createCompositorWorker();
  checkWorkerCanExecuteScript(compositorWorker.get());
  compositorWorker->terminateAndWait();
}

// Tests that the same WebThread is used for new workers if the WebThread is
// still alive.
TEST_F(CompositorWorkerThreadTest, CreateSecondAndTerminateFirst) {
  // Create the first worker and wait until it is initialized.
  std::unique_ptr<CompositorWorkerThread> firstWorker =
      createCompositorWorker();
  WebThreadSupportingGC* firstThread =
      &firstWorker->workerBackingThread().backingThread();
  checkWorkerCanExecuteScript(firstWorker.get());
  v8::Isolate* firstIsolate = firstWorker->isolate();
  ASSERT_TRUE(firstIsolate);

  // Create the second worker and immediately destroy the first worker.
  std::unique_ptr<CompositorWorkerThread> secondWorker =
      createCompositorWorker();
  // We don't use terminateAndWait here to avoid forcible termination.
  firstWorker->terminate();
  firstWorker->waitForShutdownForTesting();

  // Wait until the second worker is initialized. Verify that the second worker
  // is using the same thread and Isolate as the first worker.
  WebThreadSupportingGC* secondThread =
      &secondWorker->workerBackingThread().backingThread();
  ASSERT_EQ(firstThread, secondThread);

  v8::Isolate* secondIsolate = secondWorker->isolate();
  ASSERT_TRUE(secondIsolate);
  EXPECT_EQ(firstIsolate, secondIsolate);

  // Verify that the worker can still successfully execute script.
  checkWorkerCanExecuteScript(secondWorker.get());

  secondWorker->terminateAndWait();
}

// Tests that a new WebThread is created if all existing workers are terminated
// before a new worker is created.
TEST_F(CompositorWorkerThreadTest, TerminateFirstAndCreateSecond) {
  // Create the first worker, wait until it is initialized, and terminate it.
  std::unique_ptr<CompositorWorkerThread> compositorWorker =
      createCompositorWorker();
  WebThreadSupportingGC* firstThread =
      &compositorWorker->workerBackingThread().backingThread();
  checkWorkerCanExecuteScript(compositorWorker.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  compositorWorker->terminate();
  compositorWorker->waitForShutdownForTesting();

  // Create the second worker. The backing thread is same.
  compositorWorker = createCompositorWorker();
  WebThreadSupportingGC* secondThread =
      &compositorWorker->workerBackingThread().backingThread();
  EXPECT_EQ(firstThread, secondThread);
  checkWorkerCanExecuteScript(compositorWorker.get());

  compositorWorker->terminateAndWait();
}

// Tests that v8::Isolate and WebThread are correctly set-up if a worker is
// created while another is terminating.
TEST_F(CompositorWorkerThreadTest, CreatingSecondDuringTerminationOfFirst) {
  std::unique_ptr<CompositorWorkerThread> firstWorker =
      createCompositorWorker();
  checkWorkerCanExecuteScript(firstWorker.get());
  v8::Isolate* firstIsolate = firstWorker->isolate();
  ASSERT_TRUE(firstIsolate);

  // Request termination of the first worker and create the second worker
  // as soon as possible.
  firstWorker->terminate();
  // We don't wait for its termination.
  // Note: We rely on the assumption that the termination steps don't run
  // on the worker thread so quickly. This could be a source of flakiness.

  std::unique_ptr<CompositorWorkerThread> secondWorker =
      createCompositorWorker();

  v8::Isolate* secondIsolate = secondWorker->isolate();
  ASSERT_TRUE(secondIsolate);
  EXPECT_EQ(firstIsolate, secondIsolate);

  // Verify that the isolate can run some scripts correctly in the second
  // worker.
  checkWorkerCanExecuteScript(secondWorker.get());
  secondWorker->terminateAndWait();
}

}  // namespace blink
