// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/editing/EditingStrategy.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class EditingStrategyTest : public EditingTestBase {
};

TEST_F(EditingStrategyTest, caretMaxOffset)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>1</b><b id='two'>22</b>333</p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
    Node* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");

    EXPECT_EQ(4, EditingStrategy::caretMaxOffset(*host));
    EXPECT_EQ(1, EditingStrategy::caretMaxOffset(*one));
    EXPECT_EQ(1, EditingStrategy::caretMaxOffset(*one->firstChild()));
    EXPECT_EQ(2, EditingStrategy::caretMaxOffset(*two->firstChild()));

    EXPECT_EQ(2, EditingInComposedTreeStrategy::caretMaxOffset(*host));
    EXPECT_EQ(1, EditingInComposedTreeStrategy::caretMaxOffset(*one));
    EXPECT_EQ(1, EditingInComposedTreeStrategy::caretMaxOffset(*one->firstChild()));
    EXPECT_EQ(2, EditingInComposedTreeStrategy::caretMaxOffset(*two->firstChild()));
}

} // namespace blink
