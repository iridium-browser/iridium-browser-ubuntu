// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/base_paths.h"
#include "base/debug/profiler.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "components/version_info/version_info.h"

namespace chrome {

std::string GetChannelString() {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 VersionInfo::GetVersionStringModifier"));

#if defined(GOOGLE_CHROME_BUILD)
  base::FilePath module;
  base::string16 channel;
  if (PathService::Get(base::FILE_MODULE, &module)) {
    bool is_system_install = !InstallUtil::IsPerUserInstall(module);
    GoogleUpdateSettings::GetChromeChannelAndModifiers(is_system_install,
                                                       &channel);
  }
#if defined(SYZYASAN)
  if (base::debug::IsBinaryInstrumented())
    channel += L" SyzyASan";
#endif
  return base::UTF16ToASCII(channel);
#else
  return std::string();
#endif
}

version_info::Channel GetChannel() {
#if defined(GOOGLE_CHROME_BUILD)
  std::wstring channel(L"unknown");

  base::FilePath module;
  if (PathService::Get(base::FILE_MODULE, &module)) {
    bool is_system_install = !InstallUtil::IsPerUserInstall(module);
    channel = GoogleUpdateSettings::GetChromeChannel(is_system_install);
  }

  if (channel.empty()) {
    return version_info::Channel::STABLE;
  } else if (channel == L"beta") {
    return version_info::Channel::BETA;
  } else if (channel == L"dev") {
    return version_info::Channel::DEV;
  } else if (channel == L"canary") {
    return version_info::Channel::CANARY;
  }
#endif

  return version_info::Channel::UNKNOWN;
}

}  // namespace chrome
