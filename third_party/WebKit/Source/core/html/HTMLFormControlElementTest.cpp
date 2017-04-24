// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLFormControlElement.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLInputElement.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class HTMLFormControlElementTest : public ::testing::Test {
 protected:
  void SetUp() override;

  DummyPageHolder& page() const { return *m_dummyPageHolder; }
  Document& document() const { return *m_document; }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<Document> m_document;
};

void HTMLFormControlElementTest::SetUp() {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);

  m_document = &m_dummyPageHolder->document();
  m_document->setMimeType("text/html");
}

TEST_F(HTMLFormControlElementTest, customValidationMessageTextDirection) {
  document().documentElement()->setInnerHTML(
      "<body><input pattern='abc' value='def' id=input></body>",
      ASSERT_NO_EXCEPTION);
  document().view()->updateAllLifecyclePhases();

  HTMLInputElement* input =
      toHTMLInputElement(document().getElementById("input"));
  input->setCustomValidity(
      String::fromUTF8("\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x89"));
  input->setAttribute(
      HTMLNames::titleAttr,
      AtomicString::fromUTF8("\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x89"));

  String message = input->validationMessage().stripWhiteSpace();
  String subMessage = input->validationSubMessage().stripWhiteSpace();
  TextDirection messageDir = TextDirection::kRtl;
  TextDirection subMessageDir = TextDirection::kLtr;

  input->findCustomValidationMessageTextDirection(message, messageDir,
                                                  subMessage, subMessageDir);
  EXPECT_EQ(TextDirection::kRtl, messageDir);
  EXPECT_EQ(TextDirection::kLtr, subMessageDir);

  input->layoutObject()->mutableStyleRef().setDirection(TextDirection::kRtl);
  input->findCustomValidationMessageTextDirection(message, messageDir,
                                                  subMessage, subMessageDir);
  EXPECT_EQ(TextDirection::kRtl, messageDir);
  EXPECT_EQ(TextDirection::kLtr, subMessageDir);

  input->setCustomValidity(String::fromUTF8("Main message."));
  message = input->validationMessage().stripWhiteSpace();
  subMessage = input->validationSubMessage().stripWhiteSpace();
  input->findCustomValidationMessageTextDirection(message, messageDir,
                                                  subMessage, subMessageDir);
  EXPECT_EQ(TextDirection::kLtr, messageDir);
  EXPECT_EQ(TextDirection::kLtr, subMessageDir);

  input->setCustomValidity(String());
  message = input->validationMessage().stripWhiteSpace();
  subMessage = input->validationSubMessage().stripWhiteSpace();
  input->findCustomValidationMessageTextDirection(message, messageDir,
                                                  subMessage, subMessageDir);
  EXPECT_EQ(TextDirection::kLtr, messageDir);
  EXPECT_EQ(TextDirection::kRtl, subMessageDir);
}

}  // namespace blink
