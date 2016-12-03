// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_DEV_TOOLS_SERVER_H_
#define ANDROID_WEBVIEW_NATIVE_AW_DEV_TOOLS_SERVER_H_

#include <jni.h>

#include <memory>
#include <vector>

#include "base/macros.h"

namespace devtools_http_handler {
class DevToolsHttpHandler;
}

namespace android_webview {

// This class controls WebView-specific Developer Tools remote debugging server.
class AwDevToolsServer {
 public:
  AwDevToolsServer();
  ~AwDevToolsServer();

  // Opens linux abstract socket to be ready for remote debugging.
  void Start();

  // Closes debugging socket, stops debugging.
  void Stop();

  bool IsStarted() const;

 private:
  std::unique_ptr<devtools_http_handler::DevToolsHttpHandler>
      devtools_http_handler_;

  DISALLOW_COPY_AND_ASSIGN(AwDevToolsServer);
};

bool RegisterAwDevToolsServer(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_DEV_TOOLS_SERVER_H_
