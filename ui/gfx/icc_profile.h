// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ICC_PROFILE_H_
#define UI_GFX_ICC_PROFILE_H_

#include <stdint.h>
#include <vector>

#include "base/gtest_prod_util.h"
#include "ui/gfx/color_space.h"

#if defined(OS_MACOSX)
#include <CoreGraphics/CGColorSpace.h>
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

namespace gfx {

// Used to represent a full ICC profile, usually retrieved from a monitor. It
// can be lossily compressed into a ColorSpace object. This structure should
// only be sent from higher-privilege processes to lower-privilege processes,
// as parsing this structure is not secure.
class GFX_EXPORT ICCProfile {
 public:
  ICCProfile();
  ICCProfile(ICCProfile&& other);
  ICCProfile(const ICCProfile& other);
  ICCProfile& operator=(ICCProfile&& other);
  ICCProfile& operator=(const ICCProfile& other);
  ~ICCProfile();
  bool operator==(const ICCProfile& other) const;

  // Returns the color profile of the monitor that can best represent color.
  // This profile should be used for creating content that does not know on
  // which monitor it will be displayed.
  static ICCProfile FromBestMonitor();
#if defined(OS_MACOSX)
  static ICCProfile FromCGColorSpace(CGColorSpaceRef cg_color_space);
#endif

  // This will recover a ICCProfile from a compact ColorSpace representation.
  // Internally, this will make an effort to create an identical ICCProfile
  // to the one that created |color_space|, but this is not guaranteed.
  static ICCProfile FromColorSpace(const gfx::ColorSpace& color_space);

  // This will perform a potentially-lossy conversion to a more compact color
  // space representation.
  ColorSpace GetColorSpace() const;

  const std::vector<char>& GetData() const;

#if defined(OS_WIN)
  // This will read monitor ICC profiles from disk and cache the results for the
  // other functions to read. This should not be called on the UI or IO thread.
  static void UpdateCachedProfilesOnBackgroundThread();
  static bool CachedProfilesNeedUpdate();
#endif

 private:
  static ICCProfile FromData(const char* icc_profile, size_t size);
  static bool IsValidProfileLength(size_t length);

  bool valid_ = false;
  std::vector<char> data_;

  // This globally identifies this ICC profile. It is used to look up this ICC
  // profile from a ColorSpace object created from it.
  uint64_t id_ = 0;

  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, BT709toSRGBICC);
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
  friend int ::LLVMFuzzerTestOneInput(const uint8_t*, size_t);
  friend class ColorSpace;
  friend struct IPC::ParamTraits<gfx::ICCProfile>;
};

}  // namespace gfx

#endif  // UI_GFX_ICC_PROFILE_H_
