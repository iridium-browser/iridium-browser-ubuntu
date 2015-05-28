// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_util.h"

#include <math.h>

#include "base/metrics/histogram.h"
#include "media/base/media_keys.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"

namespace media {

// Compile asserts shared by all platforms.

#define STATIC_ASSERT_MATCHING_ENUM(name) \
  static_assert( \
  static_cast<int>(blink::WebMediaPlayerClient::MediaKeyErrorCode ## name) == \
  static_cast<int>(MediaKeys::k ## name ## Error), \
  "mismatching enum values: " #name)
STATIC_ASSERT_MATCHING_ENUM(Unknown);
STATIC_ASSERT_MATCHING_ENUM(Client);
#undef STATIC_ASSERT_MATCHING_ENUM

base::TimeDelta ConvertSecondsToTimestamp(double seconds) {
  double microseconds = seconds * base::Time::kMicrosecondsPerSecond;
  return base::TimeDelta::FromMicroseconds(
      microseconds > 0 ? microseconds + 0.5 : ceil(microseconds - 0.5));
}

blink::WebTimeRanges ConvertToWebTimeRanges(
    const Ranges<base::TimeDelta>& ranges) {
  blink::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); ++i) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

blink::WebMediaPlayer::NetworkState PipelineErrorToNetworkState(
    PipelineStatus error) {
  DCHECK_NE(error, PIPELINE_OK);

  switch (error) {
    case PIPELINE_ERROR_NETWORK:
    case PIPELINE_ERROR_READ:
      return blink::WebMediaPlayer::NetworkStateNetworkError;

    // TODO(vrk): Because OnPipelineInitialize() directly reports the
    // NetworkStateFormatError instead of calling OnPipelineError(), I believe
    // this block can be deleted. Should look into it! (crbug.com/126070)
    case PIPELINE_ERROR_INITIALIZATION_FAILED:
    case PIPELINE_ERROR_COULD_NOT_RENDER:
    case PIPELINE_ERROR_URL_NOT_FOUND:
    case DEMUXER_ERROR_COULD_NOT_OPEN:
    case DEMUXER_ERROR_COULD_NOT_PARSE:
    case DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
    case DECODER_ERROR_NOT_SUPPORTED:
      return blink::WebMediaPlayer::NetworkStateFormatError;

    case PIPELINE_ERROR_DECODE:
    case PIPELINE_ERROR_ABORT:
    case PIPELINE_ERROR_OPERATION_PENDING:
    case PIPELINE_ERROR_INVALID_STATE:
      return blink::WebMediaPlayer::NetworkStateDecodeError;

    case PIPELINE_ERROR_DECRYPT:
      // TODO(xhwang): Change to use NetworkStateDecryptError once it's added in
      // Webkit (see http://crbug.com/124486).
      return blink::WebMediaPlayer::NetworkStateDecodeError;

    case PIPELINE_OK:
      NOTREACHED() << "Unexpected status! " << error;
  }
  return blink::WebMediaPlayer::NetworkStateFormatError;
}

namespace {

// Helper enum for reporting scheme histograms.
enum URLSchemeForHistogram {
  kUnknownURLScheme,
  kMissingURLScheme,
  kHttpURLScheme,
  kHttpsURLScheme,
  kFtpURLScheme,
  kChromeExtensionURLScheme,
  kJavascriptURLScheme,
  kFileURLScheme,
  kBlobURLScheme,
  kDataURLScheme,
  kFileSystemScheme,
  kMaxURLScheme = kFileSystemScheme  // Must be equal to highest enum value.
};

URLSchemeForHistogram URLScheme(const GURL& url) {
  if (!url.has_scheme()) return kMissingURLScheme;
  if (url.SchemeIs("http")) return kHttpURLScheme;
  if (url.SchemeIs("https")) return kHttpsURLScheme;
  if (url.SchemeIs("ftp")) return kFtpURLScheme;
  if (url.SchemeIs("chrome-extension")) return kChromeExtensionURLScheme;
  if (url.SchemeIs("javascript")) return kJavascriptURLScheme;
  if (url.SchemeIs("file")) return kFileURLScheme;
  if (url.SchemeIs("blob")) return kBlobURLScheme;
  if (url.SchemeIs("data")) return kDataURLScheme;
  if (url.SchemeIs("filesystem")) return kFileSystemScheme;

  return kUnknownURLScheme;
}

}  // namespace

void ReportMediaSchemeUma(const GURL& url) {
  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(url),
                            kMaxURLScheme + 1);
}

EmeInitDataType ConvertToEmeInitDataType(
    blink::WebEncryptedMediaInitDataType init_data_type) {
  switch (init_data_type) {
    case blink::WebEncryptedMediaInitDataType::Webm:
      return EmeInitDataType::WEBM;
    case blink::WebEncryptedMediaInitDataType::Cenc:
      return EmeInitDataType::CENC;
    case blink::WebEncryptedMediaInitDataType::Keyids:
      return EmeInitDataType::KEYIDS;
    case blink::WebEncryptedMediaInitDataType::Unknown:
      return EmeInitDataType::UNKNOWN;
  }

  NOTREACHED();
  return EmeInitDataType::UNKNOWN;
}

blink::WebEncryptedMediaInitDataType ConvertToWebInitDataType(
    EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case EmeInitDataType::WEBM:
      return blink::WebEncryptedMediaInitDataType::Webm;
    case EmeInitDataType::CENC:
      return blink::WebEncryptedMediaInitDataType::Cenc;
    case EmeInitDataType::KEYIDS:
      return blink::WebEncryptedMediaInitDataType::Keyids;
    case EmeInitDataType::UNKNOWN:
      return blink::WebEncryptedMediaInitDataType::Unknown;
  }

  NOTREACHED();
  return blink::WebEncryptedMediaInitDataType::Unknown;
}

}  // namespace media
