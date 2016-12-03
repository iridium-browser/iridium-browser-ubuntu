// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/modules/webmidi/WebMIDIAccessor.h"

namespace blink {
class WebMIDIAccessorClient;
}

namespace test_runner {

class TestInterfaces;

class MockWebMIDIAccessor : public blink::WebMIDIAccessor {
 public:
  explicit MockWebMIDIAccessor(blink::WebMIDIAccessorClient* client,
                               TestInterfaces* interfaces);
  ~MockWebMIDIAccessor() override;

  // blink::WebMIDIAccessor implementation.
  void startSession() override;
  void sendMIDIData(unsigned port_index,
                    const unsigned char* data,
                    size_t length,
                    double timestamp) override;

 private:
  void ReportStartedSession(bool success);

  blink::WebMIDIAccessorClient* client_;
  TestInterfaces* interfaces_;

  base::WeakPtrFactory<MockWebMIDIAccessor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockWebMIDIAccessor);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_
