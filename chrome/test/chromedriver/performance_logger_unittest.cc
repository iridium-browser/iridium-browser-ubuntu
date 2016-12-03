// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/performance_logger.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct DevToolsCommand {
  DevToolsCommand(const std::string& in_method,
                  base::DictionaryValue* in_params)
      : method(in_method) {
    params.reset(in_params);
  }
  ~DevToolsCommand() {}

  std::string method;
  std::unique_ptr<base::DictionaryValue> params;
};

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  explicit FakeDevToolsClient(const std::string& id)
      : id_(id), listener_(nullptr), command_index_(0) {}
  ~FakeDevToolsClient() override {}

  bool PopSentCommand(DevToolsCommand** out_command) {
    if (sent_commands_.size() > command_index_) {
      *out_command = sent_commands_.get().at(command_index_++);
      return true;
    }
    return false;
  }

  Status TriggerEvent(const std::string& method) {
    base::DictionaryValue empty_params;
    return listener_->OnEvent(this, method, empty_params);
  }

  Status TriggerEvent(const std::string& method,
                      const base::DictionaryValue& params) {
    return listener_->OnEvent(this, method, params);
  }

  // Overridden from DevToolsClient:
  Status ConnectIfNecessary() override { return listener_->OnConnected(this); }

  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      std::unique_ptr<base::DictionaryValue>* result) override {
    sent_commands_.push_back(new DevToolsCommand(method,
                                                 params.DeepCopy()));
    return Status(kOk);
  }

  void AddListener(DevToolsEventListener* listener) override {
    CHECK(!listener_);
    listener_ = listener;
  }

  const std::string& GetId() override { return id_; }

 private:
  const std::string id_;  // WebView id.
  ScopedVector<DevToolsCommand> sent_commands_;  // Commands that were sent.
  DevToolsEventListener* listener_;  // The fake allows only one event listener.
  size_t command_index_;
};

struct LogEntry {
  const base::Time timestamp;
  const Log::Level level;
  const std::string source;
  const std::string message;

  LogEntry(const base::Time& timestamp,
           Log::Level level,
           const std::string& source,
           const std::string& message)
      : timestamp(timestamp), level(level), source(source), message(message) {}
};

class FakeLog : public Log {
 public:
  void AddEntryTimestamped(const base::Time& timestamp,
                           Level level,
                           const std::string& source,
                           const std::string& message) override;

  const ScopedVector<LogEntry>& GetEntries() {
    return entries_;
  }

 private:
  ScopedVector<LogEntry> entries_;
};

void FakeLog::AddEntryTimestamped(const base::Time& timestamp,
                                  Level level,
                                  const std::string& source,
                                  const std::string& message) {
  entries_.push_back(new LogEntry(timestamp, level, source, message));
}

std::unique_ptr<base::DictionaryValue> ParseDictionary(
    const std::string& json) {
  std::string error;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, nullptr, &error);
  if (value == nullptr) {
    SCOPED_TRACE(json.c_str());
    SCOPED_TRACE(error.c_str());
    ADD_FAILURE();
    return std::unique_ptr<base::DictionaryValue>();
  }
  base::DictionaryValue* dict = nullptr;
  if (!value->GetAsDictionary(&dict)) {
    SCOPED_TRACE("JSON object is not a dictionary");
    ADD_FAILURE();
    return std::unique_ptr<base::DictionaryValue>();
  }
  return std::unique_ptr<base::DictionaryValue>(dict->DeepCopy());
}

void ValidateLogEntry(const LogEntry *entry,
                      const std::string& expected_webview,
                      const std::string& expected_method,
                      const base::DictionaryValue& expected_params) {
  EXPECT_EQ(Log::kInfo, entry->level);
  EXPECT_LT(0, entry->timestamp.ToTimeT());

  std::unique_ptr<base::DictionaryValue> message(
      ParseDictionary(entry->message));
  std::string webview;
  EXPECT_TRUE(message->GetString("webview", &webview));
  EXPECT_EQ(expected_webview, webview);
  std::string method;
  EXPECT_TRUE(message->GetString("message.method", &method));
  EXPECT_EQ(expected_method, method);
  base::DictionaryValue* params;
  EXPECT_TRUE(message->GetDictionary("message.params", &params));
  EXPECT_TRUE(params->Equals(&expected_params));
}

