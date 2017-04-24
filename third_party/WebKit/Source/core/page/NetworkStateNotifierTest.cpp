/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/NetworkStateNotifier.h"

#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebConnectionType.h"
#include "public/platform/WebThread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Functional.h"

namespace blink {

namespace {
const double kNoneMaxBandwidthMbps = 0.0;
const double kBluetoothMaxBandwidthMbps = 1.0;
const double kEthernetMaxBandwidthMbps = 2.0;
}

class StateObserver : public NetworkStateNotifier::NetworkStateObserver {
 public:
  StateObserver()
      : m_observedType(WebConnectionTypeNone),
        m_observedMaxBandwidthMbps(0.0),
        m_observedOnLineState(false),
        m_callbackCount(0) {}

  virtual void connectionChange(WebConnectionType type,
                                double maxBandwidthMbps) {
    m_observedType = type;
    m_observedMaxBandwidthMbps = maxBandwidthMbps;
    m_callbackCount += 1;

    if (m_closure)
      (*m_closure)();
  }

  virtual void onLineStateChange(bool onLine) {
    m_observedOnLineState = onLine;
    m_callbackCount += 1;

    if (m_closure)
      (*m_closure)();
  }

  WebConnectionType observedType() const { return m_observedType; }
  double observedMaxBandwidth() const { return m_observedMaxBandwidthMbps; }
  bool observedOnLineState() const { return m_observedOnLineState; }
  int callbackCount() const { return m_callbackCount; }

  void setNotificationCallback(std::unique_ptr<WTF::Closure> closure) {
    m_closure = std::move(closure);
  }

 private:
  std::unique_ptr<WTF::Closure> m_closure;
  WebConnectionType m_observedType;
  double m_observedMaxBandwidthMbps;
  bool m_observedOnLineState;
  int m_callbackCount;
};

class NetworkStateNotifierTest : public ::testing::Test {
 public:
  NetworkStateNotifierTest()
      : m_document(Document::create()), m_document2(Document::create()) {
    // Initialize connection, so that future calls to setWebConnection issue
    // notifications.
    m_notifier.setWebConnection(WebConnectionTypeUnknown, 0.0);
    m_notifier.setOnLine(false);
  }

  WebTaskRunner* getTaskRunner() {
    return TaskRunnerHelper::get(TaskType::Networking, m_document.get()).get();
  }

  WebTaskRunner* getTaskRunner2() {
    return TaskRunnerHelper::get(TaskType::Networking, m_document2.get()).get();
  }

 protected:
  void setConnection(WebConnectionType type, double maxBandwidthMbps) {
    m_notifier.setWebConnection(type, maxBandwidthMbps);
    testing::runPendingTasks();
  }
  void setOnLine(bool onLine) {
    m_notifier.setOnLine(onLine);
    testing::runPendingTasks();
  }

  void addObserverOnNotification(StateObserver* observer,
                                 StateObserver* observerToAdd) {
    observer->setNotificationCallback(
        bind(&NetworkStateNotifier::addConnectionObserver,
             WTF::unretained(&m_notifier), WTF::unretained(observerToAdd),
             WTF::unretained(getTaskRunner())));
  }

  void removeObserverOnNotification(StateObserver* observer,
                                    StateObserver* observerToRemove) {
    observer->setNotificationCallback(
        bind(&NetworkStateNotifier::removeConnectionObserver,
             WTF::unretained(&m_notifier), WTF::unretained(observerToRemove),
             WTF::unretained(getTaskRunner())));
  }

  bool verifyObservations(const StateObserver& observer,
                          WebConnectionType type,
                          double maxBandwidthMbps) {
    EXPECT_EQ(observer.observedType(), type);
    EXPECT_EQ(observer.observedMaxBandwidth(), maxBandwidthMbps);
    return observer.observedType() == type &&
           observer.observedMaxBandwidth() == maxBandwidthMbps;
  }

  Persistent<Document> m_document;
  Persistent<Document> m_document2;
  NetworkStateNotifier m_notifier;
};

TEST_F(NetworkStateNotifierTest, AddObserver) {
  StateObserver observer;
  m_notifier.addConnectionObserver(&observer, getTaskRunner());
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeNone,
                                 kNoneMaxBandwidthMbps));

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_EQ(observer.callbackCount(), 1);
}

