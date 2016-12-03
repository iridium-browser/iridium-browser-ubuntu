// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Helper function to create a new bitmap with |padding| amount of empty space
// around the original bitmap.
static SkBitmap ApplyPadding(const SkBitmap& source,
                             const gfx::Insets& padding) {
  std::unique_ptr<gfx::Canvas> result(
      new gfx::Canvas(gfx::Size(source.width() + padding.width(),
                                source.height() + padding.height()),
                      1.0f, false));
  result->DrawImageInt(
      gfx::ImageSkia::CreateFrom1xBitmap(source),
      0, 0, source.width(), source.height(),
      padding.left(), padding.top(), source.width(), source.height(),
      false);
  return result->ExtractImageRep().sk_bitmap();
}

}  // namespace

ExtensionIconManager::ExtensionIconManager()
    : monochrome_(false),
      weak_ptr_factory_(this)  {
}

ExtensionIconManager::~ExtensionIconManager() {
}

void ExtensionIconManager::LoadIcon(content::BrowserContext* context,
                                    const extensions::Extension* extension) {
  extensions::ExtensionResource icon_resource =
      extensions::IconsInfo::GetIconResource(
          extension,
          extension_misc::EXTENSION_ICON_BITTY,
          ExtensionIconSet::MATCH_BIGGER);
  if (!icon_resource.extension_root().empty()) {
    // Insert into pending_icons_ first because LoadImage can call us back
    // synchronously if the image is already cached.
    pending_icons_.insert(extension->id());
    extensions::ImageLoader* loader = extensions::ImageLoader::Get(context);
    loader->LoadImageAsync(extension, icon_resource,
                           gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize),
                           base::Bind(
                               &ExtensionIconManager::OnImageLoaded,
                               weak_ptr_factory_.GetWeakPtr(),
                               extension->id()));
  }
}

const SkBitmap& ExtensionIconManager::GetIcon(const std::string& extension_id) {
  const SkBitmap* result = NULL;
  if (base::ContainsKey(icons_, extension_id)) {
    result = &icons_[extension_id];
  } else {
    EnsureDefaultIcon();
    result = &default_icon_;
  }
  DCHECK(result);
  DCHECK_EQ(gfx::kFaviconSize + padding_.width(), result->width());
  DCHECK_EQ(gfx::kFaviconSize + padding_.height(), result->height());
  return *result;
}

void ExtensionIconManager::RemoveIcon(const std::string& extension_id) {
  icons_.erase(extension_id);
  pending_icons_.erase(extension_id);
}

void ExtensionIconManager::OnImageLoaded(const std::string& extension_id,
                                         const gfx::Image& image) {
  if (image.IsEmpty())
    return;

  // We may have removed the icon while waiting for it to load. In that case,
  // do nothing.
  if (!base::ContainsKey(pending_icons_, extension_id))
    return;

  pending_icons_.erase(extension_id);
  icons_[extension_id] = ApplyTransforms(*image.ToSkBitmap());
}

void ExtensionIconManager::EnsureDefaultIcon() {
  if (default_icon_.empty()) {
    // TODO(estade): use correct scale factor instead of 1x.
    default_icon_ = ApplyPadding(
        *gfx::CreateVectorIcon(gfx::VectorIconId::EXTENSION, gfx::kFaviconSize,
                               gfx::kChromeIconGrey)
             .bitmap(),
        padding_);
  }
}

SkBitmap ExtensionIconManager::ApplyTransforms(const SkBitmap& source) {
  SkBitmap result = source;

  if (result.width() != gfx::kFaviconSize ||
      result.height() != gfx::kFaviconSize) {
    result = skia::ImageOperations::Resize(
        result, skia::ImageOperations::RESIZE_LANCZOS3,
        gfx::kFaviconSize, gfx::kFaviconSize);
  }

  if (monochrome_) {
    color_utils::HSL shift = {-1, 0, 0.6};
    result = SkBitmapOperations::CreateHSLShiftedBitmap(result, shift);
  }

  if (!padding_.IsEmpty())
    result = ApplyPadding(result, padding_);

  return result;
}
