// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_ACTIONS_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_ACTIONS_H_

#include <string>

#import <EarlGrey/EarlGrey.h>

namespace web {

// Action to longpress on element |element_id| in the shell's webview.
// If |triggers_context_menu| is true, this gesture is expected to
// cause the context menu to appear, and is not expected to trigger events
// in the webview. If |triggers_context_menu| is false, the converse is true.
// This action doesn't fail if the context menu isn't displayed; calling code
// should check for that separately with a matcher.
id<GREYAction> longPressElementForContextMenu(const std::string& element_id,
                                              bool triggers_context_menu);

}  // namespace web

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_ACTIONS_H_
