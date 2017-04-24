// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/InputMethodController.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class InputMethodControllerTest : public ::testing::Test {
 protected:
  InputMethodController& controller() {
    return frame().inputMethodController();
  }
  Document& document() const { return *m_document; }
  LocalFrame& frame() const { return m_dummyPageHolder->frame(); }
  Element* insertHTMLElement(const char* elementCode, const char* elementId);
  void createHTMLWithCompositionInputEventListeners();
  void createHTMLWithCompositionEndEventListener(const SelectionType);

 private:
  void SetUp() override;

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<Document> m_document;
};

void InputMethodControllerTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
  m_document = &m_dummyPageHolder->document();
  DCHECK(m_document);
}

Element* InputMethodControllerTest::insertHTMLElement(const char* elementCode,
                                                      const char* elementId) {
  document().write(elementCode);
  document().updateStyleAndLayout();
  Element* element = document().getElementById(elementId);
  element->focus();
  return element;
}

void InputMethodControllerTest::createHTMLWithCompositionInputEventListeners() {
  document().settings()->setScriptEnabled(true);
  Element* editable =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  Element* script = document().createElement("script");
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('beforeinput', "
      "  event => document.title = `beforeinput.data:${event.data};`);"
      "document.getElementById('sample').addEventListener('input', "
      "  event => document.title += `input.data:${event.data};`);"
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => document.title += `compositionend.data:${event.data};`);");
  document().body()->appendChild(script);
  document().view()->updateAllLifecyclePhases();
  editable->focus();
}

void InputMethodControllerTest::createHTMLWithCompositionEndEventListener(
    const SelectionType type) {
  document().settings()->setScriptEnabled(true);
  Element* editable =
      insertHTMLElement("<div id='sample' contentEditable></div>", "sample");
  Element* script = document().createElement("script");

  switch (type) {
    case NoSelection:
      script->setInnerHTML(
          // If the caret position is set before firing 'compositonend' event
          // (and it should), the final caret position will be reset to null.
          "document.getElementById('sample').addEventListener('compositionend',"
          "  event => getSelection().removeAllRanges());");
      break;
    case CaretSelection:
      script->setInnerHTML(
          // If the caret position is set before firing 'compositonend' event
          // (and it should), the final caret position will be reset to [3,3].
          "document.getElementById('sample').addEventListener('compositionend',"
          "  event => {"
          "    const node = document.getElementById('sample').firstChild;"
          "    getSelection().collapse(node, 3);"
          "});");
      break;
    case RangeSelection:
      script->setInnerHTML(
          // If the caret position is set before firing 'compositonend' event
          // (and it should), the final caret position will be reset to [2,4].
          "document.getElementById('sample').addEventListener('compositionend',"
          "  event => {"
          "    const node = document.getElementById('sample').firstChild;"
          "    const selection = getSelection();"
          "    selection.collapse(node, 2);"
          "    selection.extend(node, 4);"
          "});");
      break;
    default:
      NOTREACHED();
  }
  document().body()->appendChild(script);
  document().view()->updateAllLifecyclePhases();
  editable->focus();
}

TEST_F(InputMethodControllerTest, BackspaceFromEndOfInput) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("fooX");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("fooX", input->value().utf8().data());
  controller().extendSelectionAndDelete(0, 0);
  EXPECT_STREQ("fooX", input->value().utf8().data());

  input->setValue("fooX");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("fooX", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  input->setValue(
      String::fromUTF8("foo\xE2\x98\x85"));  // U+2605 == "black star"
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo\xE2\x98\x85", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  input->setValue(
      String::fromUTF8("foo\xF0\x9F\x8F\x86"));  // U+1F3C6 == "trophy"
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho"
  input->setValue(String::fromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  input->setValue("fooX");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("fooX", input->value().utf8().data());
  controller().extendSelectionAndDelete(0, 1);
  EXPECT_STREQ("fooX", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionFromExistingText) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable>hello world</div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 5);

  Range* range = controller().compositionRange();
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(5u, range->endOffset());

  PlainTextRange plainTextRange(PlainTextRange::create(*div, *range));
  EXPECT_EQ(0u, plainTextRange.start());
  EXPECT_EQ(5u, plainTextRange.end());
}

