// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_UTIL_H_
#define EXTENSIONS_BROWSER_EXTENSION_UTIL_H_

#include <string>

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {

struct ExtensionInfo;

namespace util {

// TODO(tmdiep): Move functions from
// chrome/browser/extensions/extension_util.h/cc that are only dependent on
// extensions/ here.

// Returns true if |extension_id| identifies an extension that is installed
// permanently and not ephemerally.
bool IsExtensionInstalledPermanently(const std::string& extension_id,
                                     content::BrowserContext* context);

// Returns true if |extension_id| identifies an ephemeral app.
bool IsEphemeralApp(const std::string& extension_id,
                    content::BrowserContext* context);

// Returns true if the extension has isolated storage.
bool HasIsolatedStorage(const ExtensionInfo& info);

// Returns true if the site URL corresponds to an extension or app and has
// isolated storage.
bool SiteHasIsolatedStorage(const GURL& extension_site_url,
                            content::BrowserContext* context);

}  // namespace util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_UTIL_H_
