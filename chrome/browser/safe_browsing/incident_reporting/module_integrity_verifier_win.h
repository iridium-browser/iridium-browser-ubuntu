// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_VERIFIER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_VERIFIER_WIN_H_

#include <stdint.h>

#include <set>
#include <string>

#include "chrome/common/safe_browsing/csd.pb.h"

namespace base {
namespace win {
class PEImage;
class PEImageAsData;
}  // namespace win
}  // namespace base

namespace safe_browsing {

// This enum defines the possible module states VerifyModule can return.
enum ModuleState {
  MODULE_STATE_UNKNOWN,
  MODULE_STATE_UNMODIFIED,
  MODULE_STATE_MODIFIED,
};

struct VerificationResult {
  ModuleState state;
  // The number of bytes with different values on disk and in memory.
  int num_bytes_different;
  // True if the relocations were ordered and the verification was fully
  // completed.
  bool verification_completed;
};

// Helper to grab the addresses and size of the code section of a PEImage.
// Returns two addresses: one for the dll loaded as a library, the other for the
// dll loaded as data.
bool GetCodeAddrsAndSize(const base::win::PEImage& mem_peimage,
                         const base::win::PEImageAsData& disk_peimage,
                         uint8_t** mem_code_addr,
                         uint8_t** disk_code_addr,
                         uint32_t* code_size);

// Examines the code section of the given module in memory and on disk, looking
// for unexpected differences.  Returns a ModuleState and and a set of the
// possibly modified exports.
ModuleState VerifyModule(const wchar_t* module_name,
                         std::set<std::string>* modified_exports,
                         int* num_bytes_different);

// Examines the code section of the given module in memory and on disk, looking
// for unexpected differences and populating |module_state| in the process.
VerificationResult NewVerifyModule(
    const wchar_t* module_name,
    ClientIncidentReport_EnvironmentData_Process_ModuleState* module_state);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_VERIFIER_WIN_H_
