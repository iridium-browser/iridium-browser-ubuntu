// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_

#include <vector>

#include "base/basictypes.h"

namespace autofill {
struct FormData;
class FormStructure;
}

@protocol AutofillDriverIOSBridge

- (void)onFormDataFilled:(uint16)query_id
                  result:(const autofill::FormData&)result;

- (void)sendAutofillTypePredictionsToRenderer:
        (const std::vector<autofill::FormStructure*>&)forms;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
