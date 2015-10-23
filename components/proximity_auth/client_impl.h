// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CLIENT_IMPL_H
#define COMPONENTS_PROXIMITY_AUTH_CLIENT_IMPL_H

#include <deque>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/proximity_auth/client.h"
#include "components/proximity_auth/connection_observer.h"

namespace base {
class DictionaryValue;
}

namespace proximity_auth {

class Connection;
class SecureContext;

// Concrete implementation of the Client interface.
class ClientImpl : public Client, public ConnectionObserver {
 public:
  // Constructs a client that sends and receives messages over the given
  // |connection|, using the |secure_context| to encrypt and decrypt the
  // messages. The |connection| must be connected. The client begins observing
  // messages as soon as it is constructed.
  ClientImpl(scoped_ptr<Connection> connection,
             scoped_ptr<SecureContext> secure_context);
  ~ClientImpl() override;

  // Client:
  void AddObserver(ClientObserver* observer) override;
  void RemoveObserver(ClientObserver* observer) override;
  bool SupportsSignIn() const override;
  void DispatchUnlockEvent() override;
  void RequestDecryption(const std::string& challenge) override;
  void RequestUnlock() override;

  // Exposed for testing.
  Connection* connection() { return connection_.get(); }

 protected:
  SecureContext* secure_context() { return secure_context_.get(); }

 private:
  // Internal data structure to represent a pending message that either hasn't
  // been sent yet or is waiting for a response from the remote device.
  struct PendingMessage {
    PendingMessage();
    PendingMessage(const base::DictionaryValue& message);
    ~PendingMessage();

    // The message, serialized as JSON.
    const std::string json_message;

    // The message type. This is possible to parse from the |json_message|; it's
    // stored redundantly for convenience.
    const std::string type;
  };

  // Pops the first of the |queued_messages_| and sends it to the remote device.
  void ProcessMessageQueue();

  // Called when the message is encoded so it can be sent over the connection.
  void OnMessageEncoded(const std::string& encoded_message);

  // Called when the message is decoded so it can be parsed.
  void OnMessageDecoded(const std::string& decoded_message);

  // Handles an incoming "status_update" |message|, parsing and notifying
  // observers of the content.
  void HandleRemoteStatusUpdateMessage(const base::DictionaryValue& message);

  // Handles an incoming "decrypt_response" message, parsing and notifying
  // observers of the decrypted content.
  void HandleDecryptResponseMessage(const base::DictionaryValue& message);

  // Handles an incoming "unlock_response" message, notifying observers of the
  // response.
  void HandleUnlockResponseMessage(const base::DictionaryValue& message);

  // ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;
  void OnMessageReceived(const Connection& connection,
                         const WireMessage& wire_message) override;
  void OnSendCompleted(const Connection& connection,
                       const WireMessage& wire_message,
                       bool success) override;

  // The connection used to send and receive events and status updates.
  scoped_ptr<Connection> connection_;

  // Used to encrypt and decrypt payloads sent and received over the
  // |connection_|.
  scoped_ptr<SecureContext> secure_context_;

  // The registered observers of |this_| client.
  base::ObserverList<ClientObserver> observers_;

  // Queue of messages to send to the remote device.
  std::deque<PendingMessage> queued_messages_;

  // The current message being sent or waiting on the remote device for a
  // response. Null if there is no message currently in this state.
  scoped_ptr<PendingMessage> pending_message_;

  base::WeakPtrFactory<ClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientImpl);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CLIENT_IMPL_H
