// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

#if !defined(OS_WIN)
#include "chrome/common/channel_info.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

using content::BrowserThread;

// static
ShellIntegration::DefaultWebClientSetPermission
    ShellIntegration::CanSetAsDefaultProtocolClient() {
  // Allowed as long as the browser can become the operating system default
  // browser.
  DefaultWebClientSetPermission permission = CanSetAsDefaultBrowser();

  // Set as default asynchronous is only supported for default web browser.
  return (permission == SET_DEFAULT_ASYNCHRONOUS) ? SET_DEFAULT_INTERACTIVE
                                                  : permission;
}

static const struct ShellIntegration::AppModeInfo* gAppModeInfo = NULL;

// static
void ShellIntegration::SetAppModeInfo(const struct AppModeInfo* info) {
  gAppModeInfo = info;
}

// static
const struct ShellIntegration::AppModeInfo* ShellIntegration::AppModeInfo() {
  return gAppModeInfo;
}

// static
bool ShellIntegration::IsRunningInAppMode() {
  return gAppModeInfo != NULL;
}

// static
base::CommandLine ShellIntegration::CommandLineArgsForLauncher(
    const GURL& url,
    const std::string& extension_app_id,
    const base::FilePath& profile_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::CommandLine new_cmd_line(base::CommandLine::NO_PROGRAM);

  AppendProfileArgs(
      extension_app_id.empty() ? base::FilePath() : profile_path,
      &new_cmd_line);

  // If |extension_app_id| is present, we use the kAppId switch rather than
  // the kApp switch (the launch url will be read from the extension app
  // during launch.
  if (!extension_app_id.empty()) {
    new_cmd_line.AppendSwitchASCII(switches::kAppId, extension_app_id);
  } else {
    // Use '--app=url' instead of just 'url' to launch the browser with minimal
    // chrome.
    // Note: Do not change this flag!  Old Gears shortcuts will break if you do!
    new_cmd_line.AppendSwitchASCII(switches::kApp, url.spec());
  }
  return new_cmd_line;
}

// static
void ShellIntegration::AppendProfileArgs(const base::FilePath& profile_path,
                                         base::CommandLine* command_line) {
  DCHECK(command_line);
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();

  // Use the same UserDataDir for new launches that we currently have set.
  base::FilePath user_data_dir =
      cmd_line.GetSwitchValuePath(switches::kUserDataDir);
#if defined(OS_MACOSX) || defined(OS_WIN)
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
#endif
  if (!user_data_dir.empty()) {
    // Make sure user_data_dir is an absolute path.
    user_data_dir = base::MakeAbsoluteFilePath(user_data_dir);
    if (!user_data_dir.empty() && base::PathExists(user_data_dir))
      command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }

#if defined(OS_CHROMEOS)
  base::FilePath profile = cmd_line.GetSwitchValuePath(
      chromeos::switches::kLoginProfile);
  if (!profile.empty())
    command_line->AppendSwitchPath(chromeos::switches::kLoginProfile, profile);
#else
  if (!profile_path.empty())
    command_line->AppendSwitchPath(switches::kProfileDirectory,
                                   profile_path.BaseName());
#endif
}

#if !defined(OS_WIN)

base::string16 ShellIntegration::GetAppShortcutsSubdirName() {
  if (chrome::GetChannel() == version_info::Channel::CANARY)
    return l10n_util::GetStringUTF16(IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY);
  return l10n_util::GetStringUTF16(IDS_APP_SHORTCUTS_SUBDIR_NAME);
}

// static
bool ShellIntegration::SetAsDefaultBrowserInteractive() {
  return false;
}

// static
bool ShellIntegration::IsSetAsDefaultAsynchronous() {
  return false;
}

// static
bool ShellIntegration::SetAsDefaultProtocolClientInteractive(
    const std::string& protocol) {
  return false;
}

// static
bool ShellIntegration::IsElevationNeededForSettingDefaultProtocolClient() {
  return false;
}

#endif  // !defined(OS_WIN)

bool ShellIntegration::DefaultWebClientObserver::IsOwnedByWorker() {
  return false;
}

bool ShellIntegration::DefaultWebClientObserver::
    IsInteractiveSetDefaultPermitted() {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultWebClientWorker
//

ShellIntegration::DefaultWebClientWorker::DefaultWebClientWorker(
    DefaultWebClientObserver* observer)
    : observer_(observer) {}

void ShellIntegration::DefaultWebClientWorker::StartCheckIsDefault() {
  if (observer_) {
    observer_->SetDefaultWebClientUIState(STATE_PROCESSING);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DefaultWebClientWorker::CheckIsDefault, this));
  }
}

