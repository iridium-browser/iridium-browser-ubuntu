// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_

#include <string>

#include "base/strings/string16.h"

namespace autofill {

class CardUnmaskDelegate {
 public:
  struct UnmaskResponse {
    UnmaskResponse();
    ~UnmaskResponse();

    // User input data.
    base::string16 cvc;

    // Two digit month.
    base::string16 exp_month;

    // Four digit year.
    base::string16 exp_year;

    // State of "copy to this device" checkbox.
    bool should_store_pan;

    // Risk fingerprint.
    std::string risk_data;

    // Whether we're providing the alternative "risk advisory data" in risk_data
    // (as opposed to the normal browser fingerprint).
    bool providing_risk_advisory_data;
  };

  // Called when the user has attempted a verification. Prompt is still
  // open at this point.
  virtual void OnUnmaskResponse(const UnmaskResponse& response) = 0;

  // Called when the unmask prompt is closed (e.g., cancelled).
  virtual void OnUnmaskPromptClosed() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_
