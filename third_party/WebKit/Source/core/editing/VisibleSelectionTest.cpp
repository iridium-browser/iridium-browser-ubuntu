// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/VisibleSelection.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

#define LOREM_IPSUM \
    "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor " \
    "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud " \
    "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " \
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur." \
    "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt " \
    "mollit anim id est laborum."

namespace blink {

class VisibleSelectionTest : public ::testing::Test {
protected:
    virtual void SetUp() override;

    Document& document() const { return m_dummyPageHolder->document(); }

    static PassRefPtrWillBeRawPtr<ShadowRoot> createShadowRootForElementWithIDAndSetInnerHTML(TreeScope&, const char* hostElementID, const char* shadowRootContent);

    void setBodyContent(const char*);
    PassRefPtrWillBeRawPtr<ShadowRoot> setShadowContent(const char*);

    // Helper function to set the VisibleSelection base/extent.
    void setSelection(VisibleSelection& selection, int base) { setSelection(selection, base, base); }

    // Helper function to set the VisibleSelection base/extent.
    void setSelection(VisibleSelection& selection, int base, int extend)
    {
        Node* node = document().body()->firstChild();
        selection.setBase(Position(node, base, Position::PositionIsOffsetInAnchor));
        selection.setExtent(Position(node, extend, Position::PositionIsOffsetInAnchor));
    }

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
};

void VisibleSelectionTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

PassRefPtrWillBeRawPtr<ShadowRoot> VisibleSelectionTest::createShadowRootForElementWithIDAndSetInnerHTML(TreeScope& scope, const char* hostElementID, const char* shadowRootContent)
{
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = scope.getElementById(AtomicString::fromUTF8(hostElementID))->createShadowRoot(ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowRootContent), ASSERT_NO_EXCEPTION);
    return shadowRoot.release();
}

void VisibleSelectionTest::setBodyContent(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
}

PassRefPtrWillBeRawPtr<ShadowRoot> VisibleSelectionTest::setShadowContent(const char* shadowContent)
{
    return createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
}

TEST_F(VisibleSelectionTest, Initialisation)
{
    setBodyContent(LOREM_IPSUM);

    VisibleSelection selection;
    setSelection(selection, 0);

    EXPECT_FALSE(selection.isNone());
    EXPECT_TRUE(selection.isCaret());

    RefPtrWillBeRawPtr<Range> range = selection.firstRange();
    EXPECT_EQ(0, range->startOffset());
    EXPECT_EQ(0, range->endOffset());
    EXPECT_EQ("", range->text());
}

TEST_F(VisibleSelectionTest, ShadowCrossing)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> host = body->querySelector("#host", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> six = shadowRoot->querySelector("#s6", ASSERT_NO_EXCEPTION);

    ASSERT_UNUSED(two, two);
    (void)six;

    VisibleSelection selection(Position::firstPositionInNode(one.get()), Position::lastPositionInNode(shadowRoot.get()));

    EXPECT_EQ(Position(host.get(), Position::PositionIsBeforeAnchor), selection.start());
    EXPECT_EQ(Position(one->firstChild(), 0, Position::PositionIsOffsetInAnchor), selection.end());
}

TEST_F(VisibleSelectionTest, ShadowDistributedNodes)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> host = body->querySelector("#host", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    ASSERT_UNUSED(host, host);
    ASSERT_UNUSED(five, five);

    VisibleSelection selection(Position::firstPositionInNode(one.get()), Position::lastPositionInNode(two.get()));

    EXPECT_EQ(Position(one->firstChild(), 0, Position::PositionIsOffsetInAnchor), selection.start());
    EXPECT_EQ(Position(two->firstChild(), 2, Position::PositionIsOffsetInAnchor), selection.end());
}

TEST_F(VisibleSelectionTest, ShadowNested)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    const char* shadowContent2 = "<span id='s7'>77</span><content></content><span id='s8'>88</span>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot2 = createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot, "s5", shadowContent2);

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> host = body->querySelector("#host", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> host2 = shadowRoot->querySelector("#host2", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> eight = shadowRoot2->querySelector("#s8", ASSERT_NO_EXCEPTION);

    ASSERT_UNUSED(two, two);
    ASSERT_UNUSED(eight, eight);
    (void)host2;

    VisibleSelection selection(Position::firstPositionInNode(one.get()), Position::lastPositionInNode(shadowRoot2.get()));

    EXPECT_EQ(Position(host.get(), Position::PositionIsBeforeAnchor), selection.start());
    EXPECT_EQ(Position(one->firstChild(), 0, Position::PositionIsOffsetInAnchor), selection.end());
}

TEST_F(VisibleSelectionTest, WordGranularity)
{
    setBodyContent(LOREM_IPSUM);

    VisibleSelection selection;

    // Beginning of a word.
    {
        setSelection(selection, 0);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(5, range->endOffset());
        EXPECT_EQ("Lorem", range->text());
    }

    // Middle of a word.
    {
        setSelection(selection, 8);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(6, range->startOffset());
        EXPECT_EQ(11, range->endOffset());
        EXPECT_EQ("ipsum", range->text());
    }

    // End of a word.
    // FIXME: that sounds buggy, we might want to select the word _before_ instead
    // of the space...
    {
        setSelection(selection, 5);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(5, range->startOffset());
        EXPECT_EQ(6, range->endOffset());
        EXPECT_EQ(" ", range->text());
    }

    // Before comma.
    // FIXME: that sounds buggy, we might want to select the word _before_ instead
    // of the comma.
    {
        setSelection(selection, 26);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(26, range->startOffset());
        EXPECT_EQ(27, range->endOffset());
        EXPECT_EQ(",", range->text());
    }

    // After comma.
    {
        setSelection(selection, 27);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(27, range->startOffset());
        EXPECT_EQ(28, range->endOffset());
        EXPECT_EQ(" ", range->text());
    }

    // When selecting part of a word.
    {
        setSelection(selection, 0, 1);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(5, range->endOffset());
        EXPECT_EQ("Lorem", range->text());
    }

    // When selecting part of two words.
    {
        setSelection(selection, 2, 8);
        selection.expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection.firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(11, range->endOffset());
        EXPECT_EQ("Lorem ipsum", range->text());
    }
}

} // namespace blink
