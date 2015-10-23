// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"

namespace extensions {

// static
ExtensionPrefs* ExtensionPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionPrefsFactory* ExtensionPrefsFactory::GetInstance() {
  return Singleton<ExtensionPrefsFactory>::get();
}

void ExtensionPrefsFactory::SetInstanceForTesting(
    content::BrowserContext* context,
    scoped_ptr<ExtensionPrefs> prefs) {
  Disassociate(context);
  Associate(context, prefs.Pass());
}

ExtensionPrefsFactory::ExtensionPrefsFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionPrefs",
          BrowserContextDependencyManager::GetInstance()) {
}

ExtensionPrefsFactory::~ExtensionPrefsFactory() {
}

KeyedService* ExtensionPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  std::vector<ExtensionPrefsObserver*> prefs_observers;
  client->GetEarlyExtensionPrefsObservers(context, &prefs_observers);
  return ExtensionPrefs::Create(
      context, client->GetPrefServiceForContext(context),
      context->GetPath().AppendASCII(extensions::kInstallDirectoryName),
      ExtensionPrefValueMapFactory::GetForBrowserContext(context),
      client->AreExtensionsDisabled(*base::CommandLine::ForCurrentProcess(),
                                    context),
      prefs_observers);
}

content::BrowserContext* ExtensionPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