TEST_F(NetworkStateNotifierTest, RemoveObserver) {
  StateObserver observer1, observer2;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  m_notifier.removeConnectionObserver(&observer1, getTaskRunner());
  m_notifier.addConnectionObserver(&observer2, getTaskRunner());

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeNone,
                                 kNoneMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveSoleObserver) {
  StateObserver observer1;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  m_notifier.removeConnectionObserver(&observer1, getTaskRunner());

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeNone,
                                 kNoneMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, AddObserverWhileNotifying) {
  StateObserver observer1, observer2;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  addObserverOnNotification(&observer1, &observer2);

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveSoleObserverWhileNotifying) {
  StateObserver observer1;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  removeObserverOnNotification(&observer1, &observer1);

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));

  setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveCurrentObserverWhileNotifying) {
  StateObserver observer1, observer2;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  m_notifier.addConnectionObserver(&observer2, getTaskRunner());
  removeObserverOnNotification(&observer1, &observer1);

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));

  setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeEthernet,
                                 kEthernetMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemovePastObserverWhileNotifying) {
  StateObserver observer1, observer2;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  m_notifier.addConnectionObserver(&observer2, getTaskRunner());
  removeObserverOnNotification(&observer2, &observer1);

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_EQ(observer1.observedType(), WebConnectionTypeBluetooth);
  EXPECT_EQ(observer2.observedType(), WebConnectionTypeBluetooth);

  setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeEthernet,
                                 kEthernetMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveFutureObserverWhileNotifying) {
  StateObserver observer1, observer2, observer3;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  m_notifier.addConnectionObserver(&observer2, getTaskRunner());
  m_notifier.addConnectionObserver(&observer3, getTaskRunner());
  removeObserverOnNotification(&observer1, &observer2);

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeNone,
                                 kNoneMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer3, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, MultipleContextsAddObserver) {
  StateObserver observer1, observer2;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  m_notifier.addConnectionObserver(&observer2, getTaskRunner2());

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveContext) {
  StateObserver observer1, observer2;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  m_notifier.addConnectionObserver(&observer2, getTaskRunner2());
  m_notifier.removeConnectionObserver(&observer2, getTaskRunner2());

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeNone,
                                 kNoneMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveAllContexts) {
  StateObserver observer1, observer2;
  m_notifier.addConnectionObserver(&observer1, getTaskRunner());
  m_notifier.addConnectionObserver(&observer2, getTaskRunner2());
  m_notifier.removeConnectionObserver(&observer1, getTaskRunner());
  m_notifier.removeConnectionObserver(&observer2, getTaskRunner2());

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeNone,
                                 kNoneMaxBandwidthMbps));
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeNone,
                                 kNoneMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, SetOverride) {
  StateObserver observer;
  m_notifier.addConnectionObserver(&observer, getTaskRunner());

  m_notifier.setOnLine(true);
  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_TRUE(m_notifier.onLine());
  EXPECT_EQ(WebConnectionTypeBluetooth, m_notifier.connectionType());
  EXPECT_EQ(kBluetoothMaxBandwidthMbps, m_notifier.maxBandwidth());

  m_notifier.setOverride(true, WebConnectionTypeEthernet,
                         kEthernetMaxBandwidthMbps);
  testing::runPendingTasks();
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeEthernet,
                                 kEthernetMaxBandwidthMbps));
  EXPECT_TRUE(m_notifier.onLine());
  EXPECT_EQ(WebConnectionTypeEthernet, m_notifier.connectionType());
  EXPECT_EQ(kEthernetMaxBandwidthMbps, m_notifier.maxBandwidth());

  // When override is active, calls to setOnLine and setConnection are temporary
  // ignored.
  m_notifier.setOnLine(false);
  setConnection(WebConnectionTypeNone, kNoneMaxBandwidthMbps);
  testing::runPendingTasks();
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeEthernet,
                                 kEthernetMaxBandwidthMbps));
  EXPECT_TRUE(m_notifier.onLine());
  EXPECT_EQ(WebConnectionTypeEthernet, m_notifier.connectionType());
  EXPECT_EQ(kEthernetMaxBandwidthMbps, m_notifier.maxBandwidth());

  m_notifier.clearOverride();
  testing::runPendingTasks();
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeNone,
                                 kNoneMaxBandwidthMbps));
  EXPECT_FALSE(m_notifier.onLine());
  EXPECT_EQ(WebConnectionTypeNone, m_notifier.connectionType());
  EXPECT_EQ(kNoneMaxBandwidthMbps, m_notifier.maxBandwidth());

  m_notifier.removeConnectionObserver(&observer, getTaskRunner());
}