TEST_F(InputMethodControllerTest, SetCompositionAfterEmoji) {
  // "trophy" = U+1F3C6 = 0xF0 0x9F 0x8F 0x86 (UTF8).
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable>&#x1f3c6</div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_EQ(2, frame()
                   .selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .start()
                   .computeOffsetInContainerNode());
  EXPECT_EQ(2, frame()
                   .selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .end()
                   .computeOffsetInContainerNode());

  controller().setComposition(String("a"), underlines, 1, 1);
  EXPECT_STREQ("\xF0\x9F\x8F\x86\x61", div->innerText().utf8().data());

  controller().setComposition(String("ab"), underlines, 2, 2);
  EXPECT_STREQ("\xF0\x9F\x8F\x86\x61\x62", div->innerText().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionWithGraphemeCluster) {
  insertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(6, 6, Color(255, 0, 0), false, 0));
  document().updateStyleAndLayout();

  // UTF16 = 0x0939 0x0947 0x0932 0x0932. Note that 0x0932 0x0932 is a grapheme
  // cluster.
  controller().setComposition(
      String::fromUTF8("\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA4\xB2"),
      underlines, 4, 4);
  EXPECT_EQ(4u, controller().getSelectionOffsets().start());
  EXPECT_EQ(4u, controller().getSelectionOffsets().end());

  // UTF16 = 0x0939 0x0947 0x0932 0x094D 0x0932 0x094B.
  controller().setComposition(
      String::fromUTF8("\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA5\x8D\xE0"
                       "\xA4\xB2\xE0\xA5\x8B"),
      underlines, 6, 6);
  EXPECT_EQ(6u, controller().getSelectionOffsets().start());
  EXPECT_EQ(6u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest,
       SetCompositionWithGraphemeClusterAndMultipleNodes) {
  Element* div =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(
      CompositionUnderline(12, 12, Color(255, 0, 0), false, 0));
  document().updateStyleAndLayout();

  // UTF16 = 0x0939 0x0947 0x0932 0x094D 0x0932 0x094B. 0x0939 0x0947 0x0932 is
  // a grapheme cluster, so is the remainding 0x0932 0x094B.
  controller().commitText(
      String::fromUTF8("\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA5\x8D\xE0"
                       "\xA4\xB2\xE0\xA5\x8B"),
      underlines, 1);
  controller().commitText("\nab ", underlines, 1);
  controller().setComposition(String("c"), underlines, 1, 1);
  EXPECT_STREQ(
      "\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA5\x8D\xE0\xA4\xB2\xE0\xA5"
      "\x8B\nab c",
      div->innerText().utf8().data());
  EXPECT_EQ(11u, controller().getSelectionOffsets().start());
  EXPECT_EQ(11u, controller().getSelectionOffsets().end());

  controller().setComposition(String("cd"), underlines, 2, 2);
  EXPECT_STREQ(
      "\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA5\x8D\xE0\xA4\xB2\xE0\xA5"
      "\x8B\nab cd",
      div->innerText().utf8().data());
  EXPECT_EQ(12u, controller().getSelectionOffsets().start());
  EXPECT_EQ(12u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, SetCompositionKeepingStyle) {
  Element* div = insertHTMLElement(
      "<div id='sample' "
      "contenteditable>abc1<b>2</b>34567<b>8</b>9d<b>e</b>f</div>",
      "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(3, 12, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 3, 12);

  // Subtract a character.
  controller().setComposition(String("12345789"), underlines, 8, 8);
  EXPECT_STREQ("abc1<b>2</b>3457<b>8</b>9d<b>e</b>f",
               div->innerHTML().utf8().data());
  EXPECT_EQ(11u, controller().getSelectionOffsets().start());
  EXPECT_EQ(11u, controller().getSelectionOffsets().end());

  // Append a character.
  controller().setComposition(String("123456789"), underlines, 9, 9);
  EXPECT_STREQ("abc1<b>2</b>34567<b>8</b>9d<b>e</b>f",
               div->innerHTML().utf8().data());
  EXPECT_EQ(12u, controller().getSelectionOffsets().start());
  EXPECT_EQ(12u, controller().getSelectionOffsets().end());

  // Subtract and append characters.
  controller().setComposition(String("123hello789"), underlines, 11, 11);
  EXPECT_STREQ("abc1<b>2</b>3hello7<b>8</b>9d<b>e</b>f",
               div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionWithEmojiKeepingStyle) {
  // U+1F3E0 = 0xF0 0x9F 0x8F 0xA0 (UTF8). It's an emoji character.
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable><b>&#x1f3e0</b></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  controller().setCompositionFromExistingText(underlines, 0, 2);

  // 0xF0 0x9F 0x8F 0xAB is also an emoji character, with the same leading
  // surrogate pair to the previous one.
  controller().setComposition(String::fromUTF8("\xF0\x9F\x8F\xAB"), underlines,
                              2, 2);
  EXPECT_STREQ("<b>\xF0\x9F\x8F\xAB</b>", div->innerHTML().utf8().data());

  controller().setComposition(String::fromUTF8("\xF0\x9F\x8F\xA0"), underlines,
                              2, 2);
  EXPECT_STREQ("<b>\xF0\x9F\x8F\xA0</b>", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest,
       SetCompositionWithTeluguSignVisargaKeepingStyle) {
  // U+0C03 = 0xE0 0xB0 0x83 (UTF8), a telugu sign visarga with one code point.
  // It's one grapheme cluster if separated. It can also form one grapheme
  // cluster with another code point(e.g, itself).
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable><b>&#xc03</b></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 1);

  // 0xE0 0xB0 0x83 0xE0 0xB0 0x83, a telugu character with 2 code points in
  // 1 grapheme cluster.
  controller().setComposition(String::fromUTF8("\xE0\xB0\x83\xE0\xB0\x83"),
                              underlines, 2, 2);
  EXPECT_STREQ("<b>\xE0\xB0\x83\xE0\xB0\x83</b>",
               div->innerHTML().utf8().data());

  controller().setComposition(String::fromUTF8("\xE0\xB0\x83"), underlines, 1,
                              1);
  EXPECT_STREQ("<b>\xE0\xB0\x83</b>", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest, FinishComposingTextKeepingStyle) {
  Element* div = insertHTMLElement(
      "<div id='sample' "
      "contenteditable>abc1<b>2</b>34567<b>8</b>9</div>",
      "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(3, 12, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 3, 12);

  controller().setComposition(String("123hello789"), underlines, 11, 11);
  EXPECT_STREQ("abc1<b>2</b>3hello7<b>8</b>9", div->innerHTML().utf8().data());

  controller().finishComposingText(InputMethodController::KeepSelection);
  EXPECT_STREQ("abc1<b>2</b>3hello7<b>8</b>9", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest, CommitTextKeepingStyle) {
  Element* div = insertHTMLElement(
      "<div id='sample' "
      "contenteditable>abc1<b>2</b>34567<b>8</b>9</div>",
      "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(3, 12, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 3, 12);

  controller().commitText(String("123789"), underlines, 0);
  EXPECT_STREQ("abc1<b>2</b>37<b>8</b>9", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest, InsertTextWithNewLine) {
  Element* div =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 11, Color(255, 0, 0), false, 0));

  controller().commitText(String("hello\nworld"), underlines, 0);
  EXPECT_STREQ("hello<div>world</div>", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest, InsertTextWithNewLineIncrementally) {
  Element* div =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 11, Color(255, 0, 0), false, 0));
  controller().setComposition("foo", underlines, 0, 2);
  EXPECT_STREQ("foo", div->innerHTML().utf8().data());

  controller().commitText(String("hello\nworld"), underlines, 0);
  EXPECT_STREQ("hello<div>world</div>", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest, SelectionOnConfirmExistingText) {
  insertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 5);

  controller().finishComposingText(InputMethodController::KeepSelection);
  EXPECT_EQ(0, frame()
                   .selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .start()
                   .computeOffsetInContainerNode());
  EXPECT_EQ(0, frame()
                   .selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .end()
                   .computeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest, DeleteBySettingEmptyComposition) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("foo ");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo ", input->value().utf8().data());
  controller().extendSelectionAndDelete(0, 0);
  EXPECT_STREQ("foo ", input->value().utf8().data());

  input->setValue("foo ");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo ", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 3, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 3);

  controller().setComposition(String(""), underlines, 0, 3);

  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       SetCompositionFromExistingTextWithCollapsedWhiteSpace) {
  // Creates a div with one leading new line char. The new line char is hidden
  // from the user and IME, but is visible to InputMethodController.
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable>\nhello world</div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 5);

  Range* range = controller().compositionRange();
  EXPECT_EQ(1u, range->startOffset());
  EXPECT_EQ(6u, range->endOffset());

  PlainTextRange plainTextRange(PlainTextRange::create(*div, *range));
  EXPECT_EQ(0u, plainTextRange.start());
  EXPECT_EQ(5u, plainTextRange.end());
}

TEST_F(InputMethodControllerTest,
       SetCompositionFromExistingTextWithInvalidOffsets) {
  insertHTMLElement("<div id='sample' contenteditable>test</div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(7, 8, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 7, 8);

  EXPECT_FALSE(controller().compositionRange());
}

TEST_F(InputMethodControllerTest, ConfirmPasswordComposition) {
  HTMLInputElement* input = toHTMLInputElement(insertHTMLElement(
      "<input id='sample' type='password' size='24'>", "sample"));

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setComposition("foo", underlines, 0, 3);
  controller().finishComposingText(InputMethodController::KeepSelection);

  EXPECT_STREQ("foo", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithEmptyText) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("");
  document().updateStyleAndLayout();
  EXPECT_STREQ("", input->value().utf8().data());
  controller().deleteSurroundingText(0, 0);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("");
  document().updateStyleAndLayout();
  EXPECT_STREQ("", input->value().utf8().data());
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("");
  document().updateStyleAndLayout();
  EXPECT_STREQ("", input->value().utf8().data());
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("");
  document().updateStyleAndLayout();
  EXPECT_STREQ("", input->value().utf8().data());
  controller().deleteSurroundingText(1, 1);
  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithRangeSelection) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(0, 0);
  EXPECT_STREQ("hello", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(1, 1);
  EXPECT_STREQ("ell", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(100, 0);
  EXPECT_STREQ("ello", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(0, 100);
  EXPECT_STREQ("hell", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(100, 100);
  EXPECT_STREQ("ell", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithCursorSelection) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("hllo", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ("helo", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(0, 0);
  EXPECT_STREQ("hello", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(1, 1);
  EXPECT_STREQ("hlo", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(100, 0);
  EXPECT_STREQ("llo", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(0, 100);
  EXPECT_STREQ("he", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(100, 100);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("h");
  document().updateStyleAndLayout();
  EXPECT_STREQ("h", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 1));
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("h");
  document().updateStyleAndLayout();
  EXPECT_STREQ("h", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnTheLeft) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // U+2605 == "black star". It takes up 1 space.
  input->setValue(String::fromUTF8("foo\xE2\x98\x85"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo\xE2\x98\x85", input->value().utf8().data());
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // U+1F3C6 == "trophy". It takes up 2 space.
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(5, 5));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho". It takes up 2 space.
  input->setValue(String::fromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(5, 5));
  EXPECT_STREQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(2, 0);
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(3, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(4, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(5, 0);
  EXPECT_STREQ("fo", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnTheRight) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // U+2605 == "black star". It takes up 1 space.
  input->setValue(String::fromUTF8("\xE2\x98\x85 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xE2\x98\x85 foo", input->value().utf8().data());
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // U+1F3C6 == "trophy". It takes up 2 space.
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86 foo", input->value().utf8().data());
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho". It takes up 2 space.
  input->setValue(String::fromUTF8("\xE0\xB8\x81\xE0\xB9\x89 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xE0\xB8\x81\xE0\xB9\x89 foo", input->value().utf8().data());
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo",
               input->value().utf8().data());
  controller().deleteSurroundingText(0, 2);
  EXPECT_STREQ("\xF0\x9F\x8F\x86 foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo",
               input->value().utf8().data());
  controller().deleteSurroundingText(0, 3);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo",
               input->value().utf8().data());
  controller().deleteSurroundingText(0, 4);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo",
               input->value().utf8().data());
  controller().deleteSurroundingText(0, 5);
  EXPECT_STREQ("foo", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnBothSides) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(1, 1);
  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextForMultipleNodes) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable>aaa"
      "<div id='sample2' contenteditable>bbb"
      "<div id='sample3' contenteditable>ccc"
      "<div id='sample4' contenteditable>ddd"
      "<div id='sample5' contenteditable>eee"
      "</div></div></div></div></div>",
      "sample");

  controller().setEditableSelectionOffsets(PlainTextRange(8, 8));
  EXPECT_STREQ("aaa\nbbb\nccc\nddd\neee", div->innerText().utf8().data());
  EXPECT_EQ(8u, controller().getSelectionOffsets().start());
  EXPECT_EQ(8u, controller().getSelectionOffsets().end());

  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("aaa\nbbbccc\nddd\neee", div->innerText().utf8().data());
  EXPECT_EQ(7u, controller().getSelectionOffsets().start());
  EXPECT_EQ(7u, controller().getSelectionOffsets().end());

  controller().deleteSurroundingText(0, 4);
  EXPECT_STREQ("aaa\nbbbddd\neee", div->innerText().utf8().data());
  EXPECT_EQ(7u, controller().getSelectionOffsets().start());
  EXPECT_EQ(7u, controller().getSelectionOffsets().end());

  controller().deleteSurroundingText(5, 5);
  EXPECT_STREQ("aaee", div->innerText().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextInCodePointsWithMultiCodeTextOnTheLeft) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text (U+0E01
  // "ka kai" + U+0E49 "mai tho").
  // A "black star" is 1 grapheme cluster. It has 1 code point, and its length
  // is 1 (abbreviated as [1,1,1]). A "trophy": [1,1,2]. The composed text:
  // [1,2,2].
  input->setValue(String::fromUTF8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89"));
  document().updateStyleAndLayout();
  // The cursor is at the end of the text.
  controller().setEditableSelectionOffsets(PlainTextRange(8, 8));

  controller().deleteSurroundingTextInCodePoints(2, 0);
  EXPECT_STREQ("a\xE2\x98\x85 \xF0\x9F\x8F\x86 ", input->value().utf8().data());
  controller().deleteSurroundingTextInCodePoints(4, 0);
  EXPECT_STREQ("a", input->value().utf8().data());

  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text
  input->setValue(String::fromUTF8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89"));
  document().updateStyleAndLayout();
  // The cursor is at the end of the text.
  controller().setEditableSelectionOffsets(PlainTextRange(8, 8));

  // TODO(yabinh): We should only delete 1 code point instead of the entire
  // grapheme cluster (2 code points). The root cause is that we adjust the
  // selection by grapheme cluster in deleteSurroundingText().
  controller().deleteSurroundingTextInCodePoints(1, 0);
  EXPECT_STREQ("a\xE2\x98\x85 \xF0\x9F\x8F\x86 ", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextInCodePointsWithMultiCodeTextOnTheRight) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text
  input->setValue(String::fromUTF8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));

  controller().deleteSurroundingTextInCodePoints(0, 5);
  EXPECT_STREQ("\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());

  controller().deleteSurroundingTextInCodePoints(0, 1);
  // TODO(yabinh): Same here. We should only delete 1 code point.
  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextInCodePointsWithMultiCodeTextOnBothSides) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text
  input->setValue(String::fromUTF8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(3, 3));
  controller().deleteSurroundingTextInCodePoints(2, 2);
  EXPECT_STREQ("a\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextInCodePointsWithImage) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable>aaa"
      "<img src='empty.png'>bbb</div>",
      "sample");

  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  controller().deleteSurroundingTextInCodePoints(1, 1);
  EXPECT_STREQ("aaabb", div->innerText().utf8().data());
  EXPECT_EQ(3u, controller().getSelectionOffsets().start());
  EXPECT_EQ(3u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextInCodePointsWithInvalidSurrogatePair) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // 'a' + high surrogate of "trophy" + "black star" + low surrogate of "trophy"
  // + SPACE
  const UChar uText[] = {'a', 0xD83C, 0x2605, 0xDFC6, ' ', '\0'};
  const String& text = String(uText);

  input->setValue(text);
  document().updateStyleAndLayout();
  // The invalid high surrogate is encoded as '\xED\xA0\xBC', and invalid low
  // surrogate is encoded as '\xED\xBF\x86'.
  EXPECT_STREQ("a\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86 ",
               input->value().utf8().data());

  controller().setEditableSelectionOffsets(PlainTextRange(5, 5));
  // Delete a SPACE.
  controller().deleteSurroundingTextInCodePoints(1, 0);
  EXPECT_STREQ("a\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86",
               input->value().utf8().data());
  // Do nothing since there is an invalid surrogate in the requested range.
  controller().deleteSurroundingTextInCodePoints(2, 0);
  EXPECT_STREQ("a\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86",
               input->value().utf8().data());

  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  // Delete 'a'.
  controller().deleteSurroundingTextInCodePoints(0, 1);
  EXPECT_STREQ("\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86",
               input->value().utf8().data());
  // Do nothing since there is an invalid surrogate in the requested range.
  controller().deleteSurroundingTextInCodePoints(0, 2);
  EXPECT_STREQ("\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86",
               input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionForInputWithNewCaretPositions) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("hello");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_STREQ("hello", input->value().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  // The caret exceeds left boundary.
  // "*heABllo", where * stands for caret.
  controller().setComposition("AB", underlines, -100, -100);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(0u, controller().getSelectionOffsets().start());
  EXPECT_EQ(0u, controller().getSelectionOffsets().end());

  // The caret is on left boundary.
  // "*heABllo".
  controller().setComposition("AB", underlines, -2, -2);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(0u, controller().getSelectionOffsets().start());
  EXPECT_EQ(0u, controller().getSelectionOffsets().end());

  // The caret is before the composing text.
  // "he*ABllo".
  controller().setComposition("AB", underlines, 0, 0);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  // The caret is after the composing text.
  // "heAB*llo".
  controller().setComposition("AB", underlines, 2, 2);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(4u, controller().getSelectionOffsets().start());
  EXPECT_EQ(4u, controller().getSelectionOffsets().end());

  // The caret is on right boundary.
  // "heABllo*".
  controller().setComposition("AB", underlines, 5, 5);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(7u, controller().getSelectionOffsets().start());
  EXPECT_EQ(7u, controller().getSelectionOffsets().end());

  // The caret exceeds right boundary.
  // "heABllo*".
  controller().setComposition("AB", underlines, 100, 100);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(7u, controller().getSelectionOffsets().start());
  EXPECT_EQ(7u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest,
       SetCompositionForContentEditableWithNewCaretPositions) {
  // There are 7 nodes and 5+1+5+1+3+4+3 characters: "hello", '\n', "world",
  // "\n", "012", "3456", "789".
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable>"
      "hello"
      "<div id='sample2' contenteditable>world"
      "<p>012<b>3456</b><i>789</i></p>"
      "</div>"
      "</div>",
      "sample");

  controller().setEditableSelectionOffsets(PlainTextRange(17, 17));
  EXPECT_STREQ("hello\nworld\n0123456789", div->innerText().utf8().data());
  EXPECT_EQ(17u, controller().getSelectionOffsets().start());
  EXPECT_EQ(17u, controller().getSelectionOffsets().end());

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  // The caret exceeds left boundary.
  // "*hello\nworld\n01234AB56789", where * stands for caret.
  controller().setComposition("AB", underlines, -100, -100);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(0u, controller().getSelectionOffsets().start());
  EXPECT_EQ(0u, controller().getSelectionOffsets().end());

  // The caret is on left boundary.
  // "*hello\nworld\n01234AB56789".
  controller().setComposition("AB", underlines, -17, -17);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(0u, controller().getSelectionOffsets().start());
  EXPECT_EQ(0u, controller().getSelectionOffsets().end());

  // The caret is in the 1st node.
  // "he*llo\nworld\n01234AB56789".
  controller().setComposition("AB", underlines, -15, -15);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  // The caret is on right boundary of the 1st node.
  // "hello*\nworld\n01234AB56789".
  controller().setComposition("AB", underlines, -12, -12);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(5u, controller().getSelectionOffsets().start());
  EXPECT_EQ(5u, controller().getSelectionOffsets().end());

  // The caret is on right boundary of the 2nd node.
  // "hello\n*world\n01234AB56789".
  controller().setComposition("AB", underlines, -11, -11);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(6u, controller().getSelectionOffsets().start());
  EXPECT_EQ(6u, controller().getSelectionOffsets().end());

  // The caret is on right boundary of the 3rd node.
  // "hello\nworld*\n01234AB56789".
  controller().setComposition("AB", underlines, -6, -6);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(11u, controller().getSelectionOffsets().start());
  EXPECT_EQ(11u, controller().getSelectionOffsets().end());

  // The caret is on right boundary of the 4th node.
  // "hello\nworld\n*01234AB56789".
  controller().setComposition("AB", underlines, -5, -5);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(12u, controller().getSelectionOffsets().start());
  EXPECT_EQ(12u, controller().getSelectionOffsets().end());

  // The caret is before the composing text.
  // "hello\nworld\n01234*AB56789".
  controller().setComposition("AB", underlines, 0, 0);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(17u, controller().getSelectionOffsets().start());
  EXPECT_EQ(17u, controller().getSelectionOffsets().end());

  // The caret is after the composing text.
  // "hello\nworld\n01234AB*56789".
  controller().setComposition("AB", underlines, 2, 2);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(19u, controller().getSelectionOffsets().start());
  EXPECT_EQ(19u, controller().getSelectionOffsets().end());

  // The caret is on right boundary.
  // "hello\nworld\n01234AB56789*".
  controller().setComposition("AB", underlines, 7, 7);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(24u, controller().getSelectionOffsets().start());
  EXPECT_EQ(24u, controller().getSelectionOffsets().end());

  // The caret exceeds right boundary.
  // "hello\nworld\n01234AB56789*".
  controller().setComposition("AB", underlines, 100, 100);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(24u, controller().getSelectionOffsets().start());
  EXPECT_EQ(24u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, SetCompositionWithEmptyText) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable>hello</div>", "sample");

  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_STREQ("hello", div->innerText().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  Vector<CompositionUnderline> underlines0;
  underlines0.push_back(CompositionUnderline(0, 0, Color(255, 0, 0), false, 0));
  Vector<CompositionUnderline> underlines2;
  underlines2.push_back(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  controller().setComposition("AB", underlines2, 2, 2);
  // With previous composition.
  controller().setComposition("", underlines0, 2, 2);
  EXPECT_STREQ("hello", div->innerText().utf8().data());
  EXPECT_EQ(4u, controller().getSelectionOffsets().start());
  EXPECT_EQ(4u, controller().getSelectionOffsets().end());

  // Without previous composition.
  controller().setComposition("", underlines0, -1, -1);
  EXPECT_STREQ("hello", div->innerText().utf8().data());
  EXPECT_EQ(3u, controller().getSelectionOffsets().start());
  EXPECT_EQ(3u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, InsertLineBreakWhileComposingText) {
  Element* div =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setComposition("hello", underlines, 5, 5);
  EXPECT_STREQ("hello", div->innerText().utf8().data());
  EXPECT_EQ(5u, controller().getSelectionOffsets().start());
  EXPECT_EQ(5u, controller().getSelectionOffsets().end());

  frame().editor().insertLineBreak();
  EXPECT_STREQ("\n\n", div->innerText().utf8().data());
  EXPECT_EQ(1u, controller().getSelectionOffsets().start());
  EXPECT_EQ(1u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, InsertLineBreakAfterConfirmingText) {
  Element* div =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));
  controller().commitText("hello", underlines, 0);
  EXPECT_STREQ("hello", div->innerText().utf8().data());

  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  frame().editor().insertLineBreak();
  EXPECT_STREQ("he\nllo", div->innerText().utf8().data());
  EXPECT_EQ(3u, controller().getSelectionOffsets().start());
  EXPECT_EQ(3u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, CompositionInputEventIsComposing) {
  document().settings()->setScriptEnabled(true);
  Element* editable =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  Element* script = document().createElement("script");
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('beforeinput', "
      "  event => document.title = "
      "  `beforeinput.isComposing:${event.isComposing};`);"
      "document.getElementById('sample').addEventListener('input', "
      "  event => document.title += "
      "  `input.isComposing:${event.isComposing};`);");
  document().body()->appendChild(script);
  document().view()->updateAllLifecyclePhases();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  editable->focus();

  document().setTitle(emptyString);
  controller().setComposition("foo", underlines, 0, 3);
  EXPECT_STREQ("beforeinput.isComposing:true;input.isComposing:true;",
               document().title().utf8().data());

  document().setTitle(emptyString);
  controller().commitText("bar", underlines, 0);
  // Last pair of InputEvent should also be inside composition scope.
  EXPECT_STREQ("beforeinput.isComposing:true;input.isComposing:true;",
               document().title().utf8().data());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForReplace) {
  createHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));

  document().setTitle(emptyString);
  controller().setComposition("hell", underlines, 4, 4);
  EXPECT_STREQ("beforeinput.data:hell;input.data:hell;",
               document().title().utf8().data());

  // Replace the existing composition.
  document().setTitle(emptyString);
  controller().setComposition("hello", underlines, 0, 0);
  EXPECT_STREQ("beforeinput.data:hello;input.data:hello;",
               document().title().utf8().data());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForConfirm) {
  createHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));

  document().setTitle(emptyString);
  controller().setComposition("hello", underlines, 5, 5);
  EXPECT_STREQ("beforeinput.data:hello;input.data:hello;",
               document().title().utf8().data());

  // Confirm the ongoing composition.
  document().setTitle(emptyString);
  controller().finishComposingText(InputMethodController::KeepSelection);
  EXPECT_STREQ("compositionend.data:hello;", document().title().utf8().data());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForDelete) {
  createHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));

  document().setTitle(emptyString);
  controller().setComposition("hello", underlines, 5, 5);
  EXPECT_STREQ("beforeinput.data:hello;input.data:hello;",
               document().title().utf8().data());

  // Delete the existing composition.
  document().setTitle(emptyString);
  controller().setComposition("", underlines, 0, 0);
  EXPECT_STREQ("beforeinput.data:;input.data:null;compositionend.data:;",
               document().title().utf8().data());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForInsert) {
  createHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));

  // Insert new text without previous composition.
  document().setTitle(emptyString);
  document().updateStyleAndLayout();
  controller().commitText("hello", underlines, 0);
  EXPECT_STREQ("beforeinput.data:hello;input.data:hello;",
               document().title().utf8().data());

  document().setTitle(emptyString);
  controller().setComposition("n", underlines, 1, 1);
  EXPECT_STREQ("beforeinput.data:n;input.data:n;",
               document().title().utf8().data());

  // Insert new text with previous composition.
  document().setTitle(emptyString);
  document().updateStyleAndLayout();
  controller().commitText("hello", underlines, 1);
  EXPECT_STREQ(
      "beforeinput.data:hello;input.data:hello;compositionend.data:hello;",
      document().title().utf8().data());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForInsertEmptyText) {
  createHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));

  // Insert empty text without previous composition.
  document().setTitle(emptyString);
  document().updateStyleAndLayout();
  controller().commitText("", underlines, 0);
  EXPECT_STREQ("", document().title().utf8().data());

  document().setTitle(emptyString);
  controller().setComposition("n", underlines, 1, 1);
  EXPECT_STREQ("beforeinput.data:n;input.data:n;",
               document().title().utf8().data());

  // Insert empty text with previous composition.
  document().setTitle(emptyString);
  document().updateStyleAndLayout();
  controller().commitText("", underlines, 1);
  EXPECT_STREQ("beforeinput.data:;input.data:null;compositionend.data:;",
               document().title().utf8().data());
}

TEST_F(InputMethodControllerTest, CompositionEndEventWithNoSelection) {
  createHTMLWithCompositionEndEventListener(NoSelection);

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));

  controller().setComposition("hello", underlines, 1, 1);
  document().updateStyleAndLayout();
  EXPECT_EQ(1u, controller().getSelectionOffsets().start());
  EXPECT_EQ(1u, controller().getSelectionOffsets().end());

  // Confirm the ongoing composition. Note that it moves the caret to the end of
  // text [5,5] before firing 'compositonend' event.
  controller().finishComposingText(InputMethodController::DoNotKeepSelection);
  document().updateStyleAndLayout();
  EXPECT_TRUE(controller().getSelectionOffsets().isNull());
}

TEST_F(InputMethodControllerTest, FinishCompositionRemovedRange) {
  Element* inputA =
      insertHTMLElement("<input id='a' /><br><input type='tel' id='b' />", "a");

  EXPECT_EQ(WebTextInputTypeText, controller().textInputType());

  // The test requires non-empty composition.
  controller().setComposition("hello", Vector<CompositionUnderline>(), 5, 5);
  EXPECT_EQ(WebTextInputTypeText, controller().textInputType());

  // Remove element 'a'.
  inputA->setOuterHTML("", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(WebTextInputTypeNone, controller().textInputType());

  document().getElementById("b")->focus();
  EXPECT_EQ(WebTextInputTypeTelephone, controller().textInputType());

  controller().finishComposingText(InputMethodController::KeepSelection);
  EXPECT_EQ(WebTextInputTypeTelephone, controller().textInputType());
}

TEST_F(InputMethodControllerTest, ReflectsSpaceWithoutNbspMangling) {
  insertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<CompositionUnderline> underlines;
  controller().commitText(String("  "), underlines, 0);

  // In a contenteditable, multiple spaces or a space at the edge needs to be
  // nbsp to affect layout properly, but it confuses some IMEs (particularly
  // Vietnamese, see crbug.com/663880) to have their spaces reflected back to
  // them as nbsp.
  EXPECT_EQ(' ', controller().textInputInfo().value.ascii()[0]);
  EXPECT_EQ(' ', controller().textInputInfo().value.ascii()[1]);
}

TEST_F(InputMethodControllerTest, SetCompositionPlainTextWithUnderline) {
  insertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 1, Color(255, 0, 0), false, 0));

  controller().setComposition(" ", underlines, 1, 1);

  ASSERT_EQ(1u, document().markers().markers().size());

  EXPECT_EQ(0u, document().markers().markers()[0]->startOffset());
  EXPECT_EQ(1u, document().markers().markers()[0]->endOffset());
}

TEST_F(InputMethodControllerTest, CommitPlainTextWithUnderlineInsert) {
  insertHTMLElement("<div id='sample' contenteditable>Initial text.</div>",
                    "sample");

  Vector<CompositionUnderline> underlines;

  controller().setEditableSelectionOffsets(PlainTextRange(8, 8));

  underlines.push_back(CompositionUnderline(1, 11, Color(255, 0, 0), false, 0));

  controller().commitText(String("underlined"), underlines, 0);

  ASSERT_EQ(1u, document().markers().markers().size());

  EXPECT_EQ(9u, document().markers().markers()[0]->startOffset());
  EXPECT_EQ(19u, document().markers().markers()[0]->endOffset());
}

TEST_F(InputMethodControllerTest, CommitPlainTextWithUnderlineReplace) {
  insertHTMLElement("<div id='sample' contenteditable>Initial text.</div>",
                    "sample");

  Vector<CompositionUnderline> underlines;

  controller().setCompositionFromExistingText(underlines, 8, 12);

  underlines.push_back(CompositionUnderline(1, 11, Color(255, 0, 0), false, 0));

  controller().commitText(String("string"), underlines, 0);

  ASSERT_EQ(1u, document().markers().markers().size());

  EXPECT_EQ(9u, document().markers().markers()[0]->startOffset());
  EXPECT_EQ(15u, document().markers().markers()[0]->endOffset());
}

TEST_F(InputMethodControllerTest,
       CompositionUnderlineAppearsCorrectlyAfterNewline) {
  Element* div =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<CompositionUnderline> underlines;
  controller().setComposition(String("hello"), underlines, 6, 6);
  controller().finishComposingText(InputMethodController::KeepSelection);
  frame().editor().insertLineBreak();

  controller().setCompositionFromExistingText(underlines, 8, 8);

  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setComposition(String("world"), underlines, 0, 0);
  ASSERT_EQ(1u, document().markers().markers().size());

  // Verify composition underline shows up on the second line, not the first
  ASSERT_EQ(0u, document()
                    .markers()
                    .markersInRange(PlainTextRange(0, 5).createRange(*div),
                                    DocumentMarker::AllMarkers())
                    .size());
  ASSERT_EQ(1u, document()
                    .markers()
                    .markersInRange(PlainTextRange(6, 11).createRange(*div),
                                    DocumentMarker::AllMarkers())
                    .size());

  // Verify marker has correct start/end offsets (measured from the beginning
  // of the node, which is the beginning of the line)
  EXPECT_EQ(0u, document().markers().markers()[0]->startOffset());
  EXPECT_EQ(5u, document().markers().markers()[0]->endOffset());
}

TEST_F(InputMethodControllerTest, SelectionWhenFocusChangeFinishesComposition) {
  document().settings()->setScriptEnabled(true);
  Element* editable =
      insertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  editable->focus();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setComposition("foo", underlines, 3, 3);

  EXPECT_TRUE(controller().hasComposition());
  EXPECT_EQ(0u, controller().compositionRange()->startOffset());
  EXPECT_EQ(3u, controller().compositionRange()->endOffset());
  EXPECT_EQ(3, frame()
                   .selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .start()
                   .computeOffsetInContainerNode());

  // Insert 'test'.
  NonThrowableExceptionState exceptionState;
  document().execCommand("insertText", false, "test", exceptionState);

  EXPECT_TRUE(controller().hasComposition());
  EXPECT_EQ(7, frame()
                   .selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .start()
                   .computeOffsetInContainerNode());

  // Focus change finishes composition.
  editable->blur();
  editable->focus();

  // Make sure that caret is still at the end of the inserted text.
  EXPECT_FALSE(controller().hasComposition());
  EXPECT_EQ(7, frame()
                   .selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .start()
                   .computeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest, SetEmptyCompositionShouldNotMoveCaret) {
  HTMLTextAreaElement* textarea =
      toHTMLTextAreaElement(insertHTMLElement("<textarea id='txt'>", "txt"));

  textarea->setValue("abc\n");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));

  Vector<CompositionUnderline> underlines;
  underlines.push_back(CompositionUnderline(0, 3, Color(255, 0, 0), false, 0));
  controller().setComposition(String("def"), underlines, 0, 3);
  controller().setComposition(String(""), underlines, 0, 3);
  controller().commitText(String("def"), underlines, 0);

  EXPECT_STREQ("abc\ndef", textarea->value().utf8().data());
}

}  // namespace blink
