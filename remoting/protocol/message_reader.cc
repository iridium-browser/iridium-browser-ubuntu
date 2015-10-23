// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/message_reader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "base/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/p2p_stream_socket.h"

namespace remoting {
namespace protocol {

static const int kReadBufferSize = 4096;

MessageReader::MessageReader()
    : socket_(nullptr),
      read_pending_(false),
      pending_messages_(0),
      closed_(false),
      weak_factory_(this) {
}

MessageReader::~MessageReader() {
}

void MessageReader::SetMessageReceivedCallback(
    const MessageReceivedCallback& callback) {
  DCHECK(CalledOnValidThread());
  message_received_callback_ = callback;
}

void MessageReader::StartReading(
    P2PStreamSocket* socket,
    const ReadFailedCallback& read_failed_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket);
  DCHECK(!read_failed_callback.is_null());

  socket_ = socket;
  read_failed_callback_ = read_failed_callback;
  DoRead();
}

void MessageReader::DoRead() {
  DCHECK(CalledOnValidThread());
  // Don't try to read again if there is another read pending or we
  // have messages that we haven't finished processing yet.
  bool read_succeeded = true;
  while (read_succeeded && !closed_ && !read_pending_ &&
         pending_messages_ == 0) {
    read_buffer_ = new net::IOBuffer(kReadBufferSize);
    int result = socket_->Read(
        read_buffer_.get(),
        kReadBufferSize,
        base::Bind(&MessageReader::OnRead, weak_factory_.GetWeakPtr()));

    HandleReadResult(result, &read_succeeded);
  }
}

void MessageReader::OnRead(int result) {
  DCHECK(CalledOnValidThread());
  DCHECK(read_pending_);
  read_pending_ = false;

  if (!closed_) {
    bool read_succeeded;
    HandleReadResult(result, &read_succeeded);
    if (read_succeeded)
      DoRead();
  }
}

void MessageReader::HandleReadResult(int result, bool* read_succeeded) {
  DCHECK(CalledOnValidThread());
  if (closed_)
    return;

  *read_succeeded = true;

  if (result > 0) {
    OnDataReceived(read_buffer_.get(), result);
    *read_succeeded = true;
  } else if (result == net::ERR_IO_PENDING) {
    read_pending_ = true;
  } else {
    DCHECK_LT(result, 0);

    // Stop reading after any error.
    closed_ = true;
    *read_succeeded = false;

    LOG(ERROR) << "Read() returned error " << result;
    read_failed_callback_.Run(result);
  }
}

void MessageReader::OnDataReceived(net::IOBuffer* data, int data_size) {
  DCHECK(CalledOnValidThread());
  message_decoder_.AddData(data, data_size);

  // Get list of all new messages first, and then call the callback
  // for all of them.
  while (true) {
    CompoundBuffer* buffer = message_decoder_.GetNextMessage();
    if (!buffer)
      break;
    pending_messages_++;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&MessageReader::RunCallback,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(make_scoped_ptr(buffer))));
  }
}

void MessageReader::RunCallback(scoped_ptr<CompoundBuffer> message) {
  if (!message_received_callback_.is_null()){
    message_received_callback_.Run(
        message.Pass(),
        base::Bind(&MessageReader::OnMessageDone, weak_factory_.GetWeakPtr()));
  }
}

void MessageReader::OnMessageDone() {
  DCHECK(CalledOnValidThread());
  pending_messages_--;
  DCHECK_GE(pending_messages_, 0);

  // Start next read if necessary.
  DoRead();
}

}  // namespace protocol
}  // namespace remoting