TEST_F(NetworkStateNotifierTest, NoExtraNotifications) {
  StateObserver observer;
  m_notifier.addConnectionObserver(&observer, getTaskRunner());

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_EQ(observer.callbackCount(), 1);

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_EQ(observer.callbackCount(), 1);

  setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeEthernet,
                                 kEthernetMaxBandwidthMbps));
  EXPECT_EQ(observer.callbackCount(), 2);

  setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
  EXPECT_EQ(observer.callbackCount(), 2);

  setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
  EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeBluetooth,
                                 kBluetoothMaxBandwidthMbps));
  EXPECT_EQ(observer.callbackCount(), 3);

  m_notifier.removeConnectionObserver(&observer, getTaskRunner());
}

TEST_F(NetworkStateNotifierTest, NoNotificationOnInitialization) {
  NetworkStateNotifier notifier;
  Persistent<Document> document(Document::create());
  StateObserver observer;

  notifier.addConnectionObserver(&observer, getTaskRunner());
  notifier.addOnLineObserver(&observer, getTaskRunner());
  testing::runPendingTasks();
  EXPECT_EQ(observer.callbackCount(), 0);

  notifier.setWebConnection(WebConnectionTypeBluetooth,
                            kBluetoothMaxBandwidthMbps);
  notifier.setOnLine(true);
  testing::runPendingTasks();
  EXPECT_EQ(observer.callbackCount(), 0);

  notifier.setOnLine(true);
  notifier.setWebConnection(WebConnectionTypeBluetooth,
                            kBluetoothMaxBandwidthMbps);
  testing::runPendingTasks();
  EXPECT_EQ(observer.callbackCount(), 0);

  notifier.setWebConnection(WebConnectionTypeEthernet,
                            kEthernetMaxBandwidthMbps);
  testing::runPendingTasks();
  EXPECT_EQ(observer.callbackCount(), 1);
  EXPECT_EQ(observer.observedType(), WebConnectionTypeEthernet);
  EXPECT_EQ(observer.observedMaxBandwidth(), kEthernetMaxBandwidthMbps);

  notifier.setOnLine(false);
  testing::runPendingTasks();
  EXPECT_EQ(observer.callbackCount(), 2);
  EXPECT_FALSE(observer.observedOnLineState());
}

TEST_F(NetworkStateNotifierTest, OnLineNotification) {
  StateObserver observer;
  m_notifier.addOnLineObserver(&observer, getTaskRunner());

  setOnLine(true);
  testing::runPendingTasks();
  EXPECT_TRUE(observer.observedOnLineState());
  EXPECT_EQ(observer.callbackCount(), 1);

  setOnLine(false);
  testing::runPendingTasks();
  EXPECT_FALSE(observer.observedOnLineState());
  EXPECT_EQ(observer.callbackCount(), 2);

  m_notifier.removeConnectionObserver(&observer, getTaskRunner());
}

TEST_F(NetworkStateNotifierTest, MultipleObservers) {
  StateObserver observer1;
  StateObserver observer2;

  // Observer1 observes online state, Observer2 observes both.
  m_notifier.addOnLineObserver(&observer1, getTaskRunner());
  m_notifier.addConnectionObserver(&observer2, getTaskRunner());
  m_notifier.addOnLineObserver(&observer2, getTaskRunner());

  m_notifier.setOnLine(true);
  testing::runPendingTasks();
  EXPECT_TRUE(observer1.observedOnLineState());
  EXPECT_TRUE(observer2.observedOnLineState());
  EXPECT_EQ(observer1.callbackCount(), 1);
  EXPECT_EQ(observer2.callbackCount(), 1);

  m_notifier.setOnLine(false);
  testing::runPendingTasks();
  EXPECT_FALSE(observer1.observedOnLineState());
  EXPECT_FALSE(observer2.observedOnLineState());
  EXPECT_EQ(observer1.callbackCount(), 2);
  EXPECT_EQ(observer2.callbackCount(), 2);

  m_notifier.setOnLine(true);
  m_notifier.setWebConnection(WebConnectionTypeEthernet,
                              kEthernetMaxBandwidthMbps);
  testing::runPendingTasks();
  EXPECT_TRUE(observer1.observedOnLineState());
  EXPECT_TRUE(observer2.observedOnLineState());
  EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeEthernet,
                                 kEthernetMaxBandwidthMbps));
  EXPECT_EQ(observer1.callbackCount(), 3);
  EXPECT_EQ(observer2.callbackCount(), 4);

  m_notifier.removeConnectionObserver(&observer1, getTaskRunner());
  m_notifier.removeConnectionObserver(&observer2, getTaskRunner());
}

}  // namespace blink