void ShellIntegration::DefaultWebClientWorker::StartSetAsDefault() {
  // Cancel the already running process if another start is requested.
  if (set_as_default_in_progress_) {
    if (set_as_default_initialized_) {
      FinalizeSetAsDefault();
      set_as_default_initialized_ = false;
    }

    ReportAttemptResult(AttemptResult::RETRY);
  }

  set_as_default_in_progress_ = true;
  bool interactive_permitted = false;
  if (observer_) {
    observer_->SetDefaultWebClientUIState(STATE_PROCESSING);
    interactive_permitted = observer_->IsInteractiveSetDefaultPermitted();

    // The initialization is only useful when there is an observer.
    set_as_default_initialized_ = InitializeSetAsDefault();
  }

  // Remember the start time.
  start_time_ = base::TimeTicks::Now();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DefaultWebClientWorker::SetAsDefault,
                                     this, interactive_permitted));
}

void ShellIntegration::DefaultWebClientWorker::ObserverDestroyed() {
  // Our associated view has gone away, so we shouldn't call back to it if
  // our worker thread returns after the view is dead.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_ = nullptr;

  if (set_as_default_initialized_) {
    FinalizeSetAsDefault();
    set_as_default_initialized_ = false;
  }

  if (set_as_default_in_progress_)
    ReportAttemptResult(AttemptResult::ABANDONED);
}

///////////////////////////////////////////////////////////////////////////////
// DefaultWebClientWorker, private:

ShellIntegration::DefaultWebClientWorker::~DefaultWebClientWorker() {}

void ShellIntegration::DefaultWebClientWorker::OnCheckIsDefaultComplete(
    DefaultWebClientState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UpdateUI(state);
  // The worker has finished everything it needs to do, so free the observer
  // if we own it.
  if (observer_ && observer_->IsOwnedByWorker()) {
    delete observer_;
    observer_ = nullptr;
  }
}

void ShellIntegration::DefaultWebClientWorker::OnSetAsDefaultAttemptComplete(
    AttemptResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Hold on to a reference because if this was called via the default browser
  // callback in StartupBrowserCreator, clearing the callback in
  // FinalizeSetAsDefault would otherwise remove the last reference and delete
  // us in the middle of this function.
  scoped_refptr<DefaultWebClientWorker> scoped_ref(this);

  if (set_as_default_in_progress_) {
    set_as_default_in_progress_ = false;

    if (set_as_default_initialized_) {
      FinalizeSetAsDefault();
      set_as_default_initialized_ = false;
    }
    if (observer_) {
      bool succeeded = result == AttemptResult::SUCCESS ||
                       result == AttemptResult::ALREADY_DEFAULT;
      observer_->OnSetAsDefaultConcluded(succeeded);
    }

    ReportAttemptResult(result);

    // Start the default browser check which will notify the observer as to
    // whether Chrome is really the default browser. This is needed because
    // detecting that the process was successful is not 100% sure.
    // For example, on Windows 10+, the user might have unchecked the "Always
    // use this app" checkbox which can't be detected.
    StartCheckIsDefault();
  }
}

void ShellIntegration::DefaultWebClientWorker::ReportAttemptResult(
    AttemptResult result) {
  if (!ShouldReportAttemptResults())
    return;

  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.AsyncSetAsDefault.Result", result,
                            AttemptResult::NUM_ATTEMPT_RESULT_TYPES);

  switch (result) {
    case SUCCESS:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "DefaultBrowser.AsyncSetAsDefault.Duration_Success",
          base::TimeTicks::Now() - start_time_);
      break;
    case FAILURE:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "DefaultBrowser.AsyncSetAsDefault.Duration_Failure",
          base::TimeTicks::Now() - start_time_);
      break;
    case ABANDONED:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "DefaultBrowser.AsyncSetAsDefault.Duration_Abandoned",
          base::TimeTicks::Now() - start_time_);
      break;
    case RETRY:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "DefaultBrowser.AsyncSetAsDefault.Duration_Retry",
          base::TimeTicks::Now() - start_time_);
      break;
    default:
      break;
  }
}

bool ShellIntegration::DefaultWebClientWorker::InitializeSetAsDefault() {
  return true;
}

void ShellIntegration::DefaultWebClientWorker::FinalizeSetAsDefault() {}

