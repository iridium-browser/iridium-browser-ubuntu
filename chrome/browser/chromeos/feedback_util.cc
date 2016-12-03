// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/feedback_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"
#include "chrome/browser/profiles/profile.h"

using feedback::FeedbackData;

namespace feedback_util {

namespace {

extensions::FeedbackService* GetFeedbackService(Profile* profile) {
  return extensions::FeedbackPrivateAPI::GetFactoryInstance()
      ->Get(profile)
      ->GetService();
}

void OnGetSystemInformation(Profile* profile,
                            const std::string& description,
                            const SendSysLogFeedbackCallback& callback,
                            const extensions::SystemInformationList& sys_info) {
  scoped_refptr<FeedbackData> feedback_data(new FeedbackData());

  feedback_data->set_context(profile);
  feedback_data->set_description(description);

  std::unique_ptr<FeedbackData::SystemLogsMap> sys_logs(
      new FeedbackData::SystemLogsMap);
  for (const extensions::api::feedback_private::SystemInformation& info :
       sys_info) {
    (*sys_logs.get())[info.key] = info.value;
  }
  feedback_data->SetAndCompressSystemInfo(std::move(sys_logs));

  GetFeedbackService(profile)->SendFeedback(profile, feedback_data, callback);
}

}  // namespace

void SendSysLogFeedback(Profile* profile,
                        const std::string& description,
                        const SendSysLogFeedbackCallback& callback) {
  GetFeedbackService(profile)->GetSystemInformation(
      base::Bind(&OnGetSystemInformation, profile, description, callback));
}

}  // namespace feedback_util
