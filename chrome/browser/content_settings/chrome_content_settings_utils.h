// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_

class GURL;

// Put utility functions only used by //chrome code here. If a function declared
// here would be meaningfully shared with other platforms, consider moving it to
// components/content_settings/core/browser/content_settings_utils.h.

namespace content_settings {

// UMA histogram for the mixed script shield. The enum values correspond to
// histogram entries, so do not remove any existing values.
enum MixedScriptAction {
  MIXED_SCRIPT_ACTION_DISPLAYED_SHIELD = 0,
  MIXED_SCRIPT_ACTION_DISPLAYED_BUBBLE,
  MIXED_SCRIPT_ACTION_CLICKED_ALLOW,
  MIXED_SCRIPT_ACTION_CLICKED_LEARN_MORE,
  MIXED_SCRIPT_ACTION_COUNT
};

void RecordMixedScriptAction(MixedScriptAction action);

// UMA histogram for the plugins broken puzzle piece. The enum values
// correspond to histogram entries, so do not remove any existing values.
enum PluginsAction {
  PLUGINS_ACTION_TOTAL_NAVIGATIONS = 0,
  PLUGINS_ACTION_DISPLAYED_BLOCKED_ICON_IN_OMNIBOX,
  PLUGINS_ACTION_DISPLAYED_BUBBLE,
  PLUGINS_ACTION_CLICKED_RUN_ALL_PLUGINS_THIS_TIME,
  PLUGINS_ACTION_CLICKED_ALWAYS_ALLOW_PLUGINS_ON_ORIGIN,
  PLUGINS_ACTION_CLICKED_MANAGE_PLUGIN_BLOCKING,
  PLUGINS_ACTION_CLICKED_LEARN_MORE,
  PLUGINS_ACTION_COUNT
};

void RecordPluginsAction(PluginsAction action);

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_
