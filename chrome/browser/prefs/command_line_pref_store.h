// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_COMMAND_LINE_PREF_STORE_H_
#define CHROME_BROWSER_PREFS_COMMAND_LINE_PREF_STORE_H_

#include "base/command_line.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/prefs/value_map_pref_store.h"

// This PrefStore keeps track of preferences set by command-line switches,
// such as proxy settings.
class CommandLinePrefStore : public ValueMapPrefStore {
 public:
  explicit CommandLinePrefStore(const base::CommandLine* command_line);

 protected:
  ~CommandLinePrefStore() override;

  // Logs a message and returns false if the proxy switches are
  // self-contradictory. Protected so it can be used in unit testing.
  bool ValidateProxySwitches();

 private:
  friend class TestCommandLinePrefStore;

  struct StringSwitchToPreferenceMapEntry {
    const char* switch_name;
    const char* preference_path;
  };

  struct PathSwitchToPreferenceMapEntry {
    const char* switch_name;
    const char* preference_path;
  };

  struct IntegerSwitchToPreferenceMapEntry {
    const char* switch_name;
    const char* preference_path;
  };

  // |set_value| indicates what the preference should be set to if the switch
  // is present.
  struct BooleanSwitchToPreferenceMapEntry {
    const char* switch_name;
    const char* preference_path;
    bool set_value;
  };

  // Using the string and boolean maps, apply command-line switches to their
  // corresponding preferences in this pref store.
  void ApplySimpleSwitches();

  // Determines the proxy mode preference from the given proxy switches.
  void ApplyProxyMode();

  // Apply the SSL/TLS preferences from the given switches.
  void ApplySSLSwitches();

  // Determines whether the background mode is force-disabled.
  void ApplyBackgroundModeSwitches();

  // Weak reference.
  const base::CommandLine* command_line_;

  // Mappings of command line switches to prefs.
  static const BooleanSwitchToPreferenceMapEntry boolean_switch_map_[];
  static const StringSwitchToPreferenceMapEntry string_switch_map_[];
  static const PathSwitchToPreferenceMapEntry path_switch_map_[];
  static const IntegerSwitchToPreferenceMapEntry integer_switch_map_[];

  DISALLOW_COPY_AND_ASSIGN(CommandLinePrefStore);
};

#endif  // CHROME_BROWSER_PREFS_COMMAND_LINE_PREF_STORE_H_
