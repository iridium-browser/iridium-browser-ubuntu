// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_

#include "base/basictypes.h"
#include "components/test_runner/web_task.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessor.h"

namespace blink {
class WebMIDIAccessorClient;
}

namespace test_runner {

class TestInterfaces;

class MockWebMIDIAccessor : public blink::WebMIDIAccessor {
 public:
  explicit MockWebMIDIAccessor(blink::WebMIDIAccessorClient* client,
                               TestInterfaces* interfaces);
  virtual ~MockWebMIDIAccessor();

  // blink::WebMIDIAccessor implementation.
  virtual void startSession() override;
  virtual void sendMIDIData(unsigned port_index,
                            const unsigned char* data,
                            size_t length,
                            double timestamp) override;

  // WebTask related methods
  WebTaskList* mutable_task_list() { return &task_list_; }

 private:
  blink::WebMIDIAccessorClient* client_;
  WebTaskList task_list_;
  TestInterfaces* interfaces_;

  DISALLOW_COPY_AND_ASSIGN(MockWebMIDIAccessor);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_
