// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/common/chrome_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#endif

namespace crash_keys {

const char kActiveURL[] = "url-chunk";

const char kFontKeyName[] = "font_key_name";

const char kExtensionID[] = "extension-%" PRIuS;
const char kNumExtensionsCount[] = "num-extensions";

const char kShutdownType[] = "shutdown-type";

#if !defined(OS_ANDROID)
const char kGPUVendorID[] = "gpu-venid";
const char kGPUDeviceID[] = "gpu-devid";
#endif
const char kGPUDriverVersion[] = "gpu-driver";
const char kGPUPixelShaderVersion[] = "gpu-psver";
const char kGPUVertexShaderVersion[] = "gpu-vsver";
#if defined(OS_MACOSX)
const char kGPUGLVersion[] = "gpu-glver";
#elif defined(OS_POSIX)
const char kGPUVendor[] = "gpu-gl-vendor";
const char kGPURenderer[] = "gpu-gl-renderer";
#endif

#if defined(OS_WIN)
const char kHungAudioThreadDetails[] = "hung-audio-thread-details";
#endif

const char kPrinterInfo[] = "prn-info-%" PRIuS;

#if defined(OS_CHROMEOS)
const char kNumberOfUsers[] = "num-users";
#endif

#if defined(OS_MACOSX)
namespace mac {

const char kFirstNSException[] = "firstexception";
const char kFirstNSExceptionTrace[] = "firstexception_bt";

const char kLastNSException[] = "lastexception";
const char kLastNSExceptionTrace[] = "lastexception_bt";

const char kNSException[] = "nsexception";
const char kNSExceptionTrace[] = "nsexception_bt";

const char kSendAction[] = "sendaction";

}  // namespace mac
#endif

#if BUILDFLAG(ENABLE_KASKO)
const char kKaskoGuid[] = "kasko-guid";
const char kKaskoEquivalentGuid[] = "kasko-equivalent-guid";
#endif

const char kViewCount[] = "view-count";

const char kZeroEncodeDetails[] = "zero-encode-details";

size_t RegisterChromeCrashKeys() {
  // The following keys may be chunked by the underlying crash logging system,
  // but ultimately constitute a single key-value pair.
  //
  // If you're adding keys here, please also add them to the following lists:
  // 1. //blimp/engine/app/blimp_engine_crash_keys.cc and
  // 2. //chrome/app/chrome_crash_reporter_client_win.cc::
  //    RegisterCrashKeysHelper().
  base::debug::CrashKey fixed_keys[] = {
#if defined(OS_MACOSX) || defined(OS_WIN)
    { kMetricsClientId, kSmallSize },
#else
    { kClientId, kSmallSize },
#endif
    { kChannel, kSmallSize },
    { kActiveURL, kLargeSize },
    { kNumVariations, kSmallSize },
    { kVariations, kLargeSize },
    { kNumExtensionsCount, kSmallSize },
    { kShutdownType, kSmallSize },
#if !defined(OS_ANDROID)
    { kGPUVendorID, kSmallSize },
    { kGPUDeviceID, kSmallSize },
#endif
    { kGPUDriverVersion, kSmallSize },
    { kGPUPixelShaderVersion, kSmallSize },
    { kGPUVertexShaderVersion, kSmallSize },
#if defined(OS_MACOSX)
    { kGPUGLVersion, kSmallSize },
#elif defined(OS_POSIX)
    { kGPUVendor, kSmallSize },
    { kGPURenderer, kSmallSize },
#endif

    // content/:
    { "bad_message_reason", kSmallSize },
    { "discardable-memory-allocated", kSmallSize },
    { "discardable-memory-free", kSmallSize },
    { kFontKeyName, kSmallSize},
    { "ppapi_path", kMediumSize },
    { "subresource_url", kLargeSize },
    { "total-discardable-memory-allocated", kSmallSize },
#if defined(OS_CHROMEOS)
    { kNumberOfUsers, kSmallSize },
#endif
#if defined(OS_MACOSX)
    { mac::kFirstNSException, kMediumSize },
    { mac::kFirstNSExceptionTrace, kMediumSize },
    { mac::kLastNSException, kMediumSize },
    { mac::kLastNSExceptionTrace, kMediumSize },
    { mac::kNSException, kMediumSize },
    { mac::kNSExceptionTrace, kMediumSize },
    { mac::kSendAction, kMediumSize },
    { mac::kZombie, kMediumSize },
    { mac::kZombieTrace, kMediumSize },
    // content/:
    { "channel_error_bt", kMediumSize },
    { "remove_route_bt", kMediumSize },
    { "rwhvm_window", kMediumSize },
    // media/:
#endif
#if BUILDFLAG(ENABLE_KASKO)
    { kKaskoGuid, kSmallSize },
    { kKaskoEquivalentGuid, kSmallSize },
#endif
    { kBug464926CrashKey, kSmallSize },
    { kViewCount, kSmallSize },

    // media/:
#if defined(OS_WIN)
    { kHungAudioThreadDetails, kSmallSize },
#endif
    { kZeroEncodeDetails, kSmallSize },

    // gin/:
    { "v8-ignition", kSmallSize },

    // Temporary for http://crbug.com/575245.
    { "swapout_frame_id", kSmallSize },
    { "swapout_proxy_id", kSmallSize },
    { "swapout_view_id", kSmallSize },
    { "commit_frame_id", kSmallSize },
    { "commit_proxy_id", kSmallSize },
    { "commit_view_id", kSmallSize },
    { "commit_main_render_frame_id", kSmallSize },
    { "newproxy_proxy_id", kSmallSize },
    { "newproxy_view_id", kSmallSize },
    { "newproxy_opener_id", kSmallSize },
    { "newproxy_parent_id", kSmallSize },
    { "rvinit_view_id", kSmallSize },
    { "rvinit_proxy_id", kSmallSize },
    { "rvinit_main_frame_id", kSmallSize },
    { "initrf_frame_id", kSmallSize },
    { "initrf_proxy_id", kSmallSize },
    { "initrf_view_id", kSmallSize },
    { "initrf_main_frame_id", kSmallSize },
    { "initrf_view_is_live", kSmallSize },

    // Temporary for https://crbug.com/591478.
    { "initrf_parent_proxy_exists", kSmallSize },
    { "initrf_render_view_is_live", kSmallSize },
    { "initrf_parent_is_in_same_site_instance", kSmallSize},
    { "initrf_parent_process_is_live", kSmallSize},
    { "initrf_root_is_in_same_site_instance", kSmallSize},
    { "initrf_root_is_in_same_site_instance_as_parent", kSmallSize},
    { "initrf_root_process_is_live", kSmallSize},
    { "initrf_root_proxy_is_live", kSmallSize},

    // Temporary for https://crbug.com/630103.
    { "origin_mismatch_url", crash_keys::kLargeSize },
    { "origin_mismatch_origin", crash_keys::kMediumSize },
    { "origin_mismatch_transition", crash_keys::kSmallSize },
    { "origin_mismatch_redirects", crash_keys::kSmallSize },
    { "origin_mismatch_same_page", crash_keys::kSmallSize },

    // Temporary for https://crbug.com/612711.
    { "aci_wrong_sp_extension_id", kSmallSize },

    // Temporary for https://crbug.com/616149.
    { "existing_extension_pref_value_type", crash_keys::kSmallSize },

    // Temporary for https://crbug.com/630495.
    { "swdh_register_cannot_host_url", crash_keys::kLargeSize },
    { "swdh_register_cannot_scope_url", crash_keys::kLargeSize },
    { "swdh_register_cannot_script_url", crash_keys::kLargeSize },

    // Temporary for https://crbug.com/619294.
    { "swdh_unregister_cannot_host_url", crash_keys::kLargeSize },
    { "swdh_unregister_cannot_scope_url", crash_keys::kLargeSize },

    // Temporary for https://crbug.com/630496.
    { "swdh_get_registration_cannot_host_url", crash_keys::kLargeSize },
    { "swdh_get_registration_cannot_document_url", crash_keys::kLargeSize },
  };

  // This dynamic set of keys is used for sets of key value pairs when gathering
  // a collection of data, like command line switches or extension IDs.
  std::vector<base::debug::CrashKey> keys(
      fixed_keys, fixed_keys + arraysize(fixed_keys));

  crash_keys::GetCrashKeysForCommandLineSwitches(&keys);

  // Register the extension IDs.
  {
    static char formatted_keys[kExtensionIDMaxCount][sizeof(kExtensionID) + 1] =
        {{ 0 }};
    const size_t formatted_key_len = sizeof(formatted_keys[0]);
    for (size_t i = 0; i < kExtensionIDMaxCount; ++i) {
      int n = base::snprintf(
          formatted_keys[i], formatted_key_len, kExtensionID, i + 1);
      DCHECK_GT(n, 0);
      base::debug::CrashKey crash_key = { formatted_keys[i], kSmallSize };
      keys.push_back(crash_key);
    }
  }

  // Register the printer info.
  {
    static char formatted_keys[kPrinterInfoCount][sizeof(kPrinterInfo) + 1] =
        {{ 0 }};
    const size_t formatted_key_len = sizeof(formatted_keys[0]);
    for (size_t i = 0; i < kPrinterInfoCount; ++i) {
      // Key names are 1-indexed.
      int n = base::snprintf(
          formatted_keys[i], formatted_key_len, kPrinterInfo, i + 1);
      DCHECK_GT(n, 0);
      base::debug::CrashKey crash_key = { formatted_keys[i], kSmallSize };
      keys.push_back(crash_key);
    }
  }

  return base::debug::InitCrashKeys(&keys.at(0), keys.size(), kChunkMaxLength);
}

static bool IsBoringSwitch(const std::string& flag) {
  static const char* const kIgnoreSwitches[] = {
    switches::kEnableLogging,
    switches::kFlagSwitchesBegin,
    switches::kFlagSwitchesEnd,
    switches::kLoggingLevel,
#if defined(OS_WIN)
    // This file is linked into both chrome.dll and chrome.exe. However //ipc
    // is only in the .dll, so this needs to be a literal rather than the
    // constant.
    "channel",  // switches::kProcessChannelID
#else
    switches::kProcessChannelID,
#endif
    switches::kProcessType,
    switches::kV,
    switches::kVModule,
#if defined(OS_WIN)
    switches::kForceFieldTrials,
#elif defined(OS_MACOSX)
    switches::kMetricsClientID,
#elif defined(OS_CHROMEOS)
    switches::kPpapiFlashArgs,
    switches::kPpapiFlashPath,
    switches::kRegisterPepperPlugins,
    switches::kUIPrioritizeInGpuProcess,
    switches::kUseGL,
    switches::kUserDataDir,
    // Cros/CC flags are specified as raw strings to avoid dependency.
    "child-wallpaper-large",
    "child-wallpaper-small",
    "default-wallpaper-large",
    "default-wallpaper-small",
    "guest-wallpaper-large",
    "guest-wallpaper-small",
    "enterprise-enable-forced-re-enrollment",
    "enterprise-enrollment-initial-modulus",
    "enterprise-enrollment-modulus-limit",
    "login-profile",
    "login-user",
    "max-unused-resource-memory-usage-percentage",
    "termination-message-file",
    "use-cras",
#endif
  };

#if defined(OS_WIN)
  // Just about everything has this, don't bother.
  if (base::StartsWith(flag, "/prefetch:", base::CompareCase::SENSITIVE))
    return true;
#endif

  if (!base::StartsWith(flag, "--", base::CompareCase::SENSITIVE))
    return false;
  size_t end = flag.find("=");
  size_t len = (end == std::string::npos) ? flag.length() - 2 : end - 2;
  for (size_t i = 0; i < arraysize(kIgnoreSwitches); ++i) {
    if (flag.compare(2, len, kIgnoreSwitches[i]) == 0)
      return true;
  }
  return false;
}

void SetCrashKeysFromCommandLine(const base::CommandLine& command_line) {
  return SetSwitchesFromCommandLine(command_line, &IsBoringSwitch);
}

void SetActiveExtensions(const std::set<std::string>& extensions) {
  base::debug::SetCrashKeyValue(kNumExtensionsCount,
      base::StringPrintf("%" PRIuS, extensions.size()));

  std::set<std::string>::const_iterator it = extensions.begin();
  for (size_t i = 0; i < kExtensionIDMaxCount; ++i) {
    std::string key = base::StringPrintf(kExtensionID, i + 1);
    if (it == extensions.end()) {
      base::debug::ClearCrashKey(key);
    } else {
      base::debug::SetCrashKeyValue(key, *it);
      ++it;
    }
  }
}

ScopedPrinterInfo::ScopedPrinterInfo(const base::StringPiece& data) {
  std::vector<std::string> info = base::SplitString(
      data.as_string(), ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < kPrinterInfoCount; ++i) {
    std::string key = base::StringPrintf(kPrinterInfo, i + 1);
    std::string value;
    if (i < info.size())
      value = info[i];
    base::debug::SetCrashKeyValue(key, value);
  }
}

ScopedPrinterInfo::~ScopedPrinterInfo() {
  for (size_t i = 0; i < kPrinterInfoCount; ++i) {
    std::string key = base::StringPrintf(kPrinterInfo, i + 1);
    base::debug::ClearCrashKey(key);
  }
}

}  // namespace crash_keys
