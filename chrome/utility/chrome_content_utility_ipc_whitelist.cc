// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
const uint32_t kMessageWhitelist[] = {
    ChromeUtilityMsg_ImageWriter_Cancel::ID,
    ChromeUtilityMsg_ImageWriter_Write::ID,
    ChromeUtilityMsg_ImageWriter_Verify::ID};
const size_t kMessageWhitelistSize = arraysize(kMessageWhitelist);
#else
// Note: Zero-size arrays are not valid C++.
const uint32_t kMessageWhitelist[] = {0};
const size_t kMessageWhitelistSize = 0;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
