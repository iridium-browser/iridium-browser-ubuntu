// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/keep_alive_impl.h"

#include <utility>

#include "base/bind.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"

namespace extensions {

// static
void KeepAliveImpl::Create(content::BrowserContext* context,
                           const Extension* extension,
                           mojo::InterfaceRequest<KeepAlive> request) {
  new KeepAliveImpl(context, extension, std::move(request));
}

KeepAliveImpl::KeepAliveImpl(content::BrowserContext* context,
                             const Extension* extension,
                             mojo::InterfaceRequest<KeepAlive> request)
    : context_(context),
      extension_(extension),
      extension_registry_observer_(this),
      binding_(this, std::move(request)) {
  ProcessManager::Get(context_)->IncrementLazyKeepaliveCount(extension_);
  binding_.set_connection_error_handler(
      base::Bind(&KeepAliveImpl::OnDisconnected, base::Unretained(this)));
  extension_registry_observer_.Add(ExtensionRegistry::Get(context_));
}

KeepAliveImpl::~KeepAliveImpl() = default;

void KeepAliveImpl::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (browser_context == context_ && extension == extension_)
    delete this;
}

void KeepAliveImpl::OnShutdown(ExtensionRegistry* registry) {
  delete this;
}

void KeepAliveImpl::OnDisconnected() {
  ProcessManager::Get(context_)->DecrementLazyKeepaliveCount(extension_);
}

}  // namespace extensions
