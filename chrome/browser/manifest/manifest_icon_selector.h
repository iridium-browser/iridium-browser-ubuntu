// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_
#define CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_

#include "base/basictypes.h"
#include "content/public/common/manifest.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace IPC {
class Message;
}  // namespace IPC

namespace gfx {
class Screen;
}

// Selects the icon most closely matching the size constraints.  This follows
// very basic heuristics -- improvements are welcome.
class ManifestIconSelector {
 public:
  // Runs the algorithm to find the best matching icon in the icons listed in
  // the Manifest.
  //
  // Size is defined in Android's density-independent pixels (dp):
  // http://developer.android.com/guide/practices/screens_support.html
  // If/when this class is generalized, it may be a good idea to switch this to
  // taking in pixels, instead.
  //
  // Returns the icon url if a suitable icon is found. An empty URL otherwise.
  static GURL FindBestMatchingIcon(
      const std::vector<content::Manifest::Icon>& icons,
      float preferred_icon_size_in_dp,
      const gfx::Screen* screen);

 private:
  explicit ManifestIconSelector(float preferred_icon_size_in_pixels);
  virtual ~ManifestIconSelector() {}

  // Runs the algorithm to find the best matching icon in the icons listed in
  // the Manifest.
  // Returns the icon url if a suitable icon is found. An empty URL otherwise.
  GURL FindBestMatchingIcon(
      const std::vector<content::Manifest::Icon>& icons,
      float density);

  // Runs an algorithm only based on icon declared sizes. It will try to find
  // size that is the closest to preferred_icon_size_in_pixels_ but bigger than
  // preferred_icon_size_in_pixels_ if possible.
  // Returns the icon url if a suitable icon is found. An empty URL otherwise.
  GURL FindBestMatchingIconForDensity(
      const std::vector<content::Manifest::Icon>& icons,
      float density);

  // Returns whether the |preferred_icon_size_in_pixels_| is in |sizes|.
  bool IconSizesContainsPreferredSize(const std::vector<gfx::Size>& sizes);

  // Returns an array containing the items in |icons| without the unsupported
  // image MIME types.
  static std::vector<content::Manifest::Icon> FilterIconsByType(
      const std::vector<content::Manifest::Icon>& icons);

  // Returns whether the 'any' (ie. gfx::Size(0,0)) is in |sizes|.
  static bool IconSizesContainsAny(const std::vector<gfx::Size>& sizes);

  const int preferred_icon_size_in_pixels_;

  friend class ManifestIconSelectorTest;

  DISALLOW_COPY_AND_ASSIGN(ManifestIconSelector);
};

#endif  // CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_
