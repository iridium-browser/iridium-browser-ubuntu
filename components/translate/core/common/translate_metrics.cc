// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_metrics.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "url/url_constants.h"

namespace translate {

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kRenderer4LanguageDetection[] = "Renderer4.LanguageDetection";
const char kTranslateContentLanguage[] = "Translate.ContentLanguage";
const char kTranslateHtmlLang[] = "Translate.HtmlLang";
const char kTranslateLanguageVerification[] = "Translate.LanguageVerification";
const char kTranslateTimeToBeReady[] = "Translate.TimeToBeReady";
const char kTranslateTimeToLoad[] = "Translate.TimeToLoad";
const char kTranslateTimeToTranslate[] = "Translate.TimeToTranslate";
const char kTranslateUserActionDuration[] = "Translate.UserActionDuration";
const char kTranslatePageScheme[] = "Translate.PageScheme";
const char kTranslateSimilarLanguageMatch[] = "Translate.SimilarLanguageMatch";

struct MetricsEntry {
  MetricsNameIndex index;
  const char* const name;
};

// This entry table should be updated when new UMA items are added.
const MetricsEntry kMetricsEntries[] = {
    {UMA_LANGUAGE_DETECTION, kRenderer4LanguageDetection},
    {UMA_CONTENT_LANGUAGE, kTranslateContentLanguage},
    {UMA_HTML_LANG, kTranslateHtmlLang},
    {UMA_LANGUAGE_VERIFICATION, kTranslateLanguageVerification},
    {UMA_TIME_TO_BE_READY, kTranslateTimeToBeReady},
    {UMA_TIME_TO_LOAD, kTranslateTimeToLoad},
    {UMA_TIME_TO_TRANSLATE, kTranslateTimeToTranslate},
    {UMA_USER_ACTION_DURATION, kTranslateUserActionDuration},
    {UMA_PAGE_SCHEME, kTranslatePageScheme},
    {UMA_SIMILAR_LANGUAGE_MATCH, kTranslateSimilarLanguageMatch}, };

static_assert(arraysize(kMetricsEntries) == UMA_MAX,
              "kMetricsEntries should have UMA_MAX elements");

LanguageCheckType GetLanguageCheckMetric(const std::string& provided_code,
                                         const std::string& revised_code) {
  if (provided_code.empty())
    return LANGUAGE_NOT_PROVIDED;
  else if (provided_code == revised_code)
    return LANGUAGE_VALID;
  return LANGUAGE_INVALID;
}

}  // namespace

void ReportContentLanguage(const std::string& provided_code,
                           const std::string& revised_code) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateContentLanguage,
                            GetLanguageCheckMetric(provided_code, revised_code),
                            LANGUAGE_MAX);
}

void ReportHtmlLang(const std::string& provided_code,
                    const std::string& revised_code) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateHtmlLang,
                            GetLanguageCheckMetric(provided_code, revised_code),
                            LANGUAGE_MAX);
}

void ReportLanguageVerification(LanguageVerificationType type) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateLanguageVerification,
                            type,
                            LANGUAGE_VERIFICATION_MAX);
}

void ReportTimeToBeReady(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kTranslateTimeToBeReady,
                             base::TimeDelta::FromMicroseconds(
                                 static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportTimeToLoad(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kTranslateTimeToLoad,
                             base::TimeDelta::FromMicroseconds(
                                 static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportTimeToTranslate(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kTranslateTimeToTranslate,
                             base::TimeDelta::FromMicroseconds(
                                 static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportUserActionDuration(base::TimeTicks begin, base::TimeTicks end) {
  UMA_HISTOGRAM_LONG_TIMES(kTranslateUserActionDuration, end - begin);
}

void ReportPageScheme(const std::string& scheme) {
  SchemeType type = SCHEME_OTHERS;
  if (scheme == url::kHttpScheme)
    type = SCHEME_HTTP;
  else if (scheme == url::kHttpsScheme)
    type = SCHEME_HTTPS;
  UMA_HISTOGRAM_ENUMERATION(kTranslatePageScheme, type, SCHEME_MAX);
}

void ReportLanguageDetectionTime(base::TimeTicks begin, base::TimeTicks end) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kRenderer4LanguageDetection, end - begin);
}

void ReportSimilarLanguageMatch(bool match) {
  UMA_HISTOGRAM_BOOLEAN(kTranslateSimilarLanguageMatch, match);
}

const char* GetMetricsName(MetricsNameIndex index) {
  for (size_t i = 0; i < arraysize(kMetricsEntries); ++i) {
    if (kMetricsEntries[i].index == index)
      return kMetricsEntries[i].name;
  }
  NOTREACHED();
  return NULL;
}

}  // namespace translate
