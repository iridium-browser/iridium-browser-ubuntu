// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/channel_info.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"

using base::win::RegKey;

namespace {

const wchar_t kModChrome[] = L"-chrome";
const wchar_t kModChromeFrame[] = L"-chromeframe";
const wchar_t kModAppHostDeprecated[] = L"-apphost";
const wchar_t kModAppLauncherDeprecated[] = L"-applauncher";
const wchar_t kModMultiInstall[] = L"-multi";
const wchar_t kModReadyMode[] = L"-readymode";
const wchar_t kModStage[] = L"-stage:";
const wchar_t kModStatsDefault[] = L"-statsdef_";
const wchar_t kSfxFull[] = L"-full";
const wchar_t kSfxMigrating[] = L"-migrating";
const wchar_t kSfxMultiFail[] = L"-multifail";

const wchar_t* const kModifiers[] = {
    kModStatsDefault,
    kModStage,
    kModMultiInstall,
    kModChrome,
    kModChromeFrame,
    kModAppHostDeprecated,
    kModAppLauncherDeprecated,
    kModReadyMode,
    kSfxMultiFail,
    kSfxMigrating,
    kSfxFull,
};

enum ModifierIndex {
  MOD_STATS_DEFAULT,
  MOD_STAGE,
  MOD_MULTI_INSTALL,
  MOD_CHROME,
  MOD_CHROME_FRAME,
  MOD_APP_HOST_DEPRECATED,
  MOD_APP_LAUNCHER_DEPRECATED,
  MOD_READY_MODE,
  SFX_MULTI_FAIL,
  SFX_MIGRATING,
  SFX_FULL,
  NUM_MODIFIERS
};

static_assert(NUM_MODIFIERS == arraysize(kModifiers),
              "kModifiers disagrees with ModifierIndex; they must match!");

// Returns true if the modifier is found, in which case |position| holds the
// location at which the modifier was found.  The number of characters in the
// modifier is returned in |length|, if non-NULL.
bool FindModifier(ModifierIndex index,
                  const base::string16& ap_value,
                  base::string16::size_type* position,
                  base::string16::size_type* length) {
  DCHECK(position != NULL);
  base::string16::size_type mod_position = base::string16::npos;
  base::string16::size_type mod_length =
      base::string16::traits_type::length(kModifiers[index]);
  char last_char = kModifiers[index][mod_length - 1];
  const bool mod_takes_arg = (last_char == L':' || last_char == L'_');
  base::string16::size_type pos = 0;
  do {
    mod_position = ap_value.find(kModifiers[index], pos, mod_length);
    if (mod_position == base::string16::npos)
      return false;  // Modifier not found.
    pos = mod_position + mod_length;
    // Modifiers that take an argument gobble up to the next separator or to the
    // end.
    if (mod_takes_arg) {
      pos = ap_value.find(L'-', pos);
      if (pos == base::string16::npos)
        pos = ap_value.size();
      break;
    }
    // Regular modifiers must be followed by '-' or the end of the string.
  } while (pos != ap_value.size() && ap_value[pos] != L'-');
  DCHECK_NE(mod_position, base::string16::npos);
  *position = mod_position;
  if (length != NULL)
    *length = pos - mod_position;
  return true;
}

bool HasModifier(ModifierIndex index, const base::string16& ap_value) {
  DCHECK(index >= 0 && index < NUM_MODIFIERS);
  base::string16::size_type position;
  return FindModifier(index, ap_value, &position, NULL);
}

base::string16::size_type FindInsertionPoint(ModifierIndex index,
                                             const base::string16& ap_value) {
  // Return the location of the next modifier.
  base::string16::size_type result;

  for (int scan = index + 1; scan < NUM_MODIFIERS; ++scan) {
    if (FindModifier(static_cast<ModifierIndex>(scan), ap_value, &result, NULL))
      return result;
  }

  return ap_value.size();
}

// Returns true if |ap_value| is modified.
bool SetModifier(ModifierIndex index, bool set, base::string16* ap_value) {
  DCHECK(index >= 0 && index < NUM_MODIFIERS);
  DCHECK(ap_value);
  base::string16::size_type position;
  base::string16::size_type length;
  bool have_modifier = FindModifier(index, *ap_value, &position, &length);
  if (set) {
    if (!have_modifier) {
      ap_value->insert(FindInsertionPoint(index, *ap_value), kModifiers[index]);
      return true;
    }
  } else {
    if (have_modifier) {
      ap_value->erase(position, length);
      return true;
    }
  }
  return false;
}

// Returns the position of the first case-insensitive match of |pattern| found
// in |str|, or base::string16::npos if none found. |pattern| must be non-empty
// lower-case ASCII, and may contain any number of '.' wildcard characters.
size_t FindSubstringMatch(const base::string16& str,
                          base::StringPiece16 pattern) {
  DCHECK(!pattern.empty());
  DCHECK(base::IsStringASCII(pattern));
  DCHECK(pattern == base::StringPiece16(base::ToLowerASCII(pattern)));

  if (str.size() < pattern.size())
    return base::string16::npos;

  for (size_t i = 0; i < str.size() - pattern.size() + 1; ++i) {
    size_t j = 0;
    while (j < pattern.size() &&
           (pattern[j] == L'.' ||
            pattern[j] == base::ToLowerASCII(str[i+j]))) {
      ++j;
    }
    if (j == pattern.size())
      return i;
  }
  return base::string16::npos;
}

// Returns the value of a modifier - that is for a modifier of the form
// "-foo:bar", returns "bar". Returns an empty string if the modifier
// is not present or does not have a value.
base::string16 GetModifierValue(ModifierIndex modifier_index,
                                const base::string16& value) {
  base::string16::size_type position;
  base::string16::size_type length;

  if (FindModifier(modifier_index, value, &position, &length)) {
    // Return the portion after the prefix.
    base::string16::size_type pfx_length =
        base::string16::traits_type::length(kModifiers[modifier_index]);
    DCHECK_LE(pfx_length, length);
    return value.substr(position + pfx_length, length - pfx_length);
  }
  return base::string16();
}

}  // namespace

