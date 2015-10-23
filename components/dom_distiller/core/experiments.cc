// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/experiments.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"

namespace dom_distiller {
DistillerHeuristicsType GetDistillerHeuristicsType() {
  // Get the field trial name first to ensure the experiment is initialized.
  const std::string group_name =
      base::FieldTrialList::FindFullName("ReaderModeUI");
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kReaderModeHeuristics);
  if (switch_value != "") {
    if (switch_value == switches::reader_mode_heuristics::kAdaBoost) {
      return DistillerHeuristicsType::ADABOOST_MODEL;
    }
    if (switch_value == switches::reader_mode_heuristics::kOGArticle) {
      return DistillerHeuristicsType::OG_ARTICLE;
    }
    if (switch_value == switches::reader_mode_heuristics::kAlwaysTrue) {
      return DistillerHeuristicsType::ALWAYS_TRUE;
    }
    if (switch_value == switches::reader_mode_heuristics::kNone) {
      return DistillerHeuristicsType::NONE;
    }
    NOTREACHED() << "Invalid value for " << switches::kReaderModeHeuristics;
  } else {
    if (group_name == "AdaBoost") {
      return DistillerHeuristicsType::ADABOOST_MODEL;
    }
    if (group_name == "OGArticle") {
      return DistillerHeuristicsType::OG_ARTICLE;
    }
  }
  return DistillerHeuristicsType::NONE;
}

bool ShouldShowFeedbackForm() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("ReaderModeUIFeedback");
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kReaderModeFeedback);
  if (switch_value != "") {
    if (switch_value == switches::reader_mode_feedback::kOn) {
      return true;
    }
    if (switch_value == switches::reader_mode_feedback::kOff) {
      return false;
    }
    NOTREACHED() << "Invalid value for " << switches::kReaderModeFeedback;
  } else {
    if (group_name == "DoNotShow") {
      return false;
    }
    if (group_name == "Show") {
      return true;
    }
  }
  return false;
}
}
