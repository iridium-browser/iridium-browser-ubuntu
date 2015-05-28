// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/base/encryptor.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "net/base/ip_endpoint.h"

namespace gcm {

FakeGCMClient::FakeGCMClient(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread)
    : delegate_(NULL),
      started_(false),
      start_mode_(DELAYED_START),
      start_mode_overridding_(RESPECT_START_MODE),
      ui_thread_(ui_thread),
      io_thread_(io_thread),
      weak_ptr_factory_(this) {
}

FakeGCMClient::~FakeGCMClient() {
}

void FakeGCMClient::Initialize(
    const ChromeBuildInfo& chrome_build_info,
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    scoped_ptr<Encryptor> encryptor,
    Delegate* delegate) {
  delegate_ = delegate;
}

void FakeGCMClient::Start(StartMode start_mode) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  if (started_)
    return;

  if (start_mode == IMMEDIATE_START)
    start_mode_ = IMMEDIATE_START;
  if (start_mode_ == DELAYED_START ||
      start_mode_overridding_ == FORCE_TO_ALWAYS_DELAY_START_GCM) {
    return;
  }

  DoStart();
}

void FakeGCMClient::DoStart() {
  started_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMClient::Started,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FakeGCMClient::Stop() {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());
  started_ = false;
  delegate_->OnDisconnected();
}

void FakeGCMClient::Register(const std::string& app_id,
                             const std::vector<std::string>& sender_ids) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  std::string registration_id = GetRegistrationIdFromSenderIds(sender_ids);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMClient::RegisterFinished,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id,
                 registration_id));
}

void FakeGCMClient::Unregister(const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMClient::UnregisterFinished,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id));
}

void FakeGCMClient::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMClient::SendFinished,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id,
                 message));
}

void FakeGCMClient::SetRecording(bool recording) {
}

void FakeGCMClient::ClearActivityLogs() {
}

GCMClient::GCMStatistics FakeGCMClient::GetStatistics() const {
  return GCMClient::GCMStatistics();
}

void FakeGCMClient::SetAccountTokens(
    const std::vector<AccountTokenInfo>& account_tokens) {
}

void FakeGCMClient::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
}

void FakeGCMClient::RemoveAccountMapping(const std::string& account_id) {
}

void FakeGCMClient::SetLastTokenFetchTime(const base::Time& time) {
}

void FakeGCMClient::UpdateHeartbeatTimer(scoped_ptr<base::Timer> timer) {
}

void FakeGCMClient::PerformDelayedStart() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMClient::DoStart, weak_ptr_factory_.GetWeakPtr()));
}

void FakeGCMClient::ReceiveMessage(const std::string& app_id,
                                   const IncomingMessage& message) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMClient::MessageReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id,
                 message));
}

void FakeGCMClient::DeleteMessages(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMClient::MessagesDeleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id));
}

std::string FakeGCMClient::GetRegistrationIdFromSenderIds(
    const std::vector<std::string>& sender_ids) const {
  // GCMService normalizes the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  // Simulate the registration_id by concaternating all sender IDs.
  // Set registration_id to empty to denote an error if sender_ids contains a
  // hint.
  std::string registration_id;
  if (sender_ids.size() != 1 ||
      sender_ids[0].find("error") == std::string::npos) {
    for (size_t i = 0; i < normalized_sender_ids.size(); ++i) {
      if (i > 0)
        registration_id += ",";
      registration_id += normalized_sender_ids[i];
    }
  }
  return registration_id;
}

void FakeGCMClient::Started() {
  delegate_->OnGCMReady(std::vector<AccountMapping>(), base::Time());
  delegate_->OnConnected(net::IPEndPoint());
}

void FakeGCMClient::RegisterFinished(const std::string& app_id,
                                     const std::string& registrion_id) {
  delegate_->OnRegisterFinished(
      app_id, registrion_id, registrion_id.empty() ? SERVER_ERROR : SUCCESS);
}

void FakeGCMClient::UnregisterFinished(const std::string& app_id) {
  delegate_->OnUnregisterFinished(app_id, GCMClient::SUCCESS);
}

void FakeGCMClient::SendFinished(const std::string& app_id,
                                 const OutgoingMessage& message) {
  delegate_->OnSendFinished(app_id, message.id, SUCCESS);

  // Simulate send error if message id contains a hint.
  if (message.id.find("error") != std::string::npos) {
    SendErrorDetails send_error_details;
    send_error_details.message_id = message.id;
    send_error_details.result = NETWORK_ERROR;
    send_error_details.additional_data = message.data;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeGCMClient::MessageSendError,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   send_error_details),
        base::TimeDelta::FromMilliseconds(200));
  } else if(message.id.find("ack") != std::string::npos) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeGCMClient::SendAcknowledgement,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   message.id),
        base::TimeDelta::FromMilliseconds(200));

  }
}

void FakeGCMClient::MessageReceived(const std::string& app_id,
                                    const IncomingMessage& message) {
  if (delegate_)
    delegate_->OnMessageReceived(app_id, message);
}

void FakeGCMClient::MessagesDeleted(const std::string& app_id) {
  if (delegate_)
    delegate_->OnMessagesDeleted(app_id);
}

void FakeGCMClient::MessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  if (delegate_)
    delegate_->OnMessageSendError(app_id, send_error_details);
}

void FakeGCMClient::SendAcknowledgement(const std::string& app_id,
                                        const std::string& message_id) {
  if (delegate_)
    delegate_->OnSendAcknowledged(app_id, message_id);
}

}  // namespace gcm
