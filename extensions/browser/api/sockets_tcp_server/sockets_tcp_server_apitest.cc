// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/dns/host_resolver_wrapper.h"
#include "extensions/browser/api/dns/mock_host_resolver_creator.h"
#include "extensions/browser/api/sockets_tcp_server/sockets_tcp_server_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/test_util.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace extensions {

const std::string kHostname = "127.0.0.1";
const int kPort = 8888;

class SocketsTcpServerApiTest : public ShellApiTest {
 public:
  SocketsTcpServerApiTest()
      : resolver_event_(true, false),
        resolver_creator_(new MockHostResolverCreator()) {}

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    HostResolverWrapper::GetInstance()->SetHostResolverForTesting(
        resolver_creator_->CreateMockHostResolver());
  }

  void TearDownOnMainThread() override {
    HostResolverWrapper::GetInstance()->SetHostResolverForTesting(NULL);
    resolver_creator_->DeleteMockHostResolver();

    ShellApiTest::TearDownOnMainThread();
  }

 private:
  base::WaitableEvent resolver_event_;

  // The MockHostResolver asserts that it's used on the same thread on which
  // it's created, which is actually a stronger rule than its real counterpart.
  // But that's fine; it's good practice.
  scoped_refptr<MockHostResolverCreator> resolver_creator_;
};

IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest, SocketTCPCreateGood) {
  scoped_refptr<core_api::SocketsTcpServerCreateFunction>
      socket_create_function(new core_api::SocketsTcpServerCreateFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  scoped_ptr<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          socket_create_function.get(), "[]", browser_context()));
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  base::DictionaryValue* value =
      static_cast<base::DictionaryValue*>(result.get());
  int socketId = -1;
  EXPECT_TRUE(value->GetInteger("socketId", &socketId));
  ASSERT_TRUE(socketId > 0);
}

IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest, SocketTCPServerExtension) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());
  ExtensionTestMessageListener listener("info_please", true);
  ASSERT_TRUE(LoadApp("sockets_tcp_server/api"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("tcp_server:%s:%d", kHostname.c_str(), kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest, SocketTCPServerUnbindOnUnload) {
  std::string path("sockets_tcp_server/unload");
  ResultCatcher catcher;
  const Extension* extension = LoadApp(path);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  UnloadApp(extension);

  ASSERT_TRUE(LoadApp(path)) << message_;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
