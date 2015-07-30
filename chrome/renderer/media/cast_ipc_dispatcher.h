// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_IPC_DISPATCHER_H_
#define CHROME_RENDERER_MEDIA_CAST_IPC_DISPATCHER_H_

#include "base/callback.h"
#include "base/id_map.h"
#include "base/thread_task_runner_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/message_filter.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_sender.h"

class CastTransportSenderIPC;

// This dispatcher listens to incoming IPC messages and sends
// the call to the correct CastTransportSenderIPC instance.
class CastIPCDispatcher : public IPC::MessageFilter {
 public:
  explicit CastIPCDispatcher(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  static CastIPCDispatcher* Get();
  void Send(IPC::Message* message);
  int32 AddSender(CastTransportSenderIPC* sender);
  void RemoveSender(int32 channel_id);

  // IPC::MessageFilter implementation
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

 protected:
  ~CastIPCDispatcher() override;

 private:
  void OnNotifyStatusChange(
      int32 channel_id,
      media::cast::CastTransportStatus status);
  void OnRtpStatistics(
      int32 channel_id,
      bool audio,
      const media::cast::RtcpSenderInfo& sender_info,
      base::TimeTicks time_sent,
      uint32 rtp_timestamp);
  void OnRawEvents(int32 channel_id,
                   const std::vector<media::cast::PacketEvent>& packet_events,
                   const std::vector<media::cast::FrameEvent>& frame_events);
  void OnRtt(int32 channel_id, uint32 ssrc, base::TimeDelta rtt);
  void OnRtcpCastMessage(int32 channel_id,
                         uint32 ssrc,
                         const media::cast::RtcpCastMessage& cast_message);
  void OnReceivedPacket(int32 channel_id, const media::cast::Packet& packet);

  static CastIPCDispatcher* global_instance_;

  // For IPC Send(); must only be accesed on |io_message_loop_|.
  IPC::Sender* sender_;

  // Task runner on which IPC calls are driven.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // A map of stream ids to delegates; must only be accessed on
  // |io_message_loop_|.
  IDMap<CastTransportSenderIPC> id_map_;
  DISALLOW_COPY_AND_ASSIGN(CastIPCDispatcher);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_IPC_DISPATCHER_H_
