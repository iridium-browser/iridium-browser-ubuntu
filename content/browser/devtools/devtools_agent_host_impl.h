// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"

namespace IPC {
class Message;
}

namespace content {

class BrowserContext;
class DevToolsProtocolHandler;

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  // Informs the hosted agent that a client host has attached.
  virtual void Attach() = 0;

  // Informs the hosted agent that a client host has detached.
  virtual void Detach() = 0;

  // Sends a message to the agent.
  bool DispatchProtocolMessage(const std::string& message) override;

  // Opens the inspector for this host.
  void Inspect(BrowserContext* browser_context);

  // DevToolsAgentHost implementation.
  void AttachClient(DevToolsAgentHostClient* client) override;
  void DetachClient() override;
  bool IsAttached() override;
  void InspectElement(int x, int y) override;
  std::string GetId() override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* wc) override;

 protected:
  DevToolsAgentHostImpl();
  ~DevToolsAgentHostImpl() override;

  scoped_ptr<DevToolsProtocolHandler> protocol_handler_;

  void set_handle_all_protocol_commands() { handle_all_commands_ = true; }
  void HostClosed();
  void SendMessageToClient(const std::string& message);
  static void NotifyCallbacks(DevToolsAgentHostImpl* agent_host, bool attached);

 private:
  friend class DevToolsAgentHost; // for static methods

  const std::string id_;
  DevToolsAgentHostClient* client_;
  bool handle_all_commands_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
