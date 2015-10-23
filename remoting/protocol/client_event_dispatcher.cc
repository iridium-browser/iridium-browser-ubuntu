// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_event_dispatcher.h"

#include "base/time/time.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {
namespace protocol {

ClientEventDispatcher::ClientEventDispatcher()
    : ChannelDispatcherBase(kEventChannelName) {
}

ClientEventDispatcher::~ClientEventDispatcher() {
}

void ClientEventDispatcher::InjectKeyEvent(const KeyEvent& event) {
  DCHECK(event.has_usb_keycode());
  DCHECK(event.has_pressed());
  EventMessage message;
  message.set_timestamp(base::Time::Now().ToInternalValue());
  message.mutable_key_event()->CopyFrom(event);
  writer()->Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientEventDispatcher::InjectTextEvent(const TextEvent& event) {
  DCHECK(event.has_text());
  EventMessage message;
  message.set_timestamp(base::Time::Now().ToInternalValue());
  message.mutable_text_event()->CopyFrom(event);
  writer()->Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientEventDispatcher::InjectMouseEvent(const MouseEvent& event) {
  EventMessage message;
  message.set_timestamp(base::Time::Now().ToInternalValue());
  message.mutable_mouse_event()->CopyFrom(event);
  writer()->Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientEventDispatcher::InjectTouchEvent(const TouchEvent& event) {
  EventMessage message;
  message.set_timestamp(base::Time::Now().ToInternalValue());
  message.mutable_touch_event()->CopyFrom(event);
  writer()->Write(SerializeAndFrameMessage(message), base::Closure());
}

}  // namespace protocol
}  // namespace remoting
