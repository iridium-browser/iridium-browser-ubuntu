// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/audio/audio_output_ipc.h"
#include "media/base/audio_hardware_config.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// MessageFilter that handles audio messages and delegates them to audio
// renderers. Created on render thread, AudioMessageFilter is operated on
// IO thread (secondary thread of render process) it intercepts audio messages
// and process them on IO thread since these messages are time critical.
class CONTENT_EXPORT AudioMessageFilter : public IPC::MessageFilter {
 public:
  explicit AudioMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Getter for the one AudioMessageFilter object.
  static AudioMessageFilter* Get();

  // Create an AudioOutputIPC to be owned by one delegate.  |render_frame_id| is
  // the RenderFrame containing the entity producing the audio.
  //
  // The returned object is not thread-safe, and must be used on
  // |io_message_loop|.
  scoped_ptr<media::AudioOutputIPC> CreateAudioOutputIPC(int render_frame_id);

  // IO message loop associated with this message filter.
  scoped_refptr<base::MessageLoopProxy> io_message_loop() const {
    return io_message_loop_;
  }

 protected:
  ~AudioMessageFilter() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Delegates);

  // Implementation of media::AudioOutputIPC which augments IPC calls with
  // stream_id and the source render_frame_id.
  class AudioOutputIPCImpl;

  // Sends an IPC message using |sender_|.
  void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on |io_message_loop|.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

  // Received when browser process has created an audio output stream.
  void OnStreamCreated(int stream_id, base::SharedMemoryHandle handle,
                       base::SyncSocket::TransitDescriptor socket_descriptor,
                       uint32 length);

  // Received when internal state of browser process' audio output device has
  // changed.
  void OnStreamStateChanged(int stream_id,
                            media::AudioOutputIPCDelegate::State state);

  // IPC sender for Send(); must only be accesed on |io_message_loop_|.
  IPC::Sender* sender_;

  // A map of stream ids to delegates; must only be accessed on
  // |io_message_loop_|.
  IDMap<media::AudioOutputIPCDelegate> delegates_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // The singleton instance for this filter.
  static AudioMessageFilter* g_filter;

  DISALLOW_COPY_AND_ASSIGN(AudioMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
