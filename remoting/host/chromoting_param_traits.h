// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_PARAM_TRAITS_H_
#define REMOTING_HOST_CHROMOTING_PARAM_TRAITS_H_

#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"
#include "remoting/host/screen_resolution.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace IPC {

template <>
struct ParamTraits<webrtc::DesktopVector> {
  typedef webrtc::DesktopVector param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webrtc::DesktopSize> {
  typedef webrtc::DesktopSize param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webrtc::DesktopRect> {
  typedef webrtc::DesktopRect param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webrtc::MouseCursor> {
  typedef webrtc::MouseCursor param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<remoting::ScreenResolution> {
  typedef remoting::ScreenResolution param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // REMOTING_HOST_CHROMOTING_PARAM_TRAITS_H_
