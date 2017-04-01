// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the struct used to describe each of a brand's install modes; see
// install_modes.h for details. For brands that integrate with Google Update,
// each mode also describes a strategy for determining its update channel.

#ifndef CHROME_INSTALL_STATIC_INSTALL_CONSTANTS_H_
#define CHROME_INSTALL_STATIC_INSTALL_CONSTANTS_H_

namespace install_static {

// Identifies different strategies for determining an update channel.
enum class ChannelStrategy {
  // Update channels are not supported. This value is for exclusive use by
  // brands that do not integrate with Google Update.
  UNSUPPORTED,
  // Update channel is determined by parsing the "ap" value in the registry.
  // This is used by Google Chrome's primary install mode to differentiate the
  // beta and dev channels from the default stable channel.
  ADDITIONAL_PARAMETERS,
  // Update channel is a fixed value. This is used by to pin Google Chrome's SxS
  // secondary install mode to the canary channel.
  FIXED,
};

// A POD-struct defining constants for a brand's install mode. A brand has one
// primary and one or more secondary install modes. Refer to kInstallModes in
// chromium_install_modes.cc and google_chrome_install_modes.cc for examples of
// typical mode definitions.
struct InstallConstants {
  // The size (in bytes) of this structure. This serves to verify that all
  // modules in a process have the same definition of the struct.
  size_t size;

  // The brand-specific index/identifier of this instance (defined in a brand's
  // BRAND_install_modes.h file). Index 0 is reserved for a brand's primary
  // install mode.
  int index;

  // The install suffix of a secondary mode (e.g., " SxS" for canary Chrome) or
  // an empty string for the primary mode. This suffix is appended to file and
  // registry paths used by the product.
  const wchar_t* install_suffix;

  // The app guid with which this mode is registered with Google Update, or an
  // empty string if the brand does not integrate with Google Update.
  const wchar_t* app_guid;

  // The default name for this mode's update channel.
  const wchar_t* default_channel_name;

  // The strategy used to determine the mode's update channel, or UNSUPPORTED if
  // the brand does not integrate with Google Update.
  ChannelStrategy channel_strategy;

  // True if this mode supports system-level installs.
  bool supports_system_level;

  // True if this mode supported the now-deprecated multi-install.
  bool supported_multi_install;
};

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INSTALL_CONSTANTS_H_