void ValidateLogEntry(const LogEntry *entry,
                      const std::string& expected_webview,
                      const std::string& expected_method) {
  base::DictionaryValue empty_params;
  ValidateLogEntry(entry, expected_webview, expected_method, empty_params);
}

void ExpectCommand(FakeDevToolsClient* client, const std::string& method) {
  DevToolsCommand* cmd;
  // Use ASSERT so that test fails if no command is returned.
  ASSERT_TRUE(client->PopSentCommand(&cmd));
  EXPECT_EQ(method, cmd->method);
}

void ExpectEnableDomains(FakeDevToolsClient* client) {
  ExpectCommand(client, "Network.enable");
  ExpectCommand(client, "Page.enable");
}

}  // namespace

TEST(PerformanceLogger, OneWebView) {
  FakeDevToolsClient client("webview-1");
  FakeLog log;
  Session session("test");
  PerformanceLogger logger(&log, &session);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  ExpectEnableDomains(&client);
  ASSERT_EQ(kOk, client.TriggerEvent("Network.gaga").code());
  ASSERT_EQ(kOk, client.TriggerEvent("Page.ulala").code());
  // Ignore -- different domain.
  ASSERT_EQ(kOk, client.TriggerEvent("Console.bad").code());

  ASSERT_EQ(2u, log.GetEntries().size());
  ValidateLogEntry(log.GetEntries()[0], "webview-1", "Network.gaga");
  ValidateLogEntry(log.GetEntries()[1], "webview-1", "Page.ulala");
}

TEST(PerformanceLogger, TwoWebViews) {
  FakeDevToolsClient client1("webview-1");
  FakeDevToolsClient client2("webview-2");
  FakeLog log;
  Session session("test");
  PerformanceLogger logger(&log, &session);

  client1.AddListener(&logger);
  client2.AddListener(&logger);
  logger.OnConnected(&client1);
  logger.OnConnected(&client2);
  ExpectEnableDomains(&client1);
  ExpectEnableDomains(&client2);
  // OnConnected sends the enable command only to that client, not others.
  client1.ConnectIfNecessary();
  ExpectEnableDomains(&client1);
  DevToolsCommand* cmd;
  ASSERT_FALSE(client2.PopSentCommand(&cmd));

  ASSERT_EQ(kOk, client1.TriggerEvent("Page.gaga1").code());
  ASSERT_EQ(kOk, client2.TriggerEvent("Network.gaga2").code());

  ASSERT_EQ(2u, log.GetEntries().size());
  ValidateLogEntry(log.GetEntries()[0], "webview-1", "Page.gaga1");
  ValidateLogEntry(log.GetEntries()[1], "webview-2", "Network.gaga2");
}

TEST(PerformanceLogger, PerfLoggingPrefs) {
  FakeDevToolsClient client("webview-1");
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  EXPECT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled,
            prefs.network);
  prefs.network = PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyDisabled;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  ExpectCommand(&client, "Page.enable");
  // Do not expect Timeline.enable command since specifying trace categories
  // implicitly disables Timeline feed.
  DevToolsCommand* cmd;
  ASSERT_FALSE(client.PopSentCommand(&cmd));
}

namespace {

class FakeBrowserwideClient : public FakeDevToolsClient {
 public:
  FakeBrowserwideClient()
      : FakeDevToolsClient(DevToolsClientImpl::kBrowserwideDevToolsClientId),
        events_handled_(false) {}
  ~FakeBrowserwideClient() override {}

  bool events_handled() const {
    return events_handled_;
  }

  // Overridden from DevToolsClient:
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override {
    TriggerEvent("Tracing.tracingComplete");
    events_handled_ = true;
    return Status(kOk);
  }

 private:
  bool events_handled_;
};

}  // namespace

