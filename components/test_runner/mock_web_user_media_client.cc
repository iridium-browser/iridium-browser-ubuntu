// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/mock_web_user_media_client.h"

#include "base/logging.h"
#include "base/macros.h"
#include "components/test_runner/mock_constraints.h"
#include "components/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/platform/WebSourceInfo.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebMediaDevicesRequest.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

using blink::WebMediaConstraints;
using blink::WebMediaDeviceInfo;
using blink::WebMediaDevicesRequest;
using blink::WebMediaStream;
using blink::WebMediaStreamSource;
using blink::WebMediaStreamTrack;
using blink::WebMediaStreamTrackSourcesRequest;
using blink::WebSourceInfo;
using blink::WebString;
using blink::WebUserMediaRequest;
using blink::WebVector;

namespace test_runner {

class UserMediaRequestTask : public WebMethodTask<MockWebUserMediaClient> {
 public:
  UserMediaRequestTask(MockWebUserMediaClient* object,
                       const WebUserMediaRequest& request,
                       const WebMediaStream result)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request),
        result_(result) {
    DCHECK(!result_.isNull());
  }

  void RunIfValid() override { request_.requestSucceeded(result_); }

 private:
  WebUserMediaRequest request_;
  WebMediaStream result_;

  DISALLOW_COPY_AND_ASSIGN(UserMediaRequestTask);
};

class UserMediaRequestConstraintFailedTask
    : public WebMethodTask<MockWebUserMediaClient> {
 public:
  UserMediaRequestConstraintFailedTask(MockWebUserMediaClient* object,
                                       const WebUserMediaRequest& request,
                                       const WebString& constraint)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request),
        constraint_(constraint) {}

  void RunIfValid() override { request_.requestFailedConstraint(constraint_); }

 private:
  WebUserMediaRequest request_;
  WebString constraint_;

  DISALLOW_COPY_AND_ASSIGN(UserMediaRequestConstraintFailedTask);
};

class UserMediaRequestPermissionDeniedTask
    : public WebMethodTask<MockWebUserMediaClient> {
 public:
  UserMediaRequestPermissionDeniedTask(MockWebUserMediaClient* object,
                                       const WebUserMediaRequest& request)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request) {}

  void RunIfValid() override { request_.requestFailed(); }

 private:
  WebUserMediaRequest request_;

  DISALLOW_COPY_AND_ASSIGN(UserMediaRequestPermissionDeniedTask);
};

class MediaDevicesRequestTask : public WebMethodTask<MockWebUserMediaClient> {
 public:
  MediaDevicesRequestTask(MockWebUserMediaClient* object,
                          const WebMediaDevicesRequest& request,
                          const WebVector<WebMediaDeviceInfo>& result)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request),
        result_(result) {}

  void RunIfValid() override { request_.requestSucceeded(result_); }

 private:
  WebMediaDevicesRequest request_;
  WebVector<WebMediaDeviceInfo> result_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesRequestTask);
};

class SourcesRequestTask : public WebMethodTask<MockWebUserMediaClient> {
 public:
  SourcesRequestTask(MockWebUserMediaClient* object,
                     const WebMediaStreamTrackSourcesRequest& request,
                     const WebVector<WebSourceInfo>& result)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request),
        result_(result) {}

  void RunIfValid() override { request_.requestSucceeded(result_); }

 private:
  WebMediaStreamTrackSourcesRequest request_;
  WebVector<WebSourceInfo> result_;

  DISALLOW_COPY_AND_ASSIGN(SourcesRequestTask);
};

class MockExtraData : public WebMediaStream::ExtraData {
 public:
  int foo;
};

MockWebUserMediaClient::MockWebUserMediaClient(WebTestDelegate* delegate)
    : delegate_(delegate) {}

