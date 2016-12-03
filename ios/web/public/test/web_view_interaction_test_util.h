// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state.h"

#include <string>

namespace web {
namespace test {

enum ElementAction { CLICK, FOCUS };

// Attempts to tap the element with |element_id| in the passed in |web_state|
// using a JavaScript click() event.
void TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id);

// Attempts to run the Javascript action specified by |action| on |element_id|
// in the passed |web_state|.
void RunActionOnWebViewElementWithId(web::WebState* web_state,
                                     const std::string& element_id,
                                     ElementAction action);

}  // namespace test
}  // namespace web
