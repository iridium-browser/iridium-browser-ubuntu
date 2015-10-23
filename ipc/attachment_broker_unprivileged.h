// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_
#define IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_

#include "ipc/attachment_broker.h"
#include "ipc/ipc_export.h"

namespace IPC {

class Endpoint;
class Sender;

// This abstract subclass of AttachmentBroker is intended for use in
// non-privileged processes.
class IPC_EXPORT AttachmentBrokerUnprivileged : public IPC::AttachmentBroker {
 public:
  AttachmentBrokerUnprivileged();
  ~AttachmentBrokerUnprivileged() override;

  // In each unprivileged process, exactly one channel should be used to
  // communicate brokerable attachments with the broker process.
  void DesignateBrokerCommunicationChannel(Endpoint* endpoint);

 protected:
  IPC::Sender* get_sender() { return sender_; }

 private:
  // |sender_| is used to send Messages to the privileged broker process.
  // |sender_| must live at least as long as this instance.
  IPC::Sender* sender_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerUnprivileged);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_
