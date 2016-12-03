// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser_tests/blimp_browser_test.h"

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "blimp/client/core/blimp_client_switches.h"
#include "blimp/client/core/session/assignment_source.h"
#include "blimp/common/switches.h"
#include "blimp/engine/app/blimp_browser_main_parts.h"
#include "blimp/engine/app/blimp_content_browser_client.h"
#include "blimp/engine/app/blimp_engine_config.h"
#include "blimp/engine/app/switches.h"
#include "blimp/engine/app/test_content_main_delegate.h"
#include "blimp/engine/session/blimp_engine_session.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"

namespace blimp {
namespace {
const char kTestDataFilePath[] = "blimp/test/data";
const char kClientTokenFilePath[] = "blimp/test/data/test_client_token";
const char kClientToken[] = "MyVoiceIsMyPassport";
}  // namespace


BlimpBrowserTest::BlimpBrowserTest()
    : completion_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {
  CreateTestServer(base::FilePath(FILE_PATH_LITERAL(kTestDataFilePath)));
}

BlimpBrowserTest::~BlimpBrowserTest() {}

void BlimpBrowserTest::RunUntilCompletion() {
  while (!completion_event_.IsSignaled()) {
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  }
  completion_event_.Reset();
}

void BlimpBrowserTest::SignalCompletion() {
  completion_event_.Signal();
}

void BlimpBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);
  BrowserTestBase::SetUp();
}

engine::BlimpEngineSession* BlimpBrowserTest::GetEngineSession() {
  return engine::TestContentMainDelegate::GetInstance()
      ->browser_client()
      ->blimp_browser_main_parts()
      ->GetBlimpEngineSession();
}

client::Assignment BlimpBrowserTest::GetAssignment() {
  client::Assignment assignment;
  assignment.client_token = kClientToken;
  assignment.engine_endpoint =
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), engine_port_);
  assignment.transport_protocol = client::Assignment::TransportProtocol::TCP;
  return assignment;
}

void BlimpBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  // Engine switches.
  blimp::engine::SetCommandLineDefaults(command_line);

  // Pass through the engine port if it is passed to the test.
  // Otherwise, use a dynamic port.
  if (!command_line->HasSwitch(blimp::engine::kEnginePort)) {
    command_line->AppendSwitchASCII(blimp::engine::kEnginePort, "0");
  }

  base::FilePath src_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  command_line->AppendSwitchASCII(kClientTokenPath,
      src_root.Append(kClientTokenFilePath).value());
}

void BlimpBrowserTest::SetUpOnMainThread() {
  // Get the connection's port number across the IO/UI thread boundary.
  GetEngineSession()->GetEnginePortForTesting(base::Bind(
      &BlimpBrowserTest::OnGetEnginePortCompletion, base::Unretained(this)));

  RunUntilCompletion();
}

void BlimpBrowserTest::TearDownOnMainThread() {
  base::MessageLoop::current()->QuitWhenIdle();
}

void BlimpBrowserTest::RunTestOnMainThreadLoop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  SetUpOnMainThread();
  RunTestOnMainThread();
  TearDownOnMainThread();

  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }
}

void BlimpBrowserTest::OnGetEnginePortCompletion(uint16_t port) {
  engine_port_ = port;
  SignalCompletion();
}

}  // namespace blimp
