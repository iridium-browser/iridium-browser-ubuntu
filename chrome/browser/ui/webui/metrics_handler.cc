// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"
#include "chrome/common/ntp_logging_events.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

using base::ListValue;
using base::UserMetricsAction;
using content::WebContents;

MetricsHandler::MetricsHandler() {}
MetricsHandler::~MetricsHandler() {}

void MetricsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordAction",
      base::Bind(&MetricsHandler::HandleRecordAction, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordInHistogram",
      base::Bind(&MetricsHandler::HandleRecordInHistogram,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:logEventTime",
      base::Bind(&MetricsHandler::HandleLogEventTime, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:logMouseover",
      base::Bind(&MetricsHandler::HandleLogMouseover, base::Unretained(this)));
}

void MetricsHandler::HandleRecordAction(const base::ListValue* args) {
  std::string string_action = base::UTF16ToUTF8(ExtractStringValue(args));
  content::RecordComputedAction(string_action);
}

void MetricsHandler::HandleRecordInHistogram(const base::ListValue* args) {
  std::string histogram_name;
  double value;
  double boundary_value;
  if (!args->GetString(0, &histogram_name) ||
      !args->GetDouble(1, &value) ||
      !args->GetDouble(2, &boundary_value)) {
    NOTREACHED();
    return;
  }

  int int_value = static_cast<int>(value);
  int int_boundary_value = static_cast<int>(boundary_value);
  if (int_boundary_value >= 4000 ||
      int_value > int_boundary_value ||
      int_value < 0) {
    NOTREACHED();
    return;
  }

  int bucket_count = int_boundary_value;
  while (bucket_count >= 100) {
    bucket_count /= 10;
  }

  // As |histogram_name| may change between calls, the UMA_HISTOGRAM_ENUMERATION
  // macro cannot be used here.
  base::HistogramBase* counter =
      base::LinearHistogram::FactoryGet(
          histogram_name, 1, int_boundary_value, bucket_count + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(int_value);
}

void MetricsHandler::HandleLogEventTime(const base::ListValue* args) {
  std::string event_name = base::UTF16ToUTF8(ExtractStringValue(args));
  WebContents* tab = web_ui()->GetWebContents();

  // Not all new tab pages get timed. In those cases, we don't have a
  // new_tab_start_time_.
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(tab);
  if (core_tab_helper->new_tab_start_time().is_null())
    return;

  base::TimeDelta duration =
      base::TimeTicks::Now() - core_tab_helper->new_tab_start_time();

  if (event_name == "Tab.NewTabScriptStart") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabScriptStart", duration);
  } else if (event_name == "Tab.NewTabDOMContentLoaded") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabDOMContentLoaded", duration);
  } else if (event_name == "Tab.NewTabOnload") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabOnload", duration);
    // The new tab page has finished loading; reset it.
    CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(tab);
    core_tab_helper->set_new_tab_start_time(base::TimeTicks());
  } else {
    NOTREACHED();
  }
}

void MetricsHandler::HandleLogMouseover(const base::ListValue* args) {
#if !defined(OS_ANDROID)
  // Android uses native UI for NTP.
  NTPUserDataLogger::GetOrCreateFromWebContents(
    web_ui()->GetWebContents())->LogEvent(NTP_MOUSEOVER,
                                          base::TimeDelta::FromMilliseconds(0));
#endif  // !defined(OS_ANDROID)
}