namespace installer {

bool ChannelInfo::Initialize(const RegKey& key) {
  LONG result = key.ReadValue(google_update::kRegApField, &value_);
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ||
      result == ERROR_INVALID_HANDLE;
}

bool ChannelInfo::Write(RegKey* key) const {
  DCHECK(key);
  // Google Update deletes the value when it is empty, so we may as well, too.
  LONG result = value_.empty() ?
      key->DeleteValue(google_update::kRegApField) :
      key->WriteValue(google_update::kRegApField, value_.c_str());
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed writing channel info; result: " << result;
    return false;
  }
  return true;
}

bool ChannelInfo::GetChannelName(base::string16* channel_name) const {
  static const wchar_t kChromeChannelBetaPattern[] = L"1.1-";
  static const wchar_t kChromeChannelBetaX64Pattern[] = L"x64-beta";
  static const wchar_t kChromeChannelDevPattern[] = L"2.0-d";
  static const wchar_t kChromeChannelDevX64Pattern[] = L"x64-dev";

  DCHECK(channel_name);
  // Report channels that are empty string or contain "stable" as stable
  // (empty string).
  if (value_.empty() || value_.find(installer::kChromeChannelStableExplicit) !=
      base::string16::npos) {
    channel_name->erase();
    return true;
  }
  // Report channels that match "/^2.0-d.*/i" as dev.
  if (FindSubstringMatch(value_, kChromeChannelDevPattern) == 0) {
    channel_name->assign(installer::kChromeChannelDev);
    return true;
  }
  // Report channels that match "/.*x64-dev.*/" as dev.
  if (value_.find(kChromeChannelDevX64Pattern) != base::string16::npos) {
    channel_name->assign(installer::kChromeChannelDev);
    return true;
  }
  // Report channels that match "/^1.1-.*/i" as beta.
  if (FindSubstringMatch(value_, kChromeChannelBetaPattern) == 0) {
    channel_name->assign(installer::kChromeChannelBeta);
    return true;
  }
  // Report channels that match "/.*x64-beta.*/" as beta.
  if (value_.find(kChromeChannelBetaX64Pattern) != base::string16::npos) {
    channel_name->assign(installer::kChromeChannelBeta);
    return true;
  }

  // There may be modifiers present. Strip them off and see if we're left
  // with the empty string (stable channel).
  base::string16 tmp_value = value_;
  for (int i = 0; i != NUM_MODIFIERS; ++i) {
    SetModifier(static_cast<ModifierIndex>(i), false, &tmp_value);
  }
  if (tmp_value.empty()) {
    channel_name->erase();
    return true;
  }

  return false;
}

