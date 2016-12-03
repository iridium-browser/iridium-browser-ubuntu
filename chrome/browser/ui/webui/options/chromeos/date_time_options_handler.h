// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DATE_TIME_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DATE_TIME_OPTIONS_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chromeos/dbus/system_clock_client.h"

namespace chromeos {
namespace options {

// Chrome OS handler for the set date/time link in the Advanced settings page.
class DateTimeOptionsHandler : public ::options::OptionsPageUIHandler,
                               public SystemClockClient::Observer {
 public:
  DateTimeOptionsHandler();
  ~DateTimeOptionsHandler() override;

  // OptionsPageUIHandler:
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;
  void RegisterMessages() override;

 private:
  // SystemClockClient::Observer:
  void SystemClockCanSetTimeChanged(bool can_set_time) override;

  // Callback for the "showSetTime" message to show the set time dialog. No
  // arguments are expected.
  void HandleShowSetTime(const base::ListValue* args);

  // Only expose the button and dialog if the system time can be set.
  bool can_set_time_;
  bool page_initialized_;

  DISALLOW_COPY_AND_ASSIGN(DateTimeOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DATE_TIME_OPTIONS_HANDLER_H_
