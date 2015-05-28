// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

}

class DevToolsProtocolTest : public ContentBrowserTest,
                             public DevToolsAgentHostClient {
 protected:
  void SendCommand(const std::string& method,
                   scoped_ptr<base::DictionaryValue> params) {
    base::DictionaryValue command;
    command.SetInteger(kIdParam, 1);
    command.SetString(kMethodParam, method);
    if (params)
      command.Set(kParamsParam, params.release());

    std::string json_command;
    base::JSONWriter::Write(&command, &json_command);
    agent_host_->DispatchProtocolMessage(json_command);
    base::MessageLoop::current()->Run();
  }

  bool HasValue(const std::string& path) {
    base::Value* value = 0;
    return result_->Get(path, &value);
  }

  bool HasListItem(const std::string& path_to_list,
                   const std::string& name,
                   const std::string& value) {
    base::ListValue* list;
    if (!result_->GetList(path_to_list, &list))
      return false;

    for (size_t i = 0; i != list->GetSize(); i++) {
      base::DictionaryValue* item;
      if (!list->GetDictionary(i, &item))
        return false;
      std::string id;
      if (!item->GetString(name, &id))
        return false;
      if (id == value)
        return true;
    }
    return false;
  }

  scoped_ptr<base::DictionaryValue> result_;
  scoped_refptr<DevToolsAgentHost> agent_host_;

 private:
  void SetUpOnMainThread() override {
    agent_host_ = DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
    agent_host_->AttachClient(this);
  }

  void TearDownOnMainThread() override {
    agent_host_->DetachClient();
    agent_host_ = NULL;
  }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    scoped_ptr<base::DictionaryValue> root(
        static_cast<base::DictionaryValue*>(base::JSONReader::Read(message)));
    base::DictionaryValue* result;
    EXPECT_TRUE(root->GetDictionary("result", &result));
    result_.reset(result->DeepCopy());
    base::MessageLoop::current()->QuitNow();
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host, bool replaced) override {
    EXPECT_TRUE(false);
  }
};

class CaptureScreenshotTest : public DevToolsProtocolTest {
 private:
#if !defined(OS_ANDROID)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
#endif
};

// Does not link on Android
#if defined(OS_ANDROID)
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#elif defined(OS_MACOSX)  // Fails on 10.9. http://crbug.com/430620
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#elif defined(MEMORY_SANITIZER)
// Also fails under MSAN. http://crbug.com/423583
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#else
#define MAYBE_CaptureScreenshot CaptureScreenshot
#endif
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest, MAYBE_CaptureScreenshot) {
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(content::ExecuteScript(
      shell()->web_contents()->GetRenderViewHost(),
      "document.body.style.background = '#123456'"));
  SendCommand("Page.captureScreenshot", nullptr);
  std::string base64;
  EXPECT_TRUE(result_->GetString("data", &base64));
  std::string png;
  EXPECT_TRUE(base::Base64Decode(base64, &png));
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(png.data()),
                        png.size(), &bitmap);
  SkColor color(bitmap.getColor(0, 0));
  EXPECT_TRUE(std::abs(0x12-(int)SkColorGetR(color)) <= 1);
  EXPECT_TRUE(std::abs(0x34-(int)SkColorGetG(color)) <= 1);
  EXPECT_TRUE(std::abs(0x56-(int)SkColorGetB(color)) <= 1);
}

}  // namespace content
