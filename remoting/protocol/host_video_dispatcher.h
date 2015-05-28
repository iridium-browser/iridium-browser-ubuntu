// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/protobuf_message_parser.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

class VideoFeedbackStub;

class HostVideoDispatcher : public ChannelDispatcherBase, public VideoStub {
 public:
  HostVideoDispatcher();
  ~HostVideoDispatcher() override;

  void set_video_feedback_stub(VideoFeedbackStub* video_feedback_stub) {
    video_feedback_stub_ = video_feedback_stub;
  }

  // VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                          const base::Closure& done) override;

 private:
  void OnVideoAck(scoped_ptr<VideoAck> ack, const base::Closure& done);

  ProtobufMessageParser<VideoAck> parser_;

  VideoFeedbackStub* video_feedback_stub_;

  DISALLOW_COPY_AND_ASSIGN(HostVideoDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_
