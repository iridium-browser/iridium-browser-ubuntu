// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_READER_H_
#define IPC_IPC_CHANNEL_READER_H_

#include <set>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "ipc/attachment_broker.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_export.h"

namespace IPC {
namespace internal {

// This class provides common pipe reading functionality for the
// platform-specific IPC channel implementations.
//
// It does the common input buffer management and message dispatch, while the
// platform-specific parts provide the pipe management through a virtual
// interface implemented on a per-platform basis.
//
// Note that there is no "writer" corresponding to this because the code for
// writing to the channel is much simpler and has very little common
// functionality that would benefit from being factored out. If we add
// something like that in the future, it would be more appropriate to add it
// here (and rename appropriately) rather than writing a different class.
class IPC_EXPORT ChannelReader : public SupportsAttachmentBrokering,
                                 public AttachmentBroker::Observer {
 public:
  explicit ChannelReader(Listener* listener);
  virtual ~ChannelReader();

  void set_listener(Listener* listener) { listener_ = listener; }

  // This type is returned by ProcessIncomingMessages to indicate the effect of
  // the method.
  enum DispatchState {
    // All messages were successfully dispatched, or there were no messages to
    // dispatch.
    DISPATCH_FINISHED,
    // There was a channel error.
    DISPATCH_ERROR,
    // Dispatching messages is blocked on receiving more information from the
    // broker.
    DISPATCH_WAITING_ON_BROKER,
  };

  // Call to process messages received from the IPC connection and dispatch
  // them.
  DispatchState ProcessIncomingMessages();

  // Handles asynchronously read data.
  //
  // Optionally call this after returning READ_PENDING from ReadData to
  // indicate that buffer was filled with the given number of bytes of
  // data. See ReadData for more.
  DispatchState AsyncReadComplete(int bytes_read);

  // Returns true if the given message is internal to the IPC implementation,
  // like the "hello" message sent on channel set-up.
  bool IsInternalMessage(const Message& m);

  // Returns true if the given message is an Hello message
  // sent on channel set-up.
  bool IsHelloMessage(const Message& m);

 protected:
  enum ReadState { READ_SUCCEEDED, READ_FAILED, READ_PENDING };

  Listener* listener() const { return listener_; }

  // Populates the given buffer with data from the pipe.
  //
  // Returns the state of the read. On READ_SUCCESS, the number of bytes
  // read will be placed into |*bytes_read| (which can be less than the
  // buffer size). On READ_FAILED, the channel will be closed.
  //
  // If the return value is READ_PENDING, it means that there was no data
  // ready for reading. The implementation is then responsible for either
  // calling AsyncReadComplete with the number of bytes read into the
  // buffer, or ProcessIncomingMessages to try the read again (depending
  // on whether the platform's async I/O is "try again" or "write
  // asynchronously into your buffer").
  virtual ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) = 0;

  // Loads the required file desciptors into the given message. Returns true
  // on success. False means a fatal channel error.
  //
  // This will read from the input_fds_ and read more handles from the FD
  // pipe if necessary.
  virtual bool ShouldDispatchInputMessage(Message* msg) = 0;

  // Overridden by subclasses to get attachments that are sent alongside the IPC
  // channel (as opposed to through a broker).
  // Returns true on success. False means a fatal channel error.
  virtual bool GetNonBrokeredAttachments(Message* msg) = 0;

  // Performs post-dispatch checks. Called when all input buffers are empty,
  // though there could be more data ready to be read from the OS.
  virtual bool DidEmptyInputBuffers() = 0;

  // Handles internal messages, like the hello message sent on channel startup.
  virtual void HandleInternalMessage(const Message& msg) = 0;

  // Exposed for testing purposes only.
  ScopedVector<Message>* get_queued_messages() { return &queued_messages_; }

  // Exposed for testing purposes only.
  virtual void DispatchMessage(Message* m);

  // Get the process ID for the sender of the message.
  virtual base::ProcessId GetSenderPID() = 0;

  // Whether the channel is an endpoint of attachment brokering.
  virtual bool IsAttachmentBrokerEndpoint() = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChannelReaderTest, AttachmentAlreadyBrokered);
  FRIEND_TEST_ALL_PREFIXES(ChannelReaderTest, AttachmentNotYetBrokered);

  typedef std::set<BrokerableAttachment::AttachmentId> AttachmentIdSet;

  // Takes the given data received from the IPC channel, translates it into
  // Messages, and puts them in queued_messages_.
  // As an optimization, after a message is translated, the message is
  // immediately dispatched if able. This prevents an otherwise unnecessary deep
  // copy of the message which is needed to store the message in the message
  // queue.
  bool TranslateInputData(const char* input_data, int input_data_len);

  // Dispatches messages from queued_messages_ to listeners. Successfully
  // dispatched messages are removed from queued_messages_.
  DispatchState DispatchMessages();

  // Attempts to fill in the brokerable attachments of |msg| with information
  // from the Attachment Broker.
  // Returns the set of ids that are still waiting to be brokered.
  AttachmentIdSet GetBrokeredAttachments(Message* msg);

  // AttachmentBroker::Observer overrides.
  void ReceivedBrokerableAttachmentWithId(
      const BrokerableAttachment::AttachmentId& id) override;

  // This class should observe the attachment broker if and only if blocked_ids_
  // is not empty.
  void StartObservingAttachmentBroker();
  void StopObservingAttachmentBroker();

  Listener* listener_;

  // We read from the pipe into this buffer. Managed by DispatchInputData, do
  // not access directly outside that function.
  char input_buf_[Channel::kReadBufferSize];

  // Large messages that span multiple pipe buffers, get built-up using
  // this buffer.
  std::string input_overflow_buf_;

  // These messages are waiting to be dispatched. If this vector is non-empty,
  // then the front Message must be blocked on receiving an attachment from the
  // AttachmentBroker.
  ScopedVector<Message> queued_messages_;

  // If the next message to be processed is blocked by the broker, then this
  // set contains the AttachmentIds that are needed to unblock the message.
  AttachmentIdSet blocked_ids_;

  DISALLOW_COPY_AND_ASSIGN(ChannelReader);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_READER_H_
