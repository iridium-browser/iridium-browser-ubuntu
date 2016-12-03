// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CRASH_KEYS_H_
#define CHROME_COMMON_CRASH_KEYS_H_

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_keys.h"
#include "third_party/kasko/kasko_features.h"

namespace base {
class CommandLine;
}

namespace crash_keys {

// Registers all of the potential crash keys that can be sent to the crash
// reporting server. Returns the size of the union of all keys.
size_t RegisterChromeCrashKeys();

// Sets the kNumSwitches key and the set of keys named using kSwitchFormat based
// on the given |command_line|.
void SetCrashKeysFromCommandLine(const base::CommandLine& command_line);

// Sets the list of "active" extensions in this process. We overload "active" to
// mean different things depending on the process type:
// - browser: all enabled extensions
// - renderer: the unique set of extension ids from all content scripts
// - extension: the id of each extension running in this process (there can be
//   multiple because of process collapsing).
void SetActiveExtensions(const std::set<std::string>& extensions);

// Sets the printer info. Data should be separated by ';' up to
// kPrinterInfoCount substrings. Each substring will be truncated if necessary.
class ScopedPrinterInfo {
 public:
  explicit ScopedPrinterInfo(const base::StringPiece& data);
  ~ScopedPrinterInfo();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedPrinterInfo);
};

// Crash Key Name Constants ////////////////////////////////////////////////////

// The URL of the active tab.
extern const char kActiveURL[];

// Installed extensions. |kExtensionID| should be formatted with an integer,
// in the range [0, kExtensionIDMaxCount).
const size_t kExtensionIDMaxCount = 10;
extern const char kExtensionID[];
// The total number of installed extensions, recorded in case it exceeds
// kExtensionIDMaxCount. Also used in chrome/app, but defined here to avoid
// a common->app dependency.
extern const char kNumExtensionsCount[];

// Type of shutdown. The value is one of "close" for WINDOW_CLOSE,
// "exit" for BROWSER_EXIT, or "end" for END_SESSION.
extern const char kShutdownType[];

// GPU information.
#if !defined(OS_ANDROID)
extern const char kGPUVendorID[];
extern const char kGPUDeviceID[];
#endif
extern const char kGPUDriverVersion[];
extern const char kGPUPixelShaderVersion[];
extern const char kGPUVertexShaderVersion[];
#if defined(OS_MACOSX)
extern const char kGPUGLVersion[];
#elif defined(OS_POSIX)
extern const char kGPUVendor[];
extern const char kGPURenderer[];
#endif

#if defined(OS_WIN)
extern const char kHungAudioThreadDetails[];
#endif

// The user's printers, up to kPrinterInfoCount. Should be set with
// ScopedPrinterInfo.
const size_t kPrinterInfoCount = 4;
extern const char kPrinterInfo[];

#if defined(OS_CHROMEOS)
// The number of simultaneous users in multi profile sessions.
extern const char kNumberOfUsers[];
#endif

#if defined(OS_MACOSX)
namespace mac {

// Used to report the first Cocoa/Mac NSException and its backtrace.
extern const char kFirstNSException[];
extern const char kFirstNSExceptionTrace[];

// Used to report the last Cocoa/Mac NSException and its backtrace.
extern const char kLastNSException[];
extern const char kLastNSExceptionTrace[];

// Records the current NSException as it is being created, and its backtrace.
extern const char kNSException[];
extern const char kNSExceptionTrace[];

// In the CrApplication, records information about the current event's
// target-action.
extern const char kSendAction[];

}  // namespace mac
#endif

#if BUILDFLAG(ENABLE_KASKO)
// Used to correlate a report sent via Kasko with one sent via Breakpad.
extern const char kKaskoGuid[];
extern const char kKaskoEquivalentGuid[];
#endif

// Numbers of active views.
extern const char kViewCount[];

// TEMPORARY: The encoder/frame details at the time a zero-length encoded frame
// was encountered.  http://crbug.com/519022
extern const char kZeroEncodeDetails[];

}  // namespace crash_keys

#endif  // CHROME_COMMON_CRASH_KEYS_H_
