// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SCRUBBED_SYSTEM_LOGS_FETCHER_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SCRUBBED_SYSTEM_LOGS_FETCHER_H_

#include "base/macros.h"
#include "chrome/browser/feedback/system_logs/system_logs_fetcher_base.h"
#include "components/feedback/anonymizer_tool.h"

namespace system_logs {

// The ScrubbedSystemLogsFetcher aggregates the scrubbed logs for sending
// with feedback reports.
// TODO: This class needs to be expanded to provide better scrubbing of
// system logs.
class ScrubbedSystemLogsFetcher : public SystemLogsFetcherBase {
 public:
  ScrubbedSystemLogsFetcher();
  ~ScrubbedSystemLogsFetcher() override;

 protected:
  void Rewrite(const std::string& source_name,
               SystemLogsResponse* response) override;

 private:
  feedback::AnonymizerTool anonymizer_;

  DISALLOW_COPY_AND_ASSIGN(ScrubbedSystemLogsFetcher);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SCRUBBED_SYSTEM_LOGS_FETCHER_H_