void MockWebUserMediaClient::requestUserMedia(
    const WebUserMediaRequest& stream_request) {
    DCHECK(!stream_request.isNull());
    WebUserMediaRequest request = stream_request;

    if (request.ownerDocument().isNull() || !request.ownerDocument().frame()) {
      delegate_->PostTask(
          new UserMediaRequestPermissionDeniedTask(this, request));
        return;
    }

    WebMediaConstraints constraints = request.audioConstraints();
    WebString failed_constraint;
    if (!constraints.isNull() &&
        !MockConstraints::VerifyConstraints(constraints, &failed_constraint)) {
      delegate_->PostTask(new UserMediaRequestConstraintFailedTask(
          this, request, failed_constraint));
      return;
    }
    constraints = request.videoConstraints();
    if (!constraints.isNull() &&
        !MockConstraints::VerifyConstraints(constraints, &failed_constraint)) {
      delegate_->PostTask(new UserMediaRequestConstraintFailedTask(
          this, request, failed_constraint));
      return;
    }

    const size_t zero = 0;
    const size_t one = 1;
    WebVector<WebMediaStreamTrack> audio_tracks(request.audio() ? one : zero);
    WebVector<WebMediaStreamTrack> video_tracks(request.video() ? one : zero);

    if (request.audio()) {
      WebMediaStreamSource source;
      source.initialize("MockAudioDevice#1",
                        WebMediaStreamSource::TypeAudio,
                        "Mock audio device",
                        false /* remote */, true /* readonly */);
      audio_tracks[0].initialize(source);
    }

    if (request.video()) {
      WebMediaStreamSource source;
      source.initialize("MockVideoDevice#1",
                        WebMediaStreamSource::TypeVideo,
                        "Mock video device",
                        false /* remote */, true /* readonly */);
      video_tracks[0].initialize(source);
    }

    WebMediaStream stream;
    stream.initialize(audio_tracks, video_tracks);

    stream.setExtraData(new MockExtraData());

    delegate_->PostTask(new UserMediaRequestTask(this, request, stream));
}

void MockWebUserMediaClient::cancelUserMediaRequest(
    const WebUserMediaRequest&) {
}

void MockWebUserMediaClient::requestMediaDevices(
    const WebMediaDevicesRequest& request) {
  struct {
    const char* device_id;
    WebMediaDeviceInfo::MediaDeviceKind kind;
    const char* label;
    const char* group_id;
  } test_devices[] = {
    {
      "device1",
      WebMediaDeviceInfo::MediaDeviceKindAudioInput,
      "Built-in microphone",
      "group1",
    },
    {
      "device2",
      WebMediaDeviceInfo::MediaDeviceKindAudioOutput,
      "Built-in speakers",
      "group1",
    },
    {
      "device3",
      WebMediaDeviceInfo::MediaDeviceKindVideoInput,
      "Build-in webcam",
      "group2",
    },
  };

  WebVector<WebMediaDeviceInfo> devices(arraysize(test_devices));
  for (size_t i = 0; i < arraysize(test_devices); ++i) {
    devices[i].initialize(WebString::fromUTF8(test_devices[i].device_id),
                          test_devices[i].kind,
                          WebString::fromUTF8(test_devices[i].label),
                          WebString::fromUTF8(test_devices[i].group_id));
  }

  delegate_->PostTask(new MediaDevicesRequestTask(this, request, devices));
}

void MockWebUserMediaClient::cancelMediaDevicesRequest(
    const WebMediaDevicesRequest&) {
}

void MockWebUserMediaClient::requestSources(
    const blink::WebMediaStreamTrackSourcesRequest& request) {
  struct {
    const char* id;
    WebSourceInfo::SourceKind kind;
    const char* label;
    WebSourceInfo::VideoFacingMode facing;
  } test_sources[] = {
    {
      "device1",
      WebSourceInfo::SourceKindAudio,
      "Built-in microphone",
      WebSourceInfo::VideoFacingModeNone,
    },
    {
      "device2",
      WebSourceInfo::SourceKindVideo,
      "Build-in webcam",
      WebSourceInfo::VideoFacingModeEnvironment,
    },
  };

  WebVector<WebSourceInfo> sources(arraysize(test_sources));
  for (size_t i = 0; i < arraysize(test_sources); ++i) {
  sources[i].initialize(WebString::fromUTF8(test_sources[i].id),
                        test_sources[i].kind,
                        WebString::fromUTF8(test_sources[i].label),
                        test_sources[i].facing);
  }

  delegate_->PostTask(new SourcesRequestTask(this, request, sources));
}

}  // namespace test_runner
