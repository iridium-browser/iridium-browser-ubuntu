// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/logging.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/console_logger.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/command_listener_proxy.h"
#include "chrome/test/chromedriver/performance_logger.h"
#include "chrome/test/chromedriver/session.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#include <unistd.h>
#endif


namespace {

Log::Level g_log_level = Log::kWarning;

int64_t g_start_time = 0;

// Array indices are the Log::Level enum values.
const char* const kLevelToName[] = {
  "ALL",  // kAll
  "DEBUG",  // kDebug
  "INFO",  // kInfo
  "WARNING",  // kWarning
  "SEVERE",  // kError
  "OFF",  // kOff
};

const char* LevelToName(Log::Level level) {
  const int index = level - Log::kAll;
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), arraysize(kLevelToName));
  return kLevelToName[index];
}

struct LevelPair {
  const char* name;
  Log::Level level;
};

const LevelPair kNameToLevel[] = {
    {"ALL", Log::kAll},
    {"DEBUG", Log::kDebug},
    {"INFO", Log::kInfo},
    {"WARNING", Log::kWarning},
    {"SEVERE", Log::kError},
    {"OFF", Log::kOff},
};

Log::Level GetLevelFromSeverity(int severity) {
  switch (severity) {
    case logging::LOG_FATAL:
    case logging::LOG_ERROR:
      return Log::kError;
    case logging::LOG_WARNING:
      return Log::kWarning;
    case logging::LOG_INFO:
      return Log::kInfo;
    case logging::LOG_VERBOSE:
    default:
      return Log::kDebug;
  }
}

WebDriverLog* GetSessionLog() {
  Session* session = GetThreadLocalSession();
  if (!session)
    return NULL;
  return session->driver_log.get();
}

bool InternalIsVLogOn(int vlog_level) {
  WebDriverLog* session_log = GetSessionLog();
  Log::Level session_level = session_log ? session_log->min_level() : Log::kOff;
  Log::Level level = g_log_level < session_level ? g_log_level : session_level;
  return GetLevelFromSeverity(vlog_level * -1) >= level;
}

bool HandleLogMessage(int severity,
                      const char* file,
                      int line,
                      size_t message_start,
                      const std::string& str) {
  Log::Level level = GetLevelFromSeverity(severity);
  std::string message = str.substr(message_start);

  if (level >= g_log_level) {
    const char* level_name = LevelToName(level);
    std::string entry = base::StringPrintf(
        "[%.3lf][%s]: %s",
        base::TimeDelta(base::TimeTicks::Now() -
                        base::TimeTicks::FromInternalValue(g_start_time))
            .InSecondsF(),
        level_name,
        message.c_str());
    fprintf(stderr, "%s", entry.c_str());
    fflush(stderr);
  }

  WebDriverLog* session_log = GetSessionLog();
  if (session_log)
    session_log->AddEntry(level, message);

  return true;
}

}  // namespace

const char WebDriverLog::kBrowserType[] = "browser";
const char WebDriverLog::kDriverType[] = "driver";
const char WebDriverLog::kPerformanceType[] = "performance";

bool WebDriverLog::NameToLevel(const std::string& name, Log::Level* out_level) {
  for (size_t i = 0; i < arraysize(kNameToLevel); ++i) {
    if (name == kNameToLevel[i].name) {
      *out_level = kNameToLevel[i].level;
      return true;
    }
  }
  return false;
}

WebDriverLog::WebDriverLog(const std::string& type, Log::Level min_level)
    : type_(type), min_level_(min_level), entries_(new base::ListValue()) {
}

WebDriverLog::~WebDriverLog() {
  VLOG(1) << "Log type '" << type_ << "' lost "
          << entries_->GetSize() << " entries on destruction";
}

std::unique_ptr<base::ListValue> WebDriverLog::GetAndClearEntries() {
  std::unique_ptr<base::ListValue> ret(entries_.release());
  entries_.reset(new base::ListValue());
  return ret;
}

std::string WebDriverLog::GetFirstErrorMessage() const {
  for (base::ListValue::iterator it = entries_->begin();
       it != entries_->end();
       ++it) {
    base::DictionaryValue* log_entry = NULL;
    (*it)->GetAsDictionary(&log_entry);
    if (log_entry != NULL) {
      std::string level;
      if (log_entry->GetString("level", &level)) {
        if (level == kLevelToName[Log::kError]) {
          std::string message;
          if (log_entry->GetString("message", &message))
            return message;
        }
      }
    }
  }
  return std::string();
}

