// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/compat_checks.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"

namespace {

// SEP stands for Symantec End Point Protection.
std::wstring GetSEPVersion() {
  const wchar_t kProductKey[] =
      L"SOFTWARE\\Symantec\\Symantec Endpoint Protection\\SMC";
  // Versions before 11MR3 were always 32-bit, so check in the 32-bit hive.
  base::win::RegKey key(
      HKEY_LOCAL_MACHINE, kProductKey, KEY_READ | KEY_WOW64_32KEY);
  std::wstring version_str;
  key.ReadValue(L"ProductVersion", &version_str);
  return version_str;
}

// The product version should be a string like "11.0.3001.2224". This function
// returns as params the first 3 values. Return value is false if anything
// does not fit the format.
bool ParseSEPVersion(const base::string16& version,
                     int* v0, int* v1, int* v2) {
  std::vector<base::StringPiece16> v = base::SplitStringPiece(
      version, L".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (v.size() != 4)
    return false;
  if (!base::StringToInt(v[0], v0))
    return false;
  if (!base::StringToInt(v[1], v1))
    return false;
  if (!base::StringToInt(v[2], v2))
    return false;
  return true;
}

// The incompatible versions are anything before 11MR3, which is 11.0.3001.
bool IsBadSEPVersion(int v0, int v1, int v2) {
  if (v0 < 11)
    return true;
  if (v1 > 0)
    return false;
  if (v2 < 3001)
    return true;
  return false;
}

}  // namespace

bool HasIncompatibleSymantecEndpointVersion(const wchar_t* version) {
  int v0, v1, v2;
  std::wstring ver_str(version ? version : GetSEPVersion());
  if (!ParseSEPVersion(ver_str, &v0, &v1, &v2))
    return false;
  return IsBadSEPVersion(v0, v1, v2);
}
