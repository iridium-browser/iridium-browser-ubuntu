// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGINS_FIELD_TRIAL_H_
#define CHROME_BROWSER_PLUGINS_PLUGINS_FIELD_TRIAL_H_

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

// This class manages the Plugins field trials.
class PluginsFieldTrial {
 public:
  // Returns the effective content setting for plugins. Passes non-plugin
  // content settings through without modification.
  static ContentSetting EffectiveContentSetting(ContentSettingsType type,
                                                ContentSetting setting);

  // Returns true if the Plugin Power Saver feature is forced on, overriding
  // the user's Content Settings.
  static bool IsForcePluginPowerSaverEnabled();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PluginsFieldTrial);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGINS_FIELD_TRIAL_H_
