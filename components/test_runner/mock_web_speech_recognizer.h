// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_SPEECH_RECOGNIZER_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_SPEECH_RECOGNIZER_H_

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "components/test_runner/web_task.h"
#include "third_party/WebKit/public/web/WebSpeechRecognizer.h"

namespace blink {
class WebSpeechRecognitionHandle;
class WebSpeechRecognitionParams;
class WebSpeechRecognizerClient;
class WebString;
}

namespace test_runner {

class WebTestDelegate;

class MockWebSpeechRecognizer : public blink::WebSpeechRecognizer {
 public:
  MockWebSpeechRecognizer();
  virtual ~MockWebSpeechRecognizer();

  void SetDelegate(WebTestDelegate* delegate);

  // WebSpeechRecognizer implementation:
  virtual void start(const blink::WebSpeechRecognitionHandle& handle,
                     const blink::WebSpeechRecognitionParams& params,
                     blink::WebSpeechRecognizerClient* client);
  virtual void stop(const blink::WebSpeechRecognitionHandle& handle,
                    blink::WebSpeechRecognizerClient* client);
  virtual void abort(const blink::WebSpeechRecognitionHandle& handle,
                     blink::WebSpeechRecognizerClient* client);

  // Methods accessed by layout tests:
  void AddMockResult(const blink::WebString& transcript, float confidence);
  void SetError(const blink::WebString& error, const blink::WebString& message);
  bool WasAborted() const { return was_aborted_; }

  // Methods accessed from Task objects:
  blink::WebSpeechRecognizerClient* Client() { return client_; }
  blink::WebSpeechRecognitionHandle& Handle() { return handle_; }
  WebTaskList* mutable_task_list() { return &task_list_; }

  class Task {
   public:
    Task(MockWebSpeechRecognizer* recognizer) : recognizer_(recognizer) {}
    virtual ~Task() {}
    virtual void run() = 0;

   protected:
    MockWebSpeechRecognizer* recognizer_;

   private:
    DISALLOW_COPY_AND_ASSIGN(Task);
  };

 private:
  void StartTaskQueue();
  void ClearTaskQueue();

  WebTaskList task_list_;
  blink::WebSpeechRecognitionHandle handle_;
  blink::WebSpeechRecognizerClient* client_;
  std::vector<blink::WebString> mock_transcripts_;
  std::vector<float> mock_confidences_;
  bool was_aborted_;

  // Queue of tasks to be run.
  std::deque<Task*> task_queue_;
  bool task_queue_running_;

  WebTestDelegate* delegate_;

  // Task for stepping the queue.
  class StepTask : public WebMethodTask<MockWebSpeechRecognizer> {
   public:
    StepTask(MockWebSpeechRecognizer* object)
        : WebMethodTask<MockWebSpeechRecognizer>(object) {}
    void RunIfValid() override;

   private:
    DISALLOW_COPY_AND_ASSIGN(StepTask);
  };

  DISALLOW_COPY_AND_ASSIGN(MockWebSpeechRecognizer);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_SPEECH_RECOGNIZER_H_
