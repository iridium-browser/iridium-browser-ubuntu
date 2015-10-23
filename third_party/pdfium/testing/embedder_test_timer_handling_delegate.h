// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_EMBEDDER_TEST_TIMER_HANDLING_DELEGATE_H_
#define TESTING_EMBEDDER_TEST_TIMER_HANDLING_DELEGATE_H_

#include <map>
#include <utility>

#include "embedder_test.h"

class EmbedderTestTimerHandlingDelegate : public EmbedderTest::Delegate {
 public:
  int SetTimer(int msecs, TimerCallback fn) override {
    expiry_to_timer_map_.insert(std::pair<int, Timer>(
        msecs + imaginary_elapsed_msecs_, Timer(++next_timer_id_, fn)));
    return next_timer_id_;
  }

  void KillTimer(int id) override {
    for (auto iter = expiry_to_timer_map_.begin();
         iter != expiry_to_timer_map_.end(); ++iter) {
      if (iter->second.first == id) {
        expiry_to_timer_map_.erase(iter);
        break;
      }
    }
  }

  void AdvanceTime(int increment_msecs) {
    imaginary_elapsed_msecs_ += increment_msecs;
    while (1) {
      auto iter = expiry_to_timer_map_.begin();
      if (iter == expiry_to_timer_map_.end()) {
        break;
      }
      Timer t = iter->second;
      if (t.first > imaginary_elapsed_msecs_) {
        break;
      }
      expiry_to_timer_map_.erase(iter);
      t.second(t.first);  // Fire timer.
    }
  }

 protected:
  using Timer = std::pair<int, TimerCallback>;     // ID, callback pair.
  std::multimap<int, Timer> expiry_to_timer_map_;  // Keyed by timeout.
  int next_timer_id_ = 0;
  int imaginary_elapsed_msecs_ = 0;
};

#endif  // TESTING_EMBEDDER_TEST_TIMER_HANDLING_DELEGATE_H_
