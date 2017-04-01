// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationReceiver.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/presentation/PresentationConnectionList.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>
#include <v8.h>

namespace blink {

class MockEventListener : public EventListener {
 public:
  MockEventListener() : EventListener(CPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  MOCK_METHOD2(handleEvent, void(ExecutionContext* executionContext, Event*));
};

class MockWebPresentationClient : public WebPresentationClient {
  void startSession(
      const WebVector<WebURL>& presentationUrls,
      std::unique_ptr<WebPresentationConnectionCallback> callbacks) override {
    return startSession_(presentationUrls, callbacks);
  }
  void joinSession(
      const WebVector<WebURL>& presentationUrls,
      const WebString& presentationId,
      std::unique_ptr<WebPresentationConnectionCallback> callbacks) override {
    return joinSession_(presentationUrls, presentationId, callbacks);
  }

  void getAvailability(const WebVector<WebURL>& availabilityURLs,
                       std::unique_ptr<WebPresentationAvailabilityCallbacks>
                           callbacks) override {
    return getAvailability_(availabilityURLs, callbacks);
  }

 public:
  MOCK_METHOD1(setController, void(WebPresentationController*));

  MOCK_METHOD1(setReceiver, void(WebPresentationReceiver*));

  MOCK_METHOD2(startSession_,
               void(const WebVector<WebURL>& presentationUrls,
                    std::unique_ptr<WebPresentationConnectionCallback>&));

  MOCK_METHOD3(joinSession_,
               void(const WebVector<WebURL>& presentationUrls,
                    const WebString& presentationId,
                    std::unique_ptr<WebPresentationConnectionCallback>&));

  MOCK_METHOD3(sendString,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId,
                    const WebString& message));

  MOCK_METHOD4(sendArrayBuffer,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId,
                    const uint8_t* data,
                    size_t length));

  MOCK_METHOD4(sendBlobData,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId,
                    const uint8_t* data,
                    size_t length));

  MOCK_METHOD2(closeSession,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId));

  MOCK_METHOD2(terminateSession,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId));

  MOCK_METHOD2(getAvailability_,
               void(const WebVector<WebURL>& availabilityUrls,
                    std::unique_ptr<WebPresentationAvailabilityCallbacks>&));

  MOCK_METHOD1(startListening, void(WebPresentationAvailabilityObserver*));

  MOCK_METHOD1(stopListening, void(WebPresentationAvailabilityObserver*));

  MOCK_METHOD1(setDefaultPresentationUrls, void(const WebVector<WebURL>&));
};

class PresentationReceiverTest : public ::testing::Test {
 public:
  void addConnectionavailableEventListener(EventListener*,
                                           const PresentationReceiver*);
  void verifyConnectionListPropertyState(ScriptPromisePropertyBase::State,
                                         const PresentationReceiver*);
  void verifyConnectionListSize(size_t expectedSize,
                                const PresentationReceiver*);
};

void PresentationReceiverTest::addConnectionavailableEventListener(
    EventListener* eventHandler,
    const PresentationReceiver* receiver) {
  receiver->m_connectionList->addEventListener(
      EventTypeNames::connectionavailable, eventHandler);
}

void PresentationReceiverTest::verifyConnectionListPropertyState(
    ScriptPromisePropertyBase::State expectedState,
    const PresentationReceiver* receiver) {
  EXPECT_EQ(expectedState, receiver->m_connectionListProperty->getState());
}

void PresentationReceiverTest::verifyConnectionListSize(
    size_t expectedSize,
    const PresentationReceiver* receiver) {
  EXPECT_EQ(expectedSize, receiver->m_connectionList->m_connections.size());
}

using ::testing::StrictMock;

TEST_F(PresentationReceiverTest, NoConnectionUnresolvedConnectionList) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.frame(), nullptr);

  auto eventHandler = new StrictMock<MockEventListener>();
  addConnectionavailableEventListener(eventHandler, receiver);
  EXPECT_CALL(*eventHandler, handleEvent(testing::_, testing::_)).Times(0);

  receiver->connectionList(scope.getScriptState());

  verifyConnectionListPropertyState(ScriptPromisePropertyBase::Pending,
                                    receiver);
  verifyConnectionListSize(0, receiver);
}

TEST_F(PresentationReceiverTest, OneConnectionResolvedConnectionListNoEvent) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.frame(), nullptr);

  auto eventHandler = new StrictMock<MockEventListener>();
  addConnectionavailableEventListener(eventHandler, receiver);
  EXPECT_CALL(*eventHandler, handleEvent(testing::_, testing::_)).Times(0);

  receiver->connectionList(scope.getScriptState());

  // Receive first connection.
  receiver->onReceiverConnectionAvailable(
      WebPresentationSessionInfo(KURL(KURL(), "http://example.com"), "id"));

  verifyConnectionListPropertyState(ScriptPromisePropertyBase::Resolved,
                                    receiver);
  verifyConnectionListSize(1, receiver);
}

TEST_F(PresentationReceiverTest, TwoConnectionsFireOnconnectionavailableEvent) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.frame(), nullptr);

  StrictMock<MockEventListener>* eventHandler =
      new StrictMock<MockEventListener>();
  addConnectionavailableEventListener(eventHandler, receiver);
  EXPECT_CALL(*eventHandler, handleEvent(testing::_, testing::_)).Times(1);

  receiver->connectionList(scope.getScriptState());

  WebPresentationSessionInfo sessionInfo(KURL(KURL(), "http://example.com"),
                                         "id");
  // Receive first connection.
  receiver->onReceiverConnectionAvailable(sessionInfo);
  // Receive second connection.
  receiver->onReceiverConnectionAvailable(sessionInfo);

  verifyConnectionListSize(2, receiver);
}

TEST_F(PresentationReceiverTest, TwoConnectionsNoEvent) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.frame(), nullptr);

  StrictMock<MockEventListener>* eventHandler =
      new StrictMock<MockEventListener>();
  addConnectionavailableEventListener(eventHandler, receiver);
  EXPECT_CALL(*eventHandler, handleEvent(testing::_, testing::_)).Times(0);

  WebPresentationSessionInfo sessionInfo(KURL(KURL(), "http://example.com"),
                                         "id");
  // Receive first connection.
  receiver->onReceiverConnectionAvailable(sessionInfo);
  // Receive second connection.
  receiver->onReceiverConnectionAvailable(sessionInfo);

  receiver->connectionList(scope.getScriptState());
  verifyConnectionListPropertyState(ScriptPromisePropertyBase::Resolved,
                                    receiver);
  verifyConnectionListSize(2, receiver);
}

TEST_F(PresentationReceiverTest, CreateReceiver) {
  MockWebPresentationClient client;
  EXPECT_CALL(client, setReceiver(testing::_));

  V8TestingScope scope;
  new PresentationReceiver(&scope.frame(), &client);
}

}  // namespace blink