TEST(PerformanceLogger, TracingStartStop) {
  FakeBrowserwideClient client;
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  DevToolsCommand* cmd;
  ASSERT_TRUE(client.PopSentCommand(&cmd));
  EXPECT_EQ("Tracing.start", cmd->method);
  std::string expected_cats;
  EXPECT_TRUE(cmd->params->GetString("categories", &expected_cats));
  EXPECT_EQ("benchmark,blink.console", expected_cats);
  int expected_interval = 0;
  EXPECT_TRUE(cmd->params->GetInteger("bufferUsageReportingInterval",
                                      &expected_interval));
  EXPECT_GT(expected_interval, 0);
  ASSERT_FALSE(client.PopSentCommand(&cmd));

  EXPECT_FALSE(client.events_handled());
  // Trigger a dump of the DevTools trace buffer.
  ASSERT_EQ(kOk, logger.BeforeCommand("GetLog").code());
  EXPECT_TRUE(client.events_handled());
  ExpectCommand(&client, "Tracing.end");
  ExpectCommand(&client, "Tracing.start");  // Tracing should re-start.
  ASSERT_FALSE(client.PopSentCommand(&cmd));
}

TEST(PerformanceLogger, RecordTraceEvents) {
  FakeBrowserwideClient client;
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  base::DictionaryValue params;
  base::ListValue* trace_events = new base::ListValue();
  base::DictionaryValue* event1 = new base::DictionaryValue();
  event1->SetString("cat", "foo");
  trace_events->Append(event1);
  base::DictionaryValue* event2 = new base::DictionaryValue();
  event2->SetString("cat", "bar");
  trace_events->Append(event2);
  params.Set("value", trace_events);
  ASSERT_EQ(kOk, client.TriggerEvent("Tracing.dataCollected", params).code());

  ASSERT_EQ(2u, log.GetEntries().size());
  ValidateLogEntry(log.GetEntries()[0],
                   DevToolsClientImpl::kBrowserwideDevToolsClientId,
                   "Tracing.dataCollected", *event1);
  ValidateLogEntry(log.GetEntries()[1],
                   DevToolsClientImpl::kBrowserwideDevToolsClientId,
                   "Tracing.dataCollected", *event2);
}

TEST(PerformanceLogger, ShouldRequestTraceEvents) {
  FakeBrowserwideClient client;
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  EXPECT_FALSE(client.events_handled());
  // Trace events should not be dumped for commands not in whitelist.
  ASSERT_EQ(kOk, logger.BeforeCommand("Blah").code());
  EXPECT_FALSE(client.events_handled());
  ASSERT_EQ(kOk, logger.BeforeCommand("Foo").code());
  EXPECT_FALSE(client.events_handled());
  // Trace events should always be dumped for GetLog command.
  ASSERT_EQ(kOk, logger.BeforeCommand("GetLog").code());
  EXPECT_TRUE(client.events_handled());
}

TEST(PerformanceLogger, WarnWhenTraceBufferFull) {
  FakeBrowserwideClient client;
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  base::DictionaryValue params;
  params.SetDouble("value", 1.0);
  ASSERT_EQ(kOk, client.TriggerEvent("Tracing.bufferUsage", params).code());

  ASSERT_EQ(1u, log.GetEntries().size());
  LogEntry* entry = log.GetEntries()[0];
  EXPECT_EQ(Log::kWarning, entry->level);
  EXPECT_LT(0, entry->timestamp.ToTimeT());
  std::unique_ptr<base::DictionaryValue> message(
      ParseDictionary(entry->message));
  std::string webview;
  EXPECT_TRUE(message->GetString("webview", &webview));
  EXPECT_EQ(DevToolsClientImpl::kBrowserwideDevToolsClientId, webview);
  std::string method;
  EXPECT_TRUE(message->GetString("message.method", &method));
  EXPECT_EQ("Tracing.bufferUsage", method);
  base::DictionaryValue* actual_params;
  EXPECT_TRUE(message->GetDictionary("message.params", &actual_params));
  EXPECT_TRUE(actual_params->HasKey("error"));
}
