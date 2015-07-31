// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/devtools_agent_host.h"
#endif  // !defined(OS_ANDROID)

ChromeDevToolsManagerDelegate::ChromeDevToolsManagerDelegate()
    : network_protocol_handler_(new DevToolsNetworkProtocolHandler()) {
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::BrowserContext* browser_context,
    content::DevToolsAgentHost* agent_host) {
#if !defined(OS_ANDROID)
  content::DevToolsAgentHost::Type type = agent_host->GetType();
  if (type != content::DevToolsAgentHost::TYPE_SHARED_WORKER &&
      type != content::DevToolsAgentHost::TYPE_SERVICE_WORKER) {
    // TODO(horo): Support other types of DevToolsAgentHost when necessary.
    NOTREACHED() << "Inspect() only supports workers.";
  }
  if (Profile* profile = Profile::FromBrowserContext(browser_context))
    DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host);
#endif  // !defined(OS_ANDROID)
}

base::DictionaryValue* ChromeDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  return network_protocol_handler_->HandleCommand(agent_host, command_dict);
}

void ChromeDevToolsManagerDelegate::DevToolsAgentStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  network_protocol_handler_->DevToolsAgentStateChanged(agent_host, attached);
}
