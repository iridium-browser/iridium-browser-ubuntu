// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace extensions {
class PermissionIDSet;
class URLPatternSet;
}

namespace permission_message_util {

enum PermissionMessageProperties { kReadOnly, kReadWrite };

// Get a list of hosts to display in a permission message from the given list of
// hosts from the manifest.
// TODO(sashab): Merge this into AddHostPermissions().
std::vector<base::string16> GetHostListFromHosts(
    const std::set<std::string>& hosts,
    PermissionMessageProperties properties);

// Adds the appropriate permissions from given hosts to |permissions|.
void AddHostPermissions(extensions::PermissionIDSet* permissions,
                        const std::set<std::string>& hosts,
                        PermissionMessageProperties properties);

std::set<std::string> GetDistinctHosts(
    const extensions::URLPatternSet& host_patterns,
    bool include_rcd,
    bool exclude_file_scheme);

}  // namespace permission_message_util

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_
