// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Autofill component.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_

#include <stddef.h>         // For size_t

namespace autofill {

// Help URL for the Autofill dialog.
extern const char kHelpURL[];

// The number of fields required by Autofill.  Ideally we could send the forms
// to Autofill no matter how many fields are in the forms; however, finding the
// label for each field is a costly operation and we can't spare the cycles if
// it's not necessary.
extern const size_t kRequiredAutofillFields;

// Options bitmask values for AutofillHostMsg_ShowPasswordSuggestions IPC
enum ShowPasswordSuggestionsOptions {
  SHOW_ALL = 1 << 0 /* show all credentials, not just ones matching username */,
  IS_PASSWORD_FIELD = 1 << 1 /* input field is a password field */
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
