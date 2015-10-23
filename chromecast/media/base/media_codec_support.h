// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_MEDIA_CODEC_SUPPORT_H_
#define CHROMECAST_MEDIA_BASE_MEDIA_CODEC_SUPPORT_H_

#include <string>

#include "base/callback.h"
#include "net/base/mime_util.h"

// TODO(gunsch/servolk): remove when this definition exists upstream.
namespace net {
typedef base::Callback<bool(const std::string&)> IsCodecSupportedCB;
}

namespace chromecast {
namespace media {

// Returns the callback to decide whether a given codec (passed in as a string
// representation of the codec id conforming to RFC 6381) is supported or not.
net::IsCodecSupportedCB GetIsCodecSupportedOnChromecastCB();

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_MEDIA_CODEC_SUPPORT_H_
