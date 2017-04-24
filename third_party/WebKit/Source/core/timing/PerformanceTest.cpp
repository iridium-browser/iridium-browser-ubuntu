// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/Performance.h"

#include "core/frame/PerformanceMonitor.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_pageHolder->document().setURL(KURL(KURL(), "https://example.com"));
    m_performance = Performance::create(&m_pageHolder->frame());

    // Create another dummy page holder and pretend this is the iframe.
    m_anotherPageHolder = DummyPageHolder::create(IntSize(400, 300));
    m_anotherPageHolder->document().setURL(
        KURL(KURL(), "https://iframed.com/bar"));
  }

  bool observingLongTasks() {
    return PerformanceMonitor::instrumentingMonitor(
        m_performance->getExecutionContext());
  }

  void addLongTaskObserver() {
    // simulate with filter options.
    m_performance->m_observerFilterOptions |= PerformanceEntry::LongTask;
  }

  void removeLongTaskObserver() {
    // simulate with filter options.
    m_performance->m_observerFilterOptions = PerformanceEntry::Invalid;
  }

  LocalFrame* frame() const { return m_pageHolder->document().frame(); }

  Document* document() const { return &m_pageHolder->document(); }

  LocalFrame* anotherFrame() const {
    return m_anotherPageHolder->document().frame();
  }

  Document* anotherDocument() const { return &m_anotherPageHolder->document(); }

  String sanitizedAttribution(ExecutionContext* context,
                              bool hasMultipleContexts,
                              LocalFrame* observerFrame) {
    return Performance::sanitizedAttribution(context, hasMultipleContexts,
                                             observerFrame)
        .first;
  }

  Persistent<Performance> m_performance;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  std::unique_ptr<DummyPageHolder> m_anotherPageHolder;
};

TEST_F(PerformanceTest, LongTaskObserverInstrumentation) {
  m_performance->updateLongTaskInstrumentation();
  EXPECT_FALSE(observingLongTasks());

  // Adding LongTask observer (with filer option) enables instrumentation.
  addLongTaskObserver();
  m_performance->updateLongTaskInstrumentation();
  EXPECT_TRUE(observingLongTasks());

  // Removing LongTask observer disables instrumentation.
  removeLongTaskObserver();
  m_performance->updateLongTaskInstrumentation();
  EXPECT_FALSE(observingLongTasks());
}

TEST_F(PerformanceTest, SanitizedLongTaskName) {
  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", sanitizedAttribution(nullptr, false, frame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("self", sanitizedAttribution(document(), false, frame()));

  // Unable to attribute, when multiple script execution contents are involved.
  EXPECT_EQ("multiple-contexts",
            sanitizedAttribution(document(), true, frame()));
}

TEST_F(PerformanceTest, SanitizedLongTaskName_CrossOrigin) {
  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", sanitizedAttribution(nullptr, false, frame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("cross-origin-unreachable",
            sanitizedAttribution(anotherDocument(), false, frame()));
}
}