#if !defined(OS_WIN)
// static
bool ShellIntegration::DefaultWebClientWorker::ShouldReportAttemptResults() {
  return false;
}
#endif  // !defined(OS_WIN)

void ShellIntegration::DefaultWebClientWorker::UpdateUI(
    DefaultWebClientState state) {
  if (observer_) {
    switch (state) {
      case NOT_DEFAULT:
        observer_->SetDefaultWebClientUIState(STATE_NOT_DEFAULT);
        break;
      case IS_DEFAULT:
        observer_->SetDefaultWebClientUIState(STATE_IS_DEFAULT);
        break;
      case UNKNOWN_DEFAULT:
        observer_->SetDefaultWebClientUIState(STATE_UNKNOWN);
        break;
      default:
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultBrowserWorker
//

ShellIntegration::DefaultBrowserWorker::DefaultBrowserWorker(
    DefaultWebClientObserver* observer)
    : DefaultWebClientWorker(observer) {
}

ShellIntegration::DefaultBrowserWorker::~DefaultBrowserWorker() {}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker, private:

void ShellIntegration::DefaultBrowserWorker::CheckIsDefault() {
  DefaultWebClientState state = GetDefaultBrowser();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DefaultBrowserWorker::OnCheckIsDefaultComplete, this, state));
}

void ShellIntegration::DefaultBrowserWorker::SetAsDefault(
    bool interactive_permitted) {
  AttemptResult result = AttemptResult::FAILURE;
  switch (CanSetAsDefaultBrowser()) {
    case SET_DEFAULT_NOT_ALLOWED:
      NOTREACHED();
      break;
    case SET_DEFAULT_UNATTENDED:
      if (SetAsDefaultBrowser())
        result = AttemptResult::SUCCESS;
      break;
    case SET_DEFAULT_INTERACTIVE:
      if (interactive_permitted && SetAsDefaultBrowserInteractive())
        result = AttemptResult::SUCCESS;
      break;
    case SET_DEFAULT_ASYNCHRONOUS:
#if defined(OS_WIN)
      if (!interactive_permitted)
        break;
      if (GetDefaultBrowser() == IS_DEFAULT) {
        // Don't start the asynchronous operation since it could result in
        // losing the default browser status.
        result = AttemptResult::ALREADY_DEFAULT;
        break;
      }
      // This function will cause OnSetAsDefaultAttemptComplete() to be called
      // asynchronously via a filter established in InitializeSetAsDefault().
      if (!SetAsDefaultBrowserAsynchronous()) {
        result = AttemptResult::LAUNCH_FAILURE;
        break;
      }
      return;
#else
      NOTREACHED();
      break;
#endif
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DefaultBrowserWorker::OnSetAsDefaultAttemptComplete, this,
                 result));
}

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultProtocolClientWorker
//

ShellIntegration::DefaultProtocolClientWorker::DefaultProtocolClientWorker(
    DefaultWebClientObserver* observer, const std::string& protocol)
    : DefaultWebClientWorker(observer),
      protocol_(protocol) {
}

///////////////////////////////////////////////////////////////////////////////
// DefaultProtocolClientWorker, private:

ShellIntegration::DefaultProtocolClientWorker::~DefaultProtocolClientWorker() {}

void ShellIntegration::DefaultProtocolClientWorker::CheckIsDefault() {
  DefaultWebClientState state = IsDefaultProtocolClient(protocol_);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DefaultProtocolClientWorker::OnCheckIsDefaultComplete, this,
                 state));
}

void ShellIntegration::DefaultProtocolClientWorker::SetAsDefault(
    bool interactive_permitted) {
  AttemptResult result = AttemptResult::FAILURE;
  switch (CanSetAsDefaultProtocolClient()) {
    case SET_DEFAULT_NOT_ALLOWED:
      // Not allowed, do nothing.
      break;
    case SET_DEFAULT_UNATTENDED:
      if (SetAsDefaultProtocolClient(protocol_))
        result = AttemptResult::SUCCESS;
      break;
    case SET_DEFAULT_INTERACTIVE:
      if (interactive_permitted &&
          SetAsDefaultProtocolClientInteractive(protocol_)) {
        result = AttemptResult::SUCCESS;
      }
      break;
    case SET_DEFAULT_ASYNCHRONOUS:
      NOTREACHED();
      break;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DefaultProtocolClientWorker::OnSetAsDefaultAttemptComplete,
                 this, result));
}
