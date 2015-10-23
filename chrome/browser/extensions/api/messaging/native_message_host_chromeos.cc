// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/native_message_host.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "components/policy/core/common/policy_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/url_pattern.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_native_messaging_host.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// A simple NativeMessageHost that mimics the implementation of
// chrome/test/data/native_messaging/native_hosts/echo.py. It is currently
// used for testing by ExtensionApiTest::NativeMessagingBasic.

const char* const kEchoHostOrigins[] = {
    // ScopedTestNativeMessagingHost::kExtensionId
    "chrome-extension://knldjmfmopnpolahpmmgbagdohdnhkik/"};

class EchoHost : public NativeMessageHost {
 public:
  static scoped_ptr<NativeMessageHost> Create() {
    return scoped_ptr<NativeMessageHost>(new EchoHost());
  }

  EchoHost() : message_number_(0), client_(NULL) {}

  void Start(Client* client) override { client_ = client; }

  void OnMessage(const std::string& request_string) override {
    scoped_ptr<base::Value> request_value(
        base::JSONReader::DeprecatedRead(request_string));
    scoped_ptr<base::DictionaryValue> request(
      static_cast<base::DictionaryValue*>(request_value.release()));
    if (request_string.find("stopHostTest") != std::string::npos) {
      client_->CloseChannel(kNativeHostExited);
    } else if (request_string.find("bigMessageTest") != std::string::npos) {
      client_->CloseChannel(kHostInputOuputError);
    } else {
      ProcessEcho(*request);
    }
  };

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return base::ThreadTaskRunnerHandle::Get();
  };

 private:
  void ProcessEcho(const base::DictionaryValue& request) {
    base::DictionaryValue response;
    response.SetInteger("id", ++message_number_);
    response.Set("echo", request.CreateDeepCopy());
    response.SetString("caller_url", kEchoHostOrigins[0]);
    std::string response_string;
    base::JSONWriter::Write(response, &response_string);
    client_->PostMessageFromNativeHost(response_string);
  }

  int message_number_;
  Client* client_;

  DISALLOW_COPY_AND_ASSIGN(EchoHost);
};

struct BuiltInHost {
  const char* const name;
  const char* const* const allowed_origins;
  int allowed_origins_count;
  scoped_ptr<NativeMessageHost>(*create_function)();
};

scoped_ptr<NativeMessageHost> CreateIt2MeHost() {
  scoped_ptr<remoting::It2MeHostFactory> host_factory(
      new remoting::It2MeHostFactory());
  host_factory->set_policy_service(g_browser_process->policy_service());
  scoped_ptr<remoting::ChromotingHostContext> context =
      remoting::ChromotingHostContext::CreateForChromeOS(
          make_scoped_refptr(g_browser_process->system_request_context()),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::IO),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::UI),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::FILE));
  scoped_ptr<NativeMessageHost> host(new remoting::It2MeNativeMessagingHost(
      context.Pass(), host_factory.Pass()));
  return host.Pass();
}

// If you modify the list of allowed_origins, don't forget to update
// remoting/host/it2me/com.google.chrome.remote_assistance.json.jinja2
// to keep the two lists in sync.
// TODO(kelvinp): Load the native messaging manifest as a resource file into
// chrome and fetch the list of allowed_origins from the manifest (see
// crbug/424743).
const char* const kRemotingIt2MeOrigins[] = {
    "chrome-extension://ljacajndfccfgnfohlgkdphmbnpkjflk/",
    "chrome-extension://gbchcmhmhahfdphkhkmpfmihenigjmpp/",
    "chrome-extension://kgngmbheleoaphbjbaiobfdepmghbfah/",
    "chrome-extension://odkaodonbgfohohmklejpjiejmcipmib/",
    "chrome-extension://dokpleeekgeeiehdhmdkeimnkmoifgdd/",
    "chrome-extension://ajoainacpilcemgiakehflpbkbfipojk/",
    "chrome-extension://hmboipgjngjoiaeicfdifdoeacilalgc/"};

static const BuiltInHost kBuiltInHost[] = {
    {"com.google.chrome.test.echo", // ScopedTestNativeMessagingHost::kHostName
     kEchoHostOrigins,
     arraysize(kEchoHostOrigins),
     &EchoHost::Create},
     {"com.google.chrome.remote_assistance",
     kRemotingIt2MeOrigins,
     arraysize(kRemotingIt2MeOrigins),
     &CreateIt2MeHost},
};

bool MatchesSecurityOrigin(const BuiltInHost& host,
                           const std::string& extension_id) {
  GURL origin(std::string(kExtensionScheme) + "://" + extension_id);
  for (int i = 0; i < host.allowed_origins_count; i++) {
    URLPattern allowed_origin(URLPattern::SCHEME_ALL, host.allowed_origins[i]);
    if (allowed_origin.MatchesSecurityOrigin(origin)) {
      return true;
    }
  }
  return false;
}

}  // namespace

scoped_ptr<NativeMessageHost> NativeMessageHost::Create(
    gfx::NativeView native_view,
    const std::string& source_extension_id,
    const std::string& native_host_name,
    bool allow_user_level,
    std::string* error) {
  for (unsigned int i = 0; i < arraysize(kBuiltInHost); i++) {
    const BuiltInHost& host = kBuiltInHost[i];
    std::string name(host.name);
    if (name == native_host_name) {
      if (MatchesSecurityOrigin(host, source_extension_id)) {
        return (*host.create_function)();
      }
      *error = kForbiddenError;
      return nullptr;
    }
  }
  *error = kNotFoundError;
  return nullptr;
}

}  // namespace extensions
