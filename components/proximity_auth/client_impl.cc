// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/client_impl.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/proximity_auth/client_observer.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/cryptauth/base64url.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/remote_status_update.h"
#include "components/proximity_auth/secure_context.h"
#include "components/proximity_auth/wire_message.h"

namespace proximity_auth {
namespace {

// The key names of JSON fields for messages sent between the devices.
const char kTypeKey[] = "type";
const char kNameKey[] = "name";
const char kDataKey[] = "data";
const char kEncryptedDataKey[] = "encrypted_data";

// The types of messages that can be sent and received.
const char kMessageTypeLocalEvent[] = "event";
const char kMessageTypeRemoteStatusUpdate[] = "status_update";
const char kMessageTypeDecryptRequest[] = "decrypt_request";
const char kMessageTypeDecryptResponse[] = "decrypt_response";
const char kMessageTypeUnlockRequest[] = "unlock_request";
const char kMessageTypeUnlockResponse[] = "unlock_response";

// The name for an unlock event originating from the local device.
const char kUnlockEventName[] = "easy_unlock";

// Serializes the |value| to a JSON string and returns the result.
std::string SerializeValueToJson(const base::Value& value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

// Returns the message type represented by the |message|. This is a convenience
// wrapper that should only be called when the |message| is known to specify its
// message type, i.e. this should not be called for untrusted input.
std::string GetMessageType(const base::DictionaryValue& message) {
  std::string type;
  message.GetString(kTypeKey, &type);
  return type;
}

}  // namespace

ClientImpl::ClientImpl(scoped_ptr<Connection> connection,
                       scoped_ptr<SecureContext> secure_context)
    : connection_(connection.Pass()),
      secure_context_(secure_context.Pass()),
      weak_ptr_factory_(this) {
  DCHECK(connection_->IsConnected());
  connection_->AddObserver(this);
}

ClientImpl::~ClientImpl() {
  if (connection_)
    connection_->RemoveObserver(this);
}

void ClientImpl::AddObserver(ClientObserver* observer) {
  observers_.AddObserver(observer);
}

void ClientImpl::RemoveObserver(ClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ClientImpl::SupportsSignIn() const {
  return (secure_context_->GetProtocolVersion() ==
          SecureContext::PROTOCOL_VERSION_THREE_ONE);
}

void ClientImpl::DispatchUnlockEvent() {
  base::DictionaryValue message;
  message.SetString(kTypeKey, kMessageTypeLocalEvent);
  message.SetString(kNameKey, kUnlockEventName);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

void ClientImpl::RequestDecryption(const std::string& challenge) {
  if (!SupportsSignIn()) {
    PA_LOG(WARNING) << "Dropping decryption request, as remote device "
                    << "does not support protocol v3.1.";
    FOR_EACH_OBSERVER(ClientObserver, observers_,
                      OnDecryptResponse(scoped_ptr<std::string>()));
    return;
  }

  // TODO(isherman): Compute the encrypted message data for realz.
  const std::string encrypted_message_data = challenge;
  std::string encrypted_message_data_base64;
  Base64UrlEncode(encrypted_message_data, &encrypted_message_data_base64);

  base::DictionaryValue message;
  message.SetString(kTypeKey, kMessageTypeDecryptRequest);
  message.SetString(kEncryptedDataKey, encrypted_message_data_base64);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

void ClientImpl::RequestUnlock() {
  if (!SupportsSignIn()) {
    PA_LOG(WARNING) << "Dropping unlock request, as remote device does not "
                    << "support protocol v3.1.";
    FOR_EACH_OBSERVER(ClientObserver, observers_, OnUnlockResponse(false));
    return;
  }

  base::DictionaryValue message;
  message.SetString(kTypeKey, kMessageTypeUnlockRequest);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

ClientImpl::PendingMessage::PendingMessage() {
}

ClientImpl::PendingMessage::PendingMessage(const base::DictionaryValue& message)
    : json_message(SerializeValueToJson(message)),
      type(GetMessageType(message)) {
}

ClientImpl::PendingMessage::~PendingMessage() {
}

void ClientImpl::ProcessMessageQueue() {
  if (pending_message_ || queued_messages_.empty() ||
      connection_->is_sending_message())
    return;

  pending_message_.reset(new PendingMessage(queued_messages_.front()));
  queued_messages_.pop_front();

  secure_context_->Encode(pending_message_->json_message,
                          base::Bind(&ClientImpl::OnMessageEncoded,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void ClientImpl::OnMessageEncoded(const std::string& encoded_message) {
  connection_->SendMessage(make_scoped_ptr(new WireMessage(encoded_message)));
}

void ClientImpl::OnMessageDecoded(const std::string& decoded_message) {
  // The decoded message should be a JSON string.
  scoped_ptr<base::Value> message_value =
      base::JSONReader::Read(decoded_message);
  if (!message_value || !message_value->IsType(base::Value::TYPE_DICTIONARY)) {
    PA_LOG(ERROR) << "Unable to parse message as JSON:\n" << decoded_message;
    return;
  }

  base::DictionaryValue* message;
  bool success = message_value->GetAsDictionary(&message);
  DCHECK(success);

  std::string type;
  if (!message->GetString(kTypeKey, &type)) {
    PA_LOG(ERROR) << "Missing '" << kTypeKey << "' key in message:\n "
                  << decoded_message;
    return;
  }

  // Remote status updates can be received out of the blue.
  if (type == kMessageTypeRemoteStatusUpdate) {
    HandleRemoteStatusUpdateMessage(*message);
    return;
  }

  // All other messages should only be received in response to a message that
  // the client sent.
  if (!pending_message_) {
    PA_LOG(WARNING) << "Unexpected message received:\n" << decoded_message;
    return;
  }

  std::string expected_type;
  if (pending_message_->type == kMessageTypeDecryptRequest)
    expected_type = kMessageTypeDecryptResponse;
  else if (pending_message_->type == kMessageTypeUnlockRequest)
    expected_type = kMessageTypeUnlockResponse;
  else
    NOTREACHED();  // There are no other message types that expect a response.

  if (type != expected_type) {
    PA_LOG(ERROR) << "Unexpected '" << kTypeKey << "' value in message. "
                  << "Expected '" << expected_type << "' but received '" << type
                  << "'.";
    return;
  }

  if (type == kMessageTypeDecryptResponse)
    HandleDecryptResponseMessage(*message);
  else if (type == kMessageTypeUnlockResponse)
    HandleUnlockResponseMessage(*message);
  else
    NOTREACHED();  // There are no other message types that expect a response.

  pending_message_.reset();
  ProcessMessageQueue();
}

void ClientImpl::HandleRemoteStatusUpdateMessage(
    const base::DictionaryValue& message) {
  scoped_ptr<RemoteStatusUpdate> status_update =
      RemoteStatusUpdate::Deserialize(message);
  if (!status_update) {
    PA_LOG(ERROR) << "Unexpected remote status update: " << message;
    return;
  }

  FOR_EACH_OBSERVER(ClientObserver, observers_,
                    OnRemoteStatusUpdate(*status_update));
}

void ClientImpl::HandleDecryptResponseMessage(
    const base::DictionaryValue& message) {
  std::string base64_data;
  std::string decrypted_data;
  scoped_ptr<std::string> response;
  if (!message.GetString(kDataKey, &base64_data) || base64_data.empty()) {
    PA_LOG(ERROR) << "Decrypt response missing '" << kDataKey << "' value.";
  } else if (!Base64UrlDecode(base64_data, &decrypted_data)) {
    PA_LOG(ERROR) << "Unable to base64-decode decrypt response.";
  } else {
    response.reset(new std::string(decrypted_data));
  }
  FOR_EACH_OBSERVER(ClientObserver, observers_,
                    OnDecryptResponse(response.Pass()));
}

void ClientImpl::HandleUnlockResponseMessage(
    const base::DictionaryValue& message) {
  FOR_EACH_OBSERVER(ClientObserver, observers_, OnUnlockResponse(true));
}

void ClientImpl::OnConnectionStatusChanged(Connection* connection,
                                           Connection::Status old_status,
                                           Connection::Status new_status) {
  DCHECK_EQ(connection, connection_.get());
  if (new_status == Connection::DISCONNECTED) {
    PA_LOG(INFO) << "Secure channel disconnected...";
    connection_->RemoveObserver(this);
    connection_.reset();
    FOR_EACH_OBSERVER(ClientObserver, observers_, OnDisconnected());
    // TODO(isherman): Determine whether it's also necessary/appropriate to fire
    // this notification from the destructor.
  }
}

void ClientImpl::OnMessageReceived(const Connection& connection,
                                   const WireMessage& wire_message) {
  secure_context_->Decode(wire_message.payload(),
                          base::Bind(&ClientImpl::OnMessageDecoded,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void ClientImpl::OnSendCompleted(const Connection& connection,
                                 const WireMessage& wire_message,
                                 bool success) {
  if (!pending_message_) {
    PA_LOG(ERROR) << "Unexpected message sent.";
    return;
  }

  // In the common case, wait for a response from the remote device.
  // Don't wait if the message could not be sent, as there won't ever be a
  // response in that case. Likewise, don't wait for a response to local
  // event messages, as there is no response for such messages.
  if (success && pending_message_->type != kMessageTypeLocalEvent)
    return;

  // Notify observer of failure if sending the message fails.
  // For local events, we don't expect a response, so on success, we
  // notify observers right away.
  if (pending_message_->type == kMessageTypeDecryptRequest) {
    FOR_EACH_OBSERVER(ClientObserver, observers_,
                      OnDecryptResponse(scoped_ptr<std::string>()));
  } else if (pending_message_->type == kMessageTypeUnlockRequest) {
    FOR_EACH_OBSERVER(ClientObserver, observers_, OnUnlockResponse(false));
  } else if (pending_message_->type == kMessageTypeLocalEvent) {
    FOR_EACH_OBSERVER(ClientObserver, observers_, OnUnlockEventSent(success));
  } else {
    PA_LOG(ERROR) << "Message of unknown type '" << pending_message_->type
                  << "' sent.";
  }

  pending_message_.reset();
  ProcessMessageQueue();
}

}  // namespace proximity_auth
