// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_

#include "base/basictypes.h"
#include "components/test_runner/web_task.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"

namespace blink {
class WebAudioSourceProvider;
class WebMediaStreamCenterClient;
};

namespace test_runner {

class TestInterfaces;

class MockWebMediaStreamCenter : public blink::WebMediaStreamCenter {
 public:
  MockWebMediaStreamCenter(blink::WebMediaStreamCenterClient* client,
                           TestInterfaces* interfaces);
  virtual ~MockWebMediaStreamCenter();

  virtual void didEnableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track);
  virtual void didDisableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track);
  virtual bool didAddMediaStreamTrack(const blink::WebMediaStream& stream,
                                      const blink::WebMediaStreamTrack& track);
  virtual bool didRemoveMediaStreamTrack(
      const blink::WebMediaStream& stream,
      const blink::WebMediaStreamTrack& track);
  virtual void didStopLocalMediaStream(const blink::WebMediaStream& stream);
  virtual bool didStopMediaStreamTrack(const blink::WebMediaStreamTrack& track);
  virtual void didCreateMediaStream(blink::WebMediaStream& stream);
  virtual blink::WebAudioSourceProvider*
      createWebAudioSourceFromMediaStreamTrack(
          const blink::WebMediaStreamTrack& track);

  // Task related methods
  WebTaskList* mutable_task_list() { return &task_list_; }

 private:
  WebTaskList task_list_;
  TestInterfaces* interfaces_;

  DISALLOW_COPY_AND_ASSIGN(MockWebMediaStreamCenter);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_
