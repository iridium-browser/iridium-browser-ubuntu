// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarms_api.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/common/api/alarms.h"
#include "extensions/common/error_utils.h"

namespace extensions {

namespace alarms = core_api::alarms;

namespace {

const char kDefaultAlarmName[] = "";
const char kBothRelativeAndAbsoluteTime[] =
    "Cannot set both when and delayInMinutes.";
const char kNoScheduledTime[] =
    "Must set at least one of when, delayInMinutes, or periodInMinutes.";
const int kReleaseDelayMinimum = 1;
const int kDevDelayMinimum = 0;

bool ValidateAlarmCreateInfo(const std::string& alarm_name,
                             const alarms::AlarmCreateInfo& create_info,
                             const Extension* extension,
                             std::string* error,
                             std::vector<std::string>* warnings) {
  if (create_info.delay_in_minutes.get() && create_info.when.get()) {
    *error = kBothRelativeAndAbsoluteTime;
    return false;
  }
  if (create_info.delay_in_minutes == NULL && create_info.when == NULL &&
      create_info.period_in_minutes == NULL) {
    *error = kNoScheduledTime;
    return false;
  }

  // Users can always use an absolute timeout to request an arbitrarily-short or
  // negative delay.  We won't honor the short timeout, but we can't check it
  // and warn the user because it would introduce race conditions (say they
  // compute a long-enough timeout, but then the call into the alarms interface
  // gets delayed past the boundary).  However, it's still worth warning about
  // relative delays that are shorter than we'll honor.
  if (create_info.delay_in_minutes.get()) {
    if (*create_info.delay_in_minutes < kReleaseDelayMinimum) {
      static_assert(kReleaseDelayMinimum == 1,
                    "warning message must be updated");
      if (Manifest::IsUnpackedLocation(extension->location()))
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            "Alarm delay is less than minimum of 1 minutes."
            " In released .crx, alarm \"*\" will fire in approximately"
            " 1 minutes.",
            alarm_name));
      else
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            "Alarm delay is less than minimum of 1 minutes."
            " Alarm \"*\" will fire in approximately 1 minutes.",
            alarm_name));
    }
  }
  if (create_info.period_in_minutes.get()) {
    if (*create_info.period_in_minutes < kReleaseDelayMinimum) {
      static_assert(kReleaseDelayMinimum == 1,
                    "warning message must be updated");
      if (Manifest::IsUnpackedLocation(extension->location()))
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            "Alarm period is less than minimum of 1 minutes."
            " In released .crx, alarm \"*\" will fire approximately"
            " every 1 minutes.",
            alarm_name));
      else
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            "Alarm period is less than minimum of 1 minutes."
            " Alarm \"*\" will fire approximately every 1 minutes.",
            alarm_name));
    }
  }

  return true;
}

}  // namespace

AlarmsCreateFunction::AlarmsCreateFunction()
    : clock_(new base::DefaultClock()), owns_clock_(true) {
}

AlarmsCreateFunction::AlarmsCreateFunction(base::Clock* clock)
    : clock_(clock), owns_clock_(false) {
}

AlarmsCreateFunction::~AlarmsCreateFunction() {
  if (owns_clock_)
    delete clock_;
}

bool AlarmsCreateFunction::RunAsync() {
  scoped_ptr<alarms::Create::Params> params(
      alarms::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const std::string& alarm_name =
      params->name.get() ? *params->name : kDefaultAlarmName;
  std::vector<std::string> warnings;
  if (!ValidateAlarmCreateInfo(alarm_name, params->alarm_info, extension(),
                               &error_, &warnings)) {
    return false;
  }
  for (std::vector<std::string>::const_iterator it = warnings.begin();
       it != warnings.end(); ++it)
    WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_WARNING, *it);

  Alarm alarm(alarm_name, params->alarm_info,
              base::TimeDelta::FromMinutes(
                  Manifest::IsUnpackedLocation(extension()->location())
                      ? kDevDelayMinimum
                      : kReleaseDelayMinimum),
              clock_->Now());
  AlarmManager::Get(browser_context())
      ->AddAlarm(extension_id(), alarm,
                 base::Bind(&AlarmsCreateFunction::Callback, this));

  return true;
}

void AlarmsCreateFunction::Callback() {
  SendResponse(true);
}

bool AlarmsGetFunction::RunAsync() {
  scoped_ptr<alarms::Get::Params> params(alarms::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.get() ? *params->name : kDefaultAlarmName;
  AlarmManager::Get(browser_context())
      ->GetAlarm(extension_id(), name,
                 base::Bind(&AlarmsGetFunction::Callback, this, name));

  return true;
}

void AlarmsGetFunction::Callback(const std::string& name,
                                 extensions::Alarm* alarm) {
  if (alarm) {
    results_ = alarms::Get::Results::Create(*alarm->js_alarm);
  }
  SendResponse(true);
}

bool AlarmsGetAllFunction::RunAsync() {
  AlarmManager::Get(browser_context())
      ->GetAllAlarms(extension_id(),
                     base::Bind(&AlarmsGetAllFunction::Callback, this));
  return true;
}

void AlarmsGetAllFunction::Callback(const AlarmList* alarms) {
  if (alarms) {
    std::vector<linked_ptr<alarms::Alarm>> create_arg;
    create_arg.reserve(alarms->size());
    for (size_t i = 0, size = alarms->size(); i < size; ++i) {
      create_arg.push_back((*alarms)[i].js_alarm);
    }
    results_ = alarms::GetAll::Results::Create(create_arg);
  } else {
    SetResult(new base::ListValue());
  }
  SendResponse(true);
}

bool AlarmsClearFunction::RunAsync() {
  scoped_ptr<alarms::Clear::Params> params(
      alarms::Clear::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.get() ? *params->name : kDefaultAlarmName;
  AlarmManager::Get(browser_context())
      ->RemoveAlarm(extension_id(), name,
                    base::Bind(&AlarmsClearFunction::Callback, this, name));

  return true;
}

void AlarmsClearFunction::Callback(const std::string& name, bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
}

bool AlarmsClearAllFunction::RunAsync() {
  AlarmManager::Get(browser_context())
      ->RemoveAllAlarms(extension_id(),
                        base::Bind(&AlarmsClearAllFunction::Callback, this));
  return true;
}

void AlarmsClearAllFunction::Callback() {
  SetResult(new base::FundamentalValue(true));
  SendResponse(true);
}

}  // namespace extensions
