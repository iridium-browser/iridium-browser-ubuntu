// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_

#include <string>

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"

class PrefService;

namespace base {
struct Feature;
}

namespace syncer {
class SyncService;
}

namespace autofill {

struct Suggestion;

extern const base::Feature kAutofillCreditCardAssist;
extern const base::Feature kAutofillCreditCardSigninPromo;
extern const base::Feature kAutofillScanCardholderName;
extern const base::Feature kAutofillCreditCardPopupLayout;
extern const char kCreditCardSigninPromoImpressionLimitParamKey[];
extern const char kAutofillCreditCardPopupSettingsSuggestionValueKey[];

// Returns true if autofill should be enabled. See also
// IsInAutofillSuggestionsDisabledExperiment below.
bool IsAutofillEnabled(const PrefService* pref_service);

// Returns true if autofill suggestions are disabled via experiment. The
// disabled experiment isn't the same as disabling autofill completely since we
// still want to run detection code for metrics purposes. This experiment just
// disables providing suggestions.
bool IsInAutofillSuggestionsDisabledExperiment();

// Returns whether the Autofill credit card signin promo should be shown.
bool IsAutofillCreditCardSigninPromoEnabled();

// Returns whether the Autofill credit card assist infobar should be shown.
bool IsAutofillCreditCardAssistEnabled();

// Returns the maximum number of impressions of the credit card signin promo, or
// 0 if there are no limits.
int GetCreditCardSigninPromoImpressionLimit();

// Returns true if the user should be offered to locally store unmasked cards.
// This controls whether the option is presented at all rather than the default
// response of the option.
bool OfferStoreUnmaskedCards();

// Returns true if uploading credit cards to Wallet servers is enabled. This
// requires the appropriate flags and user settings to be true and the user to
// be a member of a supported domain.
bool IsCreditCardUploadEnabled(const PrefService* pref_service,
                               const syncer::SyncService* sync_service,
                               const std::string& user_email);

// Returns whether the new Autofill credit card popup layout experiment is
// enabled.
bool IsAutofillCreditCardPopupLayoutExperimentEnabled();

// Returns the background color for credit card autofill popup, or
// |SK_ColorTRANSPARENT| if the new credit card autofill popup layout experiment
// is not enabled.
SkColor GetCreditCardPopupBackgroundColor();

// Returns the divider color for credit card autofill popup, or
// |SK_ColorTRANSPARENT| if the new credit card autofill popup layout experiment
// is not enabled.
SkColor GetCreditCardPopupDividerColor();

// Returns true if the credit card autofill popup suggestion value is displayed
// in bold type face.
bool IsCreditCardPopupValueBold();

// Returns the dropdown item height for autofill popup, returning 0 if the
// dropdown item height isn't configured in an experiment to tweak autofill
// popup layout.
unsigned int GetPopupDropdownItemHeight();

// Returns true if the icon in the credit card autofill popup must be displayed
// before the credit card value or any other suggestion text.
bool IsIconInCreditCardPopupAtStart();

// Modifies the suggestion value and label if the new credit card autofill popup
// experiment is enabled to tweak the display of the value and label.
void ModifyAutofillCreditCardSuggestion(struct Suggestion* suggestion);

// Returns the margin for the icon, label and between icon and label. Returns 0
// if the margin isn't configured in an experiment to tweak autofill popup
// layout.
unsigned int GetPopupMargin();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
