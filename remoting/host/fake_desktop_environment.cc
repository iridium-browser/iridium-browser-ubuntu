// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_desktop_environment.h"

#include "remoting/host/audio_capturer.h"
#include "remoting/host/fake_desktop_capturer.h"
#include "remoting/host/gnubby_auth_handler.h"
#include "remoting/host/input_injector.h"

namespace remoting {

FakeInputInjector::FakeInputInjector() {}
FakeInputInjector::~FakeInputInjector() {}

void FakeInputInjector::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
}

void FakeInputInjector::InjectKeyEvent(const protocol::KeyEvent& event) {
}

void FakeInputInjector::InjectTextEvent(const protocol::TextEvent& event) {
}

void FakeInputInjector::InjectMouseEvent(const protocol::MouseEvent& event) {
}

void FakeInputInjector::InjectTouchEvent(const protocol::TouchEvent& event) {
}

void FakeInputInjector::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
}

FakeScreenControls::FakeScreenControls() {}
FakeScreenControls::~FakeScreenControls() {}

void FakeScreenControls::SetScreenResolution(
    const ScreenResolution& resolution) {
}

FakeDesktopEnvironment::FakeDesktopEnvironment() {}

FakeDesktopEnvironment::~FakeDesktopEnvironment() {}

// DesktopEnvironment implementation.
scoped_ptr<AudioCapturer> FakeDesktopEnvironment::CreateAudioCapturer() {
  return nullptr;
}

scoped_ptr<InputInjector> FakeDesktopEnvironment::CreateInputInjector() {
  return make_scoped_ptr(new FakeInputInjector());
}

scoped_ptr<ScreenControls> FakeDesktopEnvironment::CreateScreenControls() {
  return make_scoped_ptr(new FakeScreenControls());
}

scoped_ptr<webrtc::DesktopCapturer>
FakeDesktopEnvironment::CreateVideoCapturer() {
  scoped_ptr<FakeDesktopCapturer> result(new FakeDesktopCapturer());
  if (!frame_generator_.is_null())
    result->set_frame_generator(frame_generator_);
  return result.Pass();
}

scoped_ptr<webrtc::MouseCursorMonitor>
FakeDesktopEnvironment::CreateMouseCursorMonitor() {
  return make_scoped_ptr(new FakeMouseCursorMonitor());
}

std::string FakeDesktopEnvironment::GetCapabilities() const {
  return std::string();
}

void FakeDesktopEnvironment::SetCapabilities(const std::string& capabilities) {}

scoped_ptr<GnubbyAuthHandler> FakeDesktopEnvironment::CreateGnubbyAuthHandler(
    protocol::ClientStub* client_stub) {
  return nullptr;
}

FakeDesktopEnvironmentFactory::FakeDesktopEnvironmentFactory() {}
FakeDesktopEnvironmentFactory::~FakeDesktopEnvironmentFactory() {}

// DesktopEnvironmentFactory implementation.
scoped_ptr<DesktopEnvironment> FakeDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  scoped_ptr<FakeDesktopEnvironment> result(new FakeDesktopEnvironment());
  result->set_frame_generator(frame_generator_);
  return result.Pass();
}

void FakeDesktopEnvironmentFactory::SetEnableCurtaining(bool enable) {}

bool FakeDesktopEnvironmentFactory::SupportsAudioCapture() const {
  return false;
}

void FakeDesktopEnvironmentFactory::SetEnableGnubbyAuth(bool enable) {}


}  // namespace remoting