void WebDriverLog::AddEntryTimestamped(const base::Time& timestamp,
                                       Log::Level level,
                                       const std::string& source,
                                       const std::string& message) {
  if (level < min_level_)
    return;

  std::unique_ptr<base::DictionaryValue> log_entry_dict(
      new base::DictionaryValue());
  log_entry_dict->SetDouble("timestamp",
                            static_cast<int64_t>(timestamp.ToJsTime()));
  log_entry_dict->SetString("level", LevelToName(level));
  if (!source.empty())
    log_entry_dict->SetString("source", source);
  log_entry_dict->SetString("message", message);
  entries_->Append(std::move(log_entry_dict));
}

const std::string& WebDriverLog::type() const {
  return type_;
}

void WebDriverLog::set_min_level(Level min_level) {
  min_level_ = min_level;
}

Log::Level WebDriverLog::min_level() const {
  return min_level_;
}

bool InitLogging() {
  InitLogging(&InternalIsVLogOn);
  g_start_time = base::TimeTicks::Now().ToInternalValue();

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch("log-path")) {
    g_log_level = Log::kInfo;
    base::FilePath log_path = cmd_line->GetSwitchValuePath("log-path");
#if defined(OS_WIN)
    FILE* redir_stderr = _wfreopen(log_path.value().c_str(), L"w", stderr);
#else
    FILE* redir_stderr = freopen(log_path.value().c_str(), "w", stderr);
#endif
    if (!redir_stderr) {
      printf("Failed to redirect stderr to log file.\n");
      return false;
    }
  }
  if (cmd_line->HasSwitch("silent"))
    g_log_level = Log::kOff;

  if (cmd_line->HasSwitch("verbose"))
    g_log_level = Log::kAll;

  // Turn on VLOG for chromedriver. This is parsed during logging::InitLogging.
  cmd_line->AppendSwitchASCII("vmodule", "*/chrome/test/chromedriver/*=3");

  logging::SetMinLogLevel(logging::LOG_WARNING);
  logging::SetLogItems(false,   // enable_process_id
                       false,   // enable_thread_id
                       false,   // enable_timestamp
                       false);  // enable_tickcount
  logging::SetLogMessageHandler(&HandleLogMessage);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  return logging::InitLogging(logging_settings);
}

Status CreateLogs(const Capabilities& capabilities,
                  const Session* session,
                  ScopedVector<WebDriverLog>* out_logs,
                  ScopedVector<DevToolsEventListener>* out_devtools_listeners,
                  ScopedVector<CommandListener>* out_command_listeners) {
  ScopedVector<WebDriverLog> logs;
  ScopedVector<DevToolsEventListener> devtools_listeners;
  ScopedVector<CommandListener> command_listeners;
  Log::Level browser_log_level = Log::kWarning;
  const LoggingPrefs& prefs = capabilities.logging_prefs;

  for (LoggingPrefs::const_iterator iter = prefs.begin();
       iter != prefs.end();
       ++iter) {
    std::string type = iter->first;
    Log::Level level = iter->second;
    if (type == WebDriverLog::kPerformanceType) {
      if (level != Log::kOff) {
        WebDriverLog* log = new WebDriverLog(type, Log::kAll);
        logs.push_back(log);
        PerformanceLogger* perf_log =
            new PerformanceLogger(log, session,
                                  capabilities.perf_logging_prefs);
        // We use a proxy for |perf_log|'s |CommandListener| interface.
        // Otherwise, |perf_log| would be owned by both session->chrome and
        // |session|, which would lead to memory errors on destruction.
        // session->chrome will own |perf_log|, and |session| will own |proxy|.
        // session->command_listeners (the proxy) will be destroyed first.
        CommandListenerProxy* proxy = new CommandListenerProxy(perf_log);
        devtools_listeners.push_back(perf_log);
        command_listeners.push_back(proxy);
      }
    } else if (type == WebDriverLog::kBrowserType) {
      browser_log_level = level;
    } else if (type != WebDriverLog::kDriverType) {
      // Driver "should" ignore unrecognized log types, per Selenium tests.
      // For example the Java client passes the "client" log type in the caps,
      // which the server should never provide.
      LOG(WARNING) << "Ignoring unrecognized log type: " << type;
    }
  }
  // Create "browser" log -- should always exist.
  WebDriverLog* browser_log =
      new WebDriverLog(WebDriverLog::kBrowserType, browser_log_level);
  logs.push_back(browser_log);
  // If the level is OFF, don't even bother listening for DevTools events.
  if (browser_log_level != Log::kOff)
    devtools_listeners.push_back(new ConsoleLogger(browser_log));

  out_logs->swap(logs);
  out_devtools_listeners->swap(devtools_listeners);
  out_command_listeners->swap(command_listeners);
  return Status(kOk);
}
