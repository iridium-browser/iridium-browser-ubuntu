// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "remoting/host/host_main.h"

// The common entry point for remoting_host.exe and remoting_desktop.exe. In
// order to be really small the app doesn't link against the CRT.
void HostEntryPoint() {
  // CommandLine::Init() ignores the passed parameters on Windows, so it is safe
  // to pass nullptr here.
  int exit_code = remoting::HostMain(0, nullptr);
  ExitProcess(exit_code);
}
