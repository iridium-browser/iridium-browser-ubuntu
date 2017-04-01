// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_MEDIA_PIPELINE_BACKEND_FACTORY_H_
#define CHROMECAST_MEDIA_SERVICE_MEDIA_PIPELINE_BACKEND_FACTORY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

typedef base::Callback<std::unique_ptr<MediaPipelineBackend>(
    const MediaPipelineDeviceParams&,
    const std::string& audio_device_id)>
    CreateMediaPipelineBackendCB;

}  // media
}  // chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_MEDIA_PIPELINE_BACKEND_FACTORY_H_
