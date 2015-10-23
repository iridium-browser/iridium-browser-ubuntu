// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_TEST_H_
#define CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_TEST_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

namespace IPC {
  class Message;
}

// A fake completion object for verification.
class FakeTextCheckingCompletion : public blink::WebTextCheckingCompletion {
 public:
  FakeTextCheckingCompletion();
  ~FakeTextCheckingCompletion();

  void didFinishCheckingText(
      const blink::WebVector<blink::WebTextCheckingResult>& results) override;
  void didCancelCheckingText() override;

  size_t completion_count_;
  size_t cancellation_count_;
};

// Faked test target, which stores sent message for verification.
class TestingSpellCheckProvider : public SpellCheckProvider {
 public:
  TestingSpellCheckProvider();
  // Takes ownership of |spellcheck|.
  explicit TestingSpellCheckProvider(SpellCheck* spellcheck);

  ~TestingSpellCheckProvider() override;
  bool Send(IPC::Message* message) override;
  void OnCallSpellingService(int route_id,
                             int identifier,
                             const base::string16& text,
                             const std::vector<SpellCheckMarker>& markers);
  void ResetResult();

  base::string16 text_;
  ScopedVector<IPC::Message> messages_;
  size_t spelling_service_call_count_;
};

// SpellCheckProvider test fixture.
class SpellCheckProviderTest : public testing::Test {
 public:
  SpellCheckProviderTest();
  ~SpellCheckProviderTest() override;

 protected:
  TestingSpellCheckProvider provider_;
};

#endif  // CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_TEST_H_