bool ChannelInfo::IsChrome() const {
  return HasModifier(MOD_CHROME, value_);
}

bool ChannelInfo::SetChrome(bool value) {
  return SetModifier(MOD_CHROME, value, &value_);
}

bool ChannelInfo::IsChromeFrame() const {
  return HasModifier(MOD_CHROME_FRAME, value_);
}

bool ChannelInfo::SetChromeFrame(bool value) {
  return SetModifier(MOD_CHROME_FRAME, value, &value_);
}

bool ChannelInfo::IsAppLauncher() const {
  return HasModifier(MOD_APP_LAUNCHER_DEPRECATED, value_);
}

bool ChannelInfo::SetAppLauncher(bool value) {
  // Unconditionally remove -apphost since it has been long deprecated.
  bool changed_app_host = SetModifier(MOD_APP_HOST_DEPRECATED, false, &value_);
  // Set value for -applauncher, relying on caller for policy.
  bool changed_app_launcher =
      SetModifier(MOD_APP_LAUNCHER_DEPRECATED, value, &value_);
  return changed_app_host || changed_app_launcher;
}

bool ChannelInfo::IsMultiInstall() const {
  return HasModifier(MOD_MULTI_INSTALL, value_);
}

bool ChannelInfo::SetMultiInstall(bool value) {
  return SetModifier(MOD_MULTI_INSTALL, value, &value_);
}

bool ChannelInfo::IsReadyMode() const {
  return HasModifier(MOD_READY_MODE, value_);
}

bool ChannelInfo::SetReadyMode(bool value) {
  return SetModifier(MOD_READY_MODE, value, &value_);
}

bool ChannelInfo::ClearStage() {
  base::string16::size_type position;
  base::string16::size_type length;
  if (FindModifier(MOD_STAGE, value_, &position, &length)) {
    value_.erase(position, length);
    return true;
  }
  return false;
}

base::string16 ChannelInfo::GetStatsDefault() const {
  return GetModifierValue(MOD_STATS_DEFAULT, value_);
}

bool ChannelInfo::HasFullSuffix() const {
  return HasModifier(SFX_FULL, value_);
}

bool ChannelInfo::SetFullSuffix(bool value) {
  return SetModifier(SFX_FULL, value, &value_);
}

bool ChannelInfo::HasMultiFailSuffix() const {
  return HasModifier(SFX_MULTI_FAIL, value_);
}

bool ChannelInfo::SetMultiFailSuffix(bool value) {
  return SetModifier(SFX_MULTI_FAIL, value, &value_);
}

bool ChannelInfo::SetMigratingSuffix(bool value) {
  return SetModifier(SFX_MIGRATING, value, &value_);
}

bool ChannelInfo::HasMigratingSuffix() const {
  return HasModifier(SFX_MIGRATING, value_);
}

bool ChannelInfo::RemoveAllModifiersAndSuffixes() {
  bool modified = false;

  for (int scan = 0; scan < NUM_MODIFIERS; ++scan) {
    ModifierIndex index = static_cast<ModifierIndex>(scan);
    modified = SetModifier(index, false, &value_) || modified;
  }

  return modified;
}

}  // namespace installer
