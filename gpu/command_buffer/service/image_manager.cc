// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_manager.h"

#include <stdint.h>

#include "base/logging.h"
#include "ui/gl/gl_image.h"

namespace gpu {
namespace gles2 {

ImageManager::ImageManager() {
}

ImageManager::~ImageManager() {
}

void ImageManager::Destroy(bool have_context) {
  for (GLImageMap::const_iterator iter = images_.begin(); iter != images_.end();
       ++iter)
    iter->second.get()->Destroy(have_context);
  images_.clear();
}

void ImageManager::AddImage(gl::GLImage* image, int32_t service_id) {
  DCHECK(images_.find(service_id) == images_.end());
  images_[service_id] = image;
}

void ImageManager::RemoveImage(int32_t service_id) {
  GLImageMap::iterator iter = images_.find(service_id);
  DCHECK(iter != images_.end());
  iter->second.get()->Destroy(true);
  images_.erase(iter);
}

gl::GLImage* ImageManager::LookupImage(int32_t service_id) {
  GLImageMap::const_iterator iter = images_.find(service_id);
  if (iter != images_.end())
    return iter->second.get();

  return NULL;
}

}  // namespace gles2
}  // namespace gpu
