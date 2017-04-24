// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/feature_entry.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace flags_ui {

std::string FeatureEntry::NameForOption(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_VARIATIONS_VALUE);
  DCHECK_LT(index, num_options);
  return std::string(internal_name) + testing::kMultiSeparator +
         base::IntToString(index);
}

base::string16 FeatureEntry::DescriptionForOption(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_VARIATIONS_VALUE);
  DCHECK_LT(index, num_options);
  int description_id;
  if (type == FeatureEntry::ENABLE_DISABLE_VALUE ||
      type == FeatureEntry::FEATURE_VALUE) {
    const int kEnableDisableDescriptionIds[] = {
        IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT,
        IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
        IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    };
    description_id = kEnableDisableDescriptionIds[index];
  } else if (type == FeatureEntry::FEATURE_WITH_VARIATIONS_VALUE) {
    if (index == 0) {
      description_id = IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT;
    } else if (index == 1) {
      description_id = IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED;
    } else if (index < num_options - 1) {
      // First two options do not have variations params.
      int variation_index = index - 2;
      return l10n_util::GetStringUTF16(IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED) +
             base::ASCIIToUTF16(" ") +
             base::ASCIIToUTF16(
                 feature_variations[variation_index].description_text);
    } else {
      DCHECK_EQ(num_options - 1, index);
      description_id = IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED;
    }
  } else {
    description_id = choices[index].description_id;
  }
  return l10n_util::GetStringUTF16(description_id);
}

const FeatureEntry::Choice& FeatureEntry::ChoiceForOption(int index) const {
  DCHECK_EQ(FeatureEntry::MULTI_VALUE, type);
  DCHECK_LT(index, num_options);

  return choices[index];
}

FeatureEntry::FeatureState FeatureEntry::StateForOption(int index) const {
  DCHECK(type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_VARIATIONS_VALUE);
  DCHECK_LT(index, num_options);

  if (index == 0)
    return FeatureEntry::FeatureState::DEFAULT;
  else if (index == num_options - 1)
    return FeatureEntry::FeatureState::DISABLED;
  else
    return FeatureEntry::FeatureState::ENABLED;
}

const FeatureEntry::FeatureVariation* FeatureEntry::VariationForOption(
    int index) const {
  DCHECK(type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_VARIATIONS_VALUE);
  DCHECK_LT(index, num_options);

  if (type == FeatureEntry::FEATURE_WITH_VARIATIONS_VALUE && index > 1 &&
      index < num_options - 1) {
    // We have no variations for FEATURE_VALUE type. Option at |index|
    // corresponds to variation at |index| - 2 as the list starts with "Default"
    // and "Enabled" (with default parameters).
    return &feature_variations[index - 2];
  }
  return nullptr;
}

namespace testing {

// WARNING: '@' is also used in the html file. If you update this constant you
// also need to update the html file.
const char kMultiSeparator[] = "@";

}  // namespace testing

}  // namespace flags_ui
